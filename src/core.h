// FH2Controls' parts that need neither the game nor Windows hooks: key names, what a key can be
// bound to, reading FH2Controls.ini, and turning the keys held into a controller reading.
#pragma once

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fc {

inline bool same_text(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) return false;
    return *a == *b;
}

inline std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ---- Keys ------------------------------------------------------------------------------------
// The first name of a key is the one the mod writes; the others are read too (Keyvert's .NET
// names, and the short ones driveclub-recomp's default.ini uses).
struct KeyName {
    uint8_t vk;
    const char* name;
};

inline const KeyName kKeys[] = {
    {'A', "A"}, {'B', "B"}, {'C', "C"}, {'D', "D"}, {'E', "E"}, {'F', "F"}, {'G', "G"}, {'H', "H"},
    {'I', "I"}, {'J', "J"}, {'K', "K"}, {'L', "L"}, {'M', "M"}, {'N', "N"}, {'O', "O"}, {'P', "P"},
    {'Q', "Q"}, {'R', "R"}, {'S', "S"}, {'T', "T"}, {'U', "U"}, {'V', "V"}, {'W', "W"}, {'X', "X"},
    {'Y', "Y"}, {'Z', "Z"},
    {'0', "0"}, {'1', "1"}, {'2', "2"}, {'3', "3"}, {'4', "4"}, {'5', "5"}, {'6', "6"}, {'7', "7"},
    {'8', "8"}, {'9', "9"},
    {'0', "D0"}, {'1', "D1"}, {'2', "D2"}, {'3', "D3"}, {'4', "D4"}, {'5', "D5"}, {'6', "D6"},
    {'7', "D7"}, {'8', "D8"}, {'9', "D9"},
    {VK_F1, "F1"}, {VK_F2, "F2"}, {VK_F3, "F3"}, {VK_F4, "F4"}, {VK_F5, "F5"}, {VK_F6, "F6"},
    {VK_F7, "F7"}, {VK_F8, "F8"}, {VK_F9, "F9"}, {VK_F10, "F10"}, {VK_F11, "F11"}, {VK_F12, "F12"},
    {VK_SPACE, "Space"}, {VK_RETURN, "Enter"}, {VK_RETURN, "Return"}, {VK_ESCAPE, "Escape"},
    {VK_ESCAPE, "Esc"}, {VK_BACK, "Backspace"}, {VK_BACK, "Back"}, {VK_TAB, "Tab"},
    {VK_LSHIFT, "LShift"}, {VK_RSHIFT, "RShift"}, {VK_LCONTROL, "LCtrl"}, {VK_RCONTROL, "RCtrl"},
    {VK_LMENU, "LAlt"}, {VK_RMENU, "RAlt"},
    {VK_LSHIFT, "LShiftKey"}, {VK_RSHIFT, "RShiftKey"}, {VK_LCONTROL, "LControlKey"},
    {VK_RCONTROL, "RControlKey"}, {VK_LMENU, "LMenu"}, {VK_RMENU, "RMenu"},
    {VK_CAPITAL, "CapsLock"}, {VK_CAPITAL, "Capital"},
    {VK_UP, "Up"}, {VK_DOWN, "Down"}, {VK_LEFT, "Left"}, {VK_RIGHT, "Right"},
    {VK_INSERT, "Insert"}, {VK_DELETE, "Delete"}, {VK_HOME, "Home"}, {VK_END, "End"},
    {VK_PRIOR, "PageUp"}, {VK_NEXT, "PageDown"}, {VK_PRIOR, "Prior"}, {VK_NEXT, "Next"},
    {VK_NUMPAD0, "Num0"}, {VK_NUMPAD1, "Num1"}, {VK_NUMPAD2, "Num2"}, {VK_NUMPAD3, "Num3"},
    {VK_NUMPAD4, "Num4"}, {VK_NUMPAD5, "Num5"}, {VK_NUMPAD6, "Num6"}, {VK_NUMPAD7, "Num7"},
    {VK_NUMPAD8, "Num8"}, {VK_NUMPAD9, "Num9"},
    {VK_NUMPAD0, "NumPad0"}, {VK_NUMPAD1, "NumPad1"}, {VK_NUMPAD2, "NumPad2"}, {VK_NUMPAD3, "NumPad3"},
    {VK_NUMPAD4, "NumPad4"}, {VK_NUMPAD5, "NumPad5"}, {VK_NUMPAD6, "NumPad6"}, {VK_NUMPAD7, "NumPad7"},
    {VK_NUMPAD8, "NumPad8"}, {VK_NUMPAD9, "NumPad9"},
    {VK_NUMPAD0, "KP0"}, {VK_NUMPAD1, "KP1"}, {VK_NUMPAD2, "KP2"}, {VK_NUMPAD3, "KP3"},
    {VK_NUMPAD4, "KP4"}, {VK_NUMPAD5, "KP5"}, {VK_NUMPAD6, "KP6"}, {VK_NUMPAD7, "KP7"},
    {VK_NUMPAD8, "KP8"}, {VK_NUMPAD9, "KP9"},
    {VK_ADD, "NumPlus"}, {VK_SUBTRACT, "NumMinus"}, {VK_MULTIPLY, "NumMultiply"},
    {VK_DIVIDE, "NumDivide"}, {VK_DECIMAL, "NumPeriod"},
    {VK_ADD, "Add"}, {VK_SUBTRACT, "Subtract"}, {VK_MULTIPLY, "Multiply"}, {VK_DIVIDE, "Divide"},
    {VK_DECIMAL, "Decimal"}, {VK_ADD, "KPPlus"}, {VK_SUBTRACT, "KPMinus"},
    {VK_OEM_4, "["}, {VK_OEM_6, "]"}, {VK_OEM_1, ";"}, {VK_OEM_7, "'"}, {VK_OEM_COMMA, ","},
    {VK_OEM_PERIOD, "."}, {VK_OEM_2, "/"}, {VK_OEM_5, "\\"}, {VK_OEM_MINUS, "-"}, {VK_OEM_PLUS, "="},
    {VK_OEM_3, "`"},
    {VK_OEM_4, "LeftBracket"}, {VK_OEM_6, "RightBracket"}, {VK_OEM_1, "Semicolon"},
    {VK_OEM_7, "Apostrophe"}, {VK_OEM_COMMA, "Comma"}, {VK_OEM_PERIOD, "Period"},
    {VK_OEM_2, "Slash"}, {VK_OEM_5, "Backslash"}, {VK_OEM_MINUS, "Minus"}, {VK_OEM_PLUS, "Equals"},
    {VK_OEM_3, "Grave"},
    {VK_OEM_4, "Oem4"}, {VK_OEM_6, "Oem6"}, {VK_OEM_1, "Oem1"}, {VK_OEM_7, "Oem7"},
    {VK_OEM_COMMA, "Oemcomma"}, {VK_OEM_PERIOD, "OemPeriod"}, {VK_OEM_2, "Oem2"}, {VK_OEM_5, "Oem5"},
    {VK_OEM_MINUS, "OemMinus"}, {VK_OEM_PLUS, "Oemplus"}, {VK_OEM_3, "Oem3"}, {VK_OEM_102, "Oem102"},
    {VK_OEM_4, "OemOpenBrackets"}, {VK_OEM_6, "OemCloseBrackets"},
};

