/* core.c - one Tessera v0.1 core (milestone 1).
 *
 * Milestone 1 runs integer and control instructions, HALT, CSRR, BREAK,
 * and a temporary console: SEND to the console endpoint prints bytes.
 * Other instructions stop the core in the UNSUPPORTED state.
 */
#include "core.h"
#include "isa.h"

#include <stdlib.h>
#include <string.h>

static const cap_t NULL_CAP;

static cap_t mkcap(uint8_t space, uint32_t base, uint32_t length,
                   uint16_t perms, uint32_t cursor)
{
    cap_t c;
    memset(&c, 0, sizeof c);
    c.space = space;
    c.base = base;
    c.length = length;
    c.perms = perms;
    c.cursor = cursor;
    c.tag = 1;
    return c;
}

static void stdout_console(void *ctx, const uint8_t *b, uint32_t n)
{
    (void)ctx;
    fwrite(b, 1, n, stdout);
    fflush(stdout);
}

int core_init(core_t *k, uint32_t mem_size)
{
    memset(k, 0, sizeof *k);
    k->mem = calloc(mem_size ? mem_size : 1, 1);
    if (!k->mem)
        return -1;
    k->mem_size = mem_size;
    k->grid_w = 1;
    k->grid_h = 1;
    k->console = stdout_console;
    core_boot(k);
    return 0;
}

void core_free(core_t *k)
{
    free(k->mem);
    k->mem = NULL;
}

int core_load(core_t *k, const uint8_t *image, uint32_t size)
{
    if (size > k->mem_size)
        return -1;
    memset(k->mem, 0, k->mem_size);
    if (size)
        memcpy(k->mem, image, size);
    return 0;
}

void core_boot(core_t *k)
{
    const uint16_t data = PERM_LOAD | PERM_STORE | PERM_LOADCAP | PERM_STORECAP;

    memset(k->r, 0, sizeof k->r);
    for (int i = 0; i < 16; i++)
        k->c[i] = NULL_CAP;

    /* All tiles */
    k->pcc = mkcap(SPACE_LOCAL, 0, k->mem_size,
                   PERM_LOAD | PERM_EXEC | PERM_LOADCAP, 0);
    k->c[3] = mkcap(SPACE_LOCAL, 0, k->mem_size, data, 0);
    k->c[4] = mkcap(SPACE_MAILBOX, 0, 16, PERM_RECV, 0);
    k->c[5] = mkcap(SPACE_CHANNEL, 0, 1, PERM_SEND, 0);

    /* Supervisor (0,0) only. Milestone 1 runs only this tile. */
    k->c[6] = mkcap(SPACE_GLOBAL, 0, GLOBAL_SIZE_DEFAULT, data, 0);
    k->c[7] = mkcap(SPACE_CHANNEL, 0, 0x1000000u, PERM_SEND, 0);
    k->c[8] = mkcap(SPACE_CHANNEL, CONSOLE_ENDPOINT, 1, PERM_SEND,
                    CONSOLE_ENDPOINT);
    k->fcc = NULL_CAP;

    memset(k->out_w, 0, sizeof k->out_w);
    k->out_cap = NULL_CAP;
    k->cycle = 0;
    k->instret = 0;
    k->state = CORE_RUNNING;
    k->fault_code = FAULT_NONE;
    k->fault_pc = k->fault_word = k->fault_addr = 0;
    k->note[0] = 0;
}

/* ------------------------------------------------------------ checks */

static int check_cap(const cap_t *c, int space, unsigned perm)
{
    if (!c->tag)
        return FAULT_TAG;
    if (c->otype != 0)
        return FAULT_SEALED;
    if (c->space != space)
        return FAULT_SPACE;
    if ((c->perms & perm) != perm)
        return FAULT_PERM;
    return FAULT_NONE;
}

static int check_range(const cap_t *c, int64_t addr, uint32_t size,
                       uint32_t align)
{
    if (addr < (int64_t)c->base ||
        addr + (int64_t)size > (int64_t)c->base + (int64_t)c->length)
        return FAULT_BOUNDS;
    if (align > 1 && (addr & (int64_t)(align - 1)) != 0)
        return FAULT_ALIGN;
    return FAULT_NONE;
}

