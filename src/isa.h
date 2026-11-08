/* isa.h - Tessera v0.1 instruction set: encodings, tables and names. */
#ifndef TESSERA_ISA_H
#define TESSERA_ISA_H

#include <stddef.h>
#include <stdint.h>

/* Instruction fields. The positions are the same in all formats. */
#define F_OP(w)    (((w) >> 26) & 0x3Fu)
#define F_A(w)     (((w) >> 22) & 0xFu)
#define F_B(w)     (((w) >> 18) & 0xFu)
#define F_C(w)     (((w) >> 14) & 0xFu)
#define F_FUNC(w)  ((w) & 0x3FFFu)
#define F_IMM18(w) ((w) & 0x3FFFFu)
#define F_IMM22(w) ((w) & 0x3FFFFFu)

enum { FMT_R, FMT_I, FMT_J };

enum {
    OP_ALU = 0x00, OP_ADDI = 0x01, OP_ANDI = 0x02, OP_ORI = 0x03,
    OP_XORI = 0x04, OP_SLTI = 0x05, OP_SHI = 0x06, OP_LUI = 0x07,
    OP_BEQ = 0x08, OP_BNE = 0x09, OP_BLT = 0x0A, OP_BGE = 0x0B,
    OP_BLTU = 0x0C, OP_BGEU = 0x0D, OP_JAL = 0x0E, OP_JALR = 0x0F,
    OP_LB = 0x10, OP_LBU = 0x11, OP_LH = 0x12, OP_LHU = 0x13,
    OP_LW = 0x14, OP_SB = 0x15, OP_SH = 0x16, OP_SW = 0x17,
    OP_LC = 0x18, OP_SC = 0x19, OP_CAP = 0x1A, OP_CINCADDRI = 0x1B,
    OP_MSG = 0x1C, OP_DMA = 0x1D, OP_SYS = 0x1E
};

enum {
    ALU_ADD, ALU_SUB, ALU_AND, ALU_OR, ALU_XOR, ALU_SLL, ALU_SRL, ALU_SRA,
    ALU_SLT, ALU_SLTU, ALU_MUL, ALU_MULH, ALU_MULHU, ALU_DIV, ALU_DIVU,
    ALU_REM, ALU_REMU
};

enum { SH_SLL, SH_SRL, SH_SRA };

enum {
    CAP_GETADDR, CAP_GETBASE, CAP_GETLEN, CAP_GETPERM, CAP_GETSPACE,
    CAP_GETTAG, CAP_SETADDR, CAP_INCADDR, CAP_SETBOUNDS, CAP_ANDPERM,
    CAP_MOVE, CAP_CLEARTAG, CAP_TESTSUB, CAP_SPECR, CAP_SPECW
};

enum {
    MSG_MPUT, MSG_MPUTC, MSG_SEND, MSG_RECV, MSG_TRYRECV, MSG_MGET,
    MSG_MGETC, MSG_WAIT
};

enum { DMA_GET, DMA_PUT };

enum { SYS_HALT, SYS_CSRR, SYS_BREAK };

/* Capability spaces. */
enum { SPACE_LOCAL = 0, SPACE_GLOBAL = 1, SPACE_CHANNEL = 2, SPACE_MAILBOX = 3 };

/* Capability permissions. */
#define PERM_LOAD     (1u << 0)
#define PERM_STORE    (1u << 1)
#define PERM_EXEC     (1u << 2)
#define PERM_LOADCAP  (1u << 3)
#define PERM_STORECAP (1u << 4)
#define PERM_SEND     (1u << 5)
#define PERM_RECV     (1u << 6)
#define PERM_SEAL     (1u << 7)
#define PERM_UNSEAL   (1u << 8)

/* Control and status registers. */
enum {
    CSR_TILEID, CSR_GRIDDIM, CSR_CYCLE, CSR_CYCLEH, CSR_INSTRET,
    CSR_LOCALSIZE, CSR_COUNT
};

/* Fault codes. */
enum {
    FAULT_NONE, FAULT_TAG, FAULT_SEALED, FAULT_SPACE, FAULT_PERM,
    FAULT_BOUNDS, FAULT_ALIGN, FAULT_ILLEGAL, FAULT_DIVZERO, FAULT_DERIVE,
    FAULT_SENDCAP, FAULT_BREAK, FAULT_COUNT
};

/* Endpoint of the console device: tile (255,0), mailbox 0. */
#define CONSOLE_ENDPOINT 0xFF0000u

/* Operand kinds in the instruction table. */
typedef enum {
    OPK_NONE,
    OPK_RA, OPK_RB, OPK_RC,  /* integer register in field a, b or c */
    OPK_CA, OPK_CB, OPK_CC,  /* capability register in field a, b or c */
    OPK_IMM18,               /* signed 18-bit immediate */
    OPK_UIMM18,              /* zero-extended 18-bit immediate */
    OPK_SHAMT,               /* shift amount 0-31 in imm[4:0] */
    OPK_UIMM22,              /* upper 22 bits for LUI */
    OPK_BR18,                /* branch target: label or address */
    OPK_BR22,                /* jump target: label or address */
    OPK_MEM,                 /* imm18(cB) */
    OPK_NC,                  /* small number 0-15 in field c */
    OPK_CSR,                 /* CSR name or number in field c */
    OPK_MBOX                 /* DMA completion mailbox in func bits 7:4 */
} opkind_t;

typedef struct {
    const char *name;
    uint8_t fmt;
    uint8_t opcode;
    uint16_t func;           /* R: func. SLLI/SRLI/SRAI: shift kind. */
    uint8_t nops;
    opkind_t ops[4];
} insn_def_t;

extern const insn_def_t isa_table[];
extern const int isa_table_len;

/* Sign-extends the low `bits` bits of x. */
static inline int32_t isa_sext(uint32_t x, int bits)
{
    uint32_t m = 1u << (bits - 1);
    x &= (1u << bits) - 1u;
    return (int32_t)((x ^ m) - m);
}

const insn_def_t *isa_find(const char *name);  /* case is not important */
const insn_def_t *isa_decode(uint32_t w);      /* NULL if not legal */
void isa_disasm(uint32_t w, uint32_t pc, char *buf, size_t n);

const char *fault_name(int code);
int fault_lookup(const char *name);            /* -1 if not known */
const char *csr_name(int n);                   /* NULL if not known */
int csr_lookup(const char *name);              /* -1 if not known */

#endif