inline uint8_t key_from_name(const std::string& name) {
    for (const KeyName& k : kKeys)
        if (same_text(name.c_str(), k.name)) return k.vk;
    return 0;
}

inline std::string key_name(uint8_t vk) {
    for (const KeyName& k : kKeys)
        if (k.vk == vk) return k.name;
    char s[8];
    std::snprintf(s, sizeof s, "0x%02X", vk);
    return s;
}

// ---- The controller --------------------------------------------------------------------------
// Everything on it a key can press. The buttons come first, in this order, as bits of
// Reading::buttons.
enum Elem : uint8_t {
    A, B, X, Y, LB, RB, LS, RS, View, Menu, DUp, DDown, DLeft, DRight,
    LT, RT, LUp, LDown, LLeft, LRight, RUp, RDown, RLeft, RRight,
    kElemCount
};
constexpr int kButtonCount = 14;

struct ElemInfo {
    const char* name;  // in [Keyboard]
    const char* alias; // also read: the name ControllerLayouts' column has, or another
    uint16_t wgi;      // Windows.Gaming.Input GamepadButtons
    uint16_t xinput;   // XINPUT_GAMEPAD wButtons
};

inline const ElemInfo kElems[kElemCount] = {
    {"A", "ButtonA", 0x0004, 0x1000},
    {"B", "ButtonB", 0x0008, 0x2000},
    {"X", "ButtonX", 0x0010, 0x4000},
    {"Y", "ButtonY", 0x0020, 0x8000},
    {"LB", "LeftShoulder", 0x0400, 0x0100},
    {"RB", "RightShoulder", 0x0800, 0x0200},
    {"LS", "LeftThumbButton", 0x1000, 0x0040},
    {"RS", "RightThumbButton", 0x2000, 0x0080},
    {"View", "Back", 0x0002, 0x0020},
    {"Menu", "Start", 0x0001, 0x0010},
    {"DPadUp", "DPadUp", 0x0040, 0x0001},
    {"DPadDown", "DPadDown", 0x0080, 0x0002},
    {"DPadLeft", "DPadLeft", 0x0100, 0x0004},
    {"DPadRight", "DPadRight", 0x0200, 0x0008},
    {"LT", "LeftTrigger", 0, 0},
    {"RT", "RightTrigger", 0, 0},
    {"LeftStickUp", "LeftThumbUp", 0, 0},
    {"LeftStickDown", "LeftThumbDown", 0, 0},
    {"LeftStickLeft", "LeftThumbLeft", 0, 0},
    {"LeftStickRight", "LeftThumbRight", 0, 0},
    {"RightStickUp", "RightThumbUp", 0, 0},
    {"RightStickDown", "RightThumbDown", 0, 0},
    {"RightStickLeft", "RightThumbLeft", 0, 0},
    {"RightStickRight", "RightThumbRight", 0, 0},
};

