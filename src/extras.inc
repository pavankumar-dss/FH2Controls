// Included by fh2controls.cpp, inside its namespace, after the game section: keycap button
// prompts and mouse look. (Both found with the help of the notes in Extra Options PR #101 by
// byzyn4ik, which does the same for its native keyboard & mouse; this is an implementation of
// our own.)

// ---- Keycap button prompts -----------------------------------------------------------------------
// The game's button icons are textures in media\ui\textures\data_bound\ControllerIcons.zip
// (.swatchbin, Xbox-tiled), asked for as ".../ControllerIcons/Controller_A.png" by the XAML help
// bars and as ...\ControllerIcons\Medium\Controller_A.swatchbin by the HUD. Every UI bitmap is
// loaded through game+0x1104E70(handle+0x50, path, options, callback); some arrive as bytes through
// game+0x1105510(handle+0x50, bytes, size, own, options). For a ControllerIcons handle the mod
// skips the file and hands the bitmap a keycap of its own instead, drawn with the key the button
// is on, the way Extra Options hands the game its custom gamerpic: a pixel buffer
// (game+0x165E600) in the bitmap's +0x80, queued as RGBA8 (format 3) through game+0x1105510, and a
// bitmap update (game+0x16E2DC0) on the UI manager.
constexpr uintptr_t kTextureLoad     = 0x1104E70;
constexpr uintptr_t kTextureBytes    = 0x1105510;
constexpr uintptr_t kPixelBuffer     = 0x165E600;
constexpr uintptr_t kBitmapUpdate    = 0x16E2DC0;
constexpr uintptr_t kUiManager       = 0x36E0AE8;
constexpr uintptr_t kTextureHandleVt = 0x2F31430; // AVUITextureHandle
constexpr char kTextureLoadBytes[]   = "\x4C\x89\x4C\x24\x20\x56\x57\x41\x56\x48\x83\xEC\x40";
constexpr char kTextureBytesBytes[]  = "\x48\x8B\xC4\x55\x48\x8D\x68\xA9\x48\x81\xEC\xF0\x00\x00\x00";
constexpr int kKeycap                = 128;

struct TextureOptions { // the loader's options: an MSVC std::string name, then the texture's shape
    union {
        char buf[16];
        const char* ptr;
    };
    uint64_t length, capacity;
    uint32_t priority, format, width, height;
    uint8_t no_cache, pad[7];
};

using TextureLoadFn  = void* (*)(void* context, const char* path, void* options, void** callback);
using TextureBytesFn = void (*)(void* context, const void* bytes, int size, unsigned char own, const void* options);
using PixelBufferFn  = void** (*)(void** out, int size);
using BitmapUpdateFn = void (*)(void* manager, void** bitmap);
TextureLoadFn g_orig_texture_load   = nullptr;
TextureBytesFn g_orig_texture_bytes = nullptr;
std::atomic<int> g_prompt_logs{0};

