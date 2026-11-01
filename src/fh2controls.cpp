// FH2Controls: keyboard and controller bindings for Forza Horizon 2 (Azimuth), from one ini.
//
// It does what KeyboardDrive, CustomBindings and MapButton did, in one mod:
//
//  * The keyboard is a controller of the mod's own, without ViGEmBus: Azimuth gets the
//    controllers from Windows.Gaming.Input, and the mod adds one to the list it gets (wgi.h).
//    Mods that read XInput (FnFNitro's nitro button) see it as pad 0.
//  * Keys are bound to the game's actions (Gas, ShiftUp, Map...) or to a controller input (A, LB,
//    DPadUp...). On the road a key that does an action presses whatever input the game has that
//    action on right now (read from the game's own bindings every frame), and does nothing else;
//    keys bound to inputs only press them when no action of the road uses that input. In a menu
//    keys press their controller inputs only. So the same key can be shift up on the road and
//    d-pad up in a menu, and Enter (A) can't pull the handbrake.
//    "On the road" is when the game asks for the road's actions: it does every frame while you
//    drive, and doesn't in the menus (game+0x1072510 / +0x106E3E0, player 0).
//  * [Controller] is a layout for real controllers (CustomBindings' format), put in when the
//    game loads its layout from ControllerLayouts (game+0x2DC300, the column read at +0x2DC495).
//  * Map opens the quick map while you drive: the game asks for it from WaitForPause
//    (action 22, the call at game+0x964B80), and the mod says yes for the Map key or button.
#include "fh2mod.h"

#include "core.h"
#include "wgi.h"

#include <windows.h>

#include <intrin.h>
#include <tlhelp32.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {

constexpr const char* kVersion = "2.1.0";
constexpr int kTickMs          = 4;
constexpr DWORD kRoadMs        = 250; // on the road while the game asked for its actions this recently

const FH2ModApi* g_api = nullptr;
DWORD g_exc_until = 0;        // the startup exception log (see exc_handler) runs until then
void exc_write(const char* s);
FH2Mod* g_mod          = nullptr;
std::wstring g_ini;

void log_fmt(const char* fmt, ...) {
    char line[600];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    if (g_api) g_api->log(g_mod, line);
}

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// ---- Settings -----------------------------------------------------------------------------------
std::mutex g_cfg_lock;
std::shared_ptr<const fc::Config> g_cfg = std::make_shared<fc::Config>();
std::atomic<bool> g_enabled{true};
std::atomic<float> g_steer_time{0.25f}, g_return_time{0.15f}, g_pedal_time{0.10f};
std::atomic<float> g_launch_rpm{0.60f};
FILETIME g_ini_time{};

std::shared_ptr<const fc::Config> config() {
    std::lock_guard<std::mutex> l(g_cfg_lock);
    return g_cfg;
}

bool read_file(const std::wstring& path, std::string* out, FILETIME* time) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    out->assign(static_cast<size_t>(std::min<LONGLONG>(size.QuadPart, 1 << 20)), '\0');
    DWORD got = 0;
    const BOOL ok = out->empty() || ReadFile(f, out->data(), static_cast<DWORD>(out->size()), &got, nullptr);
    out->resize(got);
    if (time) GetFileTime(f, nullptr, nullptr, time);
    CloseHandle(f);
    return ok != 0;
}

FILETIME file_time(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return FILETIME{};
    return a.ftLastWriteTime;
}

void write_setting(const char* key, const std::string& value) {
    WritePrivateProfileStringW(L"Settings", widen(key).c_str(), widen(value).c_str(), g_ini.c_str());
}

std::string read_setting(const char* key, const char* fallback) {
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"Settings", widen(key).c_str(), widen(fallback).c_str(), buf, 64, g_ini.c_str());
    std::string s;
    for (wchar_t* p = buf; *p; ++p) s += static_cast<char>(*p < 128 ? *p : '?');
    return s;
}

int g_map_column = -1; // [Controller]'s column with Map, if any

void load_config(bool first) {
    std::string text;
    FILETIME t{};
    if (!read_file(g_ini, &text, &t)) {
        log_fmt("couldn't read %ls: keeping the bindings it had", g_ini.c_str());
        return;
    }
    g_ini_time = t;
    auto cfg   = std::make_shared<fc::Config>(fc::parse_config(text));
    for (const std::string& p : cfg->problems) log_fmt("FH2Controls.ini, %s (left out)", p.c_str());
    int keys = 0;
    for (int vk = 0; vk < 256; ++vk) keys += cfg->keys.any(static_cast<uint8_t>(vk)) ? 1 : 0;
    g_enabled     = cfg->enabled;
    g_steer_time  = cfg->steer_time;
    g_return_time = cfg->return_time;
    g_pedal_time  = cfg->pedal_time;
    g_launch_rpm  = cfg->launch_rpm;
    if (g_api && !first) {
        g_api->set_setting(g_mod, "Enabled", cfg->enabled ? 1 : 0);
        g_api->set_slider(g_mod, "SteeringTime", cfg->steer_time);
        g_api->set_slider(g_mod, "ReturnTime", cfg->return_time);
        g_api->set_slider(g_mod, "PedalTime", cfg->pedal_time);
        g_api->set_slider(g_mod, "LaunchRPM", cfg->launch_rpm);
    }
    log_fmt("%s FH2Controls.ini: %d keys bound, controller layout %s", first ? "read" : "read again", keys,
            cfg->custom_layout ? "from [Controller]" : "the game's");
    std::lock_guard<std::mutex> l(g_cfg_lock);
    g_cfg = std::move(cfg);
}

// ---- Moving over from KeyboardDrive / CustomBindings ------------------------------------------------
// The first time, FH2Controls.ini is the default one with CustomBindings' layout ([Bindings] in
// fh2-mods\CustomBindings.ini) in [Controller], and KeyboardDrive's speeds in [Settings].
void write_first_ini() {
    std::string text = fc::kDefaultIni;
    const std::wstring cb = std::wstring(g_api->azimuth_dir) + L"\\fh2-mods\\CustomBindings.ini";
    if (GetFileAttributesW(cb.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::string ctrl = "[Controller]\n; from CustomBindings.ini\nCustom = 1\n";
        for (int c = 0; c < fc::kColumnCount; ++c) {
            wchar_t buf[64] = {};
            GetPrivateProfileStringW(L"Bindings", widen(fc::kColumns[c].name).c_str(), L"", buf, 64, cb.c_str());
            std::string v;
            for (wchar_t* p = buf; *p; ++p) v += static_cast<char>(*p);
            ctrl += std::string(fc::kColumns[c].name) + " = " + v + "\n";
        }
        const size_t at = text.find("\n[Controller]\n"); // the section, not the comments' mention of it
        if (at != std::string::npos) text = text.substr(0, at + 1) + ctrl;
    }
    const std::wstring kd = std::wstring(g_api->azimuth_dir) + L"\\KeyboardDrive\\KeyboardDrive.ini";
    if (GetFileAttributesW(kd.c_str()) != INVALID_FILE_ATTRIBUTES) {
        for (const char* key : {"SteeringTime", "ReturnTime", "PedalTime"}) {
            wchar_t buf[32] = {};
            GetPrivateProfileStringW(L"KeyboardDrive", widen(key).c_str(), L"", buf, 32, kd.c_str());
            if (!*buf) continue;
            std::string v;
            for (wchar_t* p = buf; *p; ++p) v += static_cast<char>(*p);
            const std::string line = std::string(key) + " = ";
            const size_t at = text.find(line);
            if (at != std::string::npos) text.replace(at + line.size(), text.find('\n', at) - at - line.size(), v);
        }
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_ini.c_str(), L"wb") == 0 && f) {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
        log_fmt("wrote FH2Controls.ini%s", GetFileAttributesW(cb.c_str()) != INVALID_FILE_ATTRIBUTES
                                                 ? " (the controller layout from CustomBindings.ini)" : "");
    }
}