inline int elem_from_name(const std::string& name) {
    for (int e = 0; e < kElemCount; ++e)
        if (same_text(name.c_str(), kElems[e].name) || same_text(name.c_str(), kElems[e].alias)) return e;
    return -1;
}

// The game's numbers for a controller's inputs, as its bindings hold them (ControllerLayouts'
// columns, game+0x1E09C40, and the bindings the game adds itself).
constexpr int kInputNone   = 0x25;
constexpr int kInputSteerX = 31; // the left stick across: Steer
inline int elem_of_input(int input) {
    switch (input) {
    case 0: return DUp;
    case 1: return DDown;
    case 2: return DLeft;
    case 3: return DRight;
    case 4: return Menu;
    case 5: return View;
    case 6: return LS;
    case 7: return RS;
    case 8: return LB;
    case 9: return RB;
    case 10: return LT;
    case 11: return RT;
    case 12: return A;
    case 13: return B;
    case 14: return X;
    case 15: return Y;
    case 19: return LUp;
    case 20: return LDown;
    case 21: return LLeft;
    case 22: return LRight;
    case 26: return RUp;
    case 27: return RDown;
    case 28: return RLeft;
    case 29: return RRight;
    default: return -1;
    }
}

// ControllerLayouts' columns, with the input each one is (game+0x1E09C40).
struct Column {
    const char* name;
    int input;
};
inline const Column kColumns[] = {
    {"LeftThumbUp", 19}, {"LeftThumbDown", 20}, {"LeftThumbButton", 6}, {"RightThumbUp", 26},
    {"RightThumbDown", 27}, {"RightThumbLeft", 28}, {"RightThumbRight", 29}, {"RightThumbButton", 7},
    {"DPadUp", 0}, {"DPadDown", 1}, {"DPadLeft", 2}, {"DPadRight", 3}, {"ButtonA", 12},
    {"ButtonB", 13}, {"ButtonX", 14}, {"ButtonY", 15}, {"LeftShoulder", 8}, {"RightShoulder", 9},
    {"LeftTrigger", 10}, {"RightTrigger", 11},
};
constexpr int kColumnCount = static_cast<int>(sizeof kColumns / sizeof kColumns[0]);

inline int column_from_name(const std::string& name) {
    for (int c = 0; c < kColumnCount; ++c)
        if (same_text(name.c_str(), kColumns[c].name)) return c;
    // the [Keyboard] names for the same inputs work here too
    const int e = elem_from_name(name);
    if (e >= 0)
        for (int c = 0; c < kColumnCount; ++c)
            if (elem_of_input(kColumns[c].input) == e) return c;
    return -1;
}

// ---- The game's actions ----------------------------------------------------------------------
// Ids are the game's (its names table at game+0x1E09D80); Map (22) is the quick map the game asks
// for while you drive. Steer is one action, two keys: SteerLeft and SteerRight.
struct Action {
    const char* name;
    int id;
    int dir; // Steer: -1 left, +1 right
    bool in_layout; // a name ControllerLayouts can hold
    int fixed = -1; // not one of the game's: the controller input it presses on the road
    bool launch = false; // LaunchControl: presses nothing, the mod reads it (launch_control below)
};

inline const Action kActions[] = {
    {"Gas", 0, 0, true},           {"Brake", 1, 0, true},         {"SteerLeft", 2, -1, false},
    {"SteerRight", 2, +1, false},  {"Handbrake", 3, 0, true},     {"ShiftUp", 4, 0, true},
    {"ShiftDown", 5, 0, true},     {"Clutch", 6, 0, true},        {"LookForward", 9, 0, true},
    {"LookBack", 10, 0, true},     {"LookLeft", 11, 0, true},     {"LookRight", 12, 0, true},
    {"LookStraightBack", 13, 0, true}, {"Telemetry", 14, 0, true}, {"PhotoMode", 15, 0, true},
    {"SwitchCamera", 17, 0, true}, {"Rewind", 20, 0, true},       {"Map", 22, 0, false},
    {"RadioLeft", 23, 0, true},    {"RadioRight", 24, 0, true},   {"MiniLeaderboard", 25, 0, true},
    // Pause on the road: Menu, but only while driving (so the same key can be B in the menus)
    {"Pause", 31, 0, false, Menu},
    // Launch control: held at a standstill with the gas, the revs are held (see launch_control)
    {"LaunchControl", 30, 0, false, -1, true},
};
constexpr int kActionCount = static_cast<int>(sizeof kActions / sizeof kActions[0]);
constexpr int kActionIds   = 32; // ids below this
constexpr int kMapAction   = 22;
constexpr int kSteerAction = 2;