static void fault(core_t *k, int code, uint32_t pc, uint32_t word,
                  uint32_t addr)
{
    k->state = CORE_FAULTED;
    k->fault_code = code;
    k->fault_pc = pc;
    k->fault_word = word;
    k->fault_addr = addr;
    k->note[0] = 0;
}

static void unsupported(core_t *k, uint32_t pc, uint32_t word,
                        const char *what)
{
    k->state = CORE_UNSUPPORTED;
    k->fault_pc = pc;
    k->fault_word = word;
    snprintf(k->note, sizeof k->note, "%s", what);
}

/* ------------------------------------------------------------ helpers */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void wr(core_t *k, unsigned r, uint32_t v)
{
    if (r)
        k->r[r] = v;
}

static void wc(core_t *k, unsigned c, cap_t v)
{
    if (c)
        k->c[c] = v;
}

static uint32_t sra(uint32_t x, unsigned s)
{
    if (s == 0)
        return x;
    return (x >> s) | ((x & 0x80000000u) ? ~(0xFFFFFFFFu >> s) : 0u);
}

/* SEND. Returns 1 if the core can continue. */
static int do_send(core_t *k, uint32_t pc, uint32_t w, unsigned b)
{
    const cap_t *ch = &k->c[b];
    int f = check_cap(ch, SPACE_CHANNEL, PERM_SEND);
    if (!f)
        f = check_range(ch, ch->cursor, 1, 1);
    if (f) {
        fault(k, f, pc, w, ch->cursor);
        return 0;
    }
    if (k->out_cap.tag && (k->out_cap.space == SPACE_LOCAL ||
                           k->out_cap.space == SPACE_MAILBOX)) {
        fault(k, FAULT_SENDCAP, pc, w, 0);
        return 0;
    }
    if (ch->cursor == CONSOLE_ENDPOINT) {
        uint8_t bytes[12];
        uint32_t n = k->out_w[0] > 12 ? 12 : k->out_w[0];
        for (int i = 0; i < 12; i++)
            bytes[i] = (uint8_t)(k->out_w[1 + i / 4] >> (8 * (i % 4)));
        if (k->console && n)
            k->console(k->console_ctx, bytes, n);
    } else {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "SEND to tile (%u,%u) mailbox %u: the network comes in milestone 3",
                 (ch->cursor >> 16) & 0xFFu, (ch->cursor >> 8) & 0xFFu,
                 ch->cursor & 0xFFu);
        unsupported(k, pc, w, msg);
        return 0;
    }
    k->out_cap = NULL_CAP;
    return 1;
}

/* ----------------------------------------------------------- execute */