// ---- Keys ---------------------------------------------------------------------------------------
using ForegroundFn = HWND(WINAPI*)();
ForegroundFn g_foreground = &GetForegroundWindow; // win32u's, when there: FH2NoPause hooks user32's

std::atomic<int> g_test_front{-1};
std::atomic<bool> g_test_keys{false};
std::atomic<bool> g_front{false};
bool g_down[256];
uint32_t g_order[256];
uint32_t g_press_count = 0;
std::atomic<bool> g_test_down[256];

bool game_in_front() {
    const int test = g_test_front;
    if (test >= 0) return test != 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(g_foreground(), &pid);
    return pid == GetCurrentProcessId();
}

void read_keys(const fc::Keymap& km, bool front) {
    for (int vk = 8; vk < 255; ++vk) {
        bool down = false;
        if (g_test_keys) down = g_test_down[vk];
        else if (front && km.any(static_cast<uint8_t>(vk))) down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down && !g_down[vk]) g_order[vk] = ++g_press_count;
        g_down[vk] = down;
    }
}

// ---- What the game has on what (kept by the game's thread, read by the pad thread) ----------------
std::atomic<int8_t> g_action_on[fc::kActionIds]; // fc::Frame::action_on
std::atomic<bool> g_input_used[fc::kElemCount];  // an input the road's actions use
std::atomic<bool> g_road{false};                 // on the road (see kRoadMs)
std::atomic<bool> g_wanted_live[fc::kActionIds]; // keys want this action, which they can't press

// The driven car, as the HUD's rev counter reads it each frame (game thread): see hud_car below.
std::atomic<float> g_car_rpm{0}, g_car_redline{0}, g_car_speed{0};
std::atomic<DWORD> g_car_at{0};
// The physics input frame (7 floats: [0] gas, [3] clutch, [4] handbrake per Extra Options), for the log.
SRWLOCK g_frame_lock = SRWLOCK_INIT;
float g_car_frame[7];

// ---- The reading: the pad thread makes it, Azimuth and XInput read it -------------------------------
SRWLOCK g_reading_lock = SRWLOCK_INIT;
fc::Reading g_reading;
std::atomic<uint32_t> g_reading_count{0};

fc::Reading current_reading() {
    AcquireSRWLockShared(&g_reading_lock);
    const fc::Reading r = g_reading;
    ReleaseSRWLockShared(&g_reading_lock);
    return r;
}

wgi::KeyPad* g_keypad = nullptr;
std::atomic<bool> g_gave_azimuth{false};

std::atomic<bool> g_read_logged{false};

void wgi_read(wgi::GamepadReading* out) {
    if (!g_read_logged.exchange(true)) log_fmt("Azimuth is reading the keyboard's controller");
    const fc::Reading r = current_reading();
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    out->Timestamp        = static_cast<uint64_t>(now.QuadPart * 1000000 / freq.QuadPart);
    out->Buttons          = fc::wgi_buttons(r.buttons);
    out->LeftTrigger      = r.lt;
    out->RightTrigger     = r.rt;
    out->LeftThumbstickX  = r.lx;
    out->LeftThumbstickY  = r.ly;
    out->RightThumbstickX = r.rx;
    out->RightThumbstickY = r.ry;
}

DWORD WINAPI pad_thread(LPVOID) {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    fc::Smooth smooth;
    fc::Launch launch;
    int launch_logs    = 0;
    bool no_car_logged = false;
    DWORD checked_ini  = GetTickCount();
    for (;;) {
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -10000LL * kTickMs;
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, 100);
        } else {
            Sleep(kTickMs);
        }
        LARGE_INTEGER now_qpc;
        QueryPerformanceCounter(&now_qpc);
        float dt = static_cast<float>(now_qpc.QuadPart - last.QuadPart) / static_cast<float>(freq.QuadPart);
        last     = now_qpc;
        if (dt > 0.1f) dt = 0.1f;

        const DWORD now = GetTickCount();
        if (now - checked_ini >= 500) { // edited while the game runs: read it again
            checked_ini = now;
            const FILETIME t = file_time(g_ini);
            if (CompareFileTime(&t, &g_ini_time) != 0 && (t.dwLowDateTime || t.dwHighDateTime)) load_config(false);
        }

        const auto cfg   = config();
        const bool front = game_in_front();
        g_front          = front;
        read_keys(cfg->keys, front);

        int8_t action_on[fc::kActionIds];
        bool used[fc::kElemCount];
        for (int i = 0; i < fc::kActionIds; ++i) action_on[i] = g_action_on[i];
        for (int e = 0; e < fc::kElemCount; ++e) used[e] = g_input_used[e];
        bool down[256];
        for (int vk = 0; vk < 256; ++vk) down[vk] = g_down[vk] && g_enabled && front;
        fc::Frame frame{down, g_order, g_road.load(), action_on, used};
        const fc::Feel feel{g_steer_time, g_return_time, g_pedal_time};
        fc::Wanted wanted;
        fc::Reading r = fc::make_reading(cfg->keys, frame, smooth, dt, feel, &wanted);
        for (int i = 0; i < fc::kActionIds; ++i) g_wanted_live[i] = wanted.action[i];

        // Launch control: only on the road, with Gas on RT (the keys' gas is the trigger then).
        fc::LaunchIn lin{};
        if (frame.driving && action_on[0] == fc::RT) {
            for (int vk = 0; vk < 256; ++vk) {
                if (!down[vk]) continue;
                for (int i = 0; i < cfg->keys.count[vk]; ++i) {
                    const int t = cfg->keys.target[vk][i];
                    if (!fc::is_action_target(t)) continue;
                    const fc::Action& a = fc::kActions[t - fc::kElemCount];
                    if (a.launch) lin.key = true;
                    if (a.id == 0) lin.gas = true;
                }
            }
        }
        lin.car_known = now - g_car_at.load() < 250;
        lin.rpm       = g_car_rpm;
        lin.redline   = g_car_redline;
        lin.speed     = g_car_speed;
        lin.share     = g_launch_rpm;
        const bool was_held  = launch.held;
        const int launches   = launch.launches;
        const float gas      = fc::launch_control(launch, lin, dt);
        if (gas >= 0) {
            r.rt = smooth.rt = gas;
            if (!was_held && launch_logs < 20) {
                ++launch_logs;
                log_fmt("launch control: holding %.0f rpm (%.0f%% of the %.0f rpm redline)", g_launch_rpm * lin.redline,
                        g_launch_rpm * 100.0f, lin.redline);
            }
        }
        if (gas >= 0) {
            // holding: logged above
        } else if (launch.launches != launches && launch_logs < 20) {
            ++launch_logs;
            log_fmt("launch control: launch at %.0f rpm", lin.rpm);
        } else if (lin.key && lin.gas && !lin.car_known && !no_car_logged) {
            no_car_logged = true;
            log_fmt("launch control: the car's revs aren't known (is the HUD hidden?): not holding them");
        }

        AcquireSRWLockExclusive(&g_reading_lock);
        const bool changed = g_reading != r;
        g_reading          = r;
        ReleaseSRWLockExclusive(&g_reading_lock);
        if (changed) ++g_reading_count;
    }
}