// What a key is called on a keycap: short.
std::wstring keycap_name(uint8_t vk) {
    switch (vk) {
    case VK_SPACE: return L"Space";
    case VK_RETURN: return L"Enter";
    case VK_ESCAPE: return L"Esc";
    case VK_BACK: return L"Bksp";
    case VK_TAB: return L"Tab";
    case VK_UP: return L"\x2191";
    case VK_DOWN: return L"\x2193";
    case VK_LEFT: return L"\x2190";
    case VK_RIGHT: return L"\x2192";
    case VK_LSHIFT: return L"Shift";
    case VK_RSHIFT: return L"RShift";
    case VK_LCONTROL: return L"Ctrl";
    case VK_RCONTROL: return L"RCtrl";
    case VK_LMENU: return L"Alt";
    case VK_RMENU: return L"AltGr";
    case VK_CAPITAL: return L"Caps";
    case VK_INSERT: return L"Ins";
    case VK_DELETE: return L"Del";
    case VK_PRIOR: return L"PgUp";
    case VK_NEXT: return L"PgDn";
    case VK_ADD: return L"Num+";
    case VK_SUBTRACT: return L"Num-";
    case VK_MULTIPLY: return L"Num*";
    case VK_DIVIDE: return L"Num/";
    case VK_DECIMAL: return L"Num.";
    default: break;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return L"Num" + std::wstring(1, static_cast<wchar_t>(L'0' + vk - VK_NUMPAD0));
    const std::string n = fc::key_name(vk);
    return std::wstring(n.begin(), n.end());
}

// The key a controller input is on in a menu (the first one bound), or 0.
uint8_t key_for(const fc::Keymap& km, int elem) {
    for (int vk = 1; vk < 256; ++vk)
        for (int i = 0; i < km.count[vk]; ++i)
            if (km.target[vk][i] == elem) return static_cast<uint8_t>(vk);
    return 0;
}

// The inputs an icon stands for, from its file name; empty for the silhouettes.
std::vector<int> icon_elems(const std::string& path) {
    std::string name = path;
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const size_t dot = name.find('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (name.rfind("controller_", 0) != 0) return {};
    name = name.substr(11);
    static const struct { const char* name; std::initializer_list<int> elems; } kIcons[] = {
        {"a", {fc::A}}, {"b", {fc::B}}, {"x", {fc::X}}, {"y", {fc::Y}}, {"lb", {fc::LB}}, {"rb", {fc::RB}},
        {"lt", {fc::LT}}, {"rt", {fc::RT}}, {"menu", {fc::Menu}}, {"view", {fc::View}}, {"l3", {fc::LS}},
        {"r3", {fc::RS}}, {"dpadup", {fc::DUp}}, {"dpaddown", {fc::DDown}}, {"dpadleft", {fc::DLeft}},
        {"dpadright", {fc::DRight}}, {"dpadupdown", {fc::DUp, fc::DDown}},
        {"dpadleftright", {fc::DLeft, fc::DRight}}, {"dpad", {fc::DUp, fc::DDown, fc::DLeft, fc::DRight}},
        {"leftthumb_up", {fc::LUp}}, {"leftthumb_down", {fc::LDown}}, {"leftthumb_left", {fc::LLeft}},
        {"leftthumb_right", {fc::LRight}}, {"leftthumb_updown", {fc::LUp, fc::LDown}},
        {"leftthumb_leftright", {fc::LLeft, fc::LRight}}, {"leftthumb", {fc::LLeft, fc::LRight}},
        {"rightthumb_up", {fc::RUp}}, {"rightthumb_down", {fc::RDown}}, {"rightthumb_left", {fc::RLeft}},
        {"rightthumb_right", {fc::RRight}}, {"rightthumb_updown", {fc::RUp, fc::RDown}},
        {"rightthumb_leftright", {fc::RLeft, fc::RRight}},
        {"rightthumb", {fc::RUp, fc::RDown, fc::RLeft, fc::RRight}},
    };
    for (const auto& i : kIcons)
        if (name == i.name) return std::vector<int>(i.elems);
    return {};
}

// "Space", "8/2"... for the inputs, from the keys; empty if none is on a key.
std::wstring keycap_label(const fc::Keymap& km, const std::vector<int>& elems) {
    std::wstring out;
    for (int e : elems) {
        const uint8_t vk = key_for(km, e);
        if (!vk) continue;
        if (!out.empty()) out += L"/";
        out += keycap_name(vk);
    }
    return out;
}

// A keycap: a dark rounded key with a light rim and the label, RGBA8, drawn at twice the size
// with GDI and scaled down for smooth edges (GDI's shapes aren't antialiased).
std::vector<uint8_t> draw_keycap(const std::wstring& label) {
    const int big = kKeycap * 2;
    BITMAPINFO bi{};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), big, -big, 1, 32, BI_RGB};
    void* bits    = nullptr;
    HDC dc        = CreateCompatibleDC(nullptr);
    HBITMAP bmp   = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    std::vector<uint8_t> out(kKeycap * kKeycap * 4, 0);
    if (!dc || !bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        if (dc) DeleteDC(dc);
        return out;
    }
    HGDIOBJ old_bmp = SelectObject(dc, bmp);
    auto* px        = static_cast<uint32_t*>(bits);
    std::vector<uint8_t> shape(big * big), rim(big * big), text(big * big);
    auto grab = [&](std::vector<uint8_t>& mask) {
        for (int i = 0; i < big * big; ++i) mask[i] = static_cast<uint8_t>(px[i] & 0xFF);
    };
    auto clear = [&] { std::memset(px, 0, big * big * 4); };
    const int pad = 10, radius = 70, rim_width = 12;
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HPEN no_pen  = static_cast<HPEN>(GetStockObject(NULL_PEN));
    // the key's shape
    clear();
    SelectObject(dc, white);
    SelectObject(dc, no_pen);
    RoundRect(dc, pad, pad, big - pad, big - pad, radius, radius);
    grab(shape);
    // its rim: the shape less a smaller one
    clear();
    RoundRect(dc, pad, pad, big - pad, big - pad, radius, radius);
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    SelectObject(dc, black);
    RoundRect(dc, pad + rim_width, pad + rim_width, big - pad - rim_width, big - pad - rim_width, radius - rim_width, radius - rim_width);
    grab(rim);
    // the label, as large as fits
    clear();
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT box{pad + rim_width + 8, pad + rim_width, big - pad - rim_width - 8, big - pad - rim_width};
    HFONT font = nullptr;
    for (int h = label.size() <= 1 ? 150 : 120; h >= 24; h -= 6) {
        if (font) DeleteObject(font);
        font = CreateFontW(-h, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(dc, font);
        SIZE size{};
        GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &size);
        if (size.cx <= box.right - box.left && size.cy <= box.bottom - box.top + 20) break;
    }
    DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();
    grab(text);
    // down to size: fill (40,40,44), rim (225,225,225), text white; alpha from the shape
    for (int y = 0; y < kKeycap; ++y)
        for (int x = 0; x < kKeycap; ++x) {
            float s = 0, r = 0, t = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const int i = (y * 2 + dy) * big + x * 2 + dx;
                    s += shape[i] / 255.0f, r += rim[i] / 255.0f, t += text[i] / 255.0f;
                }
            s /= 4, r /= 4, t /= 4;
            float c[3] = {40, 40, 44};
            for (int k = 0; k < 3; ++k) {
                c[k] += (225.0f - c[k]) * (s > 0 ? std::min(1.0f, r / s) : 0);
                c[k] += (255.0f - c[k]) * t;
            }
            uint8_t* o = &out[(y * kKeycap + x) * 4];
            o[0] = static_cast<uint8_t>(c[0]), o[1] = static_cast<uint8_t>(c[1]), o[2] = static_cast<uint8_t>(c[2]);
            o[3] = static_cast<uint8_t>(std::min(255.0f, std::max(s, t) * 235.0f));
        }
    SelectObject(dc, old_bmp);
    if (font) DeleteObject(font);
    DeleteObject(white);
    DeleteObject(black);
    DeleteObject(bmp);
    DeleteDC(dc);
    return out;
}

