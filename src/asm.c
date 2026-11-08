/* asm.c - two-pass assembler for Tessera v0.1. */
#include "asm.h"
#include "isa.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IMAGE (16u * 1024u * 1024u)

typedef struct {
    char *name;
    int64_t value;
} sym_t;

typedef struct {
    int line;
    char *src;        /* the original line, for the listing */
    char **labels;
    int nlabels;
    char *mnem;       /* NULL if the line has no instruction */
    char **ops;
    int nops;
    uint32_t addr;
    uint32_t size;
    int valid;        /* pass 1 gave the item an address */
} item_t;

typedef struct {
    const char *fname;
    FILE *err;
    int errors;
    int quiet;
    sym_t *syms;
    int nsyms, capsyms;
    item_t *items;
    int nitems, capitems;
    uint8_t *img;
    uint32_t imgsize, imgcap;
} asm_t;

/* ---------------------------------------------------------------- utils */

static void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fprintf(stderr, "tessera: out of memory\n");
        exit(2);
    }
    return q;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = xrealloc(NULL, n);
    memcpy(p, s, n);
    return p;
}

static void aerr(asm_t *A, int line, const char *fmt, ...)
{
    va_list ap;
    if (A->quiet)
        return;
    A->errors++;
    if (!A->err)
        return;
    fprintf(A->err, "%s:%d: error: ", A->fname, line);
    va_start(ap, fmt);
    vfprintf(A->err, fmt, ap);
    va_end(ap);
    fputc('\n', A->err);
}

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

static char *trim(char *s)
{
    char *e;
    while (isspace((unsigned char)*s))
        s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static int is_sym_start(int c) { return isalpha(c) || c == '_' || c == '.'; }
static int is_sym_char(int c) { return isalnum(c) || c == '_' || c == '.'; }

static int is_sym_name(const char *s)
{
    if (!is_sym_start((unsigned char)*s))
        return 0;
    for (s++; *s; s++)
        if (!is_sym_char((unsigned char)*s))
            return 0;
    return 1;
}

/* -------------------------------------------------------------- symbols */

static sym_t *sym_find(asm_t *A, const char *name)
{
    for (int i = 0; i < A->nsyms; i++)
        if (strcmp(A->syms[i].name, name) == 0)
            return &A->syms[i];
    return NULL;
}

static void sym_define(asm_t *A, int line, const char *name, int64_t value)
{
    if (sym_find(A, name)) {
        aerr(A, line, "symbol '%s' is already defined", name);
        return;
    }
    if (A->nsyms == A->capsyms) {
        A->capsyms = A->capsyms ? A->capsyms * 2 : 64;
        A->syms = xrealloc(A->syms, (size_t)A->capsyms * sizeof *A->syms);
    }
    A->syms[A->nsyms].name = xstrdup(name);
    A->syms[A->nsyms].value = value;
    A->nsyms++;
}

/* --------------------------------------------------------- expressions */

static int hexval(int c)
{
    if (isdigit(c))
        return c - '0';
    return tolower(c) - 'a' + 10;
}

/* Reads one character of a literal. Returns -1 for a bad escape. */
static int read_char(const char **pp)
{
    const char *p = *pp;
    int ch;
    if (*p == 0)
        return -1;
    if (*p != '\\') {
        ch = (unsigned char)*p++;
        *pp = p;
        return ch;
    }
    p++;
    switch (*p) {
    case 'n': ch = '\n'; p++; break;
    case 't': ch = '\t'; p++; break;
    case 'r': ch = '\r'; p++; break;
    case '0': ch = 0; p++; break;
    case '\\': ch = '\\'; p++; break;
    case '\'': ch = '\''; p++; break;
    case '"': ch = '"'; p++; break;
    case 'x': {
        int v = 0, n = 0;
        p++;
        while (n < 2 && isxdigit((unsigned char)*p)) {
            v = v * 16 + hexval((unsigned char)*p);
            p++;
            n++;
        }
        if (n == 0)
            return -1;
        ch = v;
        break;
    }
    default:
        return -1;
    }
    *pp = p;
    return ch;
}

/*
 * Evaluates "term {(+|-) term}". A term is a number (decimal, 0x hex,
 * 0b binary), a character literal or a symbol. In pass 1, an unknown
 * symbol has the value 0 and sets *unres. In pass 2, it is an error.
 */
static int eval(asm_t *A, int line, const char *s, int pass, int64_t *out,
                int *unres)
{
    const char *p = s;
    int64_t total = 0;
    int sign = 1;

    if (unres)
        *unres = 0;
    for (;;) {
        int64_t term = 0;
        while (isspace((unsigned char)*p))
            p++;
        while (*p == '+' || *p == '-') {
            if (*p == '-')
                sign = -sign;
            p++;
            while (isspace((unsigned char)*p))
                p++;
        }
        if (isdigit((unsigned char)*p)) {
            int base = 10, nd = 0;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                base = 16;
                p += 2;
            } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
                base = 2;
                p += 2;
            }
            for (;;) {
                int c = (unsigned char)*p, dv;
                if (c == '_') {
                    p++;
                    continue;
                }
                if (isdigit(c))
                    dv = c - '0';
                else if (isalpha(c))
                    dv = tolower(c) - 'a' + 10;
                else
                    break;
                if (dv >= base) {
                    aerr(A, line, "bad digit in number '%s'", s);
                    return 0;
                }
                term = term * base + dv;
                if (term > 0x1FFFFFFFFLL) {
                    aerr(A, line, "number is too large: '%s'", s);
                    return 0;
                }
                p++;
                nd++;
            }
            if (nd == 0) {
                aerr(A, line, "bad number '%s'", s);
                return 0;
            }
        } else if (*p == '\'') {
            int ch;
            p++;
            ch = read_char(&p);
            if (ch < 0 || *p != '\'') {
                aerr(A, line, "bad character literal in '%s'", s);
                return 0;
            }
            p++;
            term = ch;
        } else if (is_sym_start((unsigned char)*p)) {
            char name[128];
            int n = 0;
            sym_t *sy;
            while (is_sym_char((unsigned char)*p)) {
                if (n < (int)sizeof name - 1)
                    name[n++] = *p;
                p++;
            }
            name[n] = 0;
            sy = sym_find(A, name);
            if (sy) {
                term = sy->value;
            } else if (pass == 1) {
                if (unres)
                    *unres = 1;
            } else {
                aerr(A, line, "undefined symbol '%s'", name);
                return 0;
            }
        } else {
            aerr(A, line, "expected a value, got '%s'", s);
            return 0;
        }
        total += sign * term;
        while (isspace((unsigned char)*p))
            p++;
        if (*p == 0)
            break;
        if (*p == '+') {
            sign = 1;
        } else if (*p == '-') {
            sign = -1;
        } else {
            aerr(A, line, "bad value '%s'", s);
            return 0;
        }
        p++;
    }
    *out = total;
    return 1;
}