// ---- XInput (FnFNitro's nitro button, and anything else that reads pad 0 through it) -----------------
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetStateFn g_orig_xinput = nullptr;

int16_t xstick(float v) { return static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f)); }
BYTE xtrigger(float v) { return static_cast<BYTE>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); }

DWORD WINAPI xinput_detour(DWORD index, XINPUT_STATE* state) {
    const DWORD r = g_orig_xinput(index, state);
    if (index != 0 || !state || !g_enabled) return r;
    const fc::Reading k = current_reading();
    if (r != ERROR_SUCCESS) *state = XINPUT_STATE{};
    XINPUT_GAMEPAD& g = state->Gamepad;
    g.wButtons |= fc::xinput_buttons(k.buttons);
    g.bLeftTrigger  = std::max(g.bLeftTrigger, xtrigger(k.lt));
    g.bRightTrigger = std::max(g.bRightTrigger, xtrigger(k.rt));
    if (k.lx != 0) g.sThumbLX = xstick(k.lx);
    if (k.ly != 0) g.sThumbLY = xstick(k.ly);
    if (k.rx != 0) g.sThumbRX = xstick(k.rx);
    if (k.ry != 0) g.sThumbRY = xstick(k.ry);
    state->dwPacketNumber += g_reading_count.load();
    return ERROR_SUCCESS;
}

// ---- Azimuth's controllers ---------------------------------------------------------------------
using RoGetActivationFactoryFn = HRESULT(WINAPI*)(HSTRING, REFIID, void**);
RoGetActivationFactoryFn g_orig_factory = nullptr;

HRESULT WINAPI factory_detour(HSTRING cls, REFIID iid, void** out) {
    const HRESULT hr = g_orig_factory(cls, iid, out);
    UINT32 len        = 0;
    const wchar_t* name = WindowsGetStringRawBuffer(cls, &len);
    if (!name || wcscmp(name, wgi::kGamepadClass) != 0 || iid != wgi::kIGamepadStatics || !out) return hr;
    auto* real = SUCCEEDED(hr) ? static_cast<wgi::IGamepadStaticsRaw*>(*out) : nullptr;
    *out       = static_cast<wgi::IGamepadStaticsRaw*>(new wgi::Statics(real, g_keypad));
    if (!g_gave_azimuth.exchange(true))
        log_fmt("gave Azimuth the keyboard's controller%s", real ? ", after the real ones" : " (Windows.Gaming.Input isn't there)");
    return S_OK;
}

// azimuth.exe's import of RoGetActivationFactory.
void** factory_import() {
    auto* base = reinterpret_cast<char*>(GetModuleHandleW(nullptr));
    auto* dos  = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt   = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return nullptr;
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        if (!d->OriginalFirstThunk) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto* by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(by_name->Name), "RoGetActivationFactory") == 0)
                return reinterpret_cast<void**>(&slots->u1.Function);
        }
    }
    return nullptr;
}

// ---- The game: actions, its bindings, the layout, the map -----------------------------------------
constexpr uintptr_t kIsActionPressed = 0x1072510; // bool (manager, player, action, use it up)
constexpr uintptr_t kActionValue     = 0x106E3E0; // float (manager, player, action): 0 to 1
constexpr uintptr_t kTriggerType     = 0x106E960; // int (player's trigger table, action)
constexpr uintptr_t kButtonState     = 0x106EEF0; // int (manager, input): 0 up, 1 just pressed...
constexpr uintptr_t kInputFrame      = 0x1075524; // in the input update, once a frame
constexpr uintptr_t kInputSystem     = 0x365D7C8; // its object: trigger tables, repeat delays
constexpr uintptr_t kLoadLayout      = 0x2DC300;  // (bindings, layout id, kind: 1 pad 2 wheel 3 Bristol, options)
constexpr uintptr_t kColumnRead      = 0x2DC495;  // in LoadLayout: rax = the column's text, [r15] its input
constexpr uintptr_t kColumnTable     = 0x1E09C48; // where r15 walks
constexpr uintptr_t kMapAsk          = 0x964B85;  // WaitForPause, just after it asks for action 22
constexpr int kLayoutRows            = 13;        // ControllerLayouts' LayoutId 0..12

constexpr char kIsActionPressedTail[] = "\x55\x57\x41\x54\x41\x57\x48\x83\xEC\x48\x44";
constexpr char kActionValueTail[]     = "\x56\x41\x57\x48\x83\xEC\x58\x44\x8B\xFA\x4C";
constexpr char kTriggerTypeBytes[]    = "\x48\x89\x5C\x24\x08\x48\x89\x7C\x24\x10\x4C\x8B\x41\x10";
constexpr char kInputFrameBytes[]     = "\x41\xC6\x85\x98\xAB\x00\x00\x00\xC6\x85\x68\x01\x00\x00\x00\x33";
constexpr char kLoadLayoutBytes[]     = "\x48\x8B\xC4\x44\x88\x40\x18\x55\x41\x54\x41\x55\x41\x56\x41\x57";
constexpr char kColumnReadBytes[]     = "\x48\xC7\x45\xCF\x0F\x00\x00\x00\x48\xC7\x45\xC7\x00\x00\x00\x00";
constexpr char kMapAskBytes[]         = "\x84\xC0\x74\x0F\x48\x8D\x97\x18\x01\x00\x00";
// The HUD's rev counter update (game+0xD0D360), where rcx is the car it shows (vm+0xC8):
// vxorps xmm6, xmm6, xmm6; cmp byte ptr [rcx+0x51D6], 0
constexpr uintptr_t kHudCar           = 0xD0D3F0;
constexpr char kHudCarBytes[]         = "\xC5\xC8\x57\xF6\x80\xB9\xD6\x51\x00\x00\x00";
constexpr uintptr_t kForzaCarVtable   = 0x2C5CB80;
constexpr uintptr_t kCarTargetVtable  = 0x2C5CE28; // car+0x20
constexpr uintptr_t kPhysicsVtable    = 0x2EB5E18; // [car+0x4C08]
constexpr uintptr_t kVelocity         = 0xF646B0;  // (car+0x20, float out[4]): Extra Options' stationary check