std::mutex g_keycap_lock;
std::vector<std::pair<std::wstring, std::vector<uint8_t>>> g_keycaps; // drawn once per label

const std::vector<uint8_t>* keycap(const std::wstring& label) {
    std::lock_guard<std::mutex> l(g_keycap_lock);
    for (auto& k : g_keycaps)
        if (k.first == label) return &k.second;
    g_keycaps.emplace_back(label, draw_keycap(label));
    return &g_keycaps.back().second;
}

// The bitmap behind `context` (an AVUITextureHandle's +0x50) gets the keycap.
bool give_keycap(void* context, const std::vector<uint8_t>& rgba) {
    char* handle = static_cast<char*>(context) - 0x50;
    if (!g_game || at<char*>(handle, 0) != g_game + kTextureHandleVt) return false;
    void* manager = *reinterpret_cast<void**>(g_game + kUiManager);
    if (!manager) return false;
    void* buffer = nullptr;
    reinterpret_cast<PixelBufferFn>(g_game + kPixelBuffer)(&buffer, static_cast<int>(rgba.size()));
    if (!buffer) return false;
    void* storage = at<void*>(buffer, 0x28);
    if (!storage) return false;
    std::memcpy(storage, rgba.data(), rgba.size());
    void** member = reinterpret_cast<void**>(handle + 0x80); // keeps the pixels alive while the renderer reads them
    if (void* old = *member) {
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(static_cast<char*>(old) + 0x10)) == 0)
            reinterpret_cast<void (*)(void*)>(g_game + 0x257730)(old);
    }
    *member = buffer;
    TextureOptions options{};
    options.capacity = 15;
    options.priority = 0x60000;
    options.format   = 3; // RGBA8, with its size: no file decoding
    options.width = options.height = kKeycap;
    g_orig_texture_bytes(context, storage, static_cast<int>(rgba.size()), 0, &options);
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(handle + 0x10));
    void* owned = handle;
    reinterpret_cast<BitmapUpdateFn>(g_game + kBitmapUpdate)(manager, &owned);
    return true;
}