inline int action_from_name(const std::string& name) {
    static const struct { const char* alias; const char* name; } kAliases[] = {
        {"Throttle", "Gas"}, {"Accelerate", "Gas"}, {"Camera", "SwitchCamera"},
        {"ChangeCamera", "SwitchCamera"}, {"EBrake", "Handbrake"}, {"GearUp", "ShiftUp"},
        {"GearDown", "ShiftDown"}, {"PreviousStation", "RadioLeft"}, {"NextStation", "RadioRight"},
    };
    std::string n = name;
    for (const auto& a : kAliases)
        if (same_text(n.c_str(), a.alias)) n = a.name;
    for (int i = 0; i < kActionCount; ++i)
        if (same_text(n.c_str(), kActions[i].name)) return i;
    return -1;
}

// ---- What each key does ------------------------------------------------------------------------
// A target: 0..kElemCount-1 is a controller input, kElemCount + i is kActions[i].
constexpr int kMaxTargets = 8;
inline bool is_action_target(int t) { return t >= kElemCount; }

struct Keymap {
    uint8_t count[256] = {};
    uint8_t target[256][kMaxTargets] = {};
    bool has_action[256] = {}; // the key does at least one action

    void add(uint8_t vk, int t) {
        for (int i = 0; i < count[vk]; ++i)
            if (target[vk][i] == t) return;
        if (count[vk] >= kMaxTargets) return;
        target[vk][count[vk]++] = static_cast<uint8_t>(t);
        if (is_action_target(t)) has_action[vk] = true;
    }
    bool any(uint8_t vk) const { return count[vk] != 0; }
};

// ---- FH2Controls.ini ---------------------------------------------------------------------------
struct Config {
    Keymap keys;
    bool enabled      = true;
    float steer_time  = 0.25f;
    float return_time = 0.15f;
    float pedal_time  = 0.10f;
    float launch_rpm  = 0.60f; // the revs launch control holds, as a share of the redline
    bool key_prompts  = true;  // the game's button icons show the keys
    bool mouse_look   = true;  // the mouse turns the driving look
    float mouse_sensitivity = 1.0f;
    float mouse_return      = 0.6f; // seconds without moving before the look returns ahead (0 = stays)
    bool custom_layout = false;
    std::string column_action[kColumnCount]; // [Controller]: the action on each input ("" none)
    bool column_set[kColumnCount] = {};
    std::vector<std::string> problems;       // lines that weren't understood
};

inline bool parse_float(const std::string& s, float lo, float hi, float* out) {
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end == s.c_str() || !(v >= lo && v <= hi)) return false;
    *out = v;
    return true;
}

inline bool parse_bool(const std::string& s, bool* out) {
    if (s == "1" || same_text(s.c_str(), "true") || same_text(s.c_str(), "on") || same_text(s.c_str(), "yes")) {
        *out = true;
        return true;
    }
    if (s == "0" || same_text(s.c_str(), "false") || same_text(s.c_str(), "off") || same_text(s.c_str(), "no")) {
        *out = false;
        return true;
    }
    return false;
}

// "W, Up" -> each key; returns how many names weren't keys.
inline int each_key(const std::string& value, std::vector<uint8_t>* out) {
    int bad      = 0;
    size_t start = 0;
    while (start <= value.size()) {
        size_t end = value.find(',', start);
        if (end == std::string::npos) end = value.size();
        const std::string name = trim(value.substr(start, end - start));
        if (!name.empty()) {
            if (const uint8_t vk = key_from_name(name)) out->push_back(vk);
            else ++bad;
        }
        start = end + 1;
    }
    return bad;
}

