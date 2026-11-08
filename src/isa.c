/* isa.c - Tessera v0.1 instruction table, decoder and disassembler. */
#include "isa.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RRR {OPK_RA, OPK_RB, OPK_RC}
#define RRI {OPK_RA, OPK_RB, OPK_IMM18}
#define RRU {OPK_RA, OPK_RB, OPK_UIMM18}
#define RRS {OPK_RA, OPK_RB, OPK_SHAMT}
#define RRB {OPK_RA, OPK_RB, OPK_BR18}
#define RM  {OPK_RA, OPK_MEM}
#define CM  {OPK_CA, OPK_MEM}
#define RC  {OPK_RA, OPK_CB}
#define CCR {OPK_CA, OPK_CB, OPK_RC}
#define CC  {OPK_CA, OPK_CB}

const insn_def_t isa_table[] = {
    /* Integer, R format, opcode 0x00 */
    {"ADD",   FMT_R, OP_ALU, ALU_ADD,   3, RRR},
    {"SUB",   FMT_R, OP_ALU, ALU_SUB,   3, RRR},
    {"AND",   FMT_R, OP_ALU, ALU_AND,   3, RRR},
    {"OR",    FMT_R, OP_ALU, ALU_OR,    3, RRR},
    {"XOR",   FMT_R, OP_ALU, ALU_XOR,   3, RRR},
    {"SLL",   FMT_R, OP_ALU, ALU_SLL,   3, RRR},
    {"SRL",   FMT_R, OP_ALU, ALU_SRL,   3, RRR},
    {"SRA",   FMT_R, OP_ALU, ALU_SRA,   3, RRR},
    {"SLT",   FMT_R, OP_ALU, ALU_SLT,   3, RRR},
    {"SLTU",  FMT_R, OP_ALU, ALU_SLTU,  3, RRR},
    {"MUL",   FMT_R, OP_ALU, ALU_MUL,   3, RRR},
    {"MULH",  FMT_R, OP_ALU, ALU_MULH,  3, RRR},
    {"MULHU", FMT_R, OP_ALU, ALU_MULHU, 3, RRR},
    {"DIV",   FMT_R, OP_ALU, ALU_DIV,   3, RRR},
    {"DIVU",  FMT_R, OP_ALU, ALU_DIVU,  3, RRR},
    {"REM",   FMT_R, OP_ALU, ALU_REM,   3, RRR},
    {"REMU",  FMT_R, OP_ALU, ALU_REMU,  3, RRR},
    /* Integer, I and J formats */
    {"ADDI",  FMT_I, OP_ADDI, 0,      3, RRI},
    {"ANDI",  FMT_I, OP_ANDI, 0,      3, RRU},
    {"ORI",   FMT_I, OP_ORI,  0,      3, RRU},
    {"XORI",  FMT_I, OP_XORI, 0,      3, RRU},
    {"SLTI",  FMT_I, OP_SLTI, 0,      3, RRI},
    {"SLLI",  FMT_I, OP_SHI,  SH_SLL, 3, RRS},
    {"SRLI",  FMT_I, OP_SHI,  SH_SRL, 3, RRS},
    {"SRAI",  FMT_I, OP_SHI,  SH_SRA, 3, RRS},
    {"LUI",   FMT_J, OP_LUI,  0,      2, {OPK_RA, OPK_UIMM22}},
    /* Control */
    {"BEQ",   FMT_I, OP_BEQ,  0, 3, RRB},
    {"BNE",   FMT_I, OP_BNE,  0, 3, RRB},
    {"BLT",   FMT_I, OP_BLT,  0, 3, RRB},
    {"BGE",   FMT_I, OP_BGE,  0, 3, RRB},
    {"BLTU",  FMT_I, OP_BLTU, 0, 3, RRB},
    {"BGEU",  FMT_I, OP_BGEU, 0, 3, RRB},
    {"JAL",   FMT_J, OP_JAL,  0, 2, {OPK_CA, OPK_BR22}},
    {"JALR",  FMT_I, OP_JALR, 0, 3, {OPK_CA, OPK_CB, OPK_IMM18}},
    /* Memory */
    {"LB",    FMT_I, OP_LB,  0, 2, RM},
    {"LBU",   FMT_I, OP_LBU, 0, 2, RM},
    {"LH",    FMT_I, OP_LH,  0, 2, RM},
    {"LHU",   FMT_I, OP_LHU, 0, 2, RM},
    {"LW",    FMT_I, OP_LW,  0, 2, RM},
    {"SB",    FMT_I, OP_SB,  0, 2, RM},
    {"SH",    FMT_I, OP_SH,  0, 2, RM},
    {"SW",    FMT_I, OP_SW,  0, 2, RM},
    {"LC",    FMT_I, OP_LC,  0, 2, CM},
    {"SC",    FMT_I, OP_SC,  0, 2, CM},
    /* Capability */
    {"CGETADDR",   FMT_R, OP_CAP, CAP_GETADDR,   2, RC},
    {"CGETBASE",   FMT_R, OP_CAP, CAP_GETBASE,   2, RC},
    {"CGETLEN",    FMT_R, OP_CAP, CAP_GETLEN,    2, RC},
    {"CGETPERM",   FMT_R, OP_CAP, CAP_GETPERM,   2, RC},
    {"CGETSPACE",  FMT_R, OP_CAP, CAP_GETSPACE,  2, RC},
    {"CGETTAG",    FMT_R, OP_CAP, CAP_GETTAG,    2, RC},
    {"CSETADDR",   FMT_R, OP_CAP, CAP_SETADDR,   3, CCR},
    {"CINCADDR",   FMT_R, OP_CAP, CAP_INCADDR,   3, CCR},
    {"CSETBOUNDS", FMT_R, OP_CAP, CAP_SETBOUNDS, 3, CCR},
    {"CANDPERM",   FMT_R, OP_CAP, CAP_ANDPERM,   3, CCR},
    {"CMOVE",      FMT_R, OP_CAP, CAP_MOVE,      2, CC},
    {"CCLEARTAG",  FMT_R, OP_CAP, CAP_CLEARTAG,  2, CC},
    {"CTESTSUB",   FMT_R, OP_CAP, CAP_TESTSUB,   3, {OPK_RA, OPK_CB, OPK_CC}},
    {"CSPECR",     FMT_R, OP_CAP, CAP_SPECR,     2, {OPK_CA, OPK_NC}},
    {"CSPECW",     FMT_R, OP_CAP, CAP_SPECW,     2, {OPK_NC, OPK_CB}},
    {"CINCADDRI",  FMT_I, OP_CINCADDRI, 0,       3, {OPK_CA, OPK_CB, OPK_IMM18}},
    /* Messages */
    {"MPUT",    FMT_R, OP_MSG, MSG_MPUT,    2, {OPK_NC, OPK_RB}},
    {"MPUTC",   FMT_R, OP_MSG, MSG_MPUTC,   1, {OPK_CB}},
    {"SEND",    FMT_R, OP_MSG, MSG_SEND,    1, {OPK_CB}},
    {"RECV",    FMT_R, OP_MSG, MSG_RECV,    2, RC},
    {"TRYRECV", FMT_R, OP_MSG, MSG_TRYRECV, 2, RC},
    {"MGET",    FMT_R, OP_MSG, MSG_MGET,    2, {OPK_RA, OPK_NC}},
    {"MGETC",   FMT_R, OP_MSG, MSG_MGETC,   1, {OPK_CA}},
    {"WAIT",    FMT_R, OP_MSG, MSG_WAIT,    2, RC},
    /* DMA */
    {"DMAGET",  FMT_R, OP_DMA, DMA_GET, 4, {OPK_CA, OPK_CB, OPK_RC, OPK_MBOX}},
    {"DMAPUT",  FMT_R, OP_DMA, DMA_PUT, 4, {OPK_CA, OPK_CB, OPK_RC, OPK_MBOX}},
    /* System */
    {"HALT",    FMT_R, OP_SYS, SYS_HALT,  0, {OPK_NONE}},
    {"CSRR",    FMT_R, OP_SYS, SYS_CSRR,  2, {OPK_RA, OPK_CSR}},
    {"BREAK",   FMT_R, OP_SYS, SYS_BREAK, 0, {OPK_NONE}},
};