// The path a load is for: its second argument, or (older call sites) the options' name.
std::string texture_path(const char* path, const void* options) {
    if (path && *path) return std::string(path, strnlen(path, 300));
    if (!options) return {};
    const auto* o = static_cast<const TextureOptions*>(options);
    if (o->length == 0 || o->length > 300) return {};
    return std::string(o->capacity >= 16 ? o->ptr : o->buf, static_cast<size_t>(o->length));
}

// For a controller icon the keycap, if prompts are on and the icon's button is on a key.
bool is_controller_icon(const std::string& path) {
    std::string lower = path;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower.find("controllericons") != std::string::npos; // the game asks in lower case
}

const std::vector<uint8_t>* prompt_for(const std::string& path) {
    if (!is_controller_icon(path)) return nullptr;
    const auto cfg = config();
    if (!cfg->key_prompts || !g_enabled) return nullptr;
    const std::wstring label = keycap_label(cfg->keys, icon_elems(path));
    if (label.empty()) return nullptr;
    if (g_prompt_logs < 3) {
        ++g_prompt_logs;
        log_fmt("button prompt %s shows the keys (%ls)", path.c_str(), label.c_str());
    }
    return keycap(label);
}

void* texture_load_detour(void* context, const char* path, void* options, void** callback) {
    if (context && (!callback || !*callback)) {
        if (const auto* k = prompt_for(texture_path(path, options)))
            if (give_keycap(context, *k)) return nullptr; // the file isn't loaded
    }
    return g_orig_texture_load(context, path, options, callback);
}

void texture_bytes_detour(void* context, const void* bytes, int size, unsigned char own, const void* options) {
    const auto* o = static_cast<const TextureOptions*>(options);
    if (context && o && o->format == 0) { // a file's bytes (ours come with format 3)
        if (const auto* k = prompt_for(texture_path(nullptr, options)))
            if (give_keycap(context, *k)) return;
    }
    g_orig_texture_bytes(context, bytes, size, own, options);
}

// ---- Mouse look -----------------------------------------------------------------------------------
// The driving look (game+0x103F9D0, once a frame per look object) turns from the right stick
// (LookLeft/LookRight...); after it has run, the mouse's yaw goes in its place: the object's
// +0x18 and +0x1C (radians; LookStraightBack, +0x10, sets +0x1C to pi), clamped to its limit at
// +0x20 when there is one. The mouse comes from raw input on a thread of the mod's own. Moving it
// turns the look; left alone for MouseLookReturn seconds, the look eases back ahead and the right
// stick (IJKL) has it again.
constexpr uintptr_t kDriveLook  = 0x103F9D0;
constexpr char kDriveLookBytes[] = "\x48\x8B\xC4\x4C\x89\x40\x18\x55\x56\x57\x48\x83\xEC\x60";
using LookFn                     = void (*)(void* look, unsigned char active, void* functor);
LookFn g_orig_look               = nullptr;
std::atomic<int> g_mouse_dx{0};       // counts since the last look
std::atomic<DWORD> g_mouse_moved_at{0};
float g_look_yaw   = 0;               // the game's thread's
bool g_look_mouse  = false;           // the mouse has the look now
DWORD g_look_at    = 0;

// The mouse, through DirectInput, shared (non-exclusive, background) and polled: while the game is
// in front the mouse is taken exclusively through DirectInput (the pointer stops moving, and raw
// input and low-level hooks see nothing), but a shared DirectInput device still gets its motion.
DWORD WINAPI mouse_thread(LPVOID) {
    IDirectInput8W* di = nullptr;
    IDirectInputDevice8W* mouse = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                    reinterpret_cast<void**>(&di), nullptr);
    if (SUCCEEDED(hr)) hr = di->CreateDevice(GUID_SysMouse, &mouse, nullptr);
    if (SUCCEEDED(hr)) hr = mouse->SetDataFormat(&c_dfDIMouse);
    if (SUCCEEDED(hr)) hr = mouse->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) {
        log_fmt("couldn't read the mouse through DirectInput (0x%08lX): no mouse look", static_cast<unsigned long>(hr));
        return 0;
    }
    for (;;) {
        Sleep(4);
        DIMOUSESTATE st{};
        hr = mouse->GetDeviceState(sizeof st, &st);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            mouse->Acquire();
            continue;
        }
        if (FAILED(hr) || !st.lX) continue;
        if (g_front && g_road) {
            g_mouse_dx += st.lX;
            g_mouse_moved_at = GetTickCount();
        }
    }
}

