/* ttest.c - test runner for Tessera programs.
 *
 *   ttest file.s [file.s ...]
 *
 * Each test is an assembly file with expectations in comments:
 *
 *   ; expect halt                    the program must stop at HALT (default)
 *   ; expect fault BOUNDS at 0x14    fault code, and optional pc
 *   ; expect unsupported at 0x00     a later-milestone feature, optional pc
 *   ; expect r3 = -10                register value (decimal or 0x hex)
 *   ; expect word 0x08 = 0x1C82885A  word in the assembled image
 *   ; expect output "Hi!\n"          console output (lines are joined)
 *   ; expect limit 5000              step limit (default 1000000)
 */
#include "asm.h"
#include "core.h"
#include "isa.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHECKS 64
#define MAX_OUT 8192

typedef struct {
    int want_state;          /* -1: default (HALTED) */
    int want_fault;
    int has_pc;
    uint32_t want_pc;
    int reg[MAX_CHECKS];
    uint32_t reg_val[MAX_CHECKS];
    int nregs;
    uint32_t word_addr[MAX_CHECKS], word_val[MAX_CHECKS];
    int nwords;
    char out[MAX_OUT];
    size_t outlen;
    int has_out;
    unsigned long long limit;
} expect_t;

typedef struct {
    char buf[MAX_OUT];
    size_t len;
} capture_t;

static void capture_console(void *ctx, const uint8_t *b, uint32_t n)
{
    capture_t *c = ctx;
    for (uint32_t i = 0; i < n && c->len < sizeof c->buf; i++)
        c->buf[c->len++] = (char)b[i];
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

static int word_is(const char *p, const char *w)
{
    size_t n = strlen(w);
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)p[i]) != w[i])
            return 0;
    return !isalnum((unsigned char)p[n]) && p[n] != '_';
}

static int parse_num(const char **pp, long long *out)
{
    const char *p = skip_ws(*pp);
    int neg = 0, base = 10, nd = 0;
    long long v = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }
    for (;;) {
        int c = (unsigned char)*p, dv;
        if (isdigit(c))
            dv = c - '0';
        else if (base == 16 && isxdigit(c))
            dv = tolower(c) - 'a' + 10;
        else if (c == '_') {
            p++;
            continue;
        } else
            break;
        v = v * base + dv;
        p++;
        nd++;
    }
    if (nd == 0)
        return 0;
    *out = neg ? -v : v;
    *pp = p;
    return 1;
}

static int parse_string(const char **pp, char *out, size_t *len, size_t cap)
{
    const char *p = skip_ws(*pp);
    if (*p != '"')
        return 0;
    p++;
    while (*p && *p != '"') {
        int ch = (unsigned char)*p++;
        if (ch == '\\') {
            switch (*p++) {
            case 'n': ch = '\n'; break;
            case 't': ch = '\t'; break;
            case 'r': ch = '\r'; break;
            case '0': ch = 0; break;
            case '\\': ch = '\\'; break;
            case '"': ch = '"'; break;
            default: return 0;
            }
        }
        if (*len < cap)
            out[(*len)++] = (char)ch;
    }
    if (*p != '"')
        return 0;
    *pp = p + 1;
    return 1;
}

static int parse_expect_line(const char *p, expect_t *e)
{
    long long v, addr;
    p = skip_ws(p);
    if (word_is(p, "halt")) {
        e->want_state = CORE_HALTED;
        return 1;
    }
    if (word_is(p, "fault") || word_is(p, "unsupported")) {
        if (word_is(p, "fault")) {
            char name[32];
            int n = 0;
            p = skip_ws(p + 5);
            while (isalpha((unsigned char)*p) && n < 31)
                name[n++] = *p++;
            name[n] = 0;
            e->want_fault = fault_lookup(name);
            if (e->want_fault < 0)
                return 0;
            e->want_state = CORE_FAULTED;
        } else {
            p += 11;
            e->want_state = CORE_UNSUPPORTED;
        }
        p = skip_ws(p);
        if (word_is(p, "at")) {
            p += 2;
            if (!parse_num(&p, &v))
                return 0;
            e->has_pc = 1;
            e->want_pc = (uint32_t)v;
        }
        return 1;
    }
    if (word_is(p, "output")) {
        p += 6;
        e->has_out = 1;
        return parse_string(&p, e->out, &e->outlen, sizeof e->out);
    }
    if (word_is(p, "limit")) {
        p += 5;
        if (!parse_num(&p, &v) || v <= 0)
            return 0;
        e->limit = (unsigned long long)v;
        return 1;
    }
    if (word_is(p, "word")) {
        p += 4;
        if (!parse_num(&p, &addr))
            return 0;
        p = skip_ws(p);
        if (*p++ != '=' || !parse_num(&p, &v) || e->nwords >= MAX_CHECKS)
            return 0;
        e->word_addr[e->nwords] = (uint32_t)addr;
        e->word_val[e->nwords] = (uint32_t)v;
        e->nwords++;
        return 1;
    }
    if ((*p == 'r' || *p == 'R') && isdigit((unsigned char)p[1])) {
        int r = atoi(p + 1);
        p++;
        while (isdigit((unsigned char)*p))
            p++;
        p = skip_ws(p);
        if (r > 15 || *p++ != '=' || !parse_num(&p, &v) || e->nregs >= MAX_CHECKS)
            return 0;
        e->reg[e->nregs] = r;
        e->reg_val[e->nregs] = (uint32_t)v;
        e->nregs++;
        return 1;
    }
    return 0;
}

