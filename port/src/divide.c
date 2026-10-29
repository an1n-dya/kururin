// Integer division as the GBA does it (docs/port.md, "Risks"). The game's C divides with
// agbcc's libgcc (__divsi3, __udivsi3, __modsi3, __umodsi3: tools/agbcc-src/libgcc/lib1thumb.asm),
// which return 0 for a division by 0 (Ldiv0: "about as wrong as it could be") and INT_MIN for
// INT_MIN / -1; on an x86 both trap (#DE: SIGFPE, EXCEPTION_INT_DIVIDE_BY_ZERO or _OVERFLOW).
// Divisors that come from the ROM's data (Hud's heart animation, LevelMap_Read's Piston period,
// Sprite_SetAffineRotScale's scale) are never 0 on the recorded inputs, but nothing proves it
// for every level and state; so the port catches the trap and gives the GBA's results:
// the quotient (eax) and the remainder (edx) 0, or for INT_MIN / -1 the quotient INT_MIN and
// the remainder 0. Only the 32-bit `div`/`idiv r/m32` that GCC emits for int division (opcode
// F7 /6 and /7, no prefix) is handled; anything else still stops the program.
#define _GNU_SOURCE  // REG_EAX ... (ucontext.h)
#include "port.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <ucontext.h>
#endif

// The registers the handler reads and writes, in the x86's encoding order (eax, ecx, edx, ebx,
// esp, ebp, esi, edi)
struct Regs {
    uint32_t *r[8];
    uint32_t *eip;
};

// Decodes `div/idiv r/m32` at eip: returns its length (0: not one), *divisor its operand,
// *isSigned whether it is idiv.
static int decode(const struct Regs *regs, uint32_t *divisor, bool *isSigned) {
    const uint8_t *p = (const uint8_t *)(uintptr_t)*regs->eip;
    if (p[0] != 0xF7)
        return 0;
    uint8_t modrm = p[1];
    int mod = modrm >> 6, reg = (modrm >> 3) & 7, rm = modrm & 7, len = 2;
    if (reg != 6 && reg != 7)
        return 0;
    *isSigned = reg == 7;
    if (mod == 3) {
        *divisor = *regs->r[rm];
        return len;
    }
    uint32_t addr = 0;
    if (rm == 4) {  // a SIB byte
        uint8_t sib = p[len++];
        int scale = sib >> 6, index = (sib >> 3) & 7, base = sib & 7;
        if (index != 4)
            addr += *regs->r[index] << scale;
        if (base == 5 && mod == 0) {
            int32_t disp;
            memcpy(&disp, p + len, 4);
            addr += (uint32_t)disp, len += 4;
        } else {
            addr += *regs->r[base];
        }
    } else if (rm == 5 && mod == 0) {  // disp32 alone
        int32_t disp;
        memcpy(&disp, p + len, 4);
        addr = (uint32_t)disp, len += 4;
    } else {
        addr = *regs->r[rm];
    }
    if (mod == 1) {
        addr += (uint32_t)(int32_t)(int8_t)p[len], len += 1;
    } else if (mod == 2) {
        int32_t disp;
        memcpy(&disp, p + len, 4);
        addr += (uint32_t)disp, len += 4;
    }
    memcpy(divisor, (const void *)(uintptr_t)addr, 4);
    return len;
}

// The GBA's result for the instruction at eip, which trapped; false: not ours to handle
static bool divideAsGba(const struct Regs *regs) {
    uint32_t divisor;
    bool isSigned;
    int len = decode(regs, &divisor, &isSigned);
    uint32_t *eax = regs->r[0], *edx = regs->r[2];
    if (!len)
        return false;
    if (divisor == 0) {
        *eax = 0, *edx = 0;
    } else if (isSigned && divisor == 0xFFFFFFFFu && *eax == 0x80000000u) {
        *edx = 0;  // INT_MIN / -1: the quotient stays INT_MIN
    } else {
        return false;
    }
    static bool logged;
    if (!logged) {
        logged = true;
        fprintf(stderr, "kururin_pc: a division by %s at 0x%08X: the GBA's result (logged once)\n",
                divisor ? "-1 overflowed" : "0", *regs->eip);
    }
    *regs->eip += (uint32_t)len;
    return true;
}

#ifdef _WIN32
static LONG CALLBACK onDivide(EXCEPTION_POINTERS *e) {
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_INT_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT *c = e->ContextRecord;
    struct Regs regs = { { (uint32_t *)&c->Eax, (uint32_t *)&c->Ecx, (uint32_t *)&c->Edx, (uint32_t *)&c->Ebx,
                           (uint32_t *)&c->Esp, (uint32_t *)&c->Ebp, (uint32_t *)&c->Esi, (uint32_t *)&c->Edi },
                         (uint32_t *)&c->Eip };
    return divideAsGba(&regs) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

void Port_InstallDivideHandler(void) {
    AddVectoredExceptionHandler(1, onDivide);
}
#else
static void onDivide(int sig, siginfo_t *info, void *context) {
    greg_t *g = ((ucontext_t *)context)->uc_mcontext.gregs;
    struct Regs regs = { { (uint32_t *)&g[REG_EAX], (uint32_t *)&g[REG_ECX], (uint32_t *)&g[REG_EDX],
                           (uint32_t *)&g[REG_EBX], (uint32_t *)&g[REG_ESP], (uint32_t *)&g[REG_EBP],
                           (uint32_t *)&g[REG_ESI], (uint32_t *)&g[REG_EDI] },
                         (uint32_t *)&g[REG_EIP] };
    if (info->si_code == FPE_INTDIV || info->si_code == FPE_INTOVF) {
        if (divideAsGba(&regs))
            return;
    }
    // not a division the GBA would survive: the default action (the program stops)
    signal(sig, SIG_DFL);
}

void Port_InstallDivideHandler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = onDivide;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGFPE, &sa, NULL);
}
#endif