static int decode_string(asm_t *A, int line, const char *s, uint8_t **out,
                         uint32_t *len)
{
    const char *p = s;
    uint8_t *buf;
    uint32_t n = 0;

    while (isspace((unsigned char)*p))
        p++;
    if (*p != '"') {
        aerr(A, line, "expected a string in double quotes, got '%s'", s);
        return 0;
    }
    p++;
    buf = xrealloc(NULL, strlen(p) + 1);
    while (*p && *p != '"') {
        int ch = read_char(&p);
        if (ch < 0) {
            aerr(A, line, "bad escape in string");
            free(buf);
            return 0;
        }
        buf[n++] = (uint8_t)ch;
    }
    if (*p != '"') {
        aerr(A, line, "string has no closing quote");
        free(buf);
        return 0;
    }
    p++;
    while (isspace((unsigned char)*p))
        p++;
    if (*p) {
        aerr(A, line, "unexpected text after the string: '%s'", p);
        free(buf);
        return 0;
    }
    *out = buf;
    *len = n;
    return 1;
}

/* ----------------------------------------------------------- parsing */

static void strip_comment(char *s)
{
    char quote = 0;
    for (; *s; s++) {
        if (quote) {
            if (*s == '\\' && s[1])
                s++;
            else if (*s == quote)
                quote = 0;
        } else if (*s == '\'' || *s == '"') {
            quote = *s;
        } else if (*s == ';' || *s == '#') {
            *s = 0;
            return;
        }
    }
}

static void push_str(char ***arr, int *n, const char *s)
{
    *arr = xrealloc(*arr, (size_t)(*n + 1) * sizeof **arr);
    (*arr)[(*n)++] = xstrdup(s);
}