static int parse_expectations(const char *path, const char *src, expect_t *e)
{
    const char *p = src;
    int line = 0;
    while (*p) {
        const char *q = skip_ws(p);
        line++;
        if (*q == ';') {
            q = skip_ws(q + 1);
            if (word_is(q, "expect")) {
                char buf[512];
                size_t n = strcspn(q + 6, "\r\n");
                if (n >= sizeof buf)
                    n = sizeof buf - 1;
                memcpy(buf, q + 6, n);
                buf[n] = 0;
                if (!parse_expect_line(buf, e)) {
                    printf("FAIL %s:%d: bad expectation:%s\n", path, line, buf);
                    return 0;
                }
            }
        }
        p = strchr(p, '\n');
        if (!p)
            break;
        p++;
    }
    return 1;
}

static void print_escaped(const char *s, size_t n)
{
    putchar('"');
    for (size_t i = 0; i < n && i < 200; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n')
            fputs("\\n", stdout);
        else if (c == '"' || c == '\\')
            printf("\\%c", c);
        else if (c < 32 || c > 126)
            printf("\\x%02x", c);
        else
            putchar(c);
    }
    putchar('"');
}

static int run_test(const char *path)
{
    expect_t e;
    capture_t cap;
    core_t k;
    char *src;
    uint8_t *img;
    uint32_t size;
    int want, ok = 1;

    src = read_file(path, NULL);
    if (!src) {
        printf("FAIL %s: cannot read the file\n", path);
        return 0;
    }
    memset(&e, 0, sizeof e);
    e.want_state = -1;
    e.limit = 1000000ULL;
    if (!parse_expectations(path, src, &e)) {
        free(src);
        return 0;
    }
    if (asm_assemble(src, path, &img, &size, NULL, stdout) != 0) {
        printf("FAIL %s: assembler errors\n", path);
        free(src);
        return 0;
    }
    free(src);

    if (core_init(&k, LOCAL_SIZE_DEFAULT) != 0 || core_load(&k, img, size) != 0) {
        printf("FAIL %s: cannot load the program\n", path);
        free(img);
        return 0;
    }
    memset(&cap, 0, sizeof cap);
    k.console = capture_console;
    k.console_ctx = &cap;
    core_boot(&k);
    core_run(&k, e.limit);

    want = e.want_state < 0 ? CORE_HALTED : e.want_state;
    if ((int)k.state != want) {
        printf("FAIL %s: state is %s", path, core_state_name(k.state));
        if (k.state == CORE_FAULTED)
            printf(" (%s at 0x%04x)", fault_name(k.fault_code), k.fault_pc);
        printf(", expected %s\n", core_state_name((core_state_t)want));
        ok = 0;
    } else if (want == CORE_FAULTED && k.fault_code != e.want_fault) {
        printf("FAIL %s: fault is %s, expected %s\n", path,
               fault_name(k.fault_code), fault_name(e.want_fault));
        ok = 0;
    } else if (e.has_pc && k.fault_pc != e.want_pc) {
        printf("FAIL %s: stopped at pc 0x%04x, expected 0x%04x\n", path,
               k.fault_pc, e.want_pc);
        ok = 0;
    }
    for (int i = 0; i < e.nregs; i++) {
        if (k.r[e.reg[i]] != e.reg_val[i]) {
            printf("FAIL %s: r%d is 0x%08x, expected 0x%08x\n", path, e.reg[i],
                   k.r[e.reg[i]], e.reg_val[i]);
            ok = 0;
        }
    }
    for (int i = 0; i < e.nwords; i++) {
        uint32_t a = e.word_addr[i], w = 0;
        if (a + 4 <= size)
            w = (uint32_t)img[a] | ((uint32_t)img[a + 1] << 8) |
                ((uint32_t)img[a + 2] << 16) | ((uint32_t)img[a + 3] << 24);
        if (a + 4 > size || w != e.word_val[i]) {
            printf("FAIL %s: word at 0x%04x is 0x%08x, expected 0x%08x\n", path,
                   a, w, e.word_val[i]);
            ok = 0;
        }
    }
    if (e.has_out && (cap.len != e.outlen || memcmp(cap.buf, e.out, cap.len) != 0)) {
        printf("FAIL %s: output is ", path);
        print_escaped(cap.buf, cap.len);
        printf(", expected ");
        print_escaped(e.out, e.outlen);
        putchar('\n');
        ok = 0;
    }
    if (ok)
        printf("PASS %s\n", path);
    free(img);
    core_free(&k);
    return ok;
}

int main(int argc, char **argv)
{
    int pass = 0, fail = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: ttest file.s [file.s ...]\n");
        return 2;
    }
    for (int i = 1; i < argc; i++) {
        if (run_test(argv[i]))
            pass++;
        else
            fail++;
    }
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