const int isa_table_len = (int)(sizeof isa_table / sizeof isa_table[0]);

static const char *const fault_names[FAULT_COUNT] = {
    "NONE", "TAG", "SEALED", "SPACE", "PERM", "BOUNDS", "ALIGN",
    "ILLEGAL", "DIVZERO", "DERIVE", "SENDCAP", "BREAK"
};

static const char *const csr_names[CSR_COUNT] = {
    "TILEID", "GRIDDIM", "CYCLE", "CYCLEH", "INSTRET", "LOCALSIZE"
};

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

const insn_def_t *isa_find(const char *name)
{
    for (int i = 0; i < isa_table_len; i++)
        if (ieq(isa_table[i].name, name))
            return &isa_table[i];
    return NULL;
}

const insn_def_t *isa_decode(uint32_t w)
{
    uint32_t op = F_OP(w);
    for (int i = 0; i < isa_table_len; i++) {
        const insn_def_t *d = &isa_table[i];
        if (d->opcode != op)
            continue;
        if (d->fmt == FMT_R) {
            uint32_t func = F_FUNC(w);
            if (op == OP_DMA) {
                /* func bits 7:4 hold the completion mailbox. */
                if ((func & 0xFu) == d->func && (func >> 8) == 0)
                    return d;
            } else if (func == d->func) {
                return d;
            }
        } else if (op == OP_SHI) {
            uint32_t imm = F_IMM18(w);
            if (((imm >> 5) & 3u) == d->func && (imm >> 7) == 0)
                return d;
        } else {
            return d;
        }
    }
    return NULL;
}

