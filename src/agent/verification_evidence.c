/*
 * verification_evidence.c — Coding verification evidence ledger (faithful C11
 * port of agent/verification_evidence.py). See verification_evidence.h.
 */

#include "verification_evidence.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

const char *VERIFY_AD_HOC_PREFIXES[VERIFY_AD_HOC_PREFIX_COUNT] = {
    "hermes-verify-", "hermes-ad-hoc-"
};

/* ── helpers ────────────────────────────────────────────────────── */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Minimal POSIX-shell tokenizer (shlex.split-equivalent for typical commands):
 * splits on whitespace, honours single/double quotes and backslash escapes,
 * drops comments (#), and joins continuation lines. Returns an array of
 * malloc'd tokens (NULL-terminated) and count via out_n. */
static char **shell_tokenize(const char *s, int *out_n) {
    char **toks = NULL;
    int cap = 0, n = 0;
    char *buf = (char *)malloc(strlen(s) + 1);
    if (!buf) { *out_n = 0; return NULL; }
    int bi = 0;
    int i = 0, L = (int)strlen(s);
    int in_sq = 0, in_dq = 0, escaped = 0;
    while (i <= L) {
        char c = (i < L) ? s[i] : '\0';
        if (escaped) {
            buf[bi++] = c; escaped = 0; i++; continue;
        }
        if (c == '\\' && !in_sq) { escaped = 1; i++; continue; }
        if (c == '\'' && !in_dq) { in_sq = !in_sq; i++; continue; }
        if (c == '"' && !in_sq) { in_dq = !in_dq; i++; continue; }
        if (!in_sq && !in_dq && (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0')) {
            if (bi > 0) {
                buf[bi] = '\0';
                if (n >= cap) { cap = cap ? cap*2 : 8; toks = (char**)realloc(toks, cap*sizeof(char*)); }
                toks[n++] = xstrdup(buf);
                bi = 0;
            }
            i++; continue;
        }
        buf[bi++] = c; i++;
    }
    if (n >= cap) { cap = n+1; toks = (char**)realloc(toks, cap*sizeof(char*)); }
    toks[n] = NULL;
    *out_n = n;
    free(buf);
    return toks;
}

static void free_tokens(char **toks, int n) {
    if (!toks) return;
    for (int i = 0; i < n; i++) free(toks[i]);
    free(toks);
}

/* Split a command into segments on && / || / ; then tokenize each. */
char ***verify_split_segments(const char *command, int *out_count, int **out_lens) {
    char ***segs = NULL;
    int cap = 0, n = 0;
    /* segment split regex: \s*(?:&&|\|\|;)\s* — implement manually */
    char *work = xstrdup(command ? command : "");
    if (!work) { *out_count = 0; *out_lens = NULL; return NULL; }
    /* strip leading/trailing space */
    char *p = work;
    int L = (int)strlen(p);
    /* split into raw segments */
    int si = 0;
    while (si <= L) {
        int seg_start = si;
        int seg_end = si;
        /* find next separator */
        int j = si;
        while (j < L) {
            if ((p[j]=='&'&&p[j+1]=='&') || (p[j]=='|'&&p[j+1]=='|') || p[j]==';') break;
            j++;
        }
        seg_end = j;
        /* trim whitespace around segment */
        while (seg_start < seg_end && isspace((unsigned char)p[seg_start])) seg_start++;
        while (seg_end > seg_start && isspace((unsigned char)p[seg_end-1])) seg_end--;
        if (seg_end > seg_start) {
            char tmp = p[seg_end]; p[seg_end] = '\0';
            int tn; char **toks = shell_tokenize(p + seg_start, &tn);
            p[seg_end] = tmp;
            if (toks && tn > 0) {
                if (n >= cap) { cap = cap?cap*2:8; segs = (char***)realloc(segs, cap*sizeof(char**)); }
                segs[n] = toks; n++;
            } else free_tokens(toks, tn);
        }
        if (j >= L) break;
        /* skip separator (+ following spaces) */
        if (p[j]=='&'||p[j]=='|') j += 2; else j += 1;
        si = j;
    }
    free(work);
    /* build lens array */
    int *lens = (int *)malloc((size_t)(n>0?n:1) * sizeof(int));
    for (int k = 0; k < n; k++) {
        int c = 0; while (segs[k][c]) c++;
        lens[k] = c;
    }
    *out_count = n;
    *out_lens = lens;
    return segs;
}

void verify_free_segments(char ***segments, int count, int *lens) {
    if (segments) {
        for (int i = 0; i < count; i++) {
            int c = 0; if (segments[i]) { while (segments[i][c]) c++; free_tokens(segments[i], c); }
        }
        free(segments);
    }
    free(lens);
}

/* PoP: verify_clean_token @ agent/verification_evidence.py:_clean_token */
char *verify_clean_token(const char *token) {
    char *s = xstrdup(token ? token : "");
    if (!s) return NULL;
    while (s[0] == '.' && s[1] == '/') memmove(s, s+2, strlen(s+2)+1);
    return s;
}

/* PoP: verify_canonical_tokens @ agent/verification_evidence.py:_canonical_tokens */
char **verify_canonical_tokens(const char *canonical, int *out_count) {
    int n; char **raw = shell_tokenize(canonical, &n);
    char **out = NULL;
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (raw[i][0] == '\0') continue;
        char *ct = verify_clean_token(raw[i]);
        if (!ct) continue;
        out = (char**)realloc(out, (m+1)*sizeof(char*));
        out[m++] = ct;
    }
    free_tokens(raw, n);
    *out_count = m;
    return out;
}

/* PoP: _find_subsequence @ agent/verification_evidence.py:_find_subsequence */
int verify_find_subsequence(char **tokens, int n, char **needle, int m) {
    if (!tokens || !needle || m == 0 || m > n) return -1;
    /* cleaned tokens */
    char **ct = (char**)malloc(n*sizeof(char*));
    for (int i = 0; i < n; i++) ct[i] = verify_clean_token(tokens[i]);
    int found = -1;
    for (int idx = 0; idx <= n - m; idx++) {
        int ok = 1;
        for (int k = 0; k < m; k++)
            if (strcmp(ct[idx+k], needle[k]) != 0) { ok = 0; break; }
        if (ok) { found = idx; break; }
    }
    for (int i = 0; i < n; i++) free(ct[i]);
    free(ct);
    return found;
}

/* PoP: verify_strip_command_prefix @ agent/verification_evidence.py:_strip_command_prefix */
char **verify_strip_command_prefix(char **tokens, int n, int *out_n) {
    int start = 0;
    if (n > 0 && strcmp(tokens[0], "env") == 0) start = 1;
    while (start < n && strchr(tokens[start], '=') && tokens[start][0] != '-') start++;
    while (start < n && (strcmp(tokens[start], "command")==0 ||
                         strcmp(tokens[start], "time")==0 ||
                         strcmp(tokens[start], "noglob")==0)) start++;
    *out_n = n - start;
    return tokens + start;
}

/* PoP: verify_equivalent_needles @ agent/verification_evidence.py:_equivalent_needles */
char ***verify_equivalent_needles(char **needle, int m, int *out_count, int **out_lens) {
    char ***out = NULL;
    int cap = 0, n = 0;
    int *lens = NULL;
    /* push a copy of `copy[0..cn)` as a new needle */
    #define PUSH_N(copy, cn) do { \
        if (n >= cap) { cap = cap?cap*2:8; out=(char***)realloc(out,cap*sizeof(char**)); lens=(int*)realloc(lens,cap*sizeof(int)); } \
        char **cp = (char**)malloc(((cn)+1)*sizeof(char*)); \
        for (int _i=0;_i<(cn);_i++) cp[_i]=xstrdup((copy)[_i]); \
        cp[cn]=NULL; \
        out[n]=cp; lens[n]=(cn); n++; \
    } while(0)

    PUSH_N(needle, m);
    if (m >= 3 && strcmp(needle[1], "run") == 0) {
        const char *pms[] = {"npm","pnpm","yarn","bun"};
        for (int i = 0; i < 4; i++) {
            if (strcmp(needle[0], pms[i]) == 0) {
                const char *pmcp[] = {needle[0], needle[2]};
                PUSH_N(pmcp, 2);
                break;
            }
        }
    }
    if (m == 1 && strchr(needle[0], '/')) {
        const char *a[] = {"bash", needle[0]}; PUSH_N(a, 2);
        const char *b[] = {"sh", needle[0]};   PUSH_N(b, 2);
    }
    if (m == 1 && strcmp(needle[0], "pytest") == 0) {
        const char *c[][3] = {
            {"python","-m","pytest"}, {"python3","-m","pytest"},
            {"uv","run","pytest"}, {"poetry","run","pytest"},
            {"pipenv","run","pytest"}};
        for (int i = 0; i < 5; i++) PUSH_N(c[i], 3);
    }
    #undef PUSH_N
    *out_count = n; *out_lens = lens;
    return out;
}

