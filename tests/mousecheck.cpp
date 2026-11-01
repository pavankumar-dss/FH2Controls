// Checks how injected mouse motion (SendInput) shows up: in the pointer, and in a shared DirectInput
// mouse, outside the game. Run with nothing else moving the mouse.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "user32.lib")
int main() {
    IDirectInput8W* di = nullptr;
    IDirectInputDevice8W* m = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&di, nullptr);
    if (SUCCEEDED(hr)) hr = di->CreateDevice(GUID_SysMouse, &m, nullptr);
    if (SUCCEEDED(hr)) hr = m->SetDataFormat(&c_dfDIMouse);
    if (SUCCEEDED(hr)) hr = m->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    if (SUCCEEDED(hr)) hr = m->Acquire();
    std::printf("dinput setup 0x%08lX\n", (unsigned long)hr);
    POINT before; GetCursorPos(&before);
    long total = 0;
    for (int i = 0; i < 20; ++i) {
        INPUT in{}; in.type = INPUT_MOUSE; in.mi.dx = 10; in.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &in, sizeof in);
        Sleep(10);
        DIMOUSESTATE st{};
        if (SUCCEEDED(m->GetDeviceState(sizeof st, &st))) total += st.lX;
    }
    Sleep(20);
    DIMOUSESTATE st{};
    if (SUCCEEDED(m->GetDeviceState(sizeof st, &st))) total += st.lX;
    POINT after; GetCursorPos(&after);
    std::printf("pointer moved %ld, dinput saw %ld (200 injected)\n", after.x - before.x, total);
    SetCursorPos(before.x, before.y);
    return 0;
}