static void exec(core_t *k, uint32_t pc, uint32_t w)
{
    const insn_def_t *d = isa_decode(w);
    unsigned a, b, c;
    uint32_t ra, rb, rc, uimm, next = pc + 4u;
    int32_t imm;

    if (!d) {
        fault(k, FAULT_ILLEGAL, pc, w, 0);
        return;
    }
    a = F_A(w);
    b = F_B(w);
    c = F_C(w);
    ra = k->r[a];
    rb = k->r[b];
    rc = k->r[c];
    uimm = F_IMM18(w);
    imm = isa_sext(uimm, 18);

    switch (d->opcode) {
    case OP_ALU: {
        uint32_t x = rb, y = rc, v = 0;
        switch (d->func) {
        case ALU_ADD: v = x + y; break;
        case ALU_SUB: v = x - y; break;
        case ALU_AND: v = x & y; break;
        case ALU_OR: v = x | y; break;
        case ALU_XOR: v = x ^ y; break;
        case ALU_SLL: v = x << (y & 31u); break;
        case ALU_SRL: v = x >> (y & 31u); break;
        case ALU_SRA: v = sra(x, y & 31u); break;
        case ALU_SLT: v = (int32_t)x < (int32_t)y; break;
        case ALU_SLTU: v = x < y; break;
        case ALU_MUL: v = x * y; break;
        case ALU_MULH:
            v = (uint32_t)((uint64_t)((int64_t)(int32_t)x * (int64_t)(int32_t)y) >> 32);
            break;
        case ALU_MULHU: v = (uint32_t)(((uint64_t)x * y) >> 32); break;
        default: /* DIV, DIVU, REM, REMU */
            if (y == 0) {
                fault(k, FAULT_DIVZERO, pc, w, 0);
                return;
            }
            if (d->func == ALU_DIV)
                v = (x == 0x80000000u && y == 0xFFFFFFFFu)
                        ? x : (uint32_t)((int32_t)x / (int32_t)y);
            else if (d->func == ALU_REM)
                v = (x == 0x80000000u && y == 0xFFFFFFFFu)
                        ? 0 : (uint32_t)((int32_t)x % (int32_t)y);
            else if (d->func == ALU_DIVU)
                v = x / y;
            else
                v = x % y;
            break;
        }
        wr(k, a, v);
        break;
    }
    case OP_ADDI: wr(k, a, rb + (uint32_t)imm); break;
    case OP_ANDI: wr(k, a, rb & uimm); break;
    case OP_ORI: wr(k, a, rb | uimm); break;
    case OP_XORI: wr(k, a, rb ^ uimm); break;
    case OP_SLTI: wr(k, a, (int32_t)rb < imm); break;
    case OP_SHI: {
        unsigned sh = uimm & 31u;
        uint32_t v = d->func == SH_SLL ? rb << sh
                   : d->func == SH_SRL ? rb >> sh
                   : sra(rb, sh);
        wr(k, a, v);
        break;
    }
    case OP_LUI: wr(k, a, F_IMM22(w) << 10); break;

    case OP_BEQ: if (ra == rb) next = pc + (uint32_t)imm * 4u; break;
    case OP_BNE: if (ra != rb) next = pc + (uint32_t)imm * 4u; break;
    case OP_BLT: if ((int32_t)ra < (int32_t)rb) next = pc + (uint32_t)imm * 4u; break;
    case OP_BGE: if ((int32_t)ra >= (int32_t)rb) next = pc + (uint32_t)imm * 4u; break;
    case OP_BLTU: if (ra < rb) next = pc + (uint32_t)imm * 4u; break;
    case OP_BGEU: if (ra >= rb) next = pc + (uint32_t)imm * 4u; break;

    case OP_JAL: {
        cap_t ret = k->pcc;
        ret.cursor = pc + 4u;
        wc(k, a, ret);
        next = pc + (uint32_t)isa_sext(F_IMM22(w), 22) * 4u;
        break;
    }
    case OP_JALR: {
        cap_t target = k->c[b], ret = k->pcc;
        int f;
        target.cursor += (uint32_t)imm * 4u;
        f = check_cap(&target, SPACE_LOCAL, PERM_EXEC);
        if (f) {
            fault(k, f, pc, w, target.cursor);
            return;
        }
        ret.cursor = pc + 4u;
        wc(k, a, ret);
        k->pcc = target;  /* fetch checks the bounds */
        return;
    }

    case OP_LB: case OP_LBU: case OP_LH: case OP_LHU: case OP_LW:
    case OP_SB: case OP_SH: case OP_SW: case OP_LC: case OP_SC:
    case OP_CAP: case OP_CINCADDRI:
        unsupported(k, pc, w,
                    "memory and capability instructions come in milestone 2");
        return;

    case OP_DMA:
        unsupported(k, pc, w, "DMA comes in milestone 4");
        return;

    case OP_MSG:
        switch (d->func) {
        case MSG_MPUT:
            if (c > 3) {
                fault(k, FAULT_ILLEGAL, pc, w, 0);
                return;
            }
            k->out_w[c] = rb;
            break;
        case MSG_MPUTC:
            k->out_cap = k->c[b];
            break;
        case MSG_SEND:
            if (!do_send(k, pc, w, b))
                return;
            break;
        default:
            unsupported(k, pc, w,
                        "RECV, TRYRECV, MGET, MGETC and WAIT come in milestone 3");
            return;
        }
        break;

    case OP_SYS:
        switch (d->func) {
        case SYS_HALT:
            k->state = CORE_HALTED;
            return;  /* pc stays on the HALT instruction */
        case SYS_CSRR: {
            uint32_t v;
            switch (c) {
            case CSR_TILEID: v = k->tile_id; break;
            case CSR_GRIDDIM: v = ((uint32_t)k->grid_w << 8) | k->grid_h; break;
            case CSR_CYCLE: v = (uint32_t)k->cycle; break;
            case CSR_CYCLEH: v = (uint32_t)(k->cycle >> 32); break;
            case CSR_INSTRET: v = (uint32_t)k->instret; break;
            case CSR_LOCALSIZE: v = k->mem_size; break;
            default:
                fault(k, FAULT_ILLEGAL, pc, w, 0);
                return;
            }
            wr(k, a, v);
            break;
        }
        default: /* BREAK */
            fault(k, FAULT_BREAK, pc, w, 0);
            return;
        }
        break;

    default:
        fault(k, FAULT_ILLEGAL, pc, w, 0);
        return;
    }
    k->pcc.cursor = next;
}