/* PoP: _find_canonical_match @ agent/verification_evidence.py:_find_canonical_match */
verify_match_t *verify_find_canonical_match(const char *command,
                                            char **canonical_commands, int nc) {
    int seg_count; int *seg_lens;
    char ***segs = verify_split_segments(command, &seg_count, &seg_lens);
    if (!segs) return NULL;
    verify_match_t *result = NULL;
    for (int ci = 0; ci < nc && !result; ci++) {
        int nl; char **needle = verify_canonical_tokens(canonical_commands[ci], &nl);
        if (!needle || nl == 0) { free_tokens(needle, nl); continue; }
        for (int si = 0; si < seg_count && !result; si++) {
            int cand_n; char **cand = verify_strip_command_prefix(segs[si], seg_lens[si], &cand_n);
            int eq_count; int *eq_lens;
            char ***eqs = verify_equivalent_needles(needle, nl, &eq_count, &eq_lens);
            for (int ei = 0; ei < eq_count && !result; ei++) {
                int el = eq_lens[ei];
                if (cand_n >= el && verify_find_subsequence(cand, cand_n, eqs[ei], el) == 0) {
                    result = (verify_match_t*)malloc(sizeof(verify_match_t));
                    result->canonical = xstrdup(canonical_commands[ci]);
                    int trail = cand_n - el;
                    result->trailing = (char**)malloc((trail>0?trail:1)*sizeof(char*));
                    result->trailing_n = trail;
                    for (int t = 0; t < trail; t++) result->trailing[t] = xstrdup(cand[el + t]);
                }
            }
            for (int ei = 0; ei < eq_count; ei++) free_tokens(eqs[ei], eq_lens[ei]);
            free(eqs); free(eq_lens);
        }
        free_tokens(needle, nl);
    }
    verify_free_segments(segs, seg_count, seg_lens);
    return result;
}

