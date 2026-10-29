#ifndef SHA1_H
#define SHA1_H
#include <stddef.h>
#include <stdint.h>
void Sha1Hex(const void *data, size_t size, char out[41]);
#endif
