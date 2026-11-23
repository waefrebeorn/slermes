/*
 * _marker_eval.c — C home for upstream Hermes pm/_marker_eval.py.
 *
 * PoP: w5_markereval_a9d132_eval @ pm/_marker_eval.py:Marker.evaluate
 *
 * Faithful evaluation of the PEP 508 marker grammar used by packaging:
 *   - variables resolved from the supplied environment object,
 *   - `and` / `or` / parentheses with correct precedence,
 *   - string operators (==, !=, in, not in, <=, >=, <, >) for non-version keys,
 *   - PEP 440 version operators for version keys, plus `===`.
 * Exported symbols are w5_markereval_a9d132_ prefixed.
 */
#include "noc_w5_markereval_a9d132.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- environment lookup ---------------- */
static const char *mk_lookup(const json_t *env, const char *key) {
    if (!env || env->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(env, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* canonicalize_name (PEP 503): runs of -_. -> "-" and lowercase */
static char *mk_canonical(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;
    size_t o = 0;
    int pending = 0;
    for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        if (c == '-' || c == '_' || c == '.') { pending = 1; continue; }
        if (pending && o) out[o++] = '-';
        pending = 0;
        out[o++] = (char)tolower((unsigned char)c);
    }
    out[o] = '\0';
    return out;
}

/* ---------------- minimal PEP 440 version compare ---------------- */
typedef struct {
    long epoch;
    int rel[16];
    int nrel;
    int has_pre, pre_tag, pre_num;   /* tag: 0=a,1=b,2=rc */
    int has_post, post_num;
    int has_dev, dev_num;
    char local[64];
} mk_ver;

static int mk_parse_ver(const char *s, mk_ver *v) {
    memset(v, 0, sizeof(*v));
    if (!s) return 0;
    const char *p = s;
    /* epoch */
    const char *q = p;
    while (isdigit((unsigned char)*q)) ++q;
    if (*q == '!' && q != p) { v->epoch = strtol(p, NULL, 10); p = q + 1; }
    /* release */
    while (isdigit((unsigned char)*p) && v->nrel < 16) {
        v->rel[v->nrel++] = (int)strtol(p, (char **)&p, 10);
        if (*p == '.' && isdigit((unsigned char)p[1])) ++p; else break;
    }
    if (!v->nrel) return 0;
    for (;;) {
        while (*p == '.' || *p == '-' || *p == '_') ++p;
        if (!*p) break;
        char low[16]; size_t l = 0;
        while (isalpha((unsigned char)*p) && l < 15) low[l++] = (char)tolower((unsigned char)*p++);
        low[l] = '\0';
        int digits = (int)strtol(p, (char **)&p, 10);
        if (!strcmp(low, "dev")) { v->has_dev = 1; v->dev_num = digits; }
        else if (!strcmp(low, "post") || !strcmp(low, "rev") || !strcmp(low, "r")) { v->has_post = 1; v->post_num = digits; }
        else if (!strcmp(low, "a") || !strcmp(low, "alpha")) { v->has_pre = 1; v->pre_tag = 0; v->pre_num = digits; }
        else if (!strcmp(low, "b") || !strcmp(low, "beta")) { v->has_pre = 1; v->pre_tag = 1; v->pre_num = digits; }
        else if (!strcmp(low, "c") || !strcmp(low, "rc") || !strcmp(low, "pre") || !strcmp(low, "preview")) { v->has_pre = 1; v->pre_tag = 2; v->pre_num = digits; }
        else return 0;
    }
    if (*p == '+') {
        strncpy(v->local, p + 1, sizeof(v->local) - 1);
        v->local[sizeof(v->local) - 1] = '\0';
    }
    return 1;
}

static int mk_rel_cmp(const mk_ver *a, const mk_ver *b) {
    int n = a->nrel > b->nrel ? a->nrel : b->nrel;
    for (int i = 0; i < n; ++i) {
        int x = i < a->nrel ? a->rel[i] : 0;
        int y = i < b->nrel ? b->rel[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}
static int mk_stage(const mk_ver *v) {
    if (v->has_pre) return v->pre_tag;          /* 0=a 1=b 2=rc */
    if (v->has_dev && !v->has_post) return -1;  /* dev-only sorts before a */
    return 3;                                    /* final / post */
}
static int mk_ver_cmp(const mk_ver *a, const mk_ver *b) {
    if (a->epoch != b->epoch) return a->epoch < b->epoch ? -1 : 1;
    int r = mk_rel_cmp(a, b); if (r) return r;
    int sa = mk_stage(a), sb = mk_stage(b);
    if (sa != sb) return sa < sb ? -1 : 1;
    if (a->has_pre && b->has_pre && a->pre_num != b->pre_num)
        return a->pre_num < b->pre_num ? -1 : 1;
    /* post: absent(-1) < present */
    int pa = a->has_post ? a->post_num : -1;
    int pb = b->has_post ? b->post_num : -1;
    if (pa != pb) return pa < pb ? -1 : 1;
    /* dev: present sorts before absent */
    if (a->has_dev != b->has_dev) return a->has_dev ? -1 : 1;
    if (a->has_dev && b->has_dev && a->dev_num != b->dev_num)
        return a->dev_num < b->dev_num ? -1 : 1;
    int la = a->local[0] != 0, lb = b->local[0] != 0;
    if (la != lb) return la ? 1 : -1;
    if (la) { int c = strcmp(a->local, b->local); if (c) return c < 0 ? -1 : 1; }
    return 0;
}

static int mk_is_version_key(const char *key) {
    static const char *const keys[] = {
        "implementation_version", "platform_release", "python_full_version", "python_version"
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        if (!strcmp(key, keys[i])) return 1;
    return 0;
}

/* ---------------- tokenizer ---------------- */
typedef enum { TK_END, TK_LP, TK_RP, TK_STR, TK_WORD, TK_OP } tk_kind;
typedef struct { tk_kind kind; char text[512]; } marker_token;

static int mk_tokenize(const char *s, marker_token *toks, int cap) {
    int n = 0;
    const char *p = s;
    while (*p) {
        if (isspace((unsigned char)*p)) { ++p; continue; }
        if (n >= cap) return -1;
        marker_token *t = &toks[n];
        if (*p == '(') { t->kind = TK_LP; t->text[0] = '('; t->text[1] = 0; ++n; ++p; continue; }
        if (*p == ')') { t->kind = TK_RP; t->text[0] = ')'; t->text[1] = 0; ++n; ++p; continue; }
        if (*p == '\'' || *p == '"') {
            char quote = *p++; size_t l = 0;
            while (*p && *p != quote) {
                if (*p == '\\' && p[1]) {
                    ++p;
                    char c = *p;
                    if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; else if (c == 'r') c = '\r';
                    t->text[l++] = c; ++p;
                } else t->text[l++] = *p++;
                if (l >= sizeof(t->text) - 1) break;
            }
            t->text[l] = 0;
            if (*p == quote) ++p;
            t->kind = TK_STR; ++n; continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            size_t l = 0;
            while ((isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-') && l < sizeof(t->text) - 1)
                t->text[l++] = *p++;
            t->text[l] = 0; t->kind = TK_WORD; ++n; continue;
        }
        if (strchr("<>=!~", *p)) {
            size_t l = 0;
            while (strchr("<>=!~", *p) && l < sizeof(t->text) - 1) t->text[l++] = *p++;
            t->text[l] = 0; t->kind = TK_OP; ++n; continue;
        }
        ++p; /* ignore unknown char */
    }
    toks[n].kind = TK_END; toks[n].text[0] = 0;
    return n;
}

/* ---------------- parser / evaluator ---------------- */
typedef struct {
    const marker_token *t;
    int i;
    const json_t *env;
} mk_ctx;

static const char *mk_peek(mk_ctx *c) { return c->t[c->i].text; }
static tk_kind mk_kind(mk_ctx *c) { return c->t[c->i].kind; }
static void mk_next(mk_ctx *c) { if (mk_kind(c) != TK_END) ++c->i; }

typedef struct { const char *text; int is_var; } mk_operand;

static int mk_parse_or(mk_ctx *c);

static int mk_contains(const char *needle, const char *hay) {
    return needle && hay && strstr(hay, needle) != NULL;
}

static int mk_compare(mk_operand lhs, const char *op, mk_operand rhs, const json_t *env) {
    const char *key, *lhs_val, *rhs_val;
    if (lhs.is_var) {
        key = lhs.text;
        lhs_val = mk_lookup(env, key);
        rhs_val = rhs.text;
    } else {
        lhs_val = lhs.text;
        key = rhs.is_var ? rhs.text : "";
        rhs_val = mk_lookup(env, key);
    }
    if (!lhs_val || !rhs_val || !key) return 0;
    char *lcanon = NULL, *rcanon = NULL;
    if (!strcmp(key, "extra")) {
        lcanon = mk_canonical(lhs_val);
        rcanon = mk_canonical(rhs_val);
        lhs_val = lcanon ? lcanon : lhs_val;
        rhs_val = rcanon ? rcanon : rhs_val;
    }
    int result = 0;
    int notin = !strcmp(op, "not in");
    if (!strcmp(op, "in") || notin) {
        result = mk_contains(lhs_val, rhs_val);
        if (notin) result = !result;
        goto done;
    }
    if (mk_is_version_key(key) && strcmp(op, "in") && strcmp(op, "not in")) {
        if (!strcmp(op, "===")) { result = !strcmp(lhs_val, rhs_val); goto done; }
        mk_ver a, b;
        if (!mk_parse_ver(lhs_val, &a) || !mk_parse_ver(rhs_val, &b)) { result = 0; goto done; }
        int cmp = mk_ver_cmp(&a, &b);
        if (!strcmp(op, "==")) result = (cmp == 0);
        else if (!strcmp(op, "!=")) result = (cmp != 0);
        else if (!strcmp(op, "<")) result = (cmp < 0);
        else if (!strcmp(op, "<=")) result = (cmp <= 0);
        else if (!strcmp(op, ">")) result = (cmp > 0);
        else if (!strcmp(op, ">=")) result = (cmp >= 0);
        else if (!strcmp(op, "~=")) {
            int n = b.nrel > 1 ? b.nrel - 1 : 1;
            int match = 1;
            for (int i = 0; i < n; ++i) {
                int x = i < a.nrel ? a.rel[i] : 0;
                int y = i < b.nrel ? b.rel[i] : 0;
                if (x != y) { match = 0; break; }
            }
            result = (cmp >= 0) && match;
        } else result = 0;
        goto done;
    }
    /* non-version keys: packaging's _operators map */
    if (!strcmp(op, "==") || !strcmp(op, "<=") || !strcmp(op, ">=")) result = !strcmp(lhs_val, rhs_val);
    else if (!strcmp(op, "!=")) result = strcmp(lhs_val, rhs_val) != 0;
    else result = 0; /* < and > are always False */
done:
    free(lcanon); free(rcanon);
    return result;
}

static mk_operand mk_operand_read(mk_ctx *c) {
    mk_operand o;
    if (mk_kind(c) == TK_STR) { o.text = c->t[c->i].text; o.is_var = 0; }
    else { o.text = c->t[c->i].text; o.is_var = 1; }
    mk_next(c);
    return o;
}

static int mk_parse_atom(mk_ctx *c) {
    if (mk_kind(c) == TK_LP) {
        mk_next(c);
        int v = mk_parse_or(c);
        if (mk_kind(c) == TK_RP) mk_next(c);
        return v;
    }
    mk_operand lhs = mk_operand_read(c);
    char op[16] = "";
    if (mk_kind(c) == TK_OP) { strncpy(op, mk_peek(c), sizeof(op) - 1); op[sizeof(op) - 1] = 0; mk_next(c); }
    else if (mk_kind(c) == TK_WORD) {
        if (!strcmp(mk_peek(c), "in")) { strcpy(op, "in"); mk_next(c); }
        else if (!strcmp(mk_peek(c), "not")) {
            mk_next(c);
            if (mk_kind(c) == TK_WORD && !strcmp(mk_peek(c), "in")) { strcpy(op, "not in"); mk_next(c); }
            else strcpy(op, "not");
        } else strcpy(op, "==");
    }
    mk_operand rhs = mk_operand_read(c);
    return mk_compare(lhs, op, rhs, c->env);
}

static int mk_parse_and(mk_ctx *c) {
    int v = mk_parse_atom(c);
    while (mk_kind(c) == TK_WORD && !strcmp(mk_peek(c), "and")) {
        mk_next(c);
        int r = mk_parse_atom(c);
        v = v && r;
    }
    return v;
}

static int mk_parse_or(mk_ctx *c) {
    int v = mk_parse_and(c);
    while (mk_kind(c) == TK_WORD && !strcmp(mk_peek(c), "or")) {
        mk_next(c);
        int r = mk_parse_and(c);
        v = v || r;
    }
    return v;
}

int w5_markereval_a9d132_eval(const char *marker, const json_t *env) {
    if (!marker) return 0;
    marker_token toks[256];
    if (mk_tokenize(marker, toks, 256) < 0) return 0;
    mk_ctx c = { toks, 0, env };
    return mk_parse_or(&c) ? 1 : 0;
}
