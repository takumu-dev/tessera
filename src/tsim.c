/* tsim.c - Tessera simulator command (milestone 1: one core).
 *
 *   tsim [-t] [-d] [-n steps] [-m bytes] program.s|program.bin
 *
 * -t  trace: one line for each instruction, to stderr
 * -d  dump the registers at the end
 * -n  stop after this many instructions (default 100000000)
 * -m  local memory size in bytes (default 65536)
 *
 * A .s file is assembled first. Console output goes to stdout.
 * Exit status: 0 halted, 1 fault, 2 usage or load error,
 * 3 unsupported instruction, 4 step limit.
 */
#include "asm.h"
#include "core.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr,
            "usage: tsim [-t] [-d] [-n steps] [-m bytes] program.s|program.bin\n");
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    int trace = 0, dump = 0, rc;
    unsigned long long limit = 100000000ULL;
    unsigned long memsize = LOCAL_SIZE_DEFAULT;
    size_t n;
    char *data;
    uint8_t *img;
    uint32_t size;
    core_t k;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "-d") == 0) {
            dump = 1;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            limit = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            memsize = strtoul(argv[++i], NULL, 0);
        } else if (argv[i][0] == '-') {
            usage();
            return 2;
        } else {
            path = argv[i];
        }
    }
    if (!path) {
        usage();
        return 2;
    }
    if (memsize < 1024 || memsize > 256ul * 1024 * 1024 || memsize % 16) {
        fprintf(stderr, "tsim: -m must be a multiple of 16 from 1024 to 268435456\n");
        return 2;
    }

    data = read_file(path, &n);
    if (!data) {
        fprintf(stderr, "tsim: cannot read %s\n", path);
        return 2;
    }
    if (ends_with(path, ".s")) {
        int errors = asm_assemble(data, path, &img, &size, NULL, stderr);
        free(data);
        if (errors) {
            fprintf(stderr, "tsim: %d assembler error%s\n", errors,
                    errors == 1 ? "" : "s");
            return 2;
        }
    } else {
        img = (uint8_t *)data;
        size = (uint32_t)n;
    }

    if (core_init(&k, (uint32_t)memsize) != 0) {
        fprintf(stderr, "tsim: out of memory\n");
        return 2;
    }
    if (core_load(&k, img, size) != 0) {
        fprintf(stderr, "tsim: the program (%u bytes) is larger than local memory (%lu bytes)\n",
                size, memsize);
        return 2;
    }
    free(img);
    core_boot(&k);
    if (trace)
        k.trace = stderr;

    core_run(&k, limit);
    fflush(stdout);
    core_report(&k, stderr);
    if (dump)
        core_dump(&k, stderr);

    switch (k.state) {
    case CORE_HALTED: rc = 0; break;
    case CORE_FAULTED: rc = 1; break;
    case CORE_UNSUPPORTED: rc = 3; break;
    default: rc = 4; break;
    }
    core_free(&k);
    return rc;
}