inline Config parse_config(const std::string& text) {
    Config c;
    std::string section;
    size_t pos = 0;
    int line_no = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        ++line_no;
        if (line_no == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line = line.substr(3);
        // A comment is a line starting with ; or #, or the rest of a line after " ;" or " #" (so
        // the keys ; and # can still be bound, as Semicolon or at the end without a space).
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        for (size_t i = 1; i < line.size(); ++i)
            if ((line[i] == ';' || line[i] == '#') && (line[i - 1] == ' ' || line[i - 1] == '\t')) {
                line = trim(line.substr(0, i));
                break;
            }
        if (line.front() == '[' && line.back() == ']' && line.size() > 2 && line.find('=') == std::string::npos) {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            c.problems.push_back("line " + std::to_string(line_no) + ": no \"=\": " + line);
            continue;
        }
        const std::string name  = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        const std::string where = "line " + std::to_string(line_no) + ": ";
        if (same_text(section.c_str(), "Keyboard")) {
            const int act = action_from_name(name);
            const int el  = act < 0 ? elem_from_name(name) : -1;
            if (act < 0 && el < 0) {
                c.problems.push_back(where + "\"" + name + "\" isn't an action or a controller input");
                continue;
            }
            std::vector<uint8_t> vks;
            if (each_key(value, &vks)) c.problems.push_back(where + "a key in \"" + value + "\" isn't a key name");
            for (uint8_t vk : vks) c.keys.add(vk, act >= 0 ? kElemCount + act : el);
        } else if (same_text(section.c_str(), "Controller")) {
            if (same_text(name.c_str(), "Custom")) {
                if (!parse_bool(value, &c.custom_layout)) c.problems.push_back(where + "Custom is 1 or 0");
                continue;
            }
            const int col = column_from_name(name);
            if (col < 0) {
                c.problems.push_back(where + "\"" + name + "\" isn't a controller input");
                continue;
            }
            if (!value.empty() && !same_text(value.c_str(), "None")) {
                const int act = action_from_name(value);
                if (act < 0 || !(kActions[act].in_layout || kActions[act].id == kMapAction)) {
                    c.problems.push_back(where + "\"" + value + "\" isn't an action a button can have");
                    continue;
                }
                c.column_action[col] = kActions[act].name;
            } else {
                c.column_action[col].clear();
            }
            c.column_set[col] = true;
        } else if (same_text(section.c_str(), "Settings")) {
            bool ok = true;
            if (same_text(name.c_str(), "Enabled")) ok = parse_bool(value, &c.enabled);
            else if (same_text(name.c_str(), "SteeringTime")) ok = parse_float(value, 0.0f, 1.0f, &c.steer_time);
            else if (same_text(name.c_str(), "ReturnTime")) ok = parse_float(value, 0.0f, 1.0f, &c.return_time);
            else if (same_text(name.c_str(), "PedalTime")) ok = parse_float(value, 0.0f, 0.5f, &c.pedal_time);
            else if (same_text(name.c_str(), "LaunchRPM")) ok = parse_float(value, 0.2f, 0.95f, &c.launch_rpm);
            else if (same_text(name.c_str(), "KeyPrompts")) ok = parse_bool(value, &c.key_prompts);
            else if (same_text(name.c_str(), "MouseLook")) ok = parse_bool(value, &c.mouse_look);
            else if (same_text(name.c_str(), "MouseSensitivity")) ok = parse_float(value, 0.1f, 5.0f, &c.mouse_sensitivity);
            else if (same_text(name.c_str(), "MouseLookReturn")) ok = parse_float(value, 0.0f, 5.0f, &c.mouse_return);
            else if (same_text(name.c_str(), "ToggleKey") || same_text(name.c_str(), "ToggleModifiers") ||
                     same_text(name.c_str(), "ToggleChord")) ok = true; // the mod reads these itself
            else ok = false;
            if (!ok) c.problems.push_back(where + "\"" + name + " = " + value + "\" isn't a setting it knows");
        } else {
            c.problems.push_back(where + "outside [Keyboard], [Controller] or [Settings]");
        }
    }
    return c;
}

// ---- From keys to a controller reading ---------------------------------------------------------
struct Reading {
    uint16_t buttons = 0; // bit e for kElems[e], e < kButtonCount
    float lt = 0, rt = 0; // 0..1
    float lx = 0, ly = 0, rx = 0, ry = 0; // -1..1, up and right positive
    bool operator==(const Reading& o) const {
        return buttons == o.buttons && lt == o.lt && rt == o.rt && lx == o.lx && ly == o.ly && rx == o.rx && ry == o.ry;
    }
    bool operator!=(const Reading& o) const { return !(*this == o); }
};

inline uint16_t wgi_buttons(uint16_t b) {
    uint16_t out = 0;
    for (int e = 0; e < kButtonCount; ++e)
        if (b & (1u << e)) out |= kElems[e].wgi;
    return out;
}
inline uint16_t xinput_buttons(uint16_t b) {
    uint16_t out = 0;
    for (int e = 0; e < kButtonCount; ++e)
        if (b & (1u << e)) out |= kElems[e].xinput;
    return out;
}

// What the game has each action on now (from its own bindings, once a frame): an Elem, or
// kOnSteerAxis for Steer on the left stick, or -1 for none the keys can press.
constexpr int8_t kOnSteerAxis = 100;