/* ------------------------------------------------------------- trace */

static void trace_step(core_t *k, uint32_t pc, uint32_t w,
                       const uint32_t *old_r, const cap_t *old_c)
{
    char dis[64], capbuf[128];
    isa_disasm(w, pc, dis, sizeof dis);
    fprintf(k->trace, "%8llu  0x%04x  0x%08x  %-30s",
            (unsigned long long)k->cycle, pc, w, dis);
    for (int i = 1; i < 16; i++)
        if (k->r[i] != old_r[i])
            fprintf(k->trace, " r%d=0x%08x", i, k->r[i]);
    for (int i = 1; i < 16; i++)
        if (memcmp(&k->c[i], &old_c[i], sizeof(cap_t)) != 0) {
            cap_format(&k->c[i], capbuf, sizeof capbuf);
            fprintf(k->trace, " c%d=%s", i, capbuf);
        }
    if (k->state == CORE_FAULTED)
        fprintf(k->trace, " FAULT %s", fault_name(k->fault_code));
    else if (k->state == CORE_UNSUPPORTED)
        fprintf(k->trace, " UNSUPPORTED");
    else if (k->state == CORE_HALTED)
        fprintf(k->trace, " HALTED");
    fputc('\n', k->trace);
}

/* -------------------------------------------------------------- step */

void core_step(core_t *k)
{
    uint32_t pc, w;
    uint32_t old_r[16];
    cap_t old_c[16];
    int f;

    if (k->state != CORE_RUNNING)
        return;
    pc = k->pcc.cursor;
    f = check_cap(&k->pcc, SPACE_LOCAL, PERM_EXEC);
    if (!f)
        f = check_range(&k->pcc, pc, 4, 4);
    if (!f && (uint64_t)pc + 4u > k->mem_size)
        f = FAULT_BOUNDS;
    if (f) {
        fault(k, f, pc, 0, pc);
        snprintf(k->note, sizeof k->note, "instruction fetch");
        if (k->trace)
            fprintf(k->trace, "%8llu  0x%04x  (fetch)     %-30s FAULT %s\n",
                    (unsigned long long)k->cycle, pc, "", fault_name(f));
        return;
    }
    w = rd32(k->mem + pc);
    if (k->trace) {
        memcpy(old_r, k->r, sizeof old_r);
        memcpy(old_c, k->c, sizeof old_c);
    }
    exec(k, pc, w);
    if (k->trace)
        trace_step(k, pc, w, old_r, old_c);
    k->cycle++;
    if (k->state == CORE_RUNNING || k->state == CORE_HALTED)
        k->instret++;
}

uint64_t core_run(core_t *k, uint64_t max_steps)
{
    uint64_t n = 0;
    while (k->state == CORE_RUNNING && n < max_steps) {
        core_step(k);
        n++;
    }
    return n;
}

/* ------------------------------------------------------------ output */