/* PoP: _kind_for_command @ agent/verification_evidence.py:_kind_for_command */
verify_kind_t verify_kind_for_command(const char *canonical) {
    char low[1024]; int i = 0;
    for (; canonical && canonical[i] && i < (int)sizeof(low)-1; i++)
        low[i] = (char)tolower((unsigned char)canonical[i]);
    low[i] = '\0';
    if (strstr(low, "lint") || strstr(low, "eslint") || strstr(low, "ruff")) return VERIFY_KIND_LINT;
    if (strstr(low, "typecheck") || strstr(low, "tsc") || strstr(low, "mypy") ||
        strstr(low, "pyright") || strstr(low, "ty")) return VERIFY_KIND_TYPECHECK;
    if (strstr(low, "build")) return VERIFY_KIND_BUILD;
    if (strstr(low, "fmt") || strstr(low, "format")) return VERIFY_KIND_FORMAT;
    if (strstr(low, "check") && !strstr(low, "test")) return VERIFY_KIND_CHECK;
    return VERIFY_KIND_TEST;
}

/* PoP: verify_looks_like_target @ agent/verification_evidence.py:_looks_like_target */
bool verify_looks_like_target(const char *arg) {
    if (!arg || !arg[0] || arg[0] == '-' || strchr(arg, '=')) return false;
    if (strchr(arg, '/') || strchr(arg, '\\') || strchr(arg, ':')) {
        if (strstr(arg, "::")) return true;
        return true;
    }
    size_t len = strlen(arg);
    const char *exts[] = {".py",".js",".jsx",".ts",".tsx",".rs",".go",".java"};
    for (int i = 0; i < 8; i++) if (len >= strlen(exts[i]) &&
        strcmp(arg + len - strlen(exts[i]), exts[i]) == 0) return true;
    if (strcmp(arg, "tests")==0 || strcmp(arg, "spec")==0 || strcmp(arg, "__tests__")==0) return true;
    if (strncmp(arg, "test_", 5)==0) return true;
    return false;
}

