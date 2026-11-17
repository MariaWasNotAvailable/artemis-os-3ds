#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

/*======================================================================
 * GW-BASIC style interpreter
 *
 * A line-numbered BASIC interpreter with a Windows-95-terminal look
 * (bright text on a black background).  Stored as a regular Artemis
 * app so it can be launched from the Start menu / desktop like any
 * other window.
 *
 * Statements: REM, LET/assignment, PRINT, INPUT, CLS, COLOR, LOCATE,
 *             IF...THEN, GOTO, GOSUB/RETURN, FOR...NEXT, END, STOP
 * Direct commands: RUN, LIST, NEW, CLS, BYE
 * Functions: RND, INT, ABS, SGN, SQR, LEN, STR$, VAL, LEFT$,
 *            RIGHT$, MID$, ASC, CHR$
 *====================================================================*/

#define BASIC_MAX_LINES   512
#define BASIC_MAX_LINELEN  256
#define BASIC_NUM_VARS     26   /* A-Z */
#define BASIC_MAX_COLS     64
#define BASIC_MAX_ROWS     34

#define BASIC_CALL_STACK   64
#define BASIC_FOR_STACK    16

/* GW-BASIC palette (color codes 0-15) */
static const Color BASIC_PAL[16] = {
    {  0,   0,   0 },   /* 0 black */
    {  0,   0, 170 },   /* 1 blue */
    {  0, 170,   0 },   /* 2 green */
    {  0, 170, 170 },   /* 3 cyan */
    { 170,   0,   0 },   /* 4 red */
    { 170,   0, 170 },   /* 5 magenta */
    { 170,  85,   0 },   /* 6 brown */
    { 170, 170, 170 },   /* 7 light gray */
    {  85,  85,  85 },   /* 8 dark gray */
    {  85,  85, 255 },   /* 9 bright blue */
    {  85, 255,  85 },   /* 10 bright green */
    {  85, 255, 255 },   /* 11 bright cyan */
    { 255,  85,  85 },   /* 12 bright red */
    { 255,  85, 255 },   /* 13 bright magenta */
    { 255, 255,  85 },   /* 14 yellow */
    { 255, 255, 255 },   /* 15 bright white */
};

/*---------------------------------------------------------------------
 * Typed value.  Owned strings are linearAlloc'd and freed with
 * val_free().
 *-------------------------------------------------------------------*/
typedef struct {
    int  is_str;
    double num;
    char *str;
} Val;

static Val val_num(double n) { Val v; v.is_str = 0; v.num = n; v.str = NULL; return v; }
static Val val_str(char *s)  { Val v; v.is_str = 1; v.num = 0;  v.str = s;  return v; }
static void val_free(Val *v) { if (v->is_str && v->str) { linearFree(v->str); v->str = NULL; } }
static double val_get_num(Val *v) { return v->is_str ? 0.0 : v->num; }
static const char *val_get_str(Val *v, char *buf, int buflen) {
    if (v->is_str) return v->str;
    snprintf(buf, buflen, "%g", v->num);
    return buf;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)linearAlloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/*---------------------------------------------------------------------
 * Program line storage (sorted by line number)
 *-------------------------------------------------------------------*/
typedef struct {
    int  num;
    char *text;
} BasicLine;

typedef struct {
    /* program */
    BasicLine lines[BASIC_MAX_LINES];
    int  nlines;

    /* variables */
    double num_vars[BASIC_NUM_VARS];
    char  *str_vars[BASIC_NUM_VARS];

    /* screen grid */
    int   cols, rows;
    int   x0, y0;              /* draw origin of grid */
    char  ch[BASIC_MAX_ROWS][BASIC_MAX_COLS];
    uint8_t fg[BASIC_MAX_ROWS][BASIC_MAX_COLS];
    int   cur_x, cur_y;
    int   cur_fg, cur_bg;

    /* direct-mode input line */
    char  inbuf[BASIC_MAX_LINELEN];
    int   inlen;

    /* run state */
    int   running;
    int   pc;
    int   waiting_input;
    int   pc_set;
    int   input_var_idx;
    int   input_is_str;
    char  input_prompt[64];

    int   call_stack[BASIC_CALL_STACK];
    int   call_sp;
    int   for_var[BASIC_FOR_STACK];
    double for_from[BASIC_FOR_STACK], for_to[BASIC_FOR_STACK], for_step[BASIC_FOR_STACK];
    int   for_line[BASIC_FOR_STACK];
    int   for_sp;

    int   dirty;
} BasicState;

static BasicState g_basic_state;

/*---------------------------------------------------------------------
 * helpers
 *-------------------------------------------------------------------*/