using PressedFn = bool (*)(void* manager, int player, int action, bool use_up);
using ValueFn   = float (*)(void* manager, int player, int action);
using TypeFn    = int (*)(const void* table, int action);
using StateFn   = int (*)(void* manager, int input);
using LayoutFn  = void* (*)(void* bindings, int id, uint8_t kind, void* options);

char* g_game              = nullptr;
PressedFn g_orig_pressed  = nullptr;
ValueFn g_orig_value      = nullptr;
LayoutFn g_orig_layout    = nullptr;
TypeFn g_trigger_type     = nullptr;
StateFn g_button_state    = nullptr;
char* g_test_input_system = nullptr;
bool g_map_ours           = true;  // false while MapButton is installed
bool g_layout_ours        = true;  // false while CustomBindings is installed
bool g_test_map_anywhere  = false; // the harness asks for the map from its own code

// The game's thread's.
void* g_manager = nullptr;
DWORD g_last_ask = 0;
bool g_act_now[fc::kActionIds], g_act_before[fc::kActionIds], g_act_used[fc::kActionIds];
uint32_t g_act_repeat[fc::kActionIds];
int g_map_input = -1;
uint64_t g_bindings_sig = 0;
int g_road_logs = 0;

template <typename T> T at(const void* p, uintptr_t off) { return *reinterpret_cast<const T*>(static_cast<const char*>(p) + off); }

char* input_system() {
    if (g_test_input_system) return g_test_input_system;
    return g_game ? *reinterpret_cast<char**>(g_game + kInputSystem) : nullptr;
}

bool player_input_on(const void* manager, int player) {
    const char* set = at<const char*>(manager, 0x38);
    if (!set) return false;
    const char* table = at<const char*>(set, 0);
    if (!table) return false;
    const uint64_t pos  = at<uint64_t>(manager, 0x50);
    const uint64_t mask = at<uint64_t>(table, 0x10) - 1;
    const char* buckets = at<const char*>(table, 8);
    if (!buckets) return false;
    const char* bucket = at<const char*>(buckets, ((pos >> 1) & mask) * 8);
    if (!bucket) return false;
    return (at<uint32_t>(bucket, (pos & 1) * 8 + 4) >> player) & 1;
}

// The road's bindings (player 0): manager+0x20 holds a vector per player of 28-byte bindings
// {player, action, input, modifier, modifier, ...} (game+0x1073F00 adds them).
void read_bindings(void* manager) {
    const char* players = at<const char*>(manager, 0x20);
    const char* players_end = at<const char*>(manager, 0x28);
    int8_t on[fc::kActionIds];
    std::fill(std::begin(on), std::end(on), static_cast<int8_t>(-1));
    bool used[fc::kElemCount] = {};
    uint64_t sig = 1469598103934665603ull;
    std::string raw; // every binding, "action:input" (+modifiers), for the log
    if (players && players_end > players) {
        const char* b = at<const char*>(players, 0);
        const char* e = at<const char*>(players, 8);
        const size_t n = b && e > b ? static_cast<size_t>(e - b) / 28 : 0;
        for (size_t i = 0; i < n && i < 512; ++i) {
            const char* x   = b + i * 28;
            const int act   = at<int>(x, 4);
            const int input = at<int>(x, 8);
            const bool plain = at<int>(x, 12) == fc::kInputNone && at<int>(x, 16) == fc::kInputNone;
            sig = (sig ^ static_cast<uint64_t>(act * 64 + input + (plain ? 0 : 4096))) * 1099511628211ull;
            if (raw.size() < 400) {
                raw += " " + std::to_string(act) + ":" + std::to_string(input);
                if (!plain) raw += "+" + std::to_string(at<int>(x, 12)) + "/" + std::to_string(at<int>(x, 16));
            }
            if (act < 0 || act >= fc::kActionIds || !plain) continue;
            int8_t where = -1;
            if (act == fc::kSteerAction && input == fc::kInputSteerX) where = fc::kOnSteerAxis;
            else where = static_cast<int8_t>(fc::elem_of_input(input));
            if (where < 0) continue;
            if (on[act] < 0) on[act] = where;
            bool known = false;
            for (const fc::Action& a : fc::kActions) known = known || (a.id == act && a.fixed < 0);
            if (!known) continue;
            if (where == fc::kOnSteerAxis) used[fc::LLeft] = used[fc::LRight] = true;
            else used[where] = true;
        }
    }
    for (int i = 0; i < fc::kActionIds; ++i) g_action_on[i] = on[i];
    for (int e = 0; e < fc::kElemCount; ++e) g_input_used[e] = used[e];
    if (sig != g_bindings_sig) {
        g_bindings_sig = sig;
        std::string line;
        for (const fc::Action& a : fc::kActions) {
            if (a.dir > 0 || a.id == fc::kMapAction || a.fixed >= 0 || a.launch) continue;
            const int8_t w = on[a.id];
            line += std::string(" ") + (a.id == fc::kSteerAction ? "Steer" : a.name) + "=" +
                    (w == fc::kOnSteerAxis ? "left stick" : w >= 0 ? fc::kElems[w].name : "-");
        }
        log_fmt("the road's bindings now:%s%s", line.c_str(),
                g_map_input >= 0 ? (std::string(" Map=") + fc::kElems[fc::elem_of_input(g_map_input)].name).c_str() : "");
        log_fmt("(as the game has them, action:input:%s)", raw.c_str());
    }
}

// Once a frame, on the game's thread.
void input_frame(FH2Context*, void*) {
    const DWORD now = GetTickCount();
    const bool road = g_manager && now - g_last_ask < kRoadMs;
    if (road != g_road.load()) {
        g_road = road;
        if (g_road_logs < 40) {
            ++g_road_logs;
            log_fmt(road ? "on the road: action keys on" : "in a menu: keys are the controller's");
        }
    }
    if (g_manager) read_bindings(g_manager);
    for (int j = 0; j < fc::kActionIds; ++j) {
        g_act_before[j] = g_act_now[j];
        g_act_now[j]    = road && g_wanted_live[j];
        g_act_used[j]   = false;
    }
}