struct Frame {
    const bool* down;      // [256]
    const uint32_t* order; // [256] when each key went down (the last steering key wins)
    bool driving;          // on the road: actions go through; in a menu only the inputs do
    const int8_t* action_on; // [kActionIds]
    const bool* input_used;  // [kElemCount] inputs the road's actions are on (may be null)
};

struct Feel {
    float steer_time = 0.25f, return_time = 0.15f, pedal_time = 0.10f;
};

struct Smooth {
    float lx = 0, lt = 0, rt = 0;
};

inline float approach(float cur, float target, float dt, float time) {
    if (time <= 0.0f) return target;
    const float step = dt / time;
    return cur < target ? std::min(cur + step, target) : std::max(cur - step, target);
}

inline float steer(float cur, int target, float dt, const Feel& feel) {
    const bool opposite = (cur > 0 && target < 0) || (cur < 0 && target > 0);
    if (target == 0 || opposite) return approach(cur, 0.0f, dt, feel.return_time);
    return approach(cur, static_cast<float>(target), dt, feel.steer_time);
}

// Actions the keys want but the game has on nothing they can press (Map, or one left off the
// layout): the mod answers for them when the game asks (see fh2controls.cpp).
struct Wanted {
    bool action[kActionIds] = {};
};

// The rule: on the road a key that does an action does only that (it presses the input the
// action is on); a key that doesn't, presses its controller input, if no action of the road is
// on that input. In a menu, keys press their controller inputs only. So one key can be "shift up" on the road and "d-pad up" in a menu,
// without either leaking into the other.
inline Reading make_reading(const Keymap& km, const Frame& f, Smooth& s, float dt, const Feel& feel, Wanted* wanted) {
    Reading r;
    uint32_t left_at = 0, right_at = 0;
    int ly = 0, rx = 0, ry = 0;
    bool lt = false, rt = false;
    auto press = [&](int e, uint32_t order) {
        if (e < kButtonCount) r.buttons |= static_cast<uint16_t>(1u << e);
        else if (e == LT) lt = true;
        else if (e == RT) rt = true;
        else if (e == LUp) ++ly;
        else if (e == LDown) --ly;
        else if (e == LLeft) left_at = std::max(left_at, order | 1u);
        else if (e == LRight) right_at = std::max(right_at, order | 1u);
        else if (e == RUp) ++ry;
        else if (e == RDown) --ry;
        else if (e == RLeft) --rx;
        else if (e == RRight) ++rx;
    };
    for (int vk = 0; vk < 256; ++vk) {
        if (!f.down[vk] || !km.any(static_cast<uint8_t>(vk))) continue;
        const bool action_key = km.has_action[vk];
        for (int i = 0; i < km.count[vk]; ++i) {
            const int t = km.target[vk][i];
            if (!is_action_target(t)) {
                // on the road an input key only presses inputs no action of the road is on
                if (!(f.driving && (action_key || (f.input_used && f.input_used[t])))) press(t, f.order[vk]);
                continue;
            }
            if (!f.driving) continue;
            const Action& a = kActions[t - kElemCount];
            const int8_t on = f.action_on[a.id];
            if (a.launch) {
                // read by the launch control, presses nothing
            } else if (a.fixed >= 0) {
                press(a.fixed, f.order[vk]);
            } else if (a.id == kSteerAction) {
                if (on == kOnSteerAxis || on < 0) // not on the stick: the stick is still the game's default
                    (a.dir < 0 ? left_at : right_at) = std::max(a.dir < 0 ? left_at : right_at, f.order[vk] | 1u);
                else
                    press(on, f.order[vk]);
            } else if (on >= 0 && on < kElemCount) {
                press(on, f.order[vk]);
            } else if (wanted) {
                wanted->action[a.id] = true;
            }
        }
    }
    int lx_target = 0;
    if (left_at && right_at) lx_target = left_at > right_at ? -1 : 1;
    else if (left_at) lx_target = -1;
    else if (right_at) lx_target = 1;
    s.lx = steer(s.lx, lx_target, dt, feel);
    s.lt = approach(s.lt, lt ? 1.0f : 0.0f, dt, feel.pedal_time);
    s.rt = approach(s.rt, rt ? 1.0f : 0.0f, dt, feel.pedal_time);
    r.lx = s.lx;
    r.lt = s.lt;
    r.rt = s.rt;
    r.ly = static_cast<float>(std::clamp(ly, -1, 1));
    r.rx = static_cast<float>(std::clamp(rx, -1, 1));
    r.ry = static_cast<float>(std::clamp(ry, -1, 1));
    return r;
}

// ---- Answering for an action (the game's trigger types) -------------------------------------------
// A button's state as the game works it out (game+0x106EEF0): 0 up, 1 just pressed, 2 held,
// 4 just released.
inline int key_state(bool now, bool before, bool used) {
    if (used) return 0;
    if (now) return before ? 2 : 1;
    return before ? 4 : 0;
}