void look_detour(void* look, unsigned char active, void* functor) {
    g_orig_look(look, active, functor);
    if (!active) return; // the game updates every camera's look; only the one in use counts
    const DWORD now = GetTickCount();
    const float dt  = g_look_at ? std::min(0.1f, (now - g_look_at) / 1000.0f) : 0.0f;
    g_look_at       = now;
    const auto cfg  = config();
    const int dx    = g_mouse_dx.exchange(0);
    if (!cfg->mouse_look || !g_enabled || !g_road || !g_front) {
        g_look_mouse = false;
        g_look_yaw   = 0;
        return;
    }
    char* o = static_cast<char*>(look);
    if (at<uint8_t>(o, 0x10)) return; // looking back: the game's
    const float limit = at<float>(o, 0x20);
    if (dx) {
        g_look_yaw += dx * 0.004f * cfg->mouse_sensitivity; // ~0.23 degrees a count at 1.0
        static bool logged = false;
        if (!logged) {
            logged = true;
            log_fmt("mouse look: the mouse turns the view (limit %.0f degrees)", (limit > 0 ? limit : 3.14159f) * 57.2958f);
        }
        g_look_mouse = true;
    } else if (g_look_mouse && cfg->mouse_return > 0 && now - g_mouse_moved_at.load() > cfg->mouse_return * 1000.0f) {
        g_look_yaw = fc::approach(g_look_yaw, 0.0f, dt, 0.35f * std::max(0.2f, std::fabs(g_look_yaw) / 3.14159f));
        if (g_look_yaw == 0) g_look_mouse = false; // the right stick has it again
    }
    if (!g_look_mouse) return;
    const float lim = limit > 0 && limit == limit ? limit : 3.14159f;
    g_look_yaw      = std::clamp(g_look_yaw, -lim, lim);
    *reinterpret_cast<volatile float*>(o + 0x18) = g_look_yaw;
    *reinterpret_cast<volatile float*>(o + 0x1C) = g_look_yaw;
}

// Called from game_thread once the game's code is ready.
void hook_extras(char* game) {
    if (std::memcmp(game + kTextureLoad, kTextureLoadBytes, sizeof kTextureLoadBytes - 1) != 0 ||
        std::memcmp(game + kTextureBytes, kTextureBytesBytes, sizeof kTextureBytesBytes - 1) != 0 ||
        at<char*>(game + kTextureHandleVt, 0) == nullptr) {
        log_fmt("the game's texture code isn't as expected: the button prompts stay the controller's");
    } else if (hook_all({g_api->hook_function(g_mod, "UI texture load (button prompts)", game + kTextureLoad,
                                              reinterpret_cast<void*>(&texture_load_detour), reinterpret_cast<void**>(&g_orig_texture_load)),
                         g_api->hook_function(g_mod, "UI texture bytes (button prompts)", game + kTextureBytes,
                                              reinterpret_cast<void*>(&texture_bytes_detour), reinterpret_cast<void**>(&g_orig_texture_bytes))},
                        "the UI's textures")) {
        log_fmt("button prompts ready (KeyPrompts = %d)", config()->key_prompts ? 1 : 0);
    }
    if (std::memcmp(game + kDriveLook, kDriveLookBytes, sizeof kDriveLookBytes - 1) != 0) {
        log_fmt("the game's look code isn't as expected: no mouse look");
    } else if (hook_all({g_api->hook_function(g_mod, "driving look (mouse look)", game + kDriveLook,
                                              reinterpret_cast<void*>(&look_detour), reinterpret_cast<void**>(&g_orig_look))},
                        "the driving look")) {
        if (HANDLE t = CreateThread(nullptr, 0, mouse_thread, nullptr, 0, nullptr)) CloseHandle(t);
        log_fmt("mouse look ready (MouseLook = %d)", config()->mouse_look ? 1 : 0);
    }
}