static const char *skip_spaces(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_alnum(char c) { return is_digit(c) || is_letter(c) || c == '_'; }
static int var_index(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    return -1;
}

static void basic_cls(BasicState *bs) {
    for (int y = 0; y < bs->rows; y++) {
        for (int x = 0; x < bs->cols; x++) {
            bs->ch[y][x] = ' ';
            bs->fg[y][x] = (uint8_t)bs->cur_fg;
        }
    }
    bs->cur_x = 0; bs->cur_y = 0;
    bs->dirty = 1;
}

static void basic_scroll(BasicState *bs) {
    for (int y = 1; y < bs->rows; y++) {
        memcpy(bs->ch[y-1], bs->ch[y], bs->cols);
        memcpy(bs->fg[y-1], bs->fg[y], bs->cols);
    }
    for (int x = 0; x < bs->cols; x++) {
        bs->ch[bs->rows-1][x] = ' ';
        bs->fg[bs->rows-1][x] = (uint8_t)bs->cur_fg;
    }
}

static void basic_putc(BasicState *bs, char c) {
    if (c == '\n') { bs->cur_x = 0; bs->cur_y++; }
    else if (c == '\r') { bs->cur_x = 0; }
    else if (c == '\b') {
        if (bs->cur_x > 0) { bs->cur_x--; bs->ch[bs->cur_y][bs->cur_x] = ' '; }
        else if (bs->cur_y > 0) { bs->cur_y--; bs->cur_x = bs->cols - 1; bs->ch[bs->cur_y][bs->cur_x] = ' '; }
    } else {
        if (bs->cur_x >= bs->cols) { bs->cur_x = 0; bs->cur_y++; }
        if (bs->cur_y >= bs->rows) { basic_scroll(bs); bs->cur_y = bs->rows - 1; }
        bs->ch[bs->cur_y][bs->cur_x] = c;
        bs->fg[bs->cur_y][bs->cur_x] = (uint8_t)bs->cur_fg;
        bs->cur_x++;
    }
    if (bs->cur_y >= bs->rows) { basic_scroll(bs); bs->cur_y = bs->rows - 1; }
    bs->dirty = 1;
}

static void basic_print_str(BasicState *bs, const char *s) {
    while (*s) basic_putc(bs, *s++);
}

/*---------------------------------------------------------------------
 * line storage
 *-------------------------------------------------------------------*/
static int find_line(BasicState *bs, int num, int *out_idx) {
    int lo = 0, hi = bs->nlines - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (bs->lines[mid].num == num) { *out_idx = mid; return 1; }
        if (bs->lines[mid].num < num) lo = mid + 1;
        else hi = mid - 1;
    }
    *out_idx = lo;
    return 0;
}

static void store_line(BasicState *bs, int num, const char *text) {
    int idx;
    if (find_line(bs, num, &idx)) {
        linearFree(bs->lines[idx].text);
        bs->lines[idx].text = xstrdup(text);
        return;
    }
    if (bs->nlines >= BASIC_MAX_LINES) return;
    for (int i = bs->nlines; i > idx; i--) bs->lines[i] = bs->lines[i-1];
    bs->lines[idx].num = num;
    bs->lines[idx].text = xstrdup(text);
    bs->nlines++;
    bs->dirty = 1;
}

static void delete_line(BasicState *bs, int num) {
    int idx;
    if (!find_line(bs, num, &idx)) return;
    linearFree(bs->lines[idx].text);
    for (int i = idx; i < bs->nlines - 1; i++) bs->lines[i] = bs->lines[i+1];
    bs->nlines--;
    bs->dirty = 1;
}

static void free_str_var(int idx) {
    if (g_basic_state.str_vars[idx]) {
        linearFree(g_basic_state.str_vars[idx]);
        g_basic_state.str_vars[idx] = NULL;
    }
}

/*---------------------------------------------------------------------
 * expression evaluator
 *-------------------------------------------------------------------*/
typedef struct {
    BasicState *bs;
    const char *p;
    int err;
} Exp;

static Val exp_parse(Exp *e);
static Val exp_primary(Exp *e);