/* Splits operands at commas that are not in quotes or parentheses. */
static void split_ops(item_t *it, char *s)
{
    char quote = 0;
    int depth = 0;
    char *start = s, *p;

    if (*s == 0)
        return;
    for (p = s;; p++) {
        char c = *p;
        if (quote && c) {
            if (c == '\\' && p[1])
                p++;
            else if (c == quote)
                quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '(')
            depth++;
        else if (c == ')')
            depth--;
        if (c == 0 || (c == ',' && depth == 0)) {
            *p = 0;
            push_str(&it->ops, &it->nops, trim(start));
            start = p + 1;
            if (c == 0)
                break;
        }
    }
}

static void parse_line(asm_t *A, int line, const char *text)
{
    item_t it;
    char *work, *s;

    memset(&it, 0, sizeof it);
    it.line = line;
    it.src = xstrdup(text);
    work = xstrdup(text);
    strip_comment(work);
    s = trim(work);

    /* Labels: "name:" */
    for (;;) {
        char *q = s, *after;
        if (!is_sym_start((unsigned char)*q))
            break;
        while (is_sym_char((unsigned char)*q))
            q++;
        after = q;
        while (isspace((unsigned char)*after))
            after++;
        if (*after != ':')
            break;
        *q = 0;
        push_str(&it.labels, &it.nlabels, s);
        s = trim(after + 1);
    }

    if (*s) {
        char *q = s;
        while (*q && !isspace((unsigned char)*q))
            q++;
        if (*q)
            *q++ = 0;
        it.mnem = xstrdup(s);
        split_ops(&it, trim(q));
    }
    free(work);

    if (A->nitems == A->capitems) {
        A->capitems = A->capitems ? A->capitems * 2 : 256;
        A->items = xrealloc(A->items, (size_t)A->capitems * sizeof *A->items);
    }
    A->items[A->nitems++] = it;
}

static void parse_source(asm_t *A, const char *src)
{
    const char *p = src;
    int line = 0;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        char *text = xrealloc(NULL, n + 1);
        memcpy(text, p, n);
        text[n] = 0;
        if (n > 0 && text[n - 1] == '\r')
            text[n - 1] = 0;
        line++;
        parse_line(A, line, text);
        free(text);
        p += n;
        if (*p == '\n')
            p++;
    }
}

/* ------------------------------------------------------------- pass 1 */

static int is_pseudo(const char *m)
{
    return ieq(m, "LI") || ieq(m, "MV") || ieq(m, "NOP") || ieq(m, "J") ||
           ieq(m, "RET");
}

