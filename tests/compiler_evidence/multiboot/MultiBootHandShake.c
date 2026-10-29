// No licence is granted for this file: it derives from pret's reconstruction of Nintendo SDK
// code, which carries no licence (NOTICE.md).
// The SDK's MultiBootHandShake (pret's multiboot.c). old_agbcc folds the single-use
// ldrb of masterp[0xAC] into the orr and reloads it next to it (its thumb.c
// s_register_operand accepts a subreg of a memory operand); the ROM keeps the load where
// the source has it, as agbcc does.

#include "gba/types.h"
#include "gba/multiboot.h"

int MultiBootHandShake(struct MultiBootParam *mp)
{
    int i, j;

#define send_data (mp->system_work[0])
#define must_data (mp->system_work[1])

    switch (mp->probe_count)
    {
    case 0xE0:
    restart:
        mp->probe_count = 0xE1;
        must_data = 0x0000;
        send_data = 0x100000;
        return MultiBootSend(mp, 0x0000);
    default:
        for (i = 3; i != 0; i--)
        {
            j = *(vu16 *)(0x04000120 + i * 2);
            if (((mp->client_bit >> i) & 1) && j != must_data)
                goto restart;
        }
        mp->probe_count++;
        must_data = send_data & 0xFFFF;
        if (send_data == 0x0000)
        {
            must_data = mp->masterp[0xAC] | (mp->masterp[0xAD] << 8);
            send_data = must_data << 5;
        }
        send_data >>= 5;
    output_master_info:
        return MultiBootSend(mp, send_data);
    case 0xE7:
    case 0xE8:
        for (i = 3; i != 0; i--)
        {
            j = *(vu16 *)(0x04000120 + i * 2);
            if (((mp->client_bit >> i) & 1) && j != must_data)
            {
                MultiBootInit(mp);
                return 0x71;
            }
        }

        mp->probe_count++;
        if (mp->probe_count == 0xE9)
            return 0;

        send_data = mp->masterp[0xAE] | (mp->masterp[0xAF] << 8);
        must_data = send_data;
        goto output_master_info;
    }
#undef send_data
#undef must_data
}
