// Waits for azimuth.exe, attaches as a debugger and, if it dies of an exception nobody handles,
// prints the faulting thread's stack (module+offset) and writes a minidump. Detaches cleanly
// when the game exits normally. Usage: catch <dump path>
#include <windows.h>

#include <dbghelp.h>
#include <tlhelp32.h>

#include <cstdio>
#include <string>

#pragma comment(lib, "dbghelp.lib")

static DWORD find_pid() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W p{sizeof p};
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap, &p); ok && !pid; ok = Process32NextW(snap, &p))
        if (_wcsicmp(p.szExeFile, L"azimuth.exe") == 0) pid = p.th32ProcessID;
    CloseHandle(snap);
    return pid;
}

static std::string where(HANDLE proc, DWORD64 addr) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetProcessId(proc));
    MODULEENTRY32W m{sizeof m};
    std::string out;
    for (BOOL ok = Module32FirstW(snap, &m); ok; ok = Module32NextW(snap, &m)) {
        const DWORD64 b = reinterpret_cast<DWORD64>(m.modBaseAddr);
        if (addr >= b && addr < b + m.modBaseSize) {
            char s[300];
            std::snprintf(s, sizeof s, "%ls+0x%llX", m.szModule, addr - b);
            out = s;
            break;
        }
    }
    CloseHandle(snap);
    if (out.empty()) {
        char s[40];
        std::snprintf(s, sizeof s, "0x%llX%s", addr, addr >= 0x140000000 && addr < 0x146000000 ? " (game+0x" : "");
        out = s;
        if (addr >= 0x140000000 && addr < 0x146000000) {
            std::snprintf(s, sizeof s, "%llX)", addr - 0x140000000);
            out += s;
        }
    }
    return out;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 2;
    DWORD pid = 0;
    for (int i = 0; i < 1800 && !(pid = find_pid()); ++i) Sleep(100);
    if (!pid || !DebugActiveProcess(pid)) { std::printf("couldn't attach (%lu)\n", GetLastError()); return 1; }
    DebugSetProcessKillOnExit(FALSE);
    std::printf("attached to azimuth.exe %lu\n", pid);
    std::fflush(stdout);
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    SymInitialize(proc, nullptr, TRUE);
    DEBUG_EVENT ev{};
    for (;;) {
        if (!WaitForDebugEvent(&ev, INFINITE)) break;
        DWORD cont = DBG_CONTINUE;
        if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            const EXCEPTION_RECORD& er = ev.u.Exception.ExceptionRecord;
            cont = DBG_EXCEPTION_NOT_HANDLED;
            if (er.ExceptionCode == EXCEPTION_BREAKPOINT) cont = DBG_CONTINUE; // the attach break
            if (!ev.u.Exception.dwFirstChance) {
                std::printf("UNHANDLED exception 0x%08lX at %s (thread %lu)\n", er.ExceptionCode,
                            where(proc, reinterpret_cast<DWORD64>(er.ExceptionAddress)).c_str(), ev.dwThreadId);
                if (er.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
                    std::printf("  %s address 0x%llX\n", er.ExceptionInformation[0] ? "writing" : "reading",
                                static_cast<unsigned long long>(er.ExceptionInformation[1]));
                HANDLE th = OpenThread(THREAD_ALL_ACCESS, FALSE, ev.dwThreadId);
                CONTEXT c{};
                c.ContextFlags = CONTEXT_FULL;
                GetThreadContext(th, &c);
                STACKFRAME64 f{};
                f.AddrPC.Offset = c.Rip, f.AddrPC.Mode = AddrModeFlat;
                f.AddrFrame.Offset = c.Rbp, f.AddrFrame.Mode = AddrModeFlat;
                f.AddrStack.Offset = c.Rsp, f.AddrStack.Mode = AddrModeFlat;
                for (int i = 0; i < 40 && StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, th, &f, &c, nullptr,
                                                       SymFunctionTableAccess64, SymGetModuleBase64, nullptr); ++i)
                    std::printf("  #%02d %s\n", i, where(proc, f.AddrPC.Offset).c_str());
                // raw stack too: return-address-looking values
                std::printf("  stack words that point into modules:\n");
                for (int i = 0; i < 200; ++i) {
                    DWORD64 v = 0;
                    SIZE_T got = 0;
                    if (!ReadProcessMemory(proc, reinterpret_cast<void*>(c.Rsp + i * 8), &v, 8, &got)) break;
                    const std::string w = where(proc, v);
                    if (w.find('+') != std::string::npos) std::printf("   [rsp+0x%03X] %s\n", i * 8, w.c_str());
                }
                HANDLE file = CreateFileW(argv[1], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
                MINIDUMP_EXCEPTION_INFORMATION mei{ev.dwThreadId, nullptr, FALSE};
                EXCEPTION_POINTERS ep{const_cast<EXCEPTION_RECORD*>(&er), &c};
                (void)ep;
                MiniDumpWriteDump(proc, pid, file, static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo),
                                  nullptr, nullptr, nullptr);
                CloseHandle(file);
                CloseHandle(th);
                std::fflush(stdout);
            }
        } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            std::printf("azimuth.exe exited (code 0x%lX)\n", ev.u.ExitProcess.dwExitCode);
            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
            break;
        } else if (ev.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && ev.u.LoadDll.hFile) {
            CloseHandle(ev.u.LoadDll.hFile);
        } else if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT && ev.u.CreateProcessInfo.hFile) {
            CloseHandle(ev.u.CreateProcessInfo.hFile);
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
    }
    return 0;
}