static void pass1(asm_t *A)
{
    uint32_t addr = 0;
    char **pending = NULL;
    int npending = 0, cappending = 0;

    for (int i = 0; i < A->nitems; i++) {
        item_t *it = &A->items[i];
        const char *m = it->mnem;
        uint32_t align = 1, size = 0;

        for (int j = 0; j < it->nlabels; j++) {
            if (npending == cappending) {
                cappending = cappending ? cappending * 2 : 8;
                pending = xrealloc(pending, (size_t)cappending * sizeof *pending);
            }
            pending[npending++] = it->labels[j];
        }
        if (!m)
            continue;

        if (m[0] == '.') {
            if (ieq(m, ".equ")) {
                int64_t v;
                int un;
                if (it->nops != 2 || !is_sym_name(it->ops[0])) {
                    aerr(A, it->line, "use: .equ NAME, value");
                } else if (eval(A, it->line, it->ops[1], 1, &v, &un)) {
                    if (un)
                        aerr(A, it->line, "the value of .equ must use only symbols that are defined before it");
                    else
                        sym_define(A, it->line, it->ops[0], v);
                }
                continue;
            } else if (ieq(m, ".word")) {
                if (it->nops == 0) {
                    aerr(A, it->line, ".word needs at least one value");
                    continue;
                }
                align = 4;
                size = 4u * (uint32_t)it->nops;
            } else if (ieq(m, ".byte")) {
                if (it->nops == 0) {
                    aerr(A, it->line, ".byte needs at least one value");
                    continue;
                }
                size = (uint32_t)it->nops;
            } else if (ieq(m, ".ascii")) {
                int ok = it->nops > 0;
                if (!ok)
                    aerr(A, it->line, ".ascii needs a string");
                for (int j = 0; ok && j < it->nops; j++) {
                    uint8_t *b;
                    uint32_t n;
                    if (decode_string(A, it->line, it->ops[j], &b, &n)) {
                        size += n;
                        free(b);
                    } else {
                        ok = 0;
                    }
                }
                if (!ok)
                    continue;
            } else if (ieq(m, ".space")) {
                int64_t v;
                int un;
                if (it->nops != 1) {
                    aerr(A, it->line, "use: .space count");
                    continue;
                }
                if (!eval(A, it->line, it->ops[0], 1, &v, &un))
                    continue;
                if (un || v < 0 || v > (int64_t)MAX_IMAGE) {
                    aerr(A, it->line, ".space count must be a known value from 0 to %u", MAX_IMAGE);
                    continue;
                }
                size = (uint32_t)v;
            } else if (ieq(m, ".align")) {
                int64_t v;
                int un;
                if (it->nops != 1) {
                    aerr(A, it->line, "use: .align bytes");
                    continue;
                }
                if (!eval(A, it->line, it->ops[0], 1, &v, &un))
                    continue;
                if (un || v < 1 || v > 4096 || (v & (v - 1)) != 0) {
                    aerr(A, it->line, ".align needs a power of 2 from 1 to 4096");
                    continue;
                }
                align = (uint32_t)v;
            } else {
                aerr(A, it->line, "unknown directive '%s'", m);
                continue;
            }
        } else {
            align = 4;
            if (ieq(m, "LI")) {
                size = 8;
                if (it->nops == 2) {
                    int64_t v;
                    int un, ok;
                    A->quiet = 1;
                    ok = eval(A, it->line, it->ops[1], 1, &v, &un);
                    A->quiet = 0;
                    if (ok && !un && v >= -131072 && v <= 131071)
                        size = 4;
                }
            } else if (is_pseudo(m) || isa_find(m)) {
                size = 4;
            } else {
                aerr(A, it->line, "unknown instruction '%s'", m);
                continue;
            }
        }

        addr = (addr + align - 1u) & ~(align - 1u);
        for (int j = 0; j < npending; j++)
            sym_define(A, it->line, pending[j], addr);
        npending = 0;
        it->addr = addr;
        it->size = size;
        it->valid = 1;
        if ((uint64_t)addr + size > MAX_IMAGE) {
            aerr(A, it->line, "the program is larger than %u bytes", MAX_IMAGE);
            break;
        }
        addr += size;
    }
    for (int j = 0; j < npending; j++)
        sym_define(A, A->nitems ? A->items[A->nitems - 1].line : 0,
                   pending[j], addr);
    free(pending);
}

/* ------------------------------------------------------------- pass 2 */

static void ensure(asm_t *A, uint32_t end)
{
    if (end > A->imgcap) {
        uint32_t cap = A->imgcap ? A->imgcap : 1024;
        while (cap < end)
            cap *= 2;
        A->img = xrealloc(A->img, cap);
        memset(A->img + A->imgcap, 0, cap - A->imgcap);
        A->imgcap = cap;
    }
    if (end > A->imgsize)
        A->imgsize = end;
}

static void emit_bytes(asm_t *A, uint32_t addr, const uint8_t *b, uint32_t n)
{
    ensure(A, addr + n);
    memcpy(A->img + addr, b, n);
}

static void emit_word(asm_t *A, uint32_t addr, uint32_t w)
{
    uint8_t b[4];
    b[0] = (uint8_t)w;
    b[1] = (uint8_t)(w >> 8);
    b[2] = (uint8_t)(w >> 16);
    b[3] = (uint8_t)(w >> 24);
    emit_bytes(A, addr, b, 4);
}

static int parse_reg(const char *s, char kind)
{
    int v = 0, n = 0;
    while (isspace((unsigned char)*s))
        s++;
    if (tolower((unsigned char)*s) != kind)
        return -1;
    s++;
    if (!isdigit((unsigned char)*s))
        return -1;
    while (isdigit((unsigned char)*s)) {
        v = v * 10 + (*s - '0');
        s++;
        if (++n > 2)
            return -1;
    }
    while (isspace((unsigned char)*s))
        s++;
    if (*s || v > 15)
        return -1;
    return v;
}

