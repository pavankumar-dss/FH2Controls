// Presses keys in the game for an in-game test: brings the Azimuth window to the front, then runs
// a script of steps given as arguments:
//   down:<vk>  up:<vk>  tap:<vk>  wait:<ms>  front
// vk in hex (0x57 = W). Every key it pressed is let go at the end.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

static HWND game_window() { return FindWindowW(L"Azimuth.Game.Win32", nullptr); }

static bool bring_front() {
    HWND w = game_window();
    if (!w) return false;
    // A foreground change needs the input of the window that has it: an Alt tap allows it.
    INPUT alt[2] = {};
    alt[0].type = alt[1].type = INPUT_KEYBOARD;
    alt[0].ki.wVk = alt[1].ki.wVk = VK_MENU;
    alt[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, alt, sizeof(INPUT));
    ShowWindow(w, SW_RESTORE);
    SetForegroundWindow(w);
    Sleep(300);
    return GetForegroundWindow() == w;
}

static void key(WORD vk, bool down) {
    INPUT in{};
    in.type       = INPUT_KEYBOARD;
    in.ki.wVk     = vk;
    in.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_RSHIFT) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &in, sizeof in);
}

int main(int argc, char** argv) {
    std::set<WORD> held;
    if (!bring_front()) {
        std::printf("the game's window isn't in front (or isn't there)\n");
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const size_t colon  = a.find(':');
        const std::string op = a.substr(0, colon);
        const unsigned long n = colon == std::string::npos ? 0 : std::strtoul(a.c_str() + colon + 1, nullptr, 0);
        if (op == "front") {
            if (!bring_front()) std::printf("step %d: couldn't bring the game to the front\n", i);
        } else if (op == "wait") {
            Sleep(n);
        } else if (op == "down") {
            key(static_cast<WORD>(n), true);
            held.insert(static_cast<WORD>(n));
        } else if (op == "up") {
            key(static_cast<WORD>(n), false);
            held.erase(static_cast<WORD>(n));
        } else if (op == "mouse") { // mouse:<dx>: moves the mouse right by dx counts, in steps
            const long dx = static_cast<long>(std::strtol(a.c_str() + colon + 1, nullptr, 0));
            for (int s = 0; s < 20; ++s) {
                INPUT m{};
                m.type       = INPUT_MOUSE;
                m.mi.dx      = dx / 20;
                m.mi.dwFlags = MOUSEEVENTF_MOVE;
                SendInput(1, &m, sizeof m);
                Sleep(10);
            }
        } else if (op == "tap") {
            key(static_cast<WORD>(n), true);
            Sleep(90);
            key(static_cast<WORD>(n), false);
        }
        if (GetForegroundWindow() != game_window()) std::printf("step %d (%s): the game isn't in front any more\n", i, a.c_str());
    }
    for (WORD vk : held) key(vk, false);
    std::printf("done\n");
    return 0;
}