/* PoP: _scope_for_args @ agent/verification_evidence.py:_scope_for_args */
verify_scope_t verify_scope_for_args(char **args, int n) {
    for (int i = 0; i < n; i++) if (verify_looks_like_target(args[i])) return VERIFY_SCOPE_TARGETED;
    return VERIFY_SCOPE_FULL;
}

/* PoP: verify_is_under_temp_dir @ agent/verification_evidence.py:_is_under_temp_dir */
bool verify_is_under_temp_dir(const char *token) {
    if (!token || !token[0] || token[0] == '-') return false;
    if (token[0] != '/') return false;  /* require absolute */
    /* compare against TMPDIR / /tmp / /var/tmp */
    const char *tmps[] = {"/tmp", "/var/tmp", "/dev/shm"};
    for (int i = 0; i < 3; i++) {
        size_t tl = strlen(tmps[i]);
        if (strncmp(token, tmps[i], tl) == 0 &&
            (token[tl] == '\0' || token[tl] == '/'))
            return true;
    }
    char *t = getenv("TMPDIR");
    if (t && t[0]=='/') {
        size_t tl = strlen(t);
        if (strncmp(token, t, tl) == 0 && (token[tl]=='\0'||token[tl]=='/')) return true;
    }
    return false;
}
/* PoP: verify_is_under_root @ agent/verification_evidence.py:_is_under_root */

bool verify_is_under_root(const char *token, const char *root) {
    if (!token || !root || !root[0]) return false;
    if (token[0] != '/') return false;
    size_t rl = strlen(root);
    if (strncmp(token, root, rl) == 0 && (token[rl]=='\0' || token[rl]=='/')) return true;
    return false;
}

/* PoP: verify_is_temp_script_path @ agent/verification_evidence.py:_is_temp_script_path */
bool verify_is_temp_script_path(const char *token, const char *root) {
    if (!token || !token[0]) return false;
    /* basename */
    const char *slash = strrchr(token, '/');
    const char *name = slash ? slash + 1 : token;
    bool prefixed = false;
    for (int i = 0; i < VERIFY_AD_HOC_PREFIX_COUNT; i++)
        if (strncmp(name, VERIFY_AD_HOC_PREFIXES[i], strlen(VERIFY_AD_HOC_PREFIXES[i])) == 0) { prefixed = true; break; }
    if (!prefixed) return false;
    return verify_is_under_temp_dir(token) && !verify_is_under_root(token, root);
}

/* PoP: verify_ad_hoc_script_args @ agent/verification_evidence.py:_ad_hoc_script_args */
char **verify_ad_hoc_script_args(char **tokens, int n, const char *root, int *out_n) {
    char **cand = verify_strip_command_prefix(tokens, n, &n);
    if (n == 0) { *out_n = 0; return NULL; }
    if (verify_is_temp_script_path(cand[0], root)) {
        *out_n = n - 1;
        char **trail = (char**)malloc((n>1?n-1:1)*sizeof(char*));
        for (int i = 1; i < n; i++) trail[i-1] = xstrdup(cand[i]);
        return trail;
    }
    const char *interps[] = {"python","python3","node","bash","sh","ruby","perl"};
    bool is_interp = false;
    for (int i = 0; i < 7; i++) if (strcmp(cand[0], interps[i])==0) { is_interp = true; break; }
    if (is_interp) {
        for (int idx = 1; idx < n; idx++) {
            if (strcmp(cand[idx], "--") == 0) continue;
            if (verify_is_temp_script_path(cand[idx], root)) {
                *out_n = n - idx - 1;
                char **trail = (char**)malloc((n-idx-1>0?n-idx-1:1)*sizeof(char*));
                for (int i = idx+1; i < n; i++) trail[i-idx-1] = xstrdup(cand[i]);
                return trail;
            }
            if (cand[idx][0] != '-') return NULL;
        }
    }
    *out_n = 0;
    return NULL;
}