static int need_reg(asm_t *A, int line, const char *s, char kind, int *out)
{
    int r = parse_reg(s, kind);
    if (r < 0) {
        aerr(A, line, "expected %s, got '%s'",
             kind == 'r' ? "an integer register (r0-r15)"
                         : "a capability register (c0-c15)", s);
        return 0;
    }
    *out = r;
    return 1;
}

static int need_range(asm_t *A, int line, int64_t v, int64_t lo, int64_t hi,
                      const char *what)
{
    if (v < lo || v > hi) {
        aerr(A, line, "%s %lld is out of range (%lld to %lld)", what,
             (long long)v, (long long)lo, (long long)hi);
        return 0;
    }
    return 1;
}

static int encode(asm_t *A, int line, const insn_def_t *d, char **ops,
                  int nops, uint32_t pc, uint32_t *out)
{
    uint32_t w = (uint32_t)d->opcode << 26;

    if (nops != d->nops) {
        aerr(A, line, "%s needs %d operand%s, not %d", d->name, d->nops,
             d->nops == 1 ? "" : "s", nops);
        return 0;
    }
    if (d->fmt == FMT_R)
        w |= d->func;

    for (int i = 0; i < nops; i++) {
        const char *s = ops[i];
        int r;
        int64_t v;
        switch (d->ops[i]) {
        case OPK_RA:
            if (!need_reg(A, line, s, 'r', &r)) return 0;
            w |= (uint32_t)r << 22;
            break;
        case OPK_RB:
            if (!need_reg(A, line, s, 'r', &r)) return 0;
            w |= (uint32_t)r << 18;
            break;
        case OPK_RC:
            if (!need_reg(A, line, s, 'r', &r)) return 0;
            w |= (uint32_t)r << 14;
            break;
        case OPK_CA:
            if (!need_reg(A, line, s, 'c', &r)) return 0;
            w |= (uint32_t)r << 22;
            break;
        case OPK_CB:
            if (!need_reg(A, line, s, 'c', &r)) return 0;
            w |= (uint32_t)r << 18;
            break;
        case OPK_CC:
            if (!need_reg(A, line, s, 'c', &r)) return 0;
            w |= (uint32_t)r << 14;
            break;
        case OPK_IMM18:
            if (!eval(A, line, s, 2, &v, NULL) ||
                !need_range(A, line, v, -131072, 131071, "immediate"))
                return 0;
            w |= (uint32_t)v & 0x3FFFFu;
            break;
        case OPK_UIMM18:
            if (!eval(A, line, s, 2, &v, NULL) ||
                !need_range(A, line, v, 0, 0x3FFFF, "immediate"))
                return 0;
            w |= (uint32_t)v;
            break;
        case OPK_SHAMT:
            if (!eval(A, line, s, 2, &v, NULL) ||
                !need_range(A, line, v, 0, 31, "shift amount"))
                return 0;
            w |= (uint32_t)v | ((uint32_t)d->func << 5);
            break;
        case OPK_UIMM22:
            if (!eval(A, line, s, 2, &v, NULL) ||
                !need_range(A, line, v, 0, 0x3FFFFF, "immediate"))
                return 0;
            w |= (uint32_t)v;
            break;
        case OPK_BR18:
        case OPK_BR22: {
            int64_t off;
            if (!eval(A, line, s, 2, &v, NULL))
                return 0;
            if (v & 3) {
                aerr(A, line, "target 0x%llx is not aligned to 4 bytes",
                     (unsigned long long)v);
                return 0;
            }
            off = (v - (int64_t)pc) / 4;
            if (d->ops[i] == OPK_BR18) {
                if (!need_range(A, line, off, -131072, 131071, "branch offset"))
                    return 0;
                w |= (uint32_t)off & 0x3FFFFu;
            } else {
                if (!need_range(A, line, off, -2097152, 2097151, "jump offset"))
                    return 0;
                w |= (uint32_t)off & 0x3FFFFFu;
            }
            break;
        }
        case OPK_MEM: {
            char buf[256];
            char *t, *lp, *e;
            size_t n;
            snprintf(buf, sizeof buf, "%s", s);
            t = trim(buf);
            n = strlen(t);
            lp = strrchr(t, '(');
            if (n == 0 || t[n - 1] != ')' || !lp) {
                aerr(A, line, "expected offset(cN), got '%s'", s);
                return 0;
            }
            t[n - 1] = 0;
            *lp = 0;
            if (!need_reg(A, line, lp + 1, 'c', &r))
                return 0;
            e = trim(t);
            v = 0;
            if (*e && !eval(A, line, e, 2, &v, NULL))
                return 0;
            if (!need_range(A, line, v, -131072, 131071, "offset"))
                return 0;
            w |= ((uint32_t)r << 18) | ((uint32_t)v & 0x3FFFFu);
            break;
        }
        case OPK_NC:
            if (!eval(A, line, s, 2, &v, NULL) ||
                !need_range(A, line, v, 0, 15, "number"))
                return 0;
            w |= (uint32_t)v << 14;
            break;
        case OPK_CSR: {
            char buf[64];
            int n;
            snprintf(buf, sizeof buf, "%s", s);
            n = csr_lookup(trim(buf));
            if (n >= 0)
                v = n;
            else if (!eval(A, line, s, 2, &v, NULL))
                return 0;
            if (!need_range(A, line, v, 0, 15, "CSR number"))
                return 0;
            w |= (uint32_t)v << 14;
            break;
        }
        case OPK_MBOX:
            if (!eval(A, line, s, 2, &v, NULL) ||
                !need_range(A, line, v, 0, 15, "mailbox"))
                return 0;
            w |= (uint32_t)v << 4;
            break;
        case OPK_NONE:
            break;
        }
    }
    *out = w;
    return 1;
}