// Whether a state counts for an action's trigger type, as IsActionPressed has it
// (game+0x107261C on). Type 8 is press-then-repeat.
inline bool triggers(int type, int state, uint32_t now, uint32_t& repeat, uint32_t first, uint32_t again) {
    switch (type) {
    case 0x01: return state == 1;
    case 0x02: return state == 4;
    case 0x04: return state >= 1 && state <= 3;
    case 0x08:
        if (state == 1) {
            repeat = now + first;
            return true;
        }
        if ((state == 2 || state == 3) && static_cast<int>(now - repeat) >= 0) {
            repeat = now + again;
            return true;
        }
        return false;
    case 0x10: return state == 0 || state == 4;
    case 0x20: return state == 1 || state == 3 || state == 4;
    case 0x40: return state == 4;
    case 0x80: return state >= 1 && state <= 3;
    case 0x100: return state == 1;
    case 0x200: return state != 0;
    default: return state == 1; // not in the game's table: a press
    }
}

// ---- The ini the mod writes the first time ----------------------------------------------------------
inline const char* kDefaultIni =
    "; FH2Controls: keyboard and controller bindings for Forza Horizon 2.\n"
    "; Edit this file while the game runs: keyboard changes apply within a second. [Controller]\n"
    "; changes apply the next time the game loads your controls (a restart, or picking a layout in\n"
    "; Settings > Controls). fh2-mods\\platform.log says what it read, and any line it didn't understand.\n"
    ";\n"
    "; [Keyboard]   <what> = <key>[, <key>...]   (another line for the same thing adds more keys)\n"
    ";   <what> is one of the game's actions or a controller input:\n"
    ";   actions (on the road): Gas, Brake, SteerLeft, SteerRight, Handbrake, Clutch, ShiftUp,\n"
    ";     ShiftDown, SwitchCamera, Rewind, Map, LookForward, LookBack, LookLeft, LookRight,\n"
    ";     LookStraightBack, PhotoMode, Telemetry, RadioLeft, RadioRight, MiniLeaderboard,\n"
    ";     Pause (presses Menu, only while driving: Escape can pause there and be B in menus),\n"
    ";     LaunchControl (hold it and the gas at a standstill: the revs stay at LaunchRPM; let go\n"
    ";     of it to launch. On the handbrake's key it's a handbrake launch)\n"
    ";   controller inputs (menus, and anything else that reads the controller, like other mods):\n"
    ";     A, B, X, Y, LB, RB, LT, RT, LS, RS, View, Menu, DPadUp, DPadDown, DPadLeft, DPadRight,\n"
    ";     LeftStickUp/Down/Left/Right, RightStickUp/Down/Left/Right\n"
    ";   On the road a key bound to an action does only that action (whatever button the action is\n"
    ";   on); in a menu it does only its controller input. So Up can be ShiftUp on the road and\n"
    ";   DPadUp in the menus, and Space can be Handbrake and A, without one leaking into the other.\n"
    ";   Keys: A-Z, 0-9, F1-F12, Space, Enter, Escape, Backspace, Tab, Up, Down, Left, Right,\n"
    ";   LShift, RShift, LCtrl, RCtrl, LAlt, RAlt, CapsLock, Insert, Delete, Home, End, PageUp,\n"
    ";   PageDown, Num0-Num9, NumPlus, NumMinus, NumMultiply, NumDivide, NumPeriod, [ ] ; ' , . / \\ - = `\n"
    ";\n"
    "; [Controller]  which action each button of a real controller has (CustomBindings' format).\n"
    ";   Custom = 1 uses these in place of the layout picked in Settings > Controls; 0 leaves the\n"
    ";   game's layouts alone. A button can also have Map (opens the map while you drive).\n"
    "\n"
    "[Settings]\n"
    "Enabled = 1\n"
    "; seconds from straight ahead to full lock, back, and for gas and brake to go full and back\n"
    "SteeringTime = 0.25\n"
    "ReturnTime = 0.15\n"
    "PedalTime = 0.10\n"
    "; launch control: the revs it holds, as a share of the redline (0.20 to 0.95)\n"
    "LaunchRPM = 0.60\n"
    "; the game's button icons show your keys (menus and the HUD; a restart applies changes)\n"
    "KeyPrompts = 1\n"
    "; the mouse turns the look while you drive; it returns ahead after MouseLookReturn seconds\n"
    "MouseLook = 1\n"
    "MouseSensitivity = 1.0\n"
    "MouseLookReturn = 0.6\n"
    "\n"
    "[Keyboard]\n"
    "; on the road\n"
    "Gas = W\n"
    "Brake = S\n"
    "SteerLeft = A\n"
    "SteerRight = D\n"
    "Handbrake = Space\n"
    "LaunchControl = Space\n"
    "Clutch = LShift\n"
    "ShiftUp = E\n"
    "ShiftDown = Q\n"
    "SwitchCamera = C\n"
    "Rewind = R\n"
    "Map = M\n"
    "LookBack = V\n"
    "LookLeft = J\n"
    "LookRight = L\n"
    "LookForward = I\n"
    "PhotoMode = P\n"
    "Telemetry = T\n"
    "RadioLeft = Comma\n"
    "RadioRight = Period\n"
    "; the menus\n"
    "A = Enter, Space\n"
    "B = Backspace\n"
    "X = X\n"
    "Y = Y\n"
    "LB = [\n"
    "RB = ]\n"
    "LT = PageUp\n"
    "RT = PageDown\n"
    "Menu = Escape\n"
    "View = Tab\n"
    "DPadUp = Up\n"
    "DPadDown = Down\n"
    "DPadLeft = Left\n"
    "DPadRight = Right\n"
    "LeftStickUp = W\n"
    "LeftStickDown = S\n"
    "LeftStickLeft = A\n"
    "LeftStickRight = D\n"
    "RightStickUp = I\n"
    "RightStickDown = K\n"
    "RightStickLeft = J\n"
    "RightStickRight = L\n"
    "\n"
    "[Controller]\n"
    "Custom = 0\n"
    "LeftThumbUp =\n"
    "LeftThumbDown =\n"
    "LeftThumbButton =\n"
    "RightThumbUp = LookForward\n"
    "RightThumbDown = LookBack\n"
    "RightThumbLeft = LookLeft\n"
    "RightThumbRight = LookRight\n"
    "RightThumbButton = Map\n"
    "DPadUp = PhotoMode\n"
    "DPadDown = Telemetry\n"
    "DPadLeft = RadioLeft\n"
    "DPadRight = RadioRight\n"
    "ButtonA = Handbrake\n"
    "ButtonB = Rewind\n"
    "ButtonX = ShiftDown\n"
    "ButtonY = ShiftUp\n"
    "LeftShoulder = Clutch\n"
    "RightShoulder = SwitchCamera\n"
    "LeftTrigger = Brake\n"
    "RightTrigger = Gas\n";