static void app(char *buf, size_t n, size_t *len, const char *fmt, ...)
{
    va_list ap;
    int k;
    if (*len + 1 >= n)
        return;
    va_start(ap, fmt);
    k = vsnprintf(buf + *len, n - *len, fmt, ap);
    va_end(ap);
    if (k < 0)
        return;
    *len += (size_t)k;
    if (*len >= n)
        *len = n - 1;
}

void isa_disasm(uint32_t w, uint32_t pc, char *buf, size_t n)
{
    const insn_def_t *d = isa_decode(w);
    size_t len = 0;
    if (n == 0)
        return;
    buf[0] = 0;
    if (!d) {
        app(buf, n, &len, ".word     0x%08x", w);
        return;
    }
    app(buf, n, &len, "%-9s ", d->name);
    for (int i = 0; i < d->nops; i++) {
        const char *cn;
        if (i > 0)
            app(buf, n, &len, ", ");
        switch (d->ops[i]) {
        case OPK_RA: app(buf, n, &len, "r%u", F_A(w)); break;
        case OPK_RB: app(buf, n, &len, "r%u", F_B(w)); break;
        case OPK_RC: app(buf, n, &len, "r%u", F_C(w)); break;
        case OPK_CA: app(buf, n, &len, "c%u", F_A(w)); break;
        case OPK_CB: app(buf, n, &len, "c%u", F_B(w)); break;
        case OPK_CC: app(buf, n, &len, "c%u", F_C(w)); break;
        case OPK_IMM18: app(buf, n, &len, "%d", isa_sext(F_IMM18(w), 18)); break;
        case OPK_UIMM18: app(buf, n, &len, "0x%x", F_IMM18(w)); break;
        case OPK_SHAMT: app(buf, n, &len, "%u", F_IMM18(w) & 31u); break;
        case OPK_UIMM22: app(buf, n, &len, "0x%x", F_IMM22(w)); break;
        case OPK_BR18:
            app(buf, n, &len, "0x%04x",
                pc + (uint32_t)isa_sext(F_IMM18(w), 18) * 4u);
            break;
        case OPK_BR22:
            app(buf, n, &len, "0x%04x",
                pc + (uint32_t)isa_sext(F_IMM22(w), 22) * 4u);
            break;
        case OPK_MEM:
            app(buf, n, &len, "%d(c%u)", isa_sext(F_IMM18(w), 18), F_B(w));
            break;
        case OPK_NC: app(buf, n, &len, "%u", F_C(w)); break;
        case OPK_CSR:
            cn = csr_name((int)F_C(w));
            if (cn)
                app(buf, n, &len, "%s", cn);
            else
                app(buf, n, &len, "%u", F_C(w));
            break;
        case OPK_MBOX: app(buf, n, &len, "%u", (F_FUNC(w) >> 4) & 0xFu); break;
        case OPK_NONE: break;
        }
    }
    /* Remove the space after a mnemonic with no operands. */
    while (len > 0 && buf[len - 1] == ' ')
        buf[--len] = 0;
}

const char *fault_name(int code)
{
    if (code < 0 || code >= FAULT_COUNT)
        return "?";
    return fault_names[code];
}

int fault_lookup(const char *name)
{
    for (int i = 1; i < FAULT_COUNT; i++)
        if (ieq(fault_names[i], name))
            return i;
    return -1;
}

const char *csr_name(int n)
{
    if (n < 0 || n >= CSR_COUNT)
        return NULL;
    return csr_names[n];
}

int csr_lookup(const char *name)
{
    for (int i = 0; i < CSR_COUNT; i++)
        if (ieq(csr_names[i], name))
            return i;
    return -1;
}