bool answer_for_keys(int player, int action, bool use_up) {
    if (action < 0 || action >= fc::kActionIds || (!g_act_now[action] && !g_act_before[action])) return false;
    const char* sys = input_system();
    const char* tables = sys && g_trigger_type ? at<const char*>(sys, 0xAB10) : nullptr;
    if (!tables) return false;
    const int type  = g_trigger_type(tables + player * 32, action);
    const int state = fc::key_state(g_act_now[action], g_act_before[action], g_act_used[action]);
    const bool hit  = fc::triggers(type, state, GetTickCount(), g_act_repeat[action], at<uint32_t>(sys, 0xAB40),
                                   at<uint32_t>(sys, 0xAB44));
    if (hit && use_up) g_act_used[action] = true;
    return hit;
}

// What the game asks for, for the log: which of the road's actions in the last 2 seconds, and how
// many of the asks came with the player's input on (the first 5 minutes).
uint32_t g_asked_mask = 0;
int g_asks = 0, g_asks_on = 0;
DWORD g_summary_at = 0, g_summary_start = 0;

void note_ask(void* manager, int action) {
    g_manager       = manager;
    const bool on   = player_input_on(manager, 0);
    if (on) g_last_ask = GetTickCount(); // on the road: the game takes the player's driving input
    ++g_asks;
    g_asks_on += on ? 1 : 0;
    if (action >= 0 && action < 32) g_asked_mask |= 1u << action;
}

bool pressed_detour(void* manager, int player, int action, bool use_up) {
    const bool pressed = g_orig_pressed(manager, player, action, use_up);
    if (player != 0) return pressed;
    note_ask(manager, action);
    if (pressed) return true;
    if (!player_input_on(manager, player)) return false;
    if (action == fc::kMapAction) {
        if (!g_map_ours) return false;
        const char* from = reinterpret_cast<char*>(_ReturnAddress());
        // While the Map key is wanted: where each ask comes from, and what's said (the first 12).
        static int map_traces = 0;
        if ((g_act_now[action] || g_wanted_live[action]) && map_traces < 12) {
            ++map_traces;
            log_fmt("map trace: asked from %s+0x%llX (WaitForPause is game+0x%llX), key wanted %d, now %d before %d used %d",
                    g_game && from >= g_game && from < g_game + 0x4000000 ? "game" : "elsewhere",
                    static_cast<unsigned long long>(from - (g_game ? g_game : from)), static_cast<unsigned long long>(kMapAsk),
                    g_wanted_live[action] ? 1 : 0, g_act_now[action] ? 1 : 0, g_act_before[action] ? 1 : 0, g_act_used[action] ? 1 : 0);
        }
        if (g_game && !g_test_map_anywhere && from != g_game + kMapAsk) return false;
        if (g_map_input >= 0 && g_button_state && g_button_state(manager, g_map_input) == 1) {
            log_fmt("Map pressed (the controller): opening the map");
            return true;
        }
        if (answer_for_keys(player, action, use_up)) {
            log_fmt("Map pressed (the keyboard): opening the map");
            return true;
        }
        return false;
    }
    return answer_for_keys(player, action, use_up);
}

float value_detour(void* manager, int player, int action) {
    const float value = g_orig_value(manager, player, action);
    if (player != 0) return value;
    note_ask(manager, action);
    if (value >= 1.0f || action < 0 || action >= fc::kActionIds || !g_act_now[action]) return value;
    return player_input_on(manager, player) ? 1.0f : value;
}

// [Controller] goes in as the game reads ControllerLayouts' columns: rax points at the column's
// text (an MSVC std::string), which the mod points at its own.
struct FakeString {
    union {
        char buf[16];
        const char* ptr;
    };
    size_t size;
    size_t capacity;
};
FakeString g_column_text[fc::kColumnCount];
std::string g_column_store[fc::kColumnCount];
bool g_loading_ours = false;
std::shared_ptr<const fc::Config> g_loading_cfg;

void set_fake(FakeString& f, std::string& store, const std::string& text) {
    store  = text;
    f.size = text.size();
    if (text.size() < 16) {
        std::memset(f.buf, 0, sizeof f.buf);
        std::memcpy(f.buf, text.data(), text.size());
        f.capacity = 15;
    } else {
        f.ptr      = store.c_str();
        f.capacity = text.size();
    }
}

void column_read(FH2Context* ctx, void*) {
    if (!g_loading_ours || !g_loading_cfg || !g_game) return;
    const uintptr_t r15 = ctx->r15;
    const uintptr_t lo = reinterpret_cast<uintptr_t>(g_game) + kColumnTable;
    if (r15 < lo || r15 >= lo + 16 * fc::kColumnCount || (r15 - lo) % 16) return;
    const int col = static_cast<int>((r15 - lo) / 16);
    const fc::Config& c = *g_loading_cfg;
    std::string action = c.column_set[col] ? c.column_action[col] : std::string();
    if (fc::same_text(action.c_str(), "Map")) {
        g_map_input = fc::kColumns[col].input;
        action.clear();
    }
    set_fake(g_column_text[col], g_column_store[col], action);
    ctx->rax = reinterpret_cast<uint64_t>(&g_column_text[col]);
}

void* layout_detour(void* bindings, int id, uint8_t kind, void* options) {
    if (kind != 1) return g_orig_layout(bindings, id, kind, options);
    const auto cfg = config();
    int use = id;
    if (cfg->custom_layout) {
        use              = 0; // a row to read; every column comes from [Controller]
        g_loading_cfg    = cfg;
        g_loading_ours   = true;
        g_map_input      = -1;
    } else if (id < 0 || id >= kLayoutRows) {
        use = 0;
        log_fmt("layout %d isn't one of the game's (CustomBindings' Custom Layout 1?): the game's first layout "
                "instead, or set Custom = 1 in FH2Controls.ini's [Controller]", id + 1);
    }
    void* r = g_orig_layout(bindings, use, kind, options);
    g_loading_ours = false;
    g_loading_cfg.reset();
    if (cfg->custom_layout) log_fmt("controller layout: FH2Controls.ini's [Controller]");
    else log_fmt("controller layout: the game's layout %d", use + 1);
    g_bindings_sig = 0; // log the road's bindings again
    return r;
}

