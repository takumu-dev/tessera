/* tdis.c - Tessera disassembler command.
 *
 *   tdis program.bin
 */
#include "isa.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    size_t n;
    unsigned char *b;
    char dis[64];

    if (argc != 2) {
        fprintf(stderr, "usage: tdis program.bin\n");
        return 2;
    }
    b = (unsigned char *)read_file(argv[1], &n);
    if (!b) {
        fprintf(stderr, "tdis: cannot read %s\n", argv[1]);
        return 2;
    }
    for (size_t i = 0; i + 4 <= n; i += 4) {
        uint32_t w = (uint32_t)b[i] | ((uint32_t)b[i + 1] << 8) |
                     ((uint32_t)b[i + 2] << 16) | ((uint32_t)b[i + 3] << 24);
        isa_disasm(w, (uint32_t)i, dis, sizeof dis);
        printf("  0x%04zx  0x%08x  %s\n", i, w, dis);
    }
    if (n % 4)
        printf("  (%zu bytes at the end are not a full word)\n", n % 4);
    free(b);
    return 0;
}
