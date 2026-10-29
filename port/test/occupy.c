// occupy.exe ADDR[:SIZE]... -- PROGRAM [ARGS...]: runs PROGRAM with those addresses already taken
// (docs/port.md, "The memory map on Windows"). A test of the Windows build, run by make -C port
// test-wine OCCUPIED=1 (and test-audio-wine, test-video-wine): it starts PROGRAM suspended (only
// its exe image, ntdll and the first thread exist then), reserves each range in it with
// VirtualAllocEx (64 KiB by default), as something loaded before the game's own code would hold
// it, says whether it could, then lets PROGRAM run and returns its exit status. Against
// kururin_pc.exe the GBA's ranges are its image's already, so the reservations fail and the game
// runs; against kururin_pc_noimage.exe (no GBA sections) they succeed and the game stops with
// its memory map diagnostics.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int sep = 1;
    while (sep < argc && strcmp(argv[sep], "--"))
        sep++;
    if (sep == 1 || sep + 1 >= argc) {
        fputs("usage: occupy.exe ADDR[:SIZE]... -- PROGRAM [ARGS...]\n", stderr);
        return 2;
    }
    static char cmd[32768];
    size_t n = 0;
    for (int i = sep + 1; i < argc && n + strlen(argv[i]) + 4 < sizeof(cmd); i++)
        n += (size_t)snprintf(cmd + n, sizeof(cmd) - n, "%s\"%s\"", i > sep + 1 ? " " : "", argv[i]);
    for (char *c = cmd + 1; *c && *c != '"'; c++)
        if (*c == '/')
            *c = '\\';  // the program's path: CreateProcess wants backslashes
    STARTUPINFOA si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "occupy: cannot start %s (Windows error %lu)\n", argv[sep + 1], GetLastError());
        return 2;
    }
    for (int i = 1; i < sep; i++) {
        char *colon = strchr(argv[i], ':');
        unsigned long addr = strtoul(argv[i], NULL, 0), size = colon ? strtoul(colon + 1, NULL, 0) : 0x10000;
        void *got = VirtualAllocEx(pi.hProcess, (void *)addr, size, MEM_RESERVE, PAGE_NOACCESS);
        if (got == (void *)addr) {
            fprintf(stderr, "occupy: 0x%08lX-0x%08lX reserved in the child before it runs\n", addr, addr + size);
        } else {
            DWORD err = GetLastError();
            MEMORY_BASIC_INFORMATION m;
            VirtualQueryEx(pi.hProcess, (void *)addr, &m, sizeof(m));
            fprintf(stderr, "occupy: 0x%08lX-0x%08lX not reserved (Windows error %lu): the child holds it already "
                    "(state 0x%lX, type 0x%lX, allocation 0x%08lX)\n", addr, addr + size, err, m.State, m.Type,
                    (unsigned long)(uintptr_t)m.AllocationBase);
        }
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD status = 2;
    GetExitCodeProcess(pi.hProcess, &status);
    return (int)status;
}