static Val exp_func(Exp *e, const char *name) {
    char buf[BASIC_MAX_LINELEN];
    /* e->p points at '(' */
    e->p++;  /* skip '(' */
    e->p = skip_spaces(e->p);

    if (strcmp(name, "RND") == 0) {
        double arg = 1.0;
        if (*e->p != ')') {
            Val a = exp_parse(e); arg = val_get_num(&a); val_free(&a);
        }
        if (*e->p == ')') e->p++;
        double v = (double)rand() / (double)RAND_MAX;
        if (arg > 0) v = v * arg + 1.0;
        return val_num(v);
    }
    if (strcmp(name, "INT") == 0) {
        Val a = exp_parse(e); double v = floor(val_get_num(&a)); val_free(&a);
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    if (strcmp(name, "ABS") == 0) {
        Val a = exp_parse(e); double v = fabs(val_get_num(&a)); val_free(&a);
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    if (strcmp(name, "SGN") == 0) {
        Val a = exp_parse(e); double x = val_get_num(&a); val_free(&a);
        double v = (x > 0) ? 1.0 : ((x < 0) ? -1.0 : 0.0);
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    if (strcmp(name, "SQR") == 0) {
        Val a = exp_parse(e); double x = val_get_num(&a); val_free(&a);
        double v = (x >= 0) ? sqrt(x) : 0.0;
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    if (strcmp(name, "LEN") == 0) {
        Val a = exp_parse(e);
        const char *s = val_get_str(&a, buf, sizeof(buf)); val_free(&a);
        double v = (double)strlen(s);
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    if (strcmp(name, "VAL") == 0) {
        Val a = exp_parse(e);
        const char *s = val_get_str(&a, buf, sizeof(buf)); val_free(&a);
        double v = atof(s);
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    if (strcmp(name, "STR$") == 0) {
        Val a = exp_parse(e);
        snprintf(buf, sizeof(buf), "%g", val_get_num(&a)); val_free(&a);
        char *s = xstrdup(buf);
        if (*e->p == ')') e->p++;
        return val_str(s);
    }
    if (strcmp(name, "CHR$") == 0) {
        Val a = exp_parse(e);
        char s2[2] = { (char)(int)val_get_num(&a), 0 }; val_free(&a);
        char *s = xstrdup(s2);
        if (*e->p == ')') e->p++;
        return val_str(s);
    }
    if (strcmp(name, "ASC") == 0) {
        Val a = exp_parse(e);
        const char *s = val_get_str(&a, buf, sizeof(buf)); val_free(&a);
        double v = s[0] ? (double)(unsigned char)s[0] : 0.0;
        if (*e->p == ')') e->p++;
        return val_num(v);
    }
    e->err = 1;
    return val_num(0);
}

static Val exp_or(Exp *e);
static Val exp_and(Exp *e);
static Val exp_not(Exp *e);
static Val exp_cmp(Exp *e);
static Val exp_add(Exp *e);
static Val exp_mul(Exp *e);
static Val exp_unary(Exp *e);

static Val exp_parse(Exp *e) { return exp_or(e); }

static Val exp_or(Exp *e) {
    Val l = exp_and(e);
    for (;;) {
        e->p = skip_spaces(e->p);
        if (e->p[0] == 'O' && e->p[1] == 'R' && !is_alnum(e->p[2])) {
            e->p += 2;
            Val r = exp_and(e);
            double v = (val_get_num(&l) != 0 || val_get_num(&r) != 0) ? 1.0 : 0.0;
            val_free(&l); val_free(&r);
            l = val_num(v);
        } else break;
    }
    return l;
}

static Val exp_and(Exp *e) {
    Val l = exp_not(e);
    for (;;) {
        e->p = skip_spaces(e->p);
        if (e->p[0] == 'A' && e->p[1] == 'N' && e->p[2] == 'D' && !is_alnum(e->p[3])) {
            e->p += 3;
            Val r = exp_not(e);
            double v = (val_get_num(&l) != 0 && val_get_num(&r) != 0) ? 1.0 : 0.0;
            val_free(&l); val_free(&r);
            l = val_num(v);
        } else break;
    }
    return l;
}

static Val exp_not(Exp *e) {
    e->p = skip_spaces(e->p);
    if (e->p[0] == 'N' && e->p[1] == 'O' && e->p[2] == 'T' && !is_alnum(e->p[3])) {
        e->p += 3;
        Val r = exp_not(e);
        double v = (val_get_num(&r) == 0) ? 1.0 : 0.0;
        val_free(&r);
        return val_num(v);
    }
    return exp_cmp(e);
}

static Val exp_cmp(Exp *e) {
    Val l = exp_add(e);
    e->p = skip_spaces(e->p);
    int op = 0;
    const char *p = e->p;
    if (p[0] == '<' && p[1] == '>') { op = 1; p += 2; }        /* <> */
    else if (p[0] == '<' && p[1] == '=') { op = 2; p += 2; }   /* <= */
    else if (p[0] == '>' && p[1] == '=') { op = 3; p += 2; }   /* >= */
    else if (p[0] == '=') { op = 4; p += 1; }
    else if (p[0] == '<') { op = 2; p += 1; }                  /* treat < as <= for op code 2? no */
    else if (p[0] == '>') { op = 3; p += 1; }

    /* fix: '<' alone should be op 5 (less than), '>' alone op 6 (greater) */
    op = 0;
    p = e->p;
    if (p[0] == '<' && p[1] == '>') { op = 1; p += 2; }
    else if (p[0] == '<' && p[1] == '=') { op = 2; p += 2; }
    else if (p[0] == '>' && p[1] == '=') { op = 3; p += 2; }
    else if (p[0] == '=') { op = 4; p += 1; }
    else if (p[0] == '<') { op = 5; p += 1; }
    else if (p[0] == '>') { op = 6; p += 1; }

    if (op) {
        e->p = p;
        Val r = exp_add(e);
        int both_str = l.is_str && r.is_str;
        double ln = val_get_num(&l), rn = val_get_num(&r);
        char buf[BASIC_MAX_LINELEN];
        const char *ls = val_get_str(&l, buf, sizeof(buf));
        const char *rs = val_get_str(&r, buf, sizeof(buf));
        int res = 0;
        if (both_str) {
            int c = strcmp(ls, rs);
            switch (op) {
                case 1: res = (c != 0); break;
                case 2: res = (c <= 0); break;
                case 3: res = (c >= 0); break;
                case 4: res = (c == 0); break;
                case 5: res = (c < 0); break;
                case 6: res = (c > 0); break;
            }
        } else {
            switch (op) {
                case 1: res = (ln != rn); break;
                case 2: res = (ln <= rn); break;
                case 3: res = (ln >= rn); break;
                case 4: res = (ln == rn); break;
                case 5: res = (ln < rn); break;
                case 6: res = (ln > rn); break;
            }
        }
        val_free(&l); val_free(&r);
        return val_num(res ? 1.0 : 0.0);
    }
    return l;
}

static Val exp_add(Exp *e) {
    Val l = exp_mul(e);
    for (;;) {
        e->p = skip_spaces(e->p);
        char op = 0;
        if (e->p[0] == '+') op = '+';
        else if (e->p[0] == '-') op = '-';
        if (!op) break;
        e->p++;
        Val r = exp_mul(e);
        if (l.is_str && r.is_str && op == '+') {
            char *out = (char *)linearAlloc(strlen(l.str) + strlen(r.str) + 1);
            strcpy(out, l.str); strcat(out, r.str);
            val_free(&l); val_free(&r);
            l = val_str(out);
        } else if (!l.is_str && !r.is_str) {
            double v = (op == '+') ? val_get_num(&l) + val_get_num(&r)
                                   : val_get_num(&l) - val_get_num(&r);
            val_free(&l); val_free(&r);
            l = val_num(v);
        } else {
            val_free(&l); val_free(&r);
            e->err = 1;
            l = val_num(0);
        }
    }
    return l;
}

static Val exp_mul(Exp *e) {
    Val l = exp_unary(e);
    for (;;) {
        e->p = skip_spaces(e->p);
        char op = 0;
        if (e->p[0] == '*') op = '*';
        else if (e->p[0] == '/') op = '/';
        if (!op) break;
        e->p++;
        Val r = exp_unary(e);
        double d = val_get_num(&r);
        double v = (op == '*') ? val_get_num(&l) * d : (d != 0 ? val_get_num(&l) / d : 0.0);
        val_free(&l); val_free(&r);
        l = val_num(v);
    }
    return l;
}

static Val exp_unary(Exp *e) {
    e->p = skip_spaces(e->p);
    if (e->p[0] == '-') {
        e->p++;
        Val r = exp_unary(e);
        double v = -val_get_num(&r);
        val_free(&r);
        return val_num(v);
    }
    if (e->p[0] == '+') {
        e->p++;
        Val r = exp_unary(e);
        double v = val_get_num(&r);
        val_free(&r);
        return val_num(v);
    }
    return exp_primary(e);
}

static Val exp_primary(Exp *e) {
    BasicState *bs = e->bs;
    e->p = skip_spaces(e->p);
    const char *p = e->p;

    if (*p == '(') {
        e->p = p + 1;
        Val v = exp_parse(e);
        e->p = skip_spaces(e->p);
        if (*e->p == ')') e->p++;
        return v;
    }
    if (*p == '"') {
        e->p = p + 1;
        char *out = (char *)linearAlloc(BASIC_MAX_LINELEN);
        int n = 0;
        while (*e->p && *e->p != '"') {
            if (*e->p == '\\' && e->p[1]) { e->p++; out[n++] = *e->p; }
            else out[n++] = *e->p;
            e->p++;
        }
        if (*e->p == '"') e->p++;
        out[n] = '\0';
        return val_str(out);
    }
    if (is_digit(*p) || (*p == '.' && is_digit(p[1]))) {
        char *end;
        double v = strtod(p, &end);
        e->p = end;
        return val_num(v);
    }
    if (is_letter(*p)) {
        char name[64];
        int n = 0;
        while (is_alnum(*p) && n < (int)sizeof(name) - 1) { name[n++] = *p; p++; }
        name[n] = '\0';
        e->p = p;
        e->p = skip_spaces(e->p);
        if (*e->p == '(') {
            return exp_func(e, name);
        }
        /* variable */
        int is_str = (*e->p == '$');
        if (is_str) e->p++;
        if (n == 1) {
            int idx = var_index(name[0]);
            if (idx < 0) { e->err = 1; return val_num(0); }
            if (is_str) {
                if (!bs->str_vars[idx]) return val_str(xstrdup(""));
                return val_str(xstrdup(bs->str_vars[idx]));
            }
            return val_num(bs->num_vars[idx]);
        }
        e->err = 1;
        return val_num(0);
    }
    e->err = 1;
    return val_num(0);
}

static Val basic_eval(BasicState *bs, const char **pp) {
    Exp e; e.bs = bs; e.p = *pp; e.err = 0;
    Val v = exp_parse(&e);
    *pp = e.p;
    if (e.err) { val_free(&v); return val_num(0); }
    return v;
}

/*---------------------------------------------------------------------
 * statement execution
 *-------------------------------------------------------------------*/
typedef struct {
    BasicState *bs;
    const char *p;
    int err;
    int jump;
    int next_pc;
} Stmt;

static void stmt_print(Stmt *s) {
    BasicState *bs = s->bs;
    const char *p = s->p;
    int cr = 1;
    while (*p) {
        p = skip_spaces(p);
        if (!*p) break;
        if (*p == ',') { p++; bs->cur_x = (bs->cur_x / 14 + 1) * 14;
            if (bs->cur_x >= bs->cols) { bs->cur_x = 0; bs->cur_y++; }
            continue; }
        if (*p == ';') { p++; cr = 0; continue; }
        if (*p == '"') {
            p++;
            char out[BASIC_MAX_LINELEN]; int n = 0;
            while (*p && *p != '"') { out[n++] = *p++; }
            if (*p == '"') p++;
            out[n] = '\0';
            basic_print_str(bs, out);
            cr = 1;
        } else {
            const char *pp = p;
            Val v = basic_eval(bs, &pp);
            p = pp;
            char buf[BASIC_MAX_LINELEN];
            const char *str = val_get_str(&v, buf, sizeof(buf));
            basic_print_str(bs, str);
            val_free(&v);
            cr = 1;
        }
    }
    if (cr) basic_putc(bs, '\n');
    s->p = p;
}

static void stmt_input(Stmt *s) {
    BasicState *bs = s->bs;
    const char *p = s->p;
    bs->input_prompt[0] = '\0';
    if (*p == '"') {
        p++;
        int n = 0;
        while (*p && *p != '"') { if (n < 63) bs->input_prompt[n++] = *p; p++; }
        if (*p == '"') p++;
        bs->input_prompt[n] = '\0';
        p = skip_spaces(p);
        if (*p == ',' || *p == ';') p++;
        p = skip_spaces(p);
    }
    if (is_letter(*p)) {
    char name[8]; int n = 0;
        while (is_alnum(*p) && n < 7) { name[n++] = *p; p++; }
        name[n] = '\0';
        int is_str = (*p == '$'); if (is_str) p++;
        int idx = var_index(name[0]);
        bs->input_var_idx = idx;
        bs->input_is_str = is_str;
        bs->inlen = 0;
        bs->waiting_input = 1;
        if (bs->input_prompt[0]) basic_print_str(bs, bs->input_prompt);
        else basic_print_str(bs, "? ");
        s->jump = 1;
        s->next_pc = s->bs->pc + 1;
        s->p = p;
        return;
    }
    s->err = 1;
}

static void stmt_let(Stmt *s) {
    BasicState *bs = s->bs;
    const char *p = s->p;
    if (p[0] == 'L' && p[1] == 'E' && p[2] == 'T' && !is_alnum(p[3])) { p += 3; p = skip_spaces(p); }
    char name[8]; char numbuf[BASIC_MAX_LINELEN]; int n = 0;
    while (is_alnum(*p) && n < 7) { name[n++] = *p; p++; }
    name[n] = '\0';
    int is_str = (*p == '$'); if (is_str) p++;
    p = skip_spaces(p);
    if (*p != '=') { s->err = 1; s->p = p; return; }
    p++;
    const char *pp = p;
    Val v = basic_eval(bs, &pp);
    p = pp;
    int idx = var_index(name[0]);
    if (idx >= 0) {
        if (is_str) {
            free_str_var(idx);
            bs->str_vars[idx] = v.is_str ? v.str : xstrdup(val_get_str(&v, numbuf, sizeof(numbuf)));
            if (!v.is_str) val_free(&v);
        } else {
            val_free(&v);
            const char *pp2 = p;
            Val v2 = basic_eval(bs, &pp2);
            bs->num_vars[idx] = val_get_num(&v2);
            val_free(&v2);
        }
    } else {
        val_free(&v);
    }
    s->p = p;
}

static void stmt_goto(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    int num = 0;
    while (is_digit(*s->p)) { num = num * 10 + (*s->p - '0'); s->p++; }
    int idx;
    if (find_line(bs, num, &idx)) { s->jump = 1; s->next_pc = idx; }
}

static void stmt_gosub(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    int num = 0;
    while (is_digit(*s->p)) { num = num * 10 + (*s->p - '0'); s->p++; }
    int idx;
    if (find_line(bs, num, &idx) && bs->call_sp < BASIC_CALL_STACK) {
        bs->call_stack[bs->call_sp++] = bs->pc + 1;
        s->jump = 1; s->next_pc = idx;
    }
}

static void stmt_return(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    if (bs->call_sp > 0) { s->jump = 1; s->next_pc = bs->call_stack[--bs->call_sp]; }
}

static void stmt_for(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    if (!is_letter(*s->p)) { s->err = 1; return; }
    char vname = *s->p++;
    s->p = skip_spaces(s->p);
    if (*s->p != '=') { s->err = 1; return; }
    s->p++;
    const char *pp = s->p;
    Val from = basic_eval(bs, &pp); s->p = pp;
    s->p = skip_spaces(s->p);
    if (s->p[0] != 'T' || s->p[1] != 'O') { s->err = 1; val_free(&from); return; }
    s->p += 2;
    s->p = skip_spaces(s->p);
    pp = s->p;
    Val to = basic_eval(bs, &pp); s->p = pp;
    double step = 1.0;
    s->p = skip_spaces(s->p);
    if (s->p[0] == 'S' && s->p[1] == 'T' && s->p[2] == 'E' && !is_alnum(s->p[3])) {
        s->p += 3;
        s->p = skip_spaces(s->p);
        pp = s->p;
        Val st = basic_eval(bs, &pp); s->p = pp;
        step = val_get_num(&st); val_free(&st);
    }
    int idx = var_index(vname);
    if (idx >= 0 && bs->for_sp < BASIC_FOR_STACK) {
        bs->num_vars[idx] = val_get_num(&from);
        bs->for_var[bs->for_sp] = idx;
        bs->for_from[bs->for_sp] = val_get_num(&from);
        bs->for_to[bs->for_sp] = val_get_num(&to);
        bs->for_step[bs->for_sp] = step;
        bs->for_line[bs->for_sp] = bs->pc;
        bs->for_sp++;
    }
    val_free(&from); val_free(&to);
    s->p = s->p;
}

static void stmt_next(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    char vname = 0;
    if (is_letter(*s->p)) vname = *s->p++;
    if (bs->for_sp > 0) {
        int top = bs->for_sp - 1;
        if (vname && bs->for_var[top] != var_index(vname)) { s->err = 1; return; }
        bs->num_vars[bs->for_var[top]] += bs->for_step[top];
        double cur = bs->num_vars[bs->for_var[top]];
        int cont = (bs->for_step[top] > 0) ? (cur <= bs->for_to[top]) : (cur >= bs->for_to[top]);
        if (cont) { s->jump = 1; s->next_pc = bs->for_line[top]; }
        else bs->for_sp--;
    }
}

static void stmt_exec_simple(Stmt *s);

static void stmt_if(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    const char *pp = s->p;
    Val cond = basic_eval(bs, &pp);
    s->p = pp;
    int truth = (val_get_num(&cond) != 0);
    val_free(&cond);
    s->p = skip_spaces(s->p);
    if (s->p[0] == 'T' && s->p[1] == 'H' && s->p[2] == 'E' && !is_alnum(s->p[3])) {
        s->p += 3;
        s->p = skip_spaces(s->p);
        if (truth) {
            if (s->p[0] == 'G' && s->p[1] == 'O' && s->p[2] == 'T' && s->p[3] == 'O') {
                s->p += 4;
                stmt_goto(s);
                s->jump = 1; s->next_pc = s->next_pc; /* stmt_goto sets next_pc */
                return;
            } else if (s->p[0] == 'G' && s->p[1] == 'O' && s->p[2] == 'S' && s->p[3] == 'U' && s->p[4] == 'B') {
                s->p += 5;
                stmt_gosub(s);
                if (s->jump) { /* next_pc set */ }
                return;
            } else {
                Stmt inner = *s; inner.p = s->p;
                stmt_exec_simple(&inner);
                s->p = inner.p;
            }
        }
    }
}

static void stmt_exec_simple(Stmt *s) {
    BasicState *bs = s->bs;
    s->p = skip_spaces(s->p);
    const char *p = s->p;
    if (!*p || *p == '\'') return;
    if (p[0] == 'R' && p[1] == 'E' && p[2] == 'M') { s->p += 3; return; }
    if (p[0] == 'P' && p[1] == 'R' && p[2] == 'I' && p[3] == 'N' && p[4] == 'T' && !is_alnum(p[5])) {
        s->p += 5; stmt_print(s); return;
    }
    if (p[0] == 'I' && p[1] == 'N' && p[2] == 'P' && p[3] == 'U' && p[4] == 'T' && !is_alnum(p[5])) {
        s->p += 5; stmt_input(s); return;
    }
    if (p[0] == 'C' && p[1] == 'L' && p[2] == 'S') { s->p += 3; basic_cls(bs); return; }
    if (p[0] == 'E' && p[1] == 'N' && p[2] == 'D') { s->p += 3; bs->running = 0; return; }
    if (p[0] == 'S' && p[1] == 'T' && p[2] == 'O' && p[3] == 'P') { s->p += 3; bs->running = 0; return; }
    if (p[0] == 'G' && p[1] == 'O' && p[2] == 'T' && p[3] == 'O') { s->p += 4; stmt_goto(s); return; }
    if (p[0] == 'G' && p[1] == 'O' && p[2] == 'S' && p[3] == 'U' && p[4] == 'B') { s->p += 5; stmt_gosub(s); return; }
    if (p[0] == 'R' && p[1] == 'E' && p[2] == 'T' && p[3] == 'U' && p[4] == 'R' && p[5] == 'N') { s->p += 6; stmt_return(s); return; }
    if (p[0] == 'F' && p[1] == 'O' && p[2] == 'R') { s->p += 3; stmt_for(s); return; }
    if (p[0] == 'N' && p[1] == 'E' && p[2] == 'X' && p[3] == 'T') { s->p += 4; stmt_next(s); return; }
    if (p[0] == 'I' && p[1] == 'F') { s->p += 2; stmt_if(s); return; }
    if (p[0] == 'L' && p[1] == 'O' && p[2] == 'C' && p[3] == 'A' && p[4] == 'T' && p[5] == 'E') {
        s->p += 6; s->p = skip_spaces(s->p);
        const char *pp = s->p;
        Val v = basic_eval(bs, &pp); s->p = pp;
        int row = (int)val_get_num(&v); val_free(&v);
        s->p = skip_spaces(s->p);
        int col = 0;
        if (*s->p == ',') { s->p++; s->p = skip_spaces(s->p);
            Val v2 = basic_eval(bs, &pp); s->p = pp;
            col = (int)val_get_num(&v2); val_free(&v2);
        }
        if (row >= 1 && row <= bs->rows) bs->cur_y = row - 1;
        if (col >= 1 && col <= bs->cols) bs->cur_x = col - 1;
        return;
    }
    if (p[0] == 'C' && p[1] == 'O' && p[2] == 'L' && p[3] == 'O' && p[4] == 'R') {
        s->p += 5; s->p = skip_spaces(s->p);
        const char *pp = s->p;
        Val v = basic_eval(bs, &pp); s->p = pp;
        bs->cur_fg = (int)val_get_num(&v) & 15; val_free(&v);
        s->p = skip_spaces(s->p);
        if (*s->p == ',') { s->p++; s->p = skip_spaces(s->p);
            Val v2 = basic_eval(bs, &pp); s->p = pp;
            bs->cur_bg = (int)val_get_num(&v2) & 15; val_free(&v2);
        }
        return;
    }
    stmt_let(s);
}

/* execute one stored line */
static void execute_line(BasicState *bs) {
    int cur = bs->pc;
    bs->pc_set = 0;
    const char *p = bs->lines[cur].text;
    while (*p) {
        Stmt st; st.bs = bs; st.p = p; st.err = 0; st.jump = 0; st.next_pc = 0;
        const char *semi = p;
        int in_str = 0;
        while (*semi) {
            if (*semi == '"') in_str = !in_str;
            if (!in_str && *semi == ':') break;
            semi++;
        }
        int seglen = (int)(semi - p);
        char segbuf[BASIC_MAX_LINELEN + 1];
        if (seglen > BASIC_MAX_LINELEN) seglen = BASIC_MAX_LINELEN;
        memcpy(segbuf, p, seglen);
        segbuf[seglen] = '\0';
        st.p = segbuf;
        stmt_exec_simple(&st);
        p = semi;
        if (*p == ':') p++;
        if (st.jump) { bs->pc_set = 1; bs->pc = st.next_pc; return; }
        if (st.err) break;
        if (!bs->running) return;
    }
    if (!bs->pc_set) bs->pc = cur + 1;
}

/*---------------------------------------------------------------------
 * direct commands
 *-------------------------------------------------------------------*/
static void run_command(BasicState *bs, const char *line, Window *w) {
    const char *p = skip_spaces(line);
    char cmd[32]; int n = 0;
    while (is_alnum(*p) && n < 31) { cmd[n++] = *p; p++; }
    cmd[n] = '\0';
    p = skip_spaces(p);

    if (strcmp(cmd, "RUN") == 0) {
        if (bs->nlines == 0) { set_status(w, "No program"); return; }
        bs->running = 1;
        bs->pc = 0;
        bs->call_sp = 0; bs->for_sp = 0;
        bs->waiting_input = 0;
        bs->dirty = 1;
        return;
    }
    if (strcmp(cmd, "NEW") == 0) {
        for (int i = 0; i < bs->nlines; i++) linearFree(bs->lines[i].text);
        bs->nlines = 0;
        for (int i = 0; i < BASIC_NUM_VARS; i++) { bs->num_vars[i] = 0; free_str_var(i); }
        basic_cls(bs);
        set_status(w, "Cleared");
        return;
    }
    if (strcmp(cmd, "CLS") == 0) { basic_cls(bs); return; }
    if (strcmp(cmd, "LIST") == 0) {
        basic_putc(bs, '\n');
        for (int i = 0; i < bs->nlines; i++) {
            char buf[BASIC_MAX_LINELEN + 16];
            snprintf(buf, sizeof(buf), "%d %s", bs->lines[i].num, bs->lines[i].text);
            basic_print_str(bs, buf);
            basic_putc(bs, '\n');
        }
        return;
    }
    if (strcmp(cmd, "BYE") == 0 || strcmp(cmd, "EXIT") == 0) {
        set_status(w, "Close the window or press SELECT");
        return;
    }
    if (cmd[0] == 0) return;
    set_status(w, "?Unknown command");
}

static void process_direct_line(BasicState *bs, Window *w) {
    const char *p = skip_spaces(bs->inbuf);
    if (!*p) return;
    int num = 0;
    const char *q = p;
    while (is_digit(*q)) { num = num * 10 + (*q - '0'); q++; }
    q = skip_spaces(q);
    if (num > 0 && *q != 0) {
        store_line(bs, num, q);
        return;
    }
    if (num > 0 && *q == 0) {
        delete_line(bs, num);
        return;
    }
    run_command(bs, p, w);
}

/*---------------------------------------------------------------------
 * app callbacks
 *-------------------------------------------------------------------*/
static void basic_reset(Window *w) {
    w->kind = WK_BASIC;
    w->open = 1;
    w->x = 8; w->y = 8;
    w->w = 384; w->h = 200;
    strcpy(w->title, "BASIC");
    w->app_state = &g_basic_state;
    BasicState *bs = &g_basic_state;
    memset(bs, 0, sizeof(*bs));
    bs->cur_fg = 15;
    bs->cur_bg = 0;
    srand((unsigned)time(NULL));

    int x0 = w->x + 5;
    int y0 = w->y + 24;
    int cw = w->w - 10;
    int ch = w->h - 34;
    bs->cols = cw / 8;
    if (bs->cols > BASIC_MAX_COLS) bs->cols = BASIC_MAX_COLS;
    if (bs->cols < 1) bs->cols = 1;
    bs->rows = (ch - 20) / 10;
    if (bs->rows > BASIC_MAX_ROWS) bs->rows = BASIC_MAX_ROWS;
    if (bs->rows < 1) bs->rows = 1;
    bs->x0 = x0; bs->y0 = y0;
    basic_cls(bs);
    bring_to_front(WK_BASIC);
    if (!g_shift) { g_shift = 1; g_shift_auto = 1; }
}

static void basic_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    BasicState *bs = (BasicState *)w->app_state;
    if (!bs->cols || !bs->rows) return;

    int x0 = w->x + 5;
    int y0 = w->y + 24;
    int cw = w->w - 10;
    int ch = w->h - 34;
    fill_rect(fb, TOP_W, TOP_H, x0, y0, cw, ch, BASIC_PAL[bs->cur_bg & 15]);

    for (int y = 0; y < bs->rows; y++) {
        int ty = y0 + y * 10;
        for (int x = 0; x < bs->cols; x++) {
            char c = bs->ch[y][x];
            if (c == ' ') continue;
            int tx = x0 + x * 8;
            draw_char(fb, TOP_W, TOP_H, tx, ty, c, BASIC_PAL[bs->fg[y][x] & 15]);
        }
    }

    int show_caret = (!bs->running && caret_tick() < 30);
    if (show_caret) {
        int cx = x0 + bs->cur_x * 8;
        int cy = y0 + bs->cur_y * 10;
        vline(fb, TOP_W, TOP_H, cx, cy, 8, BASIC_PAL[15]);
    }

    if (w->status_ttl > 0 && w->status[0]) {
        int iy = y0 + bs->rows * 10 + 2;
        if (iy + 8 <= y0 + ch) {
            draw_text(fb, TOP_W, TOP_H, x0, iy, w->status, BASIC_PAL[15]);
        }
    }

    bs->dirty = 0;
}

static void basic_click(Window *w, int mx, int my) {
    (void)w; (void)mx; (void)my;
}

static void basic_input(Window *w, char c) {
    BasicState *bs = (BasicState *)w->app_state;
    if (bs->waiting_input) {
        if (c == '\n') {
            int idx = bs->input_var_idx;
            if (bs->input_is_str) {
                free_str_var(idx);
                bs->str_vars[idx] = xstrdup(bs->inbuf);
            } else {
                bs->num_vars[idx] = atof(bs->inbuf);
            }
            bs->waiting_input = 0;
            basic_putc(bs, '\n');
        } else if (c == '\b') {
            if (bs->inlen > 0) {
                bs->inlen--;
                bs->inbuf[bs->inlen] = '\0';
                basic_putc(bs, '\b');
            }
        } else if (bs->inlen < BASIC_MAX_LINELEN - 1 && c >= 32) {
            bs->inbuf[bs->inlen++] = c; bs->inbuf[bs->inlen] = '\0';
            basic_putc(bs, c);
        }
        bs->dirty = 1;
        return;
    }
    if (c == '\n') {
        bs->inbuf[bs->inlen] = '\0';
        process_direct_line(bs, w);
        bs->inlen = 0;
        basic_putc(bs, '\n');
        basic_print_str(bs, "Ok");
        basic_putc(bs, '\n');
    } else if (c == '\b') {
        if (bs->inlen > 0) {
            bs->inlen--;
            bs->inbuf[bs->inlen] = '\0';
            basic_putc(bs, '\b');
        }
    } else if (bs->inlen < BASIC_MAX_LINELEN - 1 && c >= 32) {
        bs->inbuf[bs->inlen++] = c; bs->inbuf[bs->inlen] = '\0';
        basic_putc(bs, c);
    }
    bs->dirty = 1;
}

static void basic_update(Window *w) {
    BasicState *bs = (BasicState *)w->app_state;
    if (!bs->running || bs->waiting_input) return;
    int budget = 4000;
    while (bs->running && budget-- > 0 && !bs->waiting_input) {
        if (bs->pc < 0 || bs->pc >= bs->nlines) { bs->running = 0; break; }
        execute_line(bs);
        bs->dirty = 1;
    }
    if (!bs->running) set_status(w, "Ok");
}

static int basic_load_file(Window *w, const char *fname) {
    (void)w; (void)fname;
    return 0;
}

const App g_app_basic = {
    "BASIC",
    basic_reset,
    basic_draw,
    basic_click,
    basic_input,
    basic_update,
    basic_load_file,
    NULL, NULL, NULL, NULL,
};