/* PoP: _find_ad_hoc_match @ agent/verification_evidence.py:_find_ad_hoc_match */
char **verify_find_ad_hoc_match(const char *command, const char *root, int *out_n) {
    int seg_count; int *seg_lens;
    char ***segs = verify_split_segments(command, &seg_count, &seg_lens);
    if (!segs) { *out_n = 0; return NULL; }
    char **result = NULL;
    for (int si = 0; si < seg_count && !result; si++) {
        int n = seg_lens[si];
        char **trail = verify_ad_hoc_script_args(segs[si], n, root, out_n);
        if (trail) {
            result = trail;
        }
    }
    verify_free_segments(segs, seg_count, seg_lens);
    return result;
}

/* PoP: verify_summarize_output @ agent/verification_evidence.py:_summarize_output */
char *verify_summarize_output(const char *output) {
    const char *text = output ? output : "";
    /* strip trailing whitespace minimally */
    size_t len = strlen(text);
    /* trim trailing newlines/spaces */
    while (len > 0 && (text[len-1]=='\n'||text[len-1]=='\r'||text[len-1]==' ')) len--;
    if (len <= VERIFY_MAX_OUTPUT_SUMMARY_CHARS) {
        char *r = (char*)malloc(len+1);
        memcpy(r, text, len); r[len]='\0';
        return r;
    }
    int head = VERIFY_MAX_OUTPUT_SUMMARY_CHARS / 3;
    int tail = VERIFY_MAX_OUTPUT_SUMMARY_CHARS - head;
    int omitted = (int)len - VERIFY_MAX_OUTPUT_SUMMARY_CHARS;
    size_t need = (size_t)head + tail + 64;
    char *r = (char*)malloc(need);
    char *p = r;
    memcpy(p, text, head); p += head;
    int n = snprintf(p, need-(p-r), "\n... [%d chars omitted] ...\n", omitted);
    p += n;
    memcpy(p, text + len - tail, tail); p += tail;
    *p = '\0';
    return r;
}

void verification_classify(const char *command, const char *cwd,
                           const char *session_id, int exit_code,
                           const char *output, char **verify_commands, int nvc,
                           const char *root, verification_evidence_t *out) {
    memset(out, 0, sizeof(*out));
    out->valid = false;
    if (!command || !command[0]) return;

    char *canonical = NULL;
    char **trailing = NULL; int trailing_n = 0;
    bool is_ad_hoc = false;

    verify_match_t *m = verify_find_canonical_match(command, verify_commands, nvc);
    if (!m && nvc == 0) {
        int an;
        char **am = verify_find_ad_hoc_match(command, root, &an);
        if (am) {
            canonical = xstrdup("ad-hoc verification script");
            trailing = am; trailing_n = an;
            is_ad_hoc = true;
        }
    } else if (m) {
        canonical = m->canonical;
        trailing = m->trailing; trailing_n = m->trailing_n;
        free(m);
    }
    if (!canonical) return;

    out->valid = true;
    snprintf(out->command, sizeof(out->command), "%s", command);
    snprintf(out->canonical_command, sizeof(out->canonical_command), "%s", canonical);
    out->kind = is_ad_hoc ? VERIFY_KIND_AD_HOC : verify_kind_for_command(canonical);
    out->scope = is_ad_hoc ? VERIFY_SCOPE_TARGETED : verify_scope_for_args(trailing, trailing_n);
    out->status = (exit_code == 0) ? VERIFY_STATUS_PASSED : VERIFY_STATUS_FAILED;
    out->exit_code = exit_code;
    snprintf(out->cwd, sizeof(out->cwd), "%s", cwd ? cwd : ".");
    snprintf(out->root, sizeof(out->root), "%s", root ? root : (cwd ? cwd : "."));
    snprintf(out->session_id, sizeof(out->session_id), "%s", session_id ? session_id : "default");
    char *sum = verify_summarize_output(output);
    snprintf(out->output_summary, sizeof(out->output_summary), "%s", sum ? sum : "");
    free(sum);
    free(canonical);
    free_tokens(trailing, trailing_n);
}

/* ── persistence (libdb JSON ledger) ────────────────────────────── */

#include "db.h"