// The car the HUD's rev counter shows, once a frame: its engine speed and redline (the physics
// object's +0x148 and +0x1C8, rad/s, which the rev counter's getters turn into rpm: slots 0x1A0
// and 0xF0 of game+0x2EB5E18) and its speed, for launch control.
using VelocityFn = void (*)(void* target, float* out);
void hud_car(FH2Context* ctx, void*) {
    const char* car = reinterpret_cast<const char*>(ctx->rcx);
    if (!car || !g_game || at<const char*>(car, 0) != g_game + kForzaCarVtable) return;
    const char* target = car + 0x20;
    const char* phys   = at<const char*>(car, 0x4C08);
    if (at<const char*>(target, 0) != g_game + kCarTargetVtable || !phys || at<const char*>(phys, 0) != g_game + kPhysicsVtable)
        return;
    constexpr float kRpm = 9.549296f; // rad/s to rpm
    const float spin = at<float>(phys, 0x148), limit = at<float>(phys, 0x1CC), red = at<float>(phys, 0x1C8);
    if (!(red > 0 && red < 3000 && limit >= red && spin == spin)) return;
    alignas(16) float v[4] = {};
    reinterpret_cast<VelocityFn>(g_game + kVelocity)(const_cast<char*>(target), v);
    const float speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    g_car_rpm     = std::clamp(spin, 0.0f, limit) * kRpm;
    g_car_redline = red * kRpm;
    g_car_speed   = speed == speed ? speed : 99.0f;
    g_car_at      = GetTickCount();
    // the input frame the physics runs on (slot 0x150 of game+0x2EB5E18, Extra Options' getter)
    using FrameFn = const float* (*)(const void*);
    const void* const* vt = at<const void* const*>(phys, 0);
    if (const float* f = reinterpret_cast<FrameFn>(vt[0x150 / 8])(phys)) {
        AcquireSRWLockExclusive(&g_frame_lock);
        std::memcpy(g_car_frame, f, sizeof g_car_frame);
        ReleaseSRWLockExclusive(&g_frame_lock);
    }
}

bool tail_matches(const char* at_, const char* first, const char* tail, size_t size) {
    return std::memcmp(at_ + 5, tail, size) == 0 &&
           (std::memcmp(at_, first, 5) == 0 || static_cast<unsigned char>(at_[0]) == 0xE9);
}

bool hook_all(std::initializer_list<FH2Hook*> hooks, const char* what) {
    bool made = true;
    for (FH2Hook* h : hooks) made = made && h;
    for (FH2Hook* h : hooks)
        if (made && g_api->enable_hooks(g_mod, h) != 1) made = false;
    if (!made) {
        for (FH2Hook* h : hooks)
            if (h) g_api->disable_hooks(g_mod, h);
        log_fmt("couldn't hook %s (see above)", what);
    }
    return made;
}

#include "extras.inc"

DWORD WINAPI game_thread(LPVOID) {
    if (!g_api->wait_for_code || !g_api->hook_function || !g_api->hook_instruction) return 0;
    char* game = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!game) return 0;
    g_exc_until = GetTickCount() + 90000; // the startup crash comes as the mods hook the game: from now
    exc_write("(the game's code is ready: every mod hooks it now)\r\n");
    if (g_api->wait_for_mods) g_api->wait_for_mods(30000);
    g_layout_ours = !(g_api->mod_version && g_api->mod_version("CustomBindings"));
    g_map_ours    = !(g_api->mod_version && g_api->mod_version("MapButton"));
    if (!g_layout_ours) log_fmt("CustomBindings is installed: [Controller] is off (delete fh2-mods\\CustomBindings.dll)");
    if (!g_map_ours) log_fmt("MapButton is installed: it opens the map, not FH2Controls (delete fh2-mods\\MapButton.dll)");
    if (!g_gave_azimuth)
        log_fmt("Azimuth didn't ask for its controllers through FH2Controls: there's no keyboard controller");

    if (!tail_matches(game + kIsActionPressed, "\x44\x88\x4C\x24\x20", kIsActionPressedTail, sizeof kIsActionPressedTail - 1) ||
        !tail_matches(game + kActionValue, "\x40\x53\x41\x55\x41", kActionValueTail, sizeof kActionValueTail - 1) ||
        std::memcmp(game + kTriggerType, kTriggerTypeBytes, sizeof kTriggerTypeBytes - 1) != 0 ||
        std::memcmp(game + kInputFrame, kInputFrameBytes, sizeof kInputFrameBytes - 1) != 0 ||
        std::memcmp(game + kMapAsk, kMapAskBytes, sizeof kMapAskBytes - 1) != 0) {
        log_fmt("the game's input code isn't as expected: actions are off, keys only work the controller");
        return 0;
    }
    g_game         = game;
    g_trigger_type = reinterpret_cast<TypeFn>(game + kTriggerType);
    g_button_state = reinterpret_cast<StateFn>(game + kButtonState);
    if (hook_all({g_api->hook_instruction(g_mod, "input update (once a frame)", game + kInputFrame, input_frame, nullptr),
                  g_api->hook_function(g_mod, "IsActionPressed (action keys, Map)", game + kIsActionPressed,
                                       reinterpret_cast<void*>(&pressed_detour), reinterpret_cast<void**>(&g_orig_pressed)),
                  g_api->hook_function(g_mod, "action value (action keys)", game + kActionValue,
                                       reinterpret_cast<void*>(&value_detour), reinterpret_cast<void**>(&g_orig_value))},
                 "the game's actions"))
        log_fmt("actions on");

    if (std::memcmp(game + kHudCar, kHudCarBytes, sizeof kHudCarBytes - 1) != 0)
        log_fmt("the HUD's rev counter code isn't as expected: no launch control");
    else if (hook_all({g_api->hook_instruction(g_mod, "HUD rev counter (the car's rpm, for launch control)", game + kHudCar, hud_car, nullptr)},
                      "the HUD's rev counter"))
        log_fmt("launch control ready");
    hook_extras(game);

    if (g_layout_ours) {
        const bool ok = tail_matches(game + kLoadLayout, "\x48\x8B\xC4\x44\x88", kLoadLayoutBytes + 5, sizeof kLoadLayoutBytes - 6) &&
                        std::memcmp(game + kColumnRead, kColumnReadBytes, sizeof kColumnReadBytes - 1) == 0;
        if (!ok) log_fmt("the game's layout code isn't as expected: [Controller] is off");
        else if (hook_all({g_api->hook_function(g_mod, "LoadLayout ([Controller])", game + kLoadLayout,
                                                reinterpret_cast<void*>(&layout_detour), reinterpret_cast<void**>(&g_orig_layout)),
                           g_api->hook_instruction(g_mod, "LoadLayout: column read ([Controller])", game + kColumnRead, column_read, nullptr)},
                          "the layout"))
            log_fmt("[Controller] ready (used when Custom = 1)");
    }
    return 0;
}

// ---- The Mods page -----------------------------------------------------------------------------
void setting_changed(void*, const char* key, int value) {
    if (std::strcmp(key, "Enabled") == 0) g_enabled = value != 0;
    write_setting(key, value ? "1" : "0");
}