static int pseudo_nops(asm_t *A, const item_t *it, int want)
{
    if (it->nops != want) {
        aerr(A, it->line, "%s needs %d operand%s, not %d", it->mnem, want,
             want == 1 ? "" : "s", it->nops);
        return 0;
    }
    return 1;
}

/* Encodes an instruction or pseudo-instruction. Returns the word count. */
static int encode_item(asm_t *A, item_t *it, uint32_t words[2])
{
    const char *m = it->mnem;
    char *ops[3];
    int r;
    int64_t v;

    if (ieq(m, "LI")) {
        if (!pseudo_nops(A, it, 2) || !need_reg(A, it->line, it->ops[0], 'r', &r))
            return 0;
        if (!eval(A, it->line, it->ops[1], 2, &v, NULL) ||
            !need_range(A, it->line, v, -2147483648LL, 4294967295LL, "value"))
            return 0;
        if (it->size == 4) {
            words[0] = ((uint32_t)OP_ADDI << 26) | ((uint32_t)r << 22) |
                       ((uint32_t)v & 0x3FFFFu);
            return 1;
        }
        words[0] = ((uint32_t)OP_LUI << 26) | ((uint32_t)r << 22) |
                   (((uint32_t)v >> 10) & 0x3FFFFFu);
        words[1] = ((uint32_t)OP_ORI << 26) | ((uint32_t)r << 22) |
                   ((uint32_t)r << 18) | ((uint32_t)v & 0x3FFu);
        return 2;
    }
    if (ieq(m, "MV")) {
        if (!pseudo_nops(A, it, 2))
            return 0;
        ops[0] = it->ops[0];
        ops[1] = it->ops[1];
        ops[2] = "0";
        return encode(A, it->line, isa_find("ADDI"), ops, 3, it->addr, words);
    }
    if (ieq(m, "NOP")) {
        if (!pseudo_nops(A, it, 0))
            return 0;
        ops[0] = "r0";
        ops[1] = "r0";
        ops[2] = "0";
        return encode(A, it->line, isa_find("ADDI"), ops, 3, it->addr, words);
    }
    if (ieq(m, "J")) {
        if (!pseudo_nops(A, it, 1))
            return 0;
        ops[0] = "c0";
        ops[1] = it->ops[0];
        return encode(A, it->line, isa_find("JAL"), ops, 2, it->addr, words);
    }
    if (ieq(m, "RET")) {
        if (!pseudo_nops(A, it, 0))
            return 0;
        ops[0] = "c0";
        ops[1] = "c1";
        ops[2] = "0";
        return encode(A, it->line, isa_find("JALR"), ops, 3, it->addr, words);
    }
    return encode(A, it->line, isa_find(m), it->ops, it->nops, it->addr, words);
}

static void list_plain(FILE *lst, const char *src)
{
    if (lst)
        fprintf(lst, "%22s%s\n", "", src);
}