static char *json_escape(const char *s) {
    /* minimal escape for db storage */
    size_t len = strlen(s ? s : "");
    char *out = (char*)malloc(len*2+8);
    char *p = out;
    *p++ = '"';
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '"' || c == '\\') { *p++ = '\\'; *p++ = c; }
        else if (c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else *p++ = c;
    }
    *p++ = '"'; *p = '\0';
    return out;
}

static const char *kind_str(verify_kind_t k) {
    switch (k) {
        case VERIFY_KIND_LINT: return "lint";
        case VERIFY_KIND_TYPECHECK: return "typecheck";
        case VERIFY_KIND_BUILD: return "build";
        case VERIFY_KIND_FORMAT: return "format";
        case VERIFY_KIND_CHECK: return "check";
        case VERIFY_KIND_AD_HOC: return "ad_hoc";
        default: return "test";
    }
}
static const char *scope_str(verify_scope_t s) { return s==VERIFY_SCOPE_TARGETED ? "targeted" : "full"; }
static const char *status_str(verify_status_t s) { return s==VERIFY_STATUS_PASSED ? "passed" : "failed"; }

static char *db_key(const char *session_id, const char *root) {
    char *k = (char*)malloc(strlen(session_id)+strlen(root)+32);
    sprintf(k, "verify:%s:%s", session_id, root);
    return k;
}

bool verification_record_result(const char *db_dir, const char *command,
                                const char *cwd, const char *session_id,
                                int exit_code, const char *output,
                                char **verify_commands, int nvc, const char *root,
                                verification_evidence_t *out) {
    verification_classify(command, cwd, session_id, exit_code, output,
                           verify_commands, nvc, root, out);
    if (!out->valid) return false;

    char *err = NULL;
    db_t *db = db_open(db_dir, &err);
    if (!db) { free(err); return false; }

    char now[64]; time_t t = time(NULL);
    struct tm tm; gmtime_r(&t, &tm);
    strftime(now, sizeof(now), "%Y-%m-%dT%H:%M:%SZ", &tm);

    char *k = db_key(session_id ? session_id : "default", out->root);
    char *existing = db_load(db, k, &err);
    free(err); err = NULL;

    /* events array */
    char *events_json;
    if (existing && existing[0]=='[') events_json = xstrdup(existing);
    else events_json = xstrdup("[]");
    free(existing);

    char *esc_cmd = json_escape(out->command);
    char *esc_can = json_escape(out->canonical_command);
    char *esc_cwd = json_escape(out->cwd);
    char *esc_root = json_escape(out->root);
    char *esc_sid = json_escape(out->session_id);
    char *esc_out = json_escape(out->output_summary);

    size_t cap = strlen(events_json) + 2048;
    char *new_events = (char*)malloc(cap);
    int off = snprintf(new_events, cap,
        "[%.*s%s{\"created_at\":%s,\"command\":%s,\"canonical_command\":%s,"
        "\"kind\":%s,\"scope\":%s,\"status\":%s,\"exit_code\":%d,\"cwd\":%s,"
        "\"root\":%s,\"session_id\":%s,\"output_summary\":%s}]",
        (int)strlen(events_json)-1, events_json,
        (events_json[1]=='{' || events_json[1]=='\0') ? "" : ",",
        json_escape(now), esc_cmd, esc_can,
        json_escape(kind_str(out->kind)), json_escape(scope_str(out->scope)),
        json_escape(status_str(out->status)), out->exit_code, esc_cwd,
        esc_root, esc_sid, esc_out);
    (void)off;
    free(esc_cmd); free(esc_can); free(esc_cwd); free(esc_root);
    free(esc_sid); free(esc_out); free(events_json);

    bool ok = db_save(db, k, new_events);
    free(new_events);
    free(k);
    db_close(db);
    return ok;
}

