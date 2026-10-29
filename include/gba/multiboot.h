// No licence is granted for this file: it derives from pret's reconstruction of Nintendo SDK
// code, which carries no licence (NOTICE.md).
#ifndef GUARD_GBA_MULTIBOOT_H
#define GUARD_GBA_MULTIBOOT_H

#include "gba/types.h"

#define MULTIBOOT_NCHILD        3       // Maximum number of slaves
#define MULTIBOOT_HEADER_SIZE   0xc0    // Header size
#define MULTIBOOT_SEND_SIZE_MIN 0x100   // Minimum transmission size
#define MULTIBOOT_SEND_SIZE_MAX 0x40000 // Maximum transmission size

#define MULTIBOOT_MASTER_INFO            0x62
#define MULTIBOOT_CLIENT_INFO            0x72
#define MULTIBOOT_MASTER_START_PROBE     0x61
#define MULTIBOOT_MASTER_REQUEST_DLREADY 0x63
#define MULTIBOOT_CLIENT_DLREADY         0x73
#define MULTIBOOT_MASTER_START_DL        0x64
#define MULTIBOOT_MASTER_REQUEST_CRC     0x65
#define MULTIBOOT_CLIENT_CALC_CRC        0x74
#define MULTIBOOT_CLIENT_CRCREADY        0x75
#define MULTIBOOT_MASTER_VERIFY_CRC      0x66

#define MULTIBOOT_ERROR_04                0x04
#define MULTIBOOT_ERROR_08                0x08
#define MULTIBOOT_ERROR_0c                0x0c
#define MULTIBOOT_ERROR_40                0x40
#define MULTIBOOT_ERROR_44                0x44
#define MULTIBOOT_ERROR_48                0x48
#define MULTIBOOT_ERROR_4c                0x4c
#define MULTIBOOT_ERROR_80                0x80
#define MULTIBOOT_ERROR_84                0x84
#define MULTIBOOT_ERROR_88                0x88
#define MULTIBOOT_ERROR_8c                0x8c
#define MULTIBOOT_ERROR_NO_PROBE_TARGET   0x50
#define MULTIBOOT_ERROR_NO_DLREADY        0x60
#define MULTIBOOT_ERROR_BOOT_FAILURE      0x70
#define MULTIBOOT_ERROR_HANDSHAKE_FAILURE 0x71

#define MULTIBOOT_CONNECTION_CHECK_WAIT 15

#define MULTIBOOT_SERVER_TYPE_NORMAL 0
#define MULTIBOOT_SERVER_TYPE_QUICK  1

#define MULTIBOOT_HANDSHAKE_TIMEOUT 400

// Nintendo SDK multiboot.c (AGB MultiBoot library), as reconstructed by pret
// (pokeemerald include/gba/multiboot.h). Offsets confirmed by the matches in
// tests/compiler_evidence/multiboot/.
struct MultiBootParam
{
    /*0x00*/ u32 system_work[5];
    /*0x14*/ u8 handshake_data;
    /*0x15*/ u8 padding;
    /*0x16*/ u16 handshake_timeout;
    /*0x18*/ u8 probe_count;
    /*0x19*/ u8 client_data[3];
    /*0x1C*/ u8 palette_data;
    /*0x1D*/ u8 response_bit;
    /*0x1E*/ u8 client_bit;
    /*0x1F*/ u8 reserved1;
    /*0x20*/ const u8 *boot_srcp;
    /*0x24*/ const u8 *boot_endp;
    /*0x28*/ const u8 *masterp;
    /*0x2C*/ u8 *reserved2[3];
    /*0x38*/ u32 system_work2[4];
    /*0x48*/ u8 sendflag;
    /*0x49*/ u8 probe_target_bit;
    /*0x4A*/ u8 check_wait;
    /*0x4B*/ u8 server_type;
};

void MultiBootInit(struct MultiBootParam *mp);
int MultiBootMain(struct MultiBootParam *mp);
void MultiBootStartMaster(struct MultiBootParam *mp, const u8 *srcp, int length, u8 palette_color, s8 palette_speed);
int MultiBootSend(struct MultiBootParam *mp, u16 data);
int MultiBootCheckComplete(struct MultiBootParam *mp);
int MultiBootHandShake(struct MultiBootParam *mp);
void MultiBootStartProbe(struct MultiBootParam *mp);
void MultiBootWaitCycles(u32 cycles);
void MultiBootWaitSendDone(void);

// The BIOS MultiBoot call (SWI 0x25, libagbsyscall), which MultiBootMain calls: the SDK
// declares it in AgbSyscall.h, pret's reconstruction in gba/syscall.h.
int MultiBoot(struct MultiBootParam *mp);

#endif // GUARD_GBA_MULTIBOOT_H