// ---- Launch control --------------------------------------------------------------------------------
// Hold the LaunchControl key and the gas at a standstill: the gas the game gets is worked so the
// engine sits at the launch revs (a PI controller on the car's rpm). Let go of the key, the gas
// still down, and the gas goes back to the keys (full): the launch. Moving off lets go by itself.
struct Launch {
    bool held      = false; // holding the revs now
    float integral = 0.05f;
    float last_rpm = 0, rate = 0; // how fast the revs are moving (rpm/s, smoothed)
    int launches   = 0;
};

struct LaunchIn {
    bool key, gas;      // the LaunchControl key and a Gas key held (on the road)
    bool car_known;     // rpm and speed read from the car just now
    float rpm, redline; // rpm
    float speed;        // m/s
    float share;        // LaunchRPM: the revs to hold, as a share of the redline
};

// The gas to give (0..1) while holding the revs, or -1 to leave the gas to the keys.
inline float launch_control(Launch& l, const LaunchIn& in, float dt) {
    const bool can = in.key && in.gas && in.car_known && in.redline > 1000.0f;
    if (!can || (!l.held && in.speed > 0.5f) || in.speed > 3.0f) { // arms stopped, lets go moving
        if (l.held && !in.key && in.gas) ++l.launches; // let go with the gas down: away it goes
        l.held = false;
        return -1.0f;
    }
    if (!l.held) {
        l.held     = true;
        l.integral = 0.05f;
        l.last_rpm = in.rpm;
        l.rate     = 0;
    }
    // PI on the revs, with the P term on where the revs will be 0.1 s from now: an engine free-
    // revving at a standstill loses revs slowly, so it has to ease off before the target, not
    // after (in the game, without the look-ahead, a 3600 rpm hold first overshot to 4571).
    // Tuned on an engine fitted to that trace and five others: under 5% over at 60%.
    if (dt > 0) l.rate += ((in.rpm - l.last_rpm) / dt - l.rate) * std::min(1.0f, dt / 0.05f);
    l.last_rpm         = in.rpm;
    const float target = std::clamp(in.share, 0.2f, 0.95f) * in.redline;
    const float err    = (target - in.rpm) / in.redline;                   // as a share of the redline
    const float ahead  = (target - (in.rpm + l.rate * 0.1f)) / in.redline; // 0.1 s on
    l.integral         = std::clamp(l.integral + err * 2.0f * dt, 0.0f, 1.0f);
    return std::clamp(l.integral + ahead * 6.0f, 0.0f, 1.0f);
}

} // namespace fc