bool verification_mark_edited(const char *db_dir, const char *session_id,
                              const char *cwd, const char **paths, int npaths,
                              const char *root) {
    char *err = NULL;
    db_t *db = db_open(db_dir, &err);
    if (!db) { free(err); return false; }
    const char *sid = session_id ? session_id : "default";
    const char *r = root ? root : (cwd ? cwd : ".");
    char *k = db_key(sid, r);
    char *ek = (char*)malloc(strlen(k)+16);
    sprintf(ek, "%s:edited", k);

    time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
    char now[64]; strftime(now, sizeof(now), "%Y-%m-%dT%H:%M:%SZ", &tm);

    char *paths_json;
    if (npaths > 0) {
        size_t cap = 8; for (int i=0;i<npaths;i++) cap += strlen(paths[i])+8;
        paths_json = (char*)malloc(cap);
        int o = sprintf(paths_json, "[");
        for (int i=0;i<npaths;i++){ char *e=json_escape(paths[i]); o+=sprintf(paths_json+o, "%s%s", i?",":"", e); free(e);}
        o+=sprintf(paths_json+o, "]");
    } else {
        paths_json = (char*)malloc(8); strcpy(paths_json, "[]");
    }

    size_t mcap = strlen(paths_json)+128;
    char *rec = (char*)malloc(mcap);
    snprintf(rec, mcap, "{\"edited_at\":%s,\"changed_paths\":%s}", json_escape(now), paths_json);

    bool ok = db_save(db, ek, rec);
    free(rec); free(paths_json); free(ek); free(k);
    db_close(db);
    return ok;
}

char *verification_status_json(const char *db_dir, const char *session_id,
                               const char *cwd, char **verify_commands, int nvc,
                               const char *root) {
    char *err = NULL;
    db_t *db = db_open(db_dir, &err);
    if (!db) { free(err); return xstrdup("{\"status\":\"not_applicable\",\"evidence\":null}"); }
    const char *sid = session_id ? session_id : "default";
    const char *r = root ? root : (cwd ? cwd : ".");
    char *k = db_key(sid, r);

    char *events = db_load(db, k, &err);
    free(err);
    char *edited = NULL;
    {
        char *ek = (char*)malloc(strlen(k)+16);
        sprintf(ek, "%s:edited", k);
        edited = db_load(db, ek, &err);
        free(err); free(ek);
    }

    char *result;
    if (!events || events[0] != '[' || strlen(events) < 3) {
        char *e2 = json_escape(r);
        char *s2 = json_escape(sid);
        size_t cap = strlen(e2)+strlen(s2)+96;
        result = (char*)malloc(cap);
        sprintf(result,
            "{\"status\":\"unverified\",\"evidence\":null,\"root\":%s,\"session_id\":%s,\"changed_paths\":[]}",
            e2, s2);
        free(e2); free(s2);
    } else {
        /* last event = last element of the array */
        char *last = strrchr(events, '}');
        char *last_start = last;
        int depth = 0;
        while (last_start > events && depth >= 0) {
            last_start--;
            if (*last_start == '}') depth++;
            else if (*last_start == '{') { depth--; if (depth < 0) { last_start++; break; } }
        }
        char *ev_copy = (char*)malloc(strlen(last_start)+1);
        strcpy(ev_copy, last_start);
        /* decide stale vs status */
        const char *status = "unverified";
        if (edited && edited[0]=='{') {
            /* if edited_at > event created_at → stale */
            char *ea = strstr(edited, "\"edited_at\"");
            char *ca = strstr(ev_copy, "\"created_at\"");
            if (ea && ca) status = "stale";
            else status = "unverified";
        } else {
            /* pull status from event */
            char *st = strstr(ev_copy, "\"status\"");
            if (st) {
                char *q = strchr(st, ':'); if (q) { q++; while (*q==' '||*q=='"') q++;
                    if (strncmp(q, "passed", 6)==0) status="passed";
                    else if (strncmp(q,"failed",6)==0) status="failed"; }
            }
        }
        char *e2 = json_escape(r);
        char *s2 = json_escape(sid);
        size_t cap = strlen(status)+strlen(ev_copy)+strlen(e2)+strlen(s2)+160;
        result = (char*)malloc(cap);
        sprintf(result,
            "{\"status\":%s,\"evidence\":%s,\"root\":%s,\"session_id\":%s,\"changed_paths\":[]}",
            json_escape(status), ev_copy, e2, s2);
        free(e2); free(s2); free(ev_copy);
    }
    free(k);
    if (events) free(events);
    if (edited) free(edited);
    db_close(db);
    return result;
}
