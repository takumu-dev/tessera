/* tasm.c - Tessera assembler command.
 *
 *   tasm [-l] [-o output.bin] input.s
 *
 * -l prints a listing (address, word, source line) to stdout.
 */
#include "asm.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr, "usage: tasm [-l] [-o output.bin] input.s\n");
}

int main(int argc, char **argv)
{
    const char *in = NULL, *outname = NULL;
    char *defname = NULL, *src;
    int list = 0, errors;
    uint8_t *img;
    uint32_t size;
    FILE *out;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            list = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outname = argv[++i];
        } else if (argv[i][0] == '-') {
            usage();
            return 2;
        } else {
            in = argv[i];
        }
    }
    if (!in) {
        usage();
        return 2;
    }
    src = read_file(in, NULL);
    if (!src) {
        fprintf(stderr, "tasm: cannot read %s\n", in);
        return 2;
    }

    errors = asm_assemble(src, in, &img, &size, list ? stdout : NULL, stderr);
    free(src);
    if (errors) {
        fprintf(stderr, "tasm: %d error%s\n", errors, errors == 1 ? "" : "s");
        return 1;
    }

    if (!outname) {
        size_t n = strlen(in);
        defname = malloc(n + 5);
        if (!defname)
            return 2;
        strcpy(defname, in);
        if (ends_with(defname, ".s"))
            defname[n - 2] = 0;
        strcat(defname, ".bin");
        outname = defname;
    }
    out = fopen(outname, "wb");
    if (!out || fwrite(img, 1, size, out) != size) {
        fprintf(stderr, "tasm: cannot write %s\n", outname);
        if (out)
            fclose(out);
        return 2;
    }
    fclose(out);
    if (!list)
        fprintf(stderr, "tasm: wrote %u bytes to %s\n", size, outname);
    free(img);
    free(defname);
    return 0;
}
