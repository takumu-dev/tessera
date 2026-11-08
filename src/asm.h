/* asm.h - Tessera v0.1 assembler. */
#ifndef TESSERA_ASM_H
#define TESSERA_ASM_H

#include <stdint.h>
#include <stdio.h>

/*
 * Assembles the source text `src`. `fname` is used in error messages.
 * Returns the number of errors. If there are no errors, *image is a
 * malloc'd buffer of *size bytes (the caller frees it). If `listing` is
 * not NULL, a listing goes to it. Error messages go to `err` if it is
 * not NULL.
 */
int asm_assemble(const char *src, const char *fname, uint8_t **image,
                 uint32_t *size, FILE *listing, FILE *err);

#endif
