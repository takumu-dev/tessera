/* core.h - one Tessera v0.1 core (milestone 1). */
#ifndef TESSERA_CORE_H
#define TESSERA_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LOCAL_SIZE_DEFAULT  (64u * 1024u)
#define GLOBAL_SIZE_DEFAULT (16u * 1024u * 1024u)

typedef struct {
    uint32_t cursor, base, length;
    uint16_t perms, otype;
    uint8_t space, tag;
} cap_t;

typedef enum {
    CORE_RUNNING,
    CORE_HALTED,
    CORE_FAULTED,
    CORE_UNSUPPORTED  /* the program used a feature of a later milestone */
} core_state_t;

typedef void (*console_fn)(void *ctx, const uint8_t *bytes, uint32_t n);

typedef struct {
    uint32_t r[16];
    cap_t c[16];
    cap_t pcc, fcc;
    uint8_t *mem;
    uint32_t mem_size;
    uint64_t cycle, instret;
    uint32_t out_w[4];
    cap_t out_cap;
    uint16_t tile_id;
    uint8_t grid_w, grid_h;
    core_state_t state;
    int fault_code;
    uint32_t fault_pc, fault_word, fault_addr;
    char note[160];
    console_fn console;
    void *console_ctx;
    FILE *trace;
} core_t;

int core_init(core_t *k, uint32_t mem_size);   /* 0 if OK */
void core_free(core_t *k);
int core_load(core_t *k, const uint8_t *image, uint32_t size); /* 0 if OK */
void core_boot(core_t *k);                     /* boot state of tile (0,0) */
void core_step(core_t *k);
uint64_t core_run(core_t *k, uint64_t max_steps);
void core_report(const core_t *k, FILE *f);
void core_dump(const core_t *k, FILE *f);
const char *core_state_name(core_state_t s);
void cap_format(const cap_t *c, char *buf, size_t n);

#endif