void slider_changed(void*, const char* key, float value) {
    if (std::strcmp(key, "SteeringTime") == 0) g_steer_time = value;
    if (std::strcmp(key, "ReturnTime") == 0) g_return_time = value;
    if (std::strcmp(key, "PedalTime") == 0) g_pedal_time = value;
    if (std::strcmp(key, "LaunchRPM") == 0) g_launch_rpm = value;
    char v[32];
    std::snprintf(v, sizeof v, "%.2f", value);
    write_setting(key, v);
}

void toggle_pressed(void*, const char*) {
    const bool on = !g_enabled;
    g_enabled     = on;
    write_setting("Enabled", on ? "1" : "0");
    g_api->set_setting(g_mod, "Enabled", on ? 1 : 0);
    log_fmt(on ? "keyboard on" : "keyboard off");
    MessageBeep(on ? MB_OK : MB_ICONHAND);
}

void toggle_rebound(void*, const char*, uint32_t vk, uint32_t mods, uint32_t chord) {
    write_setting("ToggleKey", std::to_string(vk));
    write_setting("ToggleModifiers", std::to_string(mods));
    write_setting("ToggleChord", std::to_string(chord));
}

void add_switch(const char* key, const char* label, const char* description, bool value) {
    FH2Setting s{};
    s.size        = sizeof s;
    s.key         = key;
    s.label       = label;
    s.description = description;
    s.value       = value ? 1 : 0;
    s.on_change   = setting_changed;
    g_api->add_setting(g_mod, &s);
}

void add_slider(const char* key, const char* label, const char* description, float value, float max, float step,
                float min = 0.0f) {
    FH2Setting s{};
    s.size          = sizeof s;
    s.key           = key;
    s.label         = label;
    s.description   = description;
    s.kind          = FH2_SETTING_SLIDER;
    s.min           = min;
    s.max           = max;
    s.step          = step;
    s.slider_value  = value;
    s.display_scale = 1.0f;
    s.decimals      = 2;
    s.on_slide      = slider_changed;
    g_api->add_setting(g_mod, &s);
}

bool running(const wchar_t* exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W p{sizeof p};
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &p); ok && !found; ok = Process32NextW(snap, &p))
        found = _wcsicmp(p.szExeFile, exe) == 0;
    CloseHandle(snap);
    return found;
}

// ---- Startup exceptions (diagnostic) -----------------------------------------------------------------
// The game sometimes dies at startup inside ntdll, as every mod enables its hooks, with no crash
// report: an exception the game would handle, then a fault while Windows unwinds it. To see the
// first exception, a vectored handler notes every exception for the first 90 s (code, where, thread)
// in fh2-mods\FH2Controls-exceptions.log, written straight to the file (no locks, no allocation).
HANDLE g_exc_file = INVALID_HANDLE_VALUE;
volatile LONG g_exc_count = 0;

void exc_write(const char* s) {
    DWORD n = 0;
    if (g_exc_file != INVALID_HANDLE_VALUE) WriteFile(g_exc_file, s, static_cast<DWORD>(std::strlen(s)), &n, nullptr);
}

void exc_where(uintptr_t addr, char* out, size_t size) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(addr), &m) && m) {
        GetModuleFileNameA(m, name, MAX_PATH);
        const char* base = std::strrchr(name, '\\');
        std::snprintf(out, size, "%s+0x%llX", base ? base + 1 : name, static_cast<unsigned long long>(addr - reinterpret_cast<uintptr_t>(m)));
    } else if (g_game && addr >= reinterpret_cast<uintptr_t>(g_game) && addr < reinterpret_cast<uintptr_t>(g_game) + 0x4000000) {
        std::snprintf(out, size, "game+0x%llX", static_cast<unsigned long long>(addr - reinterpret_cast<uintptr_t>(g_game)));
    } else {
        std::snprintf(out, size, "0x%llX", static_cast<unsigned long long>(addr));
    }
}

