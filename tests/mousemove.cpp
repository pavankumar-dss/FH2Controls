// Moves the mouse right by <dx> counts in 20 steps (SendInput), for in-game tests: mousemove <dx>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#pragma comment(lib, "user32.lib")
int main(int argc, char** argv) {
    const long dx = argc > 1 ? std::strtol(argv[1], nullptr, 0) : 200;
    UINT sent = 0;
    for (int i = 0; i < 20; ++i) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dx = dx / 20;
        in.mi.dwFlags = MOUSEEVENTF_MOVE;
        sent += SendInput(1, &in, sizeof in);
        Sleep(10);
    }
    std::printf("sent %u of 20 (INPUT is %zu bytes)\n", sent, sizeof(INPUT));
    return 0;
}