static void list_words(FILE *lst, uint32_t addr, const uint32_t *w, int n,
                       const char *src)
{
    if (!lst)
        return;
    for (int i = 0; i < n; i++)
        fprintf(lst, "  0x%04x  0x%08x  %s\n", addr + 4u * (uint32_t)i, w[i],
                i == 0 ? src : "");
}

static void list_bytes(FILE *lst, uint32_t addr, uint32_t n, const char *src)
{
    char buf[32];
    if (!lst)
        return;
    snprintf(buf, sizeof buf, "(%u bytes)", n);
    fprintf(lst, "  0x%04x  %-10s  %s\n", addr, buf, src);
}

static void pass2(asm_t *A, FILE *lst)
{
    for (int i = 0; i < A->nitems; i++) {
        item_t *it = &A->items[i];
        const char *m = it->mnem;

        if (!m || !it->valid) {
            list_plain(lst, it->src);
            continue;
        }
        if (ieq(m, ".word")) {
            uint32_t *w = xrealloc(NULL, (size_t)it->nops * sizeof *w);
            int ok = 1;
            for (int j = 0; j < it->nops; j++) {
                int64_t v = 0;
                if (!eval(A, it->line, it->ops[j], 2, &v, NULL) ||
                    !need_range(A, it->line, v, -2147483648LL, 4294967295LL, "value"))
                    ok = 0;
                w[j] = (uint32_t)v;
                emit_word(A, it->addr + 4u * (uint32_t)j, w[j]);
            }
            if (ok)
                list_words(lst, it->addr, w, it->nops, it->src);
            free(w);
        } else if (ieq(m, ".byte")) {
            for (int j = 0; j < it->nops; j++) {
                int64_t v = 0;
                uint8_t b;
                if (eval(A, it->line, it->ops[j], 2, &v, NULL))
                    need_range(A, it->line, v, -128, 255, "byte");
                b = (uint8_t)v;
                emit_bytes(A, it->addr + (uint32_t)j, &b, 1);
            }
            list_bytes(lst, it->addr, it->size, it->src);
        } else if (ieq(m, ".ascii")) {
            uint32_t at = it->addr;
            for (int j = 0; j < it->nops; j++) {
                uint8_t *b;
                uint32_t n;
                if (decode_string(A, it->line, it->ops[j], &b, &n)) {
                    if (n)
                        emit_bytes(A, at, b, n);
                    at += n;
                    free(b);
                }
            }
            list_bytes(lst, it->addr, it->size, it->src);
        } else if (ieq(m, ".space")) {
            ensure(A, it->addr + it->size);
            list_bytes(lst, it->addr, it->size, it->src);
        } else if (m[0] == '.') {
            list_plain(lst, it->src);  /* .equ and .align */
        } else {
            uint32_t words[2];
            int n = encode_item(A, it, words);
            for (int j = 0; j < n; j++)
                emit_word(A, it->addr + 4u * (uint32_t)j, words[j]);
            if (n)
                list_words(lst, it->addr, words, n, it->src);
            else
                list_plain(lst, it->src);
        }
    }
}

/* -------------------------------------------------------------- entry */

static void free_asm(asm_t *A)
{
    for (int i = 0; i < A->nsyms; i++)
        free(A->syms[i].name);
    free(A->syms);
    for (int i = 0; i < A->nitems; i++) {
        item_t *it = &A->items[i];
        for (int j = 0; j < it->nlabels; j++)
            free(it->labels[j]);
        for (int j = 0; j < it->nops; j++)
            free(it->ops[j]);
        free(it->labels);
        free(it->ops);
        free(it->mnem);
        free(it->src);
    }
    free(A->items);
}

int asm_assemble(const char *src, const char *fname, uint8_t **image,
                 uint32_t *size, FILE *listing, FILE *err)
{
    asm_t A;
    memset(&A, 0, sizeof A);
    A.fname = fname ? fname : "<input>";
    A.err = err;

    parse_source(&A, src);
    pass1(&A);
    /* Pass 2 also runs after errors, so that all errors are reported. */
    pass2(&A, A.errors ? NULL : listing);

    if (A.errors) {
        free(A.img);
        *image = NULL;
        *size = 0;
    } else {
        if (!A.img)
            A.img = xrealloc(NULL, 1);
        *image = A.img;
        *size = A.imgsize;
    }
    free_asm(&A);
    return A.errors;
}
