/* util.c - small helpers for the command-line tools. */
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    size_t cap = 4096, n = 0;
    char *buf;

    if (!f)
        return NULL;
    buf = malloc(cap + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    for (;;) {
        size_t got;
        if (n == cap) {
            char *nb = realloc(buf, cap * 2 + 1);
            if (!nb) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = nb;
            cap *= 2;
        }
        got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0)
            break;
    }
    fclose(f);
    buf[n] = 0;
    if (size)
        *size = n;
    return buf;
}

int ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}