LONG CALLBACK exc_handler(EXCEPTION_POINTERS* ep) {
    if (GetTickCount() > g_exc_until || InterlockedIncrement(&g_exc_count) > 300) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == 0x406D1388 || code == DBG_PRINTEXCEPTION_C || code == 0x4001000A) return EXCEPTION_CONTINUE_SEARCH; // thread names, debug prints
    char where[300], caller[300], line[900];
    exc_where(reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress), where, sizeof where);
    // the first few return addresses on the stack, to see who called in
    std::string stack;
    const uintptr_t* sp = reinterpret_cast<const uintptr_t*>(ep->ContextRecord->Rsp);
    int found = 0;
    for (int i = 0; i < 64 && found < 6; ++i) {
        MEMORY_BASIC_INFORMATION mbi;
        const uintptr_t v = sp[i];
        if (!VirtualQuery(reinterpret_cast<void*>(v), &mbi, sizeof mbi) || !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
            continue;
        exc_where(v, caller, sizeof caller);
        stack += std::string(" ") + caller;
        ++found;
    }
    std::snprintf(line, sizeof line, "%lu ms thread %lu: exception 0x%08lX at %s (%s 0x%llX) stack:%s\r\n", GetTickCount(),
                  GetCurrentThreadId(), code, where,
                  code == EXCEPTION_ACCESS_VIOLATION ? (ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading") : "info",
                  static_cast<unsigned long long>(ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0),
                  stack.c_str());
    exc_write(line);
    return EXCEPTION_CONTINUE_SEARCH;
}

void exc_start(const std::wstring& dir) {
    g_exc_file = CreateFileW((dir + L"\\fh2-mods\\FH2Controls-exceptions.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                             nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    g_exc_until = GetTickCount() + 600000; // until the game has started (then 90 s more: game_thread)
    exc_write("FH2Controls: exceptions in the first 90 s of this run (diagnostic)\r\n");
    AddVectoredExceptionHandler(1, exc_handler);
}

} // namespace

// ---- For tests\harness.cpp -----------------------------------------------------------------------
extern "C" __declspec(dllexport) void fc_test_key(int vk, int down) {
    g_test_keys = true;
    if (vk > 0 && vk < 256) g_test_down[vk] = down != 0;
}
extern "C" __declspec(dllexport) void fc_test_front(int front) { g_test_front = front; }
extern "C" __declspec(dllexport) void fc_test_reading(wgi::GamepadReading* out) { wgi_read(out); }
extern "C" __declspec(dllexport) HRESULT fc_test_factory(void* real_factory_fn, HSTRING cls, REFIID iid, void** out) {
    g_orig_factory = reinterpret_cast<RoGetActivationFactoryFn>(real_factory_fn);
    return factory_detour(cls, iid, out);
}
extern "C" __declspec(dllexport) void fc_test_game_input(char* input_system, TypeFn trigger_type, StateFn state) {
    g_test_input_system = input_system;
    g_trigger_type      = trigger_type;
    g_button_state      = state;
}
extern "C" __declspec(dllexport) void fc_test_frame() { input_frame(nullptr, nullptr); }
extern "C" __declspec(dllexport) int fc_test_keycap(const char* icon_path, unsigned char* rgba, wchar_t* label, int label_size) {
    const std::wstring l = keycap_label(config()->keys, icon_elems(icon_path));
    wcsncpy_s(label, label_size, l.c_str(), _TRUNCATE);
    if (l.empty()) return 0;
    const auto& k = *keycap(l);
    std::memcpy(rgba, k.data(), k.size());
    return kKeycap;
}
extern "C" __declspec(dllexport) void fc_test_map_from_anywhere(int on) { g_test_map_anywhere = on != 0; }
extern "C" __declspec(dllexport) int fc_test_road() { return g_road ? 1 : 0; }
extern "C" __declspec(dllexport) DWORD fc_test_xinput(void* real, DWORD index, XINPUT_STATE* s) {
    g_orig_xinput = reinterpret_cast<XInputGetStateFn>(real);
    return xinput_detour(index, s);
}

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (api->api_version < 9) return 1; // needs hotkeys (API 9)
    g_api = api;

    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "FH2Controls";
    info.version          = kVersion;
    info.github_repo      = "pavankumar-dss/FH2Controls";
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = 1;
    info.description      = "Keyboard and controller bindings from one ini (fh2-mods\\FH2Controls.ini): keys for the "
                            "game's actions and for the menus, a layout for your controller, and a Map button. No "
                            "driver needed.";
    info.offline_only     = 0;
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 0;

    exc_start(api->azimuth_dir);

    // First, before Azimuth asks Windows for its controllers.
    g_keypad = new wgi::KeyPad(&wgi_read);
    if (void** slot = factory_import()) {
        FH2Hook* h = api->hook_pointer(g_mod, "azimuth.exe: RoGetActivationFactory (the keyboard's controller)", slot,
                                       reinterpret_cast<void*>(&factory_detour), reinterpret_cast<void**>(&g_orig_factory));
        if (!h || api->enable_hooks(g_mod, h) != 1) log_fmt("couldn't hook Azimuth's controllers: no keyboard controller");
    } else {
        log_fmt("azimuth.exe doesn't import RoGetActivationFactory: no keyboard controller");
    }
    if (HMODULE x = LoadLibraryW(L"xinput1_4.dll"))
        if (void* f = reinterpret_cast<void*>(GetProcAddress(x, "XInputGetState"))) {
            FH2Hook* h = api->hook_function(g_mod, "XInputGetState (the keyboard as pad 0)", f,
                                            reinterpret_cast<void*>(&xinput_detour), reinterpret_cast<void**>(&g_orig_xinput));
            if (!h || api->enable_hooks(g_mod, h) != 1) log_fmt("couldn't hook XInput: mods that read it won't see the keyboard");
        }

    if (HMODULE m = GetModuleHandleW(L"win32u.dll"))
        if (auto f = reinterpret_cast<ForegroundFn>(GetProcAddress(m, "NtUserGetForegroundWindow"))) g_foreground = f;

    g_ini = std::wstring(api->azimuth_dir) + L"\\fh2-mods\\FH2Controls.ini";
    if (GetFileAttributesW(g_ini.c_str()) == INVALID_FILE_ATTRIBUTES) write_first_ini();
    load_config(true);

    add_switch("Enabled", "Keyboard",
               "Drive and use the menus with the keyboard. Keys are set in fh2-mods\\FH2Controls.ini; changes there "
               "apply while you play.",
               g_enabled);
    add_slider("SteeringTime", "Steering speed",
               "Seconds from straight ahead to full lock while you hold a steering key: lower is quicker, 0 is at once.",
               g_steer_time, 1.0f, 0.05f);
    add_slider("ReturnTime", "Steering return speed",
               "Seconds from full lock back to straight ahead when you let go (and on the way to the other side).",
               g_return_time, 1.0f, 0.05f);
    add_slider("PedalTime", "Pedal speed",
               "Seconds for gas and brake to go from off to full and back: a little stops wheelspin from a standing start.",
               g_pedal_time, 0.5f, 0.02f);
    add_switch("MouseLook", "Mouse look",
               "While you drive, moving the mouse turns the view (the right stick's look); leave it still for a "
               "moment and the view returns ahead.",
               config()->mouse_look);
    add_slider("MouseSensitivity", "Mouse look speed",
               "How far the view turns for the mouse's movement: 1.00 is about a quarter of a degree a count.",
               config()->mouse_sensitivity, 5.0f, 0.1f, 0.1f);
    add_slider("LaunchRPM", "Launch control revs",
               "The revs launch control holds, as a share of the car's redline (0.60 is 60%). Hold the LaunchControl key "
               "(Space, with the handbrake) and the gas at a standstill, then let go of it to launch. Lower spins the "
               "tyres less off the line; higher launches harder on a car with grip to spare.",
               g_launch_rpm, 0.95f, 0.05f, 0.2f);

    FH2Hotkey toggle{};
    toggle.size      = sizeof toggle;
    toggle.key       = "Toggle";
    toggle.label     = "Keyboard on/off";
    toggle.vk        = static_cast<uint32_t>(std::atoi(read_setting("ToggleKey", "0").c_str()));
    toggle.modifiers = static_cast<uint32_t>(std::atoi(read_setting("ToggleModifiers", "0").c_str()));
    toggle.pad_chord = static_cast<uint32_t>(std::atoi(read_setting("ToggleChord", "0").c_str()));
    toggle.on_press  = toggle_pressed;
    toggle.on_rebind = toggle_rebound;
    api->add_hotkey(g_mod, &toggle);

    std::string notice;
    if (running(L"Keyvert.exe")) notice = "FH2Controls: close Keyvert while you play";
    const std::wstring mods = std::wstring(api->azimuth_dir) + L"\\fh2-mods\\";
    for (const wchar_t* old : {L"KeyboardDrive.dll", L"CustomBindings.dll", L"MapButton.dll"})
        if (GetFileAttributesW((mods + old).c_str()) != INVALID_FILE_ATTRIBUTES) {
            log_fmt("fh2-mods\\%ls is still there: FH2Controls does its job, delete it", old);
            if (notice.empty()) notice = "FH2Controls: delete KeyboardDrive, CustomBindings, MapButton";
        }
    api->set_notice(g_mod, notice.c_str());

    if (HANDLE t = CreateThread(nullptr, 0, pad_thread, nullptr, 0, nullptr)) CloseHandle(t);
    if (HANDLE t = CreateThread(nullptr, 0, game_thread, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
