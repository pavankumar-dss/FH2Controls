// Tests FH2Controls without the game: the ini, the road/menu rule, then the DLL against a stub
// platform API: the controller it gives Azimuth (through a stand-in for Windows.Gaming.Input with
// one real pad), XInput pad 0, the answers for actions and Map (with a stand-in for the game's
// input manager), the layout from [Controller], and reading the ini again when it changes.
#include "fh2mod.h"

#include "../src/core.h"
#include "../src/wgi.h"

#include <windows.h>

#include <roapi.h>
#include <xinput.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#pragma comment(lib, "runtimeobject.lib")

static int g_failures = 0;
static int g_checks   = 0;
static void expect(bool ok, const char* what) {
    ++g_checks;
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

// ---- The ini and the rule ---------------------------------------------------------------------------
static void test_core() {
    using namespace fc;
    Config c = parse_config(kDefaultIni);
    expect(c.problems.empty(), "the default ini reads with no problems");
    expect(c.keys.count['W'] == 2 && c.keys.has_action['W'], "W: Gas, and the left stick up in menus");
    expect(!c.custom_layout && c.column_set[12] && c.column_action[12] == "Handbrake", "[Controller]: ButtonA = Handbrake, Custom 0");

    Config bad = parse_config("[Keyboard]\nGas = W, Nope\nFly = F\nShiftUp = Up ; a comment\n[Settings]\nPedalTime = 9\n[Controller]\nButtonA = Sing\n");
    expect(bad.problems.size() == 4, "a bad key, an unknown action, a setting out of range and a bad action are reported");
    expect(bad.keys.count['W'] == 1 && bad.keys.count[VK_UP] == 1, "and the rest of those lines still count");

    Config two = parse_config("[Keyboard]\nRewind = R\nRewind = Back\nA = Enter\nA = Space\nLB = [\nRB = ]\n");
    expect(two.problems.empty() && two.keys.any('R') && two.keys.any(VK_BACK) && two.keys.any(VK_OEM_4) && two.keys.any(VK_OEM_6),
           "several lines for one thing add keys; [ and ] are keys");

    // The rule, with the game's default layout: Gas RT, Brake LT, Steer on the stick, Handbrake A...
    Config u = parse_config("[Keyboard]\nGas = W\nLeftStickUp = W\nSteerLeft = Left\nSteerRight = Right\n"
                            "DPadLeft = Left\nDPadRight = Right\nHandbrake = Space\nA = Enter, Space\nMenu = Escape\n"
                            "ShiftUp = Up\nDPadUp = Up\nMap = M\nLS = LAlt\n");
    int8_t on[kActionIds];
    for (auto& o : on) o = -1;
    on[0] = RT, on[1] = LT, on[2] = kOnSteerAxis, on[3] = A, on[4] = Y;
    bool used[kElemCount] = {};
    used[RT] = used[LT] = used[A] = used[Y] = used[LLeft] = used[LRight] = true;
    bool down[256] = {};
    uint32_t order[256] = {};
    Smooth s;
    Feel instant{0, 0, 0};
    auto reading = [&](bool road, Wanted* w = nullptr) {
        Frame f{down, order, road, on, used};
        return make_reading(u.keys, f, s, 0.004f, instant, w);
    };
    down['W'] = true;
    Reading r = reading(false);
    expect(r.ly == 1 && r.rt == 0, "menu: W is the left stick up, not gas");
    r = reading(true);
    expect(r.rt == 1 && r.ly == 0, "road: W is gas (RT), not the stick");
    down['W'] = false;
    down[VK_RETURN] = true;
    expect(reading(false).buttons == (1u << A), "menu: Enter is A");
    expect(reading(true).buttons == 0, "road: Enter (A) doesn't pull the handbrake, A is the handbrake's");
    down[VK_RETURN] = false;
    down[VK_SPACE] = true;
    expect(reading(true).buttons == (1u << A), "road: Space is the handbrake (on A)");
    down[VK_SPACE] = false;
    down[VK_ESCAPE] = true;
    expect(reading(true).buttons == (1u << Menu), "road: Escape still pauses (Menu has no road action)");
    down[VK_ESCAPE] = false;
    {
        Config p = parse_config("[Keyboard]\nPause = Escape\nB = Escape\nRewind = R\n");
        int8_t on2[kActionIds];
        for (auto& o : on2) o = -1;
        on2[20] = B; // Rewind on B, as the layout has it
        bool used2[kElemCount] = {};
        used2[B] = true;
        bool d2[256] = {};
        d2[VK_ESCAPE] = true;
        Smooth s2;
        Frame road{d2, order, true, on2, used2}, menu{d2, order, false, on2, used2};
        expect(p.problems.empty() && make_reading(p.keys, road, s2, 0.004f, instant, nullptr).buttons == (1u << Menu) &&
                   make_reading(p.keys, menu, s2, 0.004f, instant, nullptr).buttons == (1u << B),
               "Pause = Escape, B = Escape: Escape pauses on the road and is B in menus");
    }
    down[VK_LMENU] = true;
    expect(reading(true).buttons == (1u << LS), "road: Left Alt presses LS (free: the nitro button)");
    down[VK_LMENU] = false;
    down[VK_UP] = true;
    expect(reading(false).buttons == (1u << DUp), "menu: Up is the d-pad");
    expect(reading(true).buttons == (1u << Y), "road: Up is shift up (Y), not the d-pad");
    down[VK_UP] = false;
    down[VK_LEFT] = true, order[VK_LEFT] = 1;
    r = reading(true);
    expect(r.lx == -1 && r.buttons == 0, "road: Left steers left, without the d-pad");
    down[VK_RIGHT] = true, order[VK_RIGHT] = 2;
    const float through = reading(true).lx;
    expect(through == 0 && reading(true).lx == 1, "road: both held, the one pressed last wins (through the centre)");
    expect(reading(false).buttons == ((1u << DLeft) | (1u << DRight)) && reading(false).lx == 0, "menu: they're the d-pad");
    down[VK_LEFT] = down[VK_RIGHT] = false;
    down['M'] = true;
    Wanted w;
    reading(true, &w);
    expect(w.action[kMapAction] && reading(true).buttons == 0, "road: M wants Map, which isn't on a button");
    Wanted w2;
    reading(false, &w2);
    expect(!w2.action[kMapAction], "menu: M wants nothing");
    down['M'] = false;

    // launch control, on an engine fitted to the game's own trace (a 6000 rpm car on the
    // handbrake: ~5900 rpm/s at full gas, 3600 rpm at 27% gas), with the game's ~50 ms lag
    {
        Config lc = parse_config(kDefaultIni);
        expect(lc.keys.count[VK_SPACE] >= 2 && lc.launch_rpm == 0.60f, "default ini: LaunchControl on Space (with Handbrake), LaunchRPM 0.60");
        Launch l;
        LaunchIn in{true, true, true, 800.0f, 6000.0f, 0.0f, 0.60f};
        float rpm = 800.0f, gas = 0, applied = 0, peak = 0;
        for (int i = 0; i < 1000; ++i) { // 4 s at the pad thread's 4 ms
            in.rpm = rpm;
            gas    = launch_control(l, in, 0.004f);
            applied += (gas - applied) * 0.004f / 0.05f;
            rpm += (6499.0f * applied - 0.629f * (rpm - 800.0f)) * 0.004f;
            peak = std::max(peak, rpm);
        }
        expect(l.held && std::fabs(rpm - 3600.0f) < 60.0f, "launch control holds the revs at 60% of the redline (3600 of 6000)");
        expect(peak < 3600.0f * 1.05f, "without overshooting by more than 5% on the way (the game's first try went to 4571)");
        in.key = false;
        expect(launch_control(l, in, 0.004f) < 0 && !l.held && l.launches == 1, "let go of the key with the gas down: launch, the gas goes back to the keys");
        in.key = true;
        in.speed = 5.0f;
        expect(launch_control(l, in, 0.004f) < 0 && !l.held, "moving: launch control doesn't hold the revs");
        in.speed = 0.0f;
        in.car_known = false;
        expect(launch_control(l, in, 0.004f) < 0, "revs not known (no HUD): it leaves the gas alone");
        in.car_known = true;
        in.gas = false;
        expect(launch_control(l, in, 0.004f) < 0, "no gas: nothing to hold");
    }

    // easing
    Smooth e;
    Feel feel{0.25f, 0.15f, 0.10f};
    down['W'] = true;
    Frame f{down, order, true, on, used};
    const float a = make_reading(u.keys, f, e, 0.05f, feel, nullptr).rt;
    const float b = make_reading(u.keys, f, e, 0.05f, feel, nullptr).rt;
    expect(a > 0.4f && a < 0.6f && b == 1.0f, "gas eases in over the pedal time");
    down['W'] = false;
}

// ---- A stub platform --------------------------------------------------------------------------------
static std::wstring g_dir;
static char* g_game = nullptr;
static std::map<std::string, FH2Setting> g_settings;
static std::vector<std::string> g_log;
struct Hook { std::string name; void* target; void* detour; void** original; FH2InstructionFn fn; };
static std::vector<Hook> g_hooks;

static bool logged(const char* part) {
    for (const auto& l : g_log)
        if (l.find(part) != std::string::npos) return true;
    return false;
}
static Hook* hook_named(const char* part) {
    for (auto& h : g_hooks)
        if (h.name.find(part) != std::string::npos) return &h;
    return nullptr;
}

static FH2Mod* register_mod(const FH2ModInfo* info) {
    std::printf("registered %s %s\n", info->name, info->version);
    return reinterpret_cast<FH2Mod*>(1);
}
static void log_line(FH2Mod*, const char* s) { std::printf("log: %s\n", s); g_log.push_back(s); }
static int add_setting(FH2Mod*, const FH2Setting* s) { g_settings[s->key] = *s; return 1; }
static void set_setting(FH2Mod*, const char*, int) {}
static void set_slider(FH2Mod*, const char*, float) {}
static int add_hotkey(FH2Mod*, const FH2Hotkey*) { return 1; }
static void set_notice(FH2Mod*, const char* s) { std::printf("notice: \"%s\"\n", s ? s : ""); }
static void* wait_for_code(uint32_t) { return g_game; }
static int wait_for_mods(uint32_t) { return 1; }
static const char* mod_version(const char*) { return nullptr; }

// Stand-ins for the game's functions the mod calls on through.
static bool g_own_pressed = false;
static bool fake_pressed(void*, int, int, bool) { return g_own_pressed; }
static float fake_value(void*, int, int) { return 0.0f; }
static DWORD WINAPI fake_xinput(DWORD, XINPUT_STATE*) { return ERROR_DEVICE_NOT_CONNECTED; }

// LoadLayout: reads each column the way the game does, through the instruction hook.
struct GameString { union { char buf[16]; const char* ptr; }; size_t size, capacity; };
static std::vector<std::string> g_columns_read;
static int g_layout_id = -1;
static void* fake_load_layout(void*, int id, uint8_t, void*) {
    g_layout_id = id;
    g_columns_read.clear();
    Hook* h = hook_named("column read");
    GameString game_text{};
    std::memcpy(game_text.buf, "Gas", 4);
    game_text.size = 3, game_text.capacity = 15;
    for (int c = 0; c < fc::kColumnCount; ++c) {
        FH2Context ctx{};
        ctx.rax = reinterpret_cast<uint64_t>(&game_text);
        ctx.r15 = reinterpret_cast<uint64_t>(g_game + 0x1E09C48 + 16 * c);
        if (h) h->fn(&ctx, nullptr);
        const auto* s = reinterpret_cast<const GameString*>(ctx.rax);
        g_columns_read.emplace_back(s->capacity >= 16 ? s->ptr : s->buf, s->size);
    }
    return nullptr;
}

static FH2Hook* hook_function(FH2Mod*, const char* name, void* target, void* detour, void** original) {
    const std::string n = name ? name : "";
    if (n.find("IsActionPressed") != std::string::npos) *original = reinterpret_cast<void*>(&fake_pressed);
    else if (n.find("action value") != std::string::npos) *original = reinterpret_cast<void*>(&fake_value);
    else if (n.find("XInput") != std::string::npos) *original = reinterpret_cast<void*>(&fake_xinput);
    else if (n.find("LoadLayout") != std::string::npos) *original = reinterpret_cast<void*>(&fake_load_layout);
    else *original = target;
    g_hooks.push_back({n, target, detour, original, nullptr});
    return reinterpret_cast<FH2Hook*>(g_hooks.size());
}
static FH2Hook* hook_pointer(FH2Mod*, const char* name, void** slot, void* detour, void** original) {
    *original = *slot;
    g_hooks.push_back({name ? name : "", slot, detour, original, nullptr});
    return reinterpret_cast<FH2Hook*>(g_hooks.size());
}
static FH2Hook* hook_instruction(FH2Mod*, const char* name, void* at, FH2InstructionFn fn, void*) {
    g_hooks.push_back({name ? name : "", at, nullptr, nullptr, fn});
    return reinterpret_cast<FH2Hook*>(g_hooks.size());
}
static int enable_hooks(FH2Mod*, FH2Hook*) { return 1; }
static int disable_hooks(FH2Mod*, FH2Hook*) { return 1; }

// ---- A stand-in for Windows.Gaming.Input with one real pad ------------------------------------------
struct RealPad final : wgi::IGamepadRaw {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** o) override { *o = this; return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE get_Vibration(wgi::GamepadVibration*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE put_Vibration(wgi::GamepadVibration) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentReading(wgi::GamepadReading* r) override { *r = {}; r->Buttons = 0x4; return S_OK; }
} g_real_pad;
struct RealView final : wgi::IVectorViewRaw {
    ULONG refs = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** o) override { *o = this; return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetAt(UINT32 i, IInspectable** o) override { if (i) return E_BOUNDS; *o = &g_real_pad; return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Size(UINT32* n) override { *n = 1; return S_OK; }
    HRESULT STDMETHODCALLTYPE IndexOf(IInspectable* it, UINT32* i, boolean* f) override { *i = 0; *f = it == &g_real_pad; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetMany(UINT32, UINT32, IInspectable**, UINT32* a) override { *a = 0; return S_OK; }
} g_real_view;
static int g_added = 0;
struct RealStatics final : wgi::IGamepadStaticsRaw {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** o) override { *o = this; return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE add_GamepadAdded(IUnknown*, EventRegistrationToken* t) override { ++g_added; t->value = 7; return S_OK; }
    HRESULT STDMETHODCALLTYPE remove_GamepadAdded(EventRegistrationToken) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE add_GamepadRemoved(IUnknown*, EventRegistrationToken* t) override { t->value = 8; return S_OK; }
    HRESULT STDMETHODCALLTYPE remove_GamepadRemoved(EventRegistrationToken) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Gamepads(IInspectable** v) override { g_real_view.AddRef(); *v = &g_real_view; return S_OK; }
} g_real_statics;
static HRESULT WINAPI fake_factory(HSTRING, REFIID, void** out) { *out = &g_real_statics; return S_OK; }
static HRESULT WINAPI no_factory(HSTRING, REFIID, void** out) { *out = nullptr; return REGDB_E_CLASSNOTREG; }

// ---- A stand-in for the game's input manager ------------------------------------------------------
alignas(16) static char g_bucket[16];
static char* g_buckets[1] = {g_bucket};
alignas(16) static char g_table[0x20];
static char* g_set[1] = {g_table};
alignas(16) static char g_manager[0x60];
alignas(16) static char g_tables[64 * 32];
static std::vector<char> g_system(0xAC00);
static int32_t g_bindings[8][7];
static char* g_players[3];
static int fake_trigger_type(const void*, int action) { return action == 17 ? 0x08 : action == 6 ? 0x04 : 0x01; }
static int g_button_state_value = 0;
static int fake_button_state(void*, int) { return g_button_state_value; }

static void make_game_input() {
    *reinterpret_cast<uint32_t*>(g_bucket + 4) = 1; // player 0 takes input
    *reinterpret_cast<char***>(g_table + 8)    = g_buckets;
    *reinterpret_cast<uint64_t*>(g_table + 0x10) = 1;
    *reinterpret_cast<char***>(g_manager + 0x38) = g_set;
    const int32_t b[][2] = {{0, 11}, {1, 10}, {2, 31}, {3, 12}, {4, 15}, {5, 14}, {20, 13}};
    int n = 0;
    for (const auto& x : b) {
        int32_t* e = g_bindings[n++];
        e[0] = 0, e[1] = x[0], e[2] = x[1], e[3] = 0x25, e[4] = 0x25, e[5] = 0, e[6] = 0;
    }
    g_players[0] = reinterpret_cast<char*>(g_bindings);
    g_players[1] = reinterpret_cast<char*>(g_bindings) + 28 * n;
    g_players[2] = g_players[1];
    *reinterpret_cast<char**>(g_manager + 0x20) = reinterpret_cast<char*>(g_players);
    *reinterpret_cast<char**>(g_manager + 0x28) = reinterpret_cast<char*>(g_players) + 24;
    *reinterpret_cast<char**>(g_system.data() + 0xAB10) = g_tables;
    *reinterpret_cast<uint32_t*>(g_system.data() + 0xAB40) = 300;
    *reinterpret_cast<uint32_t*>(g_system.data() + 0xAB44) = 100;
}

using KeyFn      = void (*)(int, int);
using FrontFn    = void (*)(int);
using ReadFn     = void (*)(wgi::GamepadReading*);
using FactoryFn  = HRESULT (*)(void*, HSTRING, REFIID, void**);
using GameFn     = void (*)(char*, int (*)(const void*, int), int (*)(void*, int));
using FrameFn    = void (*)();
using RoadFn     = int (*)();
using XFn        = DWORD (*)(void*, DWORD, XINPUT_STATE*);
using PressedFn  = bool (*)(void*, int, int, bool);
using ValueFn    = float (*)(void*, int, int);
using LayoutFn   = void* (*)(void*, int, uint8_t, void*);
using AnyFn      = void (*)(int);

static void write_png(const std::wstring& path, const unsigned char* rgba, int w, int h) {
    // stored (uncompressed) deflate blocks: enough to look at
    std::vector<unsigned char> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + y * w * 4, rgba + (y + 1) * w * 4);
    }
    auto crc = [](const unsigned char* d, size_t n) {
        unsigned c = 0xffffffffu;
        for (size_t i = 0; i < n; ++i) {
            c ^= d[i];
            for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
        }
        return ~c;
    };
    std::vector<unsigned char> z{0x78, 0x01};
    unsigned a = 1, b = 0;
    for (size_t pos = 0; pos < raw.size();) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<unsigned char>(n & 0xff));
        z.push_back(static_cast<unsigned char>(n >> 8));
        z.push_back(static_cast<unsigned char>(~n & 0xff));
        z.push_back(static_cast<unsigned char>((~n >> 8) & 0xff));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    for (unsigned char c : raw) a = (a + c) % 65521, b = (b + a) % 65521;
    const unsigned adler = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) z.push_back(static_cast<unsigned char>((adler >> s) & 0xff));
    std::vector<unsigned char> png{0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    auto chunk = [&](const char* type, const std::vector<unsigned char>& data) {
        const unsigned n = static_cast<unsigned>(data.size());
        for (int s = 24; s >= 0; s -= 8) png.push_back(static_cast<unsigned char>((n >> s) & 0xff));
        std::vector<unsigned char> td(type, type + 4);
        td.insert(td.end(), data.begin(), data.end());
        png.insert(png.end(), td.begin(), td.end());
        const unsigned c = crc(td.data(), td.size());
        for (int s = 24; s >= 0; s -= 8) png.push_back(static_cast<unsigned char>((c >> s) & 0xff));
    };
    std::vector<unsigned char> ihdr;
    for (int v : {w, h})
        for (int s = 24; s >= 0; s -= 8) ihdr.push_back(static_cast<unsigned char>((v >> s) & 0xff));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wb");
    if (f) {
        std::fwrite(png.data(), 1, png.size(), f);
        std::fclose(f);
    }
}

static void write_ini(const std::wstring& path, const char* text) {
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wb");
    std::fwrite(text, 1, std::strlen(text), f);
    std::fclose(f);
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) { std::printf("usage: harness <FH2Controls.dll> <scratch azimuth dir> <game.exe>\n"); return 2; }
    test_core();

    g_dir = argv[2];
    HMODULE exe = LoadLibraryExW(argv[3], nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
    if (!exe) { std::printf("can't map the game exe (%lu)\n", GetLastError()); return 2; }
    g_game = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(exe) & ~uintptr_t(3));
    // So this exe imports RoGetActivationFactory, as azimuth.exe does.
    void* volatile keep = reinterpret_cast<void*>(&RoGetActivationFactory);
    (void)keep;

    FH2ModApi api{};
    api.size             = sizeof api;
    api.api_version      = FH2MOD_API_VERSION;
    api.platform_version = "2.8.0";
    api.azimuth_dir      = g_dir.c_str();
    api.game_dir         = L"";
    api.register_mod     = register_mod;
    api.log              = log_line;
    api.add_setting      = add_setting;
    api.set_setting      = set_setting;
    api.set_slider       = set_slider;
    api.add_hotkey       = add_hotkey;
    api.set_notice       = set_notice;
    api.wait_for_code    = wait_for_code;
    api.wait_for_mods    = wait_for_mods;
    api.mod_version      = mod_version;
    api.hook_function    = hook_function;
    api.hook_pointer     = hook_pointer;
    api.hook_instruction = hook_instruction;
    api.enable_hooks     = enable_hooks;
    api.disable_hooks    = disable_hooks;

    HMODULE mod = LoadLibraryW(argv[1]);
    if (!mod) { std::printf("can't load the mod (%lu)\n", GetLastError()); return 2; }
    auto init    = reinterpret_cast<int (*)(const FH2ModApi*)>(GetProcAddress(mod, "fh2mod_init"));
    auto key     = reinterpret_cast<KeyFn>(GetProcAddress(mod, "fc_test_key"));
    auto front   = reinterpret_cast<FrontFn>(GetProcAddress(mod, "fc_test_front"));
    auto read    = reinterpret_cast<ReadFn>(GetProcAddress(mod, "fc_test_reading"));
    auto factory = reinterpret_cast<FactoryFn>(GetProcAddress(mod, "fc_test_factory"));
    auto game    = reinterpret_cast<GameFn>(GetProcAddress(mod, "fc_test_game_input"));
    auto frame   = reinterpret_cast<FrameFn>(GetProcAddress(mod, "fc_test_frame"));
    auto road    = reinterpret_cast<RoadFn>(GetProcAddress(mod, "fc_test_road"));
    auto xinput  = reinterpret_cast<XFn>(GetProcAddress(mod, "fc_test_xinput"));
    auto anymap  = reinterpret_cast<AnyFn>(GetProcAddress(mod, "fc_test_map_from_anywhere"));
    expect(init && key && front && read && factory && game && frame && road && xinput && anymap, "fh2mod_init and the test entry points are exported");
    if (!(init && key && front && read && factory && game && frame && road && xinput && anymap)) return 1;
    front(1);
    for (int vk = 1; vk < 256; ++vk) key(vk, 0);

    CreateDirectoryW((g_dir + L"\\fh2-mods").c_str(), nullptr);
    write_ini(g_dir + L"\\fh2-mods\\CustomBindings.ini", "[Bindings]\nButtonA=Handbrake\nButtonB=Rewind\nRightThumbButton=Map\nLeftTrigger=Brake\nRightTrigger=Gas\n");
    expect(init(&api) == 0, "fh2mod_init returns 0");
    const std::wstring ini = g_dir + L"\\fh2-mods\\FH2Controls.ini";
    expect(GetFileAttributesW(ini.c_str()) != INVALID_FILE_ATTRIBUTES && logged("wrote FH2Controls.ini (the controller layout from CustomBindings.ini)"),
           "the first time, FH2Controls.ini is written, with CustomBindings' layout");
    expect(g_settings.count("Enabled") && g_settings.count("SteeringTime") && g_settings.count("PedalTime"), "Mods page: Keyboard, and the speed sliders");
    expect(hook_named("RoGetActivationFactory") && hook_named("XInputGetState"), "hooks: Azimuth's RoGetActivationFactory import, XInputGetState");

    // ---- The controller Azimuth gets -------------------------------------------------------------------
    HSTRING cls = nullptr;
    WindowsCreateString(wgi::kGamepadClass, static_cast<UINT32>(wcslen(wgi::kGamepadClass)), &cls);
    void* out = nullptr;
    expect(factory(reinterpret_cast<void*>(&fake_factory), cls, wgi::kIGamepadStatics, &out) == S_OK && out && out != &g_real_statics,
           "Azimuth gets the statics wrapped");
    auto* statics = static_cast<wgi::IGamepadStaticsRaw*>(out);
    EventRegistrationToken tok{};
    expect(statics->add_GamepadAdded(nullptr, &tok) == S_OK && g_added == 1 && tok.value == 7, "GamepadAdded goes to the real ones");
    IInspectable* view_i = nullptr;
    statics->get_Gamepads(&view_i);
    auto* view = static_cast<wgi::IVectorViewRaw*>(view_i);
    UINT32 n = 0;
    view->get_Size(&n);
    IInspectable* first = nullptr;
    IInspectable* second = nullptr;
    view->GetAt(0, &first);
    view->GetAt(1, &second);
    expect(n == 2 && first == &g_real_pad && second && second != first, "the list: the real pad, then the keyboard's");
    IUnknown* unk = nullptr;
    void* gp = nullptr;
    expect(second->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&unk)) == S_OK &&
               second->QueryInterface(wgi::kIGamepad, &gp) == S_OK && gp == second,
           "the keyboard's pad is an IGamepad");
    void* marshal = nullptr;
    expect(second->QueryInterface(__uuidof(IMarshal), &marshal) == S_OK && marshal, "and agile (the free-threaded marshaler)");
    if (marshal) static_cast<IUnknown*>(marshal)->Release();
    void* vmarshal = nullptr;
    expect(statics->QueryInterface(__uuidof(IMarshal), &vmarshal) == S_OK, "the statics too");
    if (vmarshal) static_cast<IUnknown*>(vmarshal)->Release();
    auto* keypad = static_cast<wgi::IGamepadRaw*>(second);
    UINT32 idx = 9;
    boolean found = false;
    view->IndexOf(second, &idx, &found);
    expect(found && idx == 1, "IndexOf finds it");
    view->Release();
    void* none = nullptr;
    expect(factory(reinterpret_cast<void*>(&no_factory), cls, wgi::kIGamepadStatics, &none) == S_OK && none,
           "without Windows.Gaming.Input there's still the keyboard's pad");
    static_cast<wgi::IGamepadStaticsRaw*>(none)->get_Gamepads(&view_i);
    static_cast<wgi::IVectorViewRaw*>(view_i)->get_Size(&n);
    expect(n == 1, "and it's the only one");

    // Keys through it (the default ini: Enter is A in a menu, W the left stick up)
    Sleep(50);
    wgi::GamepadReading r{};
    keypad->GetCurrentReading(&r);
    expect(r.Buttons == 0 && r.LeftThumbstickY == 0 && r.Timestamp != 0, "it sits still with no keys held");
    key(VK_RETURN, 1);
    key('W', 1);
    Sleep(60);
    keypad->GetCurrentReading(&r);
    expect(r.Buttons == 0x4 && r.LeftThumbstickY == 1.0, "menu: Enter is A, W the stick up (as Azimuth reads it)");
    XINPUT_STATE xs{};
    expect(xinput(reinterpret_cast<void*>(&fake_xinput), 0, &xs) == ERROR_SUCCESS && xs.Gamepad.wButtons == XINPUT_GAMEPAD_A &&
               xs.Gamepad.sThumbLY == 32767,
           "XInput pad 0 is the keyboard when there's no pad");
    expect(xinput(reinterpret_cast<void*>(&fake_xinput), 1, &xs) == ERROR_DEVICE_NOT_CONNECTED, "pads 1-3 are left alone");
    front(0);
    Sleep(60);
    keypad->GetCurrentReading(&r);
    expect(r.Buttons == 0, "with the game in the background the keys do nothing");
    front(1);
    key(VK_RETURN, 0);
    key('W', 0);

    // ---- The game: the road, the answers, Map ---------------------------------------------------------
    make_game_input();
    game(g_system.data(), fake_trigger_type, fake_button_state);
    Hook* pressed_h = hook_named("IsActionPressed");
    Hook* value_h   = hook_named("action value");
    Hook* layout_h  = hook_named("LoadLayout (");
    Hook* frame_h   = hook_named("input update");
    expect(pressed_h && value_h && layout_h && frame_h && hook_named("column read") && logged("actions on") &&
               logged("[Controller] ready"),
           "the game's hooks are made (its code checked out against the exe)");
    expect(frame_h && frame_h->target == g_game + 0x1075524 && pressed_h->target == g_game + 0x1072510 &&
               layout_h->target == g_game + 0x2DC300,
           "at the right places in the exe");
    if (!(pressed_h && value_h && layout_h)) return 1;
    auto pressed = reinterpret_cast<PressedFn>(pressed_h->detour);
    auto value   = reinterpret_cast<ValueFn>(value_h->detour);
    auto layout  = reinterpret_cast<LayoutFn>(layout_h->detour);
    anymap(1);

    frame();
    expect(!road(), "no asks yet: in a menu");
    pressed(g_manager, 0, 0, false);
    frame();
    expect(road() && logged("the road's bindings now: Gas=RT Brake=LT Steer=left stick Handbrake=A ShiftUp=Y ShiftDown=X"),
           "the game asks for the road's actions: on the road, and its bindings are read");
    key(VK_RETURN, 1);
    Sleep(60);
    keypad->GetCurrentReading(&r);
    expect(r.Buttons == 0, "road: Enter (A, the handbrake's button) presses nothing");
    key(VK_RETURN, 0);
    key('W', 1);
    Sleep(250);
    keypad->GetCurrentReading(&r);
    expect(r.RightTrigger == 1.0 && r.LeftThumbstickY == 0, "road: W is gas on RT");
    key('W', 0);
    key('M', 1); // Map: on no button, so the mod answers when WaitForPause asks
    Sleep(30);
    pressed(g_manager, 0, 0, false);
    frame();
    expect(pressed(g_manager, 0, 22, true), "Map key: the game's ask for the map gets a yes, once");
    expect(!pressed(g_manager, 0, 22, true), "and not again that frame (used up)");
    frame();
    expect(!pressed(g_manager, 0, 22, true), "nor while it's held");
    key('M', 0);
    g_button_state_value = 1;
    expect(!pressed(g_manager, 0, 22, true), "a controller's button only opens it when [Controller] has Map on it");

    // ---- The layout ---------------------------------------------------------------------------------
    layout(nullptr, 13, 1, nullptr); // the profile's "Custom Layout 1", as CustomBindings left it
    expect(g_layout_id == 0 && g_columns_read.size() == 20 && g_columns_read[12] == "Handbrake" &&
               g_columns_read[13] == "Rewind" && g_columns_read[19] == "Gas" && g_columns_read[7].empty() &&
               g_columns_read[0].empty(),
           "CustomBindings' layout, moved over: in place of the game's, Map kept off the game's columns");
    pressed(g_manager, 0, 0, false);
    frame();
    g_button_state_value = 1;
    expect(pressed(g_manager, 0, 22, true), "and its Map button (RS) opens the map");
    g_button_state_value = 0;
    write_ini(ini, "[Controller]\nCustom = 1\nButtonA = Handbrake\nRightThumbButton = Map\nLeftShoulder = LookStraightBack\n");
    Sleep(1200); // the pad thread reads it again
    expect(logged("read again FH2Controls.ini"), "an edited ini is read again while the game runs");
    layout(nullptr, 13, 1, nullptr);
    expect(g_layout_id == 0 && g_columns_read[12] == "Handbrake" && g_columns_read[16] == "LookStraightBack" &&
               g_columns_read[7].empty() && g_columns_read[13].empty(),
           "Custom = 1: [Controller]'s actions go in (a 16-letter one too), Map and the rest unbound for the game");
    pressed(g_manager, 0, 0, false);
    frame();
    g_button_state_value = 1;
    expect(pressed(g_manager, 0, 22, true), "and a controller's Map button (RS) opens the map");
    g_button_state_value = 0;
    write_ini(ini, "[Controller]\nCustom = 0\n");
    Sleep(1200);
    layout(nullptr, 13, 1, nullptr);
    expect(g_layout_id == 0 && logged("isn't one of the game's"), "a layout the game doesn't have (CustomBindings' 14th) loads its first");

    // ---- Answering for an action that isn't on a button ------------------------------------------------
    write_ini(ini, "[Keyboard]\nLookStraightBack = B\nClutch = RShift\n");
    Sleep(1200);
    key('B', 1);
    Sleep(30);
    pressed(g_manager, 0, 0, false);
    frame();
    expect(pressed(g_manager, 0, 13, false), "an action on no button: the key answers for it");
    key('B', 0);
    key(VK_RSHIFT, 1);
    Sleep(30);
    frame();
    expect(value(g_manager, 0, 6) == 1.0f, "and for one read as a value (clutch)");
    key(VK_RSHIFT, 0);
    g_own_pressed = true;
    expect(pressed(g_manager, 0, 13, false), "the game's own yes still comes through");
    g_own_pressed = false;
    expect(!pressed(g_manager, 1, 13, false), "another player's actions are left alone");

    // road -> menu when the asks stop
    Sleep(300);
    frame();
    expect(!road(), "no asks for a while: back in a menu");

    // ---- Keycap prompts (the default ini's keys), written out as PNGs to look at ----------------------
    write_ini(ini, fc::kDefaultIni);
    Sleep(1200);
    using KeycapFn = int (*)(const char*, unsigned char*, wchar_t*, int);
    auto keycap_fn = reinterpret_cast<KeycapFn>(GetProcAddress(mod, "fc_test_keycap"));
    std::vector<unsigned char> px(128 * 128 * 4);
    wchar_t label[64];
    struct { const char* path; const wchar_t* want; } icons[] = {
        {"/ForzaUI;component/Textures/Data_Bound/ControllerIcons/Controller_A.png", L"Enter/Space"},
        {"game:\\Media\\UI\\Textures\\Data_Bound\\ControllerIcons\\Medium\\Controller_B.swatchbin", L"Bksp"},
        {"ControllerIcons/Controller_LB.png", L"["},
        {"ControllerIcons/Controller_DPadUpDown.png", L"\x2191/\x2193"},
        {"ControllerIcons/Controller_Menu.png", L"Esc"},
    };
    bool all = keycap_fn != nullptr;
    for (const auto& ic : icons) {
        const int n = keycap_fn ? keycap_fn(ic.path, px.data(), label, 64) : 0;
        std::printf("keycap %s -> \"%ls\"\n", ic.path, label);
        all = all && n == 128 && px[(64 * 128 + 64) * 4 + 3] > 200 && px[3] == 0;
        if (n) write_png(g_dir + L"\\keycap_" + std::to_wstring(&ic - icons) + L".png", px.data(), 128, 128);
    }
    expect(all, "button prompts: keycaps drawn for A, B, LB, d-pad up/down, Menu (opaque middle, clear corners)");
    expect(keycap_fn && keycap_fn("ControllerIcons/Bumper_Silhouette.png", px.data(), label, 64) == 0, "silhouette icons stay the game's");

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
