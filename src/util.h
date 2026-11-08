/* util.h - small helpers for the command-line tools. */
#ifndef TESSERA_UTIL_H
#define TESSERA_UTIL_H

#include <stddef.h>

/* Reads a whole file. The buffer ends with a 0 byte. NULL on error. */
char *read_file(const char *path, size_t *size);

/* 1 if s ends with suffix. */
int ends_with(const char *s, const char *suffix);

#endif