const char *core_state_name(core_state_t s)
{
    switch (s) {
    case CORE_RUNNING: return "RUNNING";
    case CORE_HALTED: return "HALTED";
    case CORE_FAULTED: return "FAULTED";
    case CORE_UNSUPPORTED: return "UNSUPPORTED";
    }
    return "?";
}

static void perm_string(uint16_t p, char *buf, size_t n)
{
    static const char *const names[] = {
        "R", "W", "X", "LC", "SC", "SEND", "RECV", "SEAL", "UNSEAL"
    };
    size_t len = 0;
    buf[0] = 0;
    for (int i = 0; i < 9; i++) {
        if (!(p & (1u << i)))
            continue;
        len += (size_t)snprintf(buf + len, n > len ? n - len : 0, "%s%s",
                                len ? "," : "", names[i]);
        if (len >= n)
            return;
    }
    if (len == 0)
        snprintf(buf, n, "-");
}

void cap_format(const cap_t *c, char *buf, size_t n)
{
    static const char *const spaces[] = { "LOCAL", "GLOBAL", "CHANNEL", "MAILBOX" };
    char perms[64];
    if (!c->tag && c->cursor == 0 && c->base == 0 && c->length == 0 &&
        c->perms == 0) {
        snprintf(buf, n, "null");
        return;
    }
    perm_string(c->perms, perms, sizeof perms);
    snprintf(buf, n, "{%s %s base=0x%x len=0x%x cur=0x%x %s}",
             c->tag ? "tag" : "notag",
             c->space < 4 ? spaces[c->space] : "RESERVED",
             c->base, c->length, c->cursor, perms);
}

void core_report(const core_t *k, FILE *f)
{
    char dis[64];
    isa_disasm(k->fault_word, k->fault_pc, dis, sizeof dis);
    switch (k->state) {
    case CORE_RUNNING:
        fprintf(f, "stopped: step limit reached at pc 0x%04x after %llu instructions\n",
                k->pcc.cursor, (unsigned long long)k->instret);
        break;
    case CORE_HALTED:
        fprintf(f, "halted at pc 0x%04x after %llu instructions\n",
                k->pcc.cursor, (unsigned long long)k->instret);
        break;
    case CORE_FAULTED:
        fprintf(f, "FAULT %s (code %d) at pc 0x%04x\n",
                fault_name(k->fault_code), k->fault_code, k->fault_pc);
        if (k->note[0])
            fprintf(f, "  during      %s\n", k->note);
        else
            fprintf(f, "  instruction 0x%08x  %s\n", k->fault_word, dis);
        fprintf(f, "  address     0x%x\n", k->fault_addr);
        fprintf(f, "  report      %s\n", k->fcc.tag
                ? "would go to fcc (the network comes in milestone 3)"
                : "not sent: fcc is null on the supervisor");
        break;
    case CORE_UNSUPPORTED:
        fprintf(f, "stopped at pc 0x%04x: %s\n", k->fault_pc, k->note);
        fprintf(f, "  instruction 0x%08x  %s\n", k->fault_word, dis);
        break;
    }
}

void core_dump(const core_t *k, FILE *f)
{
    char buf[128];
    for (int i = 0; i < 16; i += 4)
        fprintf(f, "  r%-2d 0x%08x   r%-2d 0x%08x   r%-2d 0x%08x   r%-2d 0x%08x\n",
                i, k->r[i], i + 1, k->r[i + 1], i + 2, k->r[i + 2], i + 3,
                k->r[i + 3]);
    cap_format(&k->pcc, buf, sizeof buf);
    fprintf(f, "  pcc %s\n", buf);
    cap_format(&k->fcc, buf, sizeof buf);
    fprintf(f, "  fcc %s\n", buf);
    for (int i = 1; i < 16; i++) {
        if (!k->c[i].tag && k->c[i].length == 0 && k->c[i].cursor == 0)
            continue;
        cap_format(&k->c[i], buf, sizeof buf);
        fprintf(f, "  c%-2d %s\n", i, buf);
    }
    fprintf(f, "  cycle %llu  instret %llu\n", (unsigned long long)k->cycle,
            (unsigned long long)k->instret);
}
