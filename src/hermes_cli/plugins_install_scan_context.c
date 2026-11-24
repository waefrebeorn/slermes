/*
 * plugins_install_scan_context.c — C11 home for upstream Hermes
 * hermes_cli/plugins_install_scan_context.py (gates w29_pluginsins_4e3ca4_p1,
 * w29_pluginsins_4e3ca4_p2).
 *
 * Upstream lowers a finding's severity when the matched text sits in context
 * that cannot run on the installing host: README prose, a forge CI workflow, a
 * locale catalog, a test tree, quoted test data, base64 media, JSON prose, a
 * build script's repo-root climb, an advisory `sudo` hint, a safety-negation
 * clause, and so on.  Nothing is deleted; every helper is a pure predicate on
 * (finding, line, path).
 *
 * PoP: hermes_cli_plugins_install_scan_context.is_doc_prose @ hermes_cli/plugins_install_scan_context.py:50
 * PoP: hermes_cli_plugins_install_scan_context.is_ci_workflow @ hermes_cli/plugins_install_scan_context.py:66
 * PoP: hermes_cli_plugins_install_scan_context.is_agent_facing @ hermes_cli/plugins_install_scan_context.py:78
 * PoP: hermes_cli_plugins_install_scan_context.prose_cap @ hermes_cli/plugins_install_scan_context.py:84
 * PoP: hermes_cli_plugins_install_scan_context.is_trusted_installer_pipe @ hermes_cli/plugins_install_scan_context.py:114
 * PoP: hermes_cli_plugins_install_scan_context.is_locale_catalog @ hermes_cli/plugins_install_scan_context.py:137
 * PoP: hermes_cli_plugins_install_scan_context.catalog_cap @ hermes_cli/plugins_install_scan_context.py:143
 * PoP: hermes_cli_plugins_install_scan_context.is_self_uninstall_doc @ hermes_cli/plugins_install_scan_context.py:191
 * PoP: hermes_cli_plugins_install_scan_context.is_test_tree @ hermes_cli/plugins_install_scan_context.py:279
 * PoP: hermes_cli_plugins_install_scan_context.is_disclosed_config_note @ hermes_cli/plugins_install_scan_context.py:563
 * PoP: hermes_cli_plugins_install_scan_context.is_json_prose_value @ hermes_cli/plugins_install_scan_context.py:714
 * PoP: hermes_cli_plugins_install_scan_context.is_repo_root_traversal @ hermes_cli/plugins_install_scan_context.py:506
 * PoP: hermes_cli_plugins_install_scan_context.is_pip_install_in_prose_literal @ hermes_cli/plugins_install_scan_context.py:624
 * PoP: hermes_cli_plugins_install_scan_context.is_hex_in_char_class @ hermes_cli/plugins_install_scan_context.py:654
 * PoP: hermes_cli_plugins_install_scan_context.logical_line @ hermes_cli/plugins_install_scan_context.py:676
 * PoP: hermes_cli_plugins_install_scan_context.is_inert_fixture_line @ hermes_cli/plugins_install_scan_context.py:255
 * PoP: hermes_cli_plugins_install_scan_context.is_base64_media @ hermes_cli/plugins_install_scan_context.py:309
 * PoP: hermes_cli_plugins_install_scan_context.is_base64_archive_roundtrip @ hermes_cli/plugins_install_scan_context.py:330
 * PoP: hermes_cli_plugins_install_scan_context.is_regex_alternation_token @ hermes_cli/plugins_install_scan_context.py:375
 * PoP: hermes_cli_plugins_install_scan_context.is_sudo_inert @ hermes_cli/plugins_install_scan_context.py:426
 * PoP: hermes_cli_plugins_install_scan_context.is_env_dump_in_safety_clause @ hermes_cli/plugins_install_scan_context.py:459
 * PoP: hermes_cli_plugins_install_scan_context.is_quoted_injection_phrase @ hermes_cli/plugins_install_scan_context.py:478
 * PoP: hermes_cli_plugins_install_scan_context.is_benign_lazy_os_import @ hermes_cli/plugins_install_scan_context.py:539
 * PoP: hermes_cli_plugins_install_scan_context.is_authorized_keys_prose @ hermes_cli/plugins_install_scan_context.py:584
 * PoP: hermes_cli_plugins_install_scan_context.is_loopback_only @ hermes_cli/plugins_install_scan_context.py:604
 * PoP: hermes_cli_plugins_install_scan_context.is_loopback_continuation @ hermes_cli/plugins_install_scan_context.py:686
 * PoP: hermes_cli_plugins_install_scan_context.is_coin_name_only @ hermes_cli/plugins_install_scan_context.py:752
 * PoP: hermes_cli_plugins_install_scan_context.is_public_client_key @ hermes_cli/plugins_install_scan_context.py:772
 * PoP: hermes_cli_plugins_install_scan_context.is_own_proc_read @ hermes_cli/plugins_install_scan_context.py:795
 *
 * SEAMS (both gates)
 *  - Filesystem: never read.  ``rel_path`` arrives as a posix relative path
 *    from the fixture; Path.parts / .name / .suffix are reproduced here.
 *  - Finding: reduced to (pattern_id, severity, category) — exactly the three
 *    fields these predicates read.
 *  - Threat regex table: the module keys on tools.skills_guard's compiled
 *    patterns to locate a finding's own token on the line.  The PATTERN TABLE
 *    below carries the subset of those pattern texts these predicates use,
 *    matched with Python's leftmost-first, non-overlapping finditer order and
 *    its greedy-quantifier backtracking.  A pattern id outside the table
 *    yields no hits, so the predicate answers "not inert" (the conservative
 *    answer).  ``base64_decode_pipe`` is deliberately absent: no predicate
 *    scans with it (they key on the id, not the regex).
 *  - ASCII domain: fixtures are ASCII; the unicode quote pairs of the
 *    _QUOTED_SPAN class («» “” ‘’) are out of scope.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w29_pluginsins_4e3ca4_p1.h"
#include "noc_w29_pluginsins_4e3ca4_p2.h"

/* ────────────────────────────── spans ─────────────────────────────────── */
#define PIS_MAX_SPANS 512

typedef struct { int a, b; } pis_span;
typedef struct { pis_span v[PIS_MAX_SPANS]; int n; } pis_spans;

static void pis_add(pis_spans *sp, int a, int b)
{
    if (sp->n < PIS_MAX_SPANS) { sp->v[sp->n].a = a; sp->v[sp->n].b = b; sp->n++; }
}

/* ─────────────────────────── text helpers ─────────────────────────────── */
static int pis_lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int pis_is_digit(int c) { return c >= '0' && c <= '9'; }
static int pis_is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int pis_is_alnum(int c) { return pis_is_alpha(c) || pis_is_digit(c); }
static int pis_is_word(int c) { return pis_is_alnum(c) || c == '_'; }
static int pis_is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static int pis_is_hspace(int c) { return c == ' ' || c == '\t'; }
static int pis_len(const char *s) { return s ? (int)strlen(s) : 0; }

static int pis_ci_at(const char *s, int i, const char *kw)
{
    int k = 0;
    for (; kw[k]; k++) {
        if (!s[i + k]) return 0;
        if (pis_lc((unsigned char)s[i + k]) != pis_lc((unsigned char)kw[k])) return 0;
    }
    return 1;
}

static int pis_at(const char *s, int i, const char *lit) { return strncmp(s + i, lit, strlen(lit)) == 0; }

static int pis_find_ci(const char *s, int from, const char *kw)
{
    int n = pis_len(s);
    for (int i = from; i < n; i++)
        if (pis_ci_at(s, i, kw)) return i;
    return -1;
}

static int pis_has_ci(const char *s, const char *kw) { return pis_find_ci(s, 0, kw) >= 0; }

/* \b<word> start: previous char is not a word char (or the line starts) */
static int pis_before_boundary(const char *s, int i) { return i == 0 || !pis_is_word((unsigned char)s[i - 1]); }
/* \b<word> end: next char is not a word char (or the line ends) */
static int pis_after_boundary(const char *s, int i) { return !s[i] || !pis_is_word((unsigned char)s[i]); }

/* ^\s* */
static int pis_skip_ws(const char *s, int i)
{
    while (pis_is_space((unsigned char)s[i])) i++;
    return i;
}
/* \s+ */
static int pis_eat_ws(const char *s, int i)
{
    int j = pis_skip_ws(s, i);
    return j > i ? j : -1;
}
/* [ \t]+ */
static int pis_eat_hspace(const char *s, int i)
{
    int j = i;
    while (pis_is_hspace((unsigned char)s[j])) j++;
    return j > i ? j : -1;
}

/* ───────────────────────────── path algebra ───────────────────────────── */
typedef struct {
    const char *seg[64];
    int len[64];
    int n;
    const char *name;
    int namelen;
    const char *suf;
    int suflen;
} pis_path;

static void pis_path_init(pis_path *p, const char *rel)
{
    const char *s = rel ? rel : "";
    int n = (int)strlen(s), i = 0;
    memset(p, 0, sizeof(*p));
    while (i < n) {
        while (i < n && s[i] == '/') i++;
        if (i >= n) break;
        int start = i;
        while (i < n && s[i] != '/') i++;
        if (p->n < 64) {
            p->seg[p->n] = s + start;
            p->len[p->n] = i - start;
            p->n++;
        }
    }
    if (p->n > 0) {
        p->name = p->seg[p->n - 1];
        p->namelen = p->len[p->n - 1];
    } else {
        p->name = "";
        p->namelen = 0;
    }
    /* PurePath.suffix: last '.', not at index 0, not the final char */
    p->suf = NULL;
    p->suflen = 0;
    for (int k = p->namelen - 1; k > 0; k--) {
        if (p->name[k] == '.') {
            if (k == p->namelen - 1) break;      /* 'x.' has no suffix */
            p->suf = p->name + k;
            p->suflen = p->namelen - k;
            break;
        }
    }
}

static int pis_seg_is(const pis_path *p, int idx, const char *lit)
{
    size_t l = strlen(lit);
    if (idx < 0 || idx >= p->n || (size_t)p->len[idx] != l) return 0;
    for (size_t k = 0; k < l; k++)
        if (pis_lc((unsigned char)p->seg[idx][k]) != pis_lc((unsigned char)lit[k])) return 0;
    return 1;
}

static int pis_suf_is(const pis_path *p, const char *lit)
{
    size_t l = strlen(lit);
    if (!p->suf || (size_t)p->suflen != l) return 0;
    for (size_t k = 0; k < l; k++)
        if (pis_lc((unsigned char)p->suf[k]) != pis_lc((unsigned char)lit[k])) return 0;
    return 1;
}

static int pis_name_is(const pis_path *p, const char *lit)
{
    size_t l = strlen(lit);
    if ((size_t)p->namelen != l) return 0;
    for (size_t k = 0; k < l; k++)
        if (pis_lc((unsigned char)p->name[k]) != pis_lc((unsigned char)lit[k])) return 0;
    return 1;
}

/* ─────────────────────── literal / regex-literal spans ─────────────────── */
/*
 * _LITERAL_SPANS:
 *   [rRbBuUfF]{0,2}"(?:[^"\\\n]|\\.)*"     (same for '...' and `...`)
 * | (?<![\w)\]])/(?:[^/\\\n\[]|\\.|\[(?:[^\]\\\n]|\\.)*\])+/[dgimsuvy]*(?![A-Za-z])
 */
static int pis_prefix_char(int c) { return c && strchr("rRbBuUfF", c) != NULL; }

static int pis_str_lit_at(const char *s, int i, int *pa, int *pb)
{
    int n = pis_len(s);
    for (int k = 2; k >= 0; k--) {
        if (k >= 1 && !pis_prefix_char((unsigned char)s[i])) continue;
        if (k == 2 && !pis_prefix_char((unsigned char)s[i + 1])) continue;
        int q = i + k;
        if (q >= n) continue;
        char quote = s[q];
        if (quote != '"' && quote != '\'' && quote != '`') continue;
        int j = q + 1, closed = 0;
        while (j < n) {
            char c = s[j];
            if (c == '\\') {
                if (j + 1 < n && s[j + 1] != '\n') { j += 2; continue; }
                return 0;
            }
            if (c == quote) { closed = 1; break; }
            if (c == '\n') return 0;
            j++;
        }
        if (!closed) return 0;
        *pa = i;
        *pb = j + 1;
        return 1;
    }
    return 0;
}

static int pis_rx_lit_at(const char *s, int i, int *pa, int *pb)
{
    int n = pis_len(s);
    if (s[i] != '/') return 0;
    if (i > 0) {
        char p = s[i - 1];
        if (pis_is_word((unsigned char)p) || p == ')' || p == ']') return 0;
    }
    int j = i + 1, body = 0;
    for (;;) {
        if (j >= n) return 0;
        char c = s[j];
        if (c == '/') break;
        if (c == '\\') {
            if (j + 1 < n && s[j + 1] != '\n') { j += 2; body++; continue; }
            return 0;
        }
        if (c == '[') {
            int k = j + 1, ok = 0;
            while (k < n) {
                char d = s[k];
                if (d == ']') { ok = 1; break; }
                if (d == '\\') {
                    if (k + 1 < n && s[k + 1] != '\n') { k += 2; continue; }
                    return 0;
                }
                if (d == '\n') return 0;
                k++;
            }
            if (!ok) return 0;
            j = k + 1;
            body++;
            continue;
        }
        if (c == '\n') return 0;
        j++;
        body++;
    }
    if (body == 0) return 0;
    j++;
    while (j < n && s[j] && strchr("dgimsuvy", s[j])) j++;
    if (j < n && pis_is_alpha((unsigned char)s[j])) return 0;
    *pa = i;
    *pb = j;
    return 1;
}

static void pis_literal_spans(const char *s, pis_spans *out)
{
    int n = pis_len(s), i = 0, a, b;
    out->n = 0;
    while (i < n) {
        if (pis_str_lit_at(s, i, &a, &b) || pis_rx_lit_at(s, i, &a, &b)) {
            pis_add(out, a, b);
            i = b;
        } else {
            i++;
        }
    }
}

static int pis_span_of(const pis_spans *sp, int st, int en)
{
    for (int k = 0; k < sp->n; k++)
        if (sp->v[k].a <= st && en <= sp->v[k].b) return k;
    return -1;
}

static int pis_in_spans(const pis_spans *sp, int st, int en) { return pis_span_of(sp, st, en) >= 0; }

static int pis_span_starting_at(const pis_spans *sp, int at)
{
    for (int k = 0; k < sp->n; k++)
        if (sp->v[k].a == at) return k;
    return -1;
}

/* ───────────────────────── exec detection seam ─────────────────────────── */
/*
 * _EXEC_ON_LINE / _EXEC_CALL_OPEN share this alternation:
 *   \b(?:system|popen|run|call|check_output|check_call|Popen|exec|execv\w*|
 *      spawn\w*|eval|execSync|execFile\w*|spawnSync|child_process|source|
 *      os\.startfile|open)\s*\( | \$\( | (?<![\w\\])`
 */
static const char *const PIS_EXEC_VERBS[] = {
    "system", "popen", "run", "call", "check_output", "check_call", "Popen", "exec",
    "spawn", "eval", "execSync", "execFile", "spawnSync", "child_process", "source",
    "os.startfile", "open", NULL
};
static const char *const PIS_EXEC_VERBS_W[] = { "execv", "spawn", "execFile", NULL };

/* verb + \s*\(\s*  -> position after the open paren and whitespace, else -1.
 * Alternatives are tried in source order and the first one followed by "("
 * wins, which is what Python's alternation backtracking produces. */
static int pis_exec_call_open_end(const char *s, int i)
{
    if (!pis_before_boundary(s, i)) return -1;
    for (int k = 0; PIS_EXEC_VERBS[k]; k++) {
        const char *v = PIS_EXEC_VERBS[k];
        if (!pis_ci_at(s, i, v)) continue;
        int e = i + (int)strlen(v), starred = 0;
        for (int m = 0; PIS_EXEC_VERBS_W[m]; m++)
            if (!strcmp(PIS_EXEC_VERBS_W[m], v)) starred = 1;
        if (starred) {
            int r = e;
            while (pis_is_word((unsigned char)s[r])) r++;
            if (r > e) e = r;
        }
        int j = pis_skip_ws(s, e);
        if (s[j] != '(') continue;
        return pis_skip_ws(s, j + 1);
    }
    return -1;
}

static void pis_exec_call_open_spans(const char *s, pis_spans *out)
{
    int n = pis_len(s), i = 0;
    out->n = 0;
    while (i < n) {
        int end = pis_exec_call_open_end(s, i);
        if (end >= 0) { pis_add(out, i, end); i = end > i ? end : i + 1; }
        else i++;
    }
}

static int pis_exec_on_line(const char *s)
{
    int n = pis_len(s);
    for (int i = 0; i < n; i++) {
        if (pis_exec_call_open_end(s, i) >= 0) return 1;
        if (s[i] == '$' && s[i + 1] == '(') return 1;
        if (s[i] == '`' && (i == 0 || (!pis_is_word((unsigned char)s[i - 1]) && s[i - 1] != '\\')))
            return 1;
    }
    return 0;
}

/* _LITERAL_SPANS.sub(<keep the quotes, blank the interior>, line) */
static void pis_strip_literals(const char *s, char *out, size_t cap)
{
    pis_spans sp;
    int n = pis_len(s);
    size_t w = 0;
    int cur = 0;
    pis_literal_spans(s, &sp);
    for (int k = 0; k < sp.n && w + 2 < cap; k++) {
        for (; cur < sp.v[k].a && w + 1 < cap; cur++) out[w++] = s[cur];
        if (sp.v[k].b > sp.v[k].a) {
            out[w++] = s[sp.v[k].a];
            for (int q = sp.v[k].a + 1; q < sp.v[k].b - 1 && w + 2 < cap; q++) out[w++] = ' ';
            out[w++] = s[sp.v[k].b - 1];
        }
        cur = sp.v[k].b;
    }
    for (; cur < n && w + 1 < cap; cur++) out[w++] = s[cur];
    out[w] = '\0';
}

static int pis_suffix_in(const pis_path *p, const char *const *set)
{
    for (int k = 0; set[k]; k++)
        if (pis_suf_is(p, set[k])) return 1;
    return 0;
}

static int pis_suffix_quotes_never_execute(const pis_path *p)
{
    static const char *const set[] = { ".py", ".pyi", ".js", ".mjs", ".cjs", ".jsx",
                                       ".ts", ".mts", ".cts", ".tsx", NULL };
    return pis_suffix_in(p, set);
}

/* _executes(line, rel_path) */
static int pis_executes(const char *line, const char *rel_path)
{
    static char buf[65536];
    pis_path p;
    pis_path_init(&p, rel_path);
    if (pis_suffix_quotes_never_execute(&p)) {
        pis_strip_literals(line ? line : "", buf, sizeof(buf));
        return pis_exec_on_line(buf);
    }
    return pis_exec_on_line(line ? line : "");
}

/* ─────────────────────────── base64 helpers ────────────────────────────── */
static int pis_b64val(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64.b64decode(head, validate=True); decoded length or -1 on error */
static int pis_b64decode_strict(const char *s, int n, unsigned char *out, int cap)
{
    int o = 0;
    if (n % 4 != 0) return -1;
    for (int i = 0; i < n; i++) {
        if (s[i] != '=' && pis_b64val((unsigned char)s[i]) < 0) return -1;
        if (s[i] == '=' && (s[i + 1] != '=' && s[i + 1] != '\0' && n == i + 1)) return -1;
    }
    /* '=' may only appear in the final one or two positions */
    for (int i = 0; i < n; i++) {
        if (s[i] == '=' && i < n - 2) return -1;
    }
    for (int i = 0; i < n; i += 4) {
        int v[4], dat = 0;
        for (int k = 0; k < 4; k++) {
            if (s[i + k] == '=') v[k] = 0;
            else { v[k] = pis_b64val((unsigned char)s[i + k]); dat++; }
        }
        unsigned t = ((unsigned)v[0] << 18) | ((unsigned)v[1] << 12) | ((unsigned)v[2] << 6) | (unsigned)v[3];
        if (o < cap) out[o++] = (unsigned char)((t >> 16) & 0xff);
        if (dat >= 3 && o < cap) out[o++] = (unsigned char)((t >> 8) & 0xff);
        if (dat >= 4 && o < cap) out[o++] = (unsigned char)(t & 0xff);
    }
    return o;
}

/* Media/document magic numbers; the length is explicit so a leading NUL byte
 * (the true-type/sqlite headers) is not mistaken for an empty prefix. */
typedef struct { const char *p; int len; } pis_magic;

static const pis_magic PIS_MEDIA_MAGIC[] = {
    { "\x89PNG", 4 }, { "\xff\xd8\xff", 3 }, { "GIF8", 4 }, { "RIFF", 4 },
    { "wOFF", 4 }, { "wOF2", 4 }, { "OTTO", 4 }, { "\x00\x01\x00\x00", 4 },
    { "ttcf", 4 }, { "BM", 2 }, { "%PDF", 4 }, { "\x00\x00\x01\x00", 4 },
    { "<svg", 4 }, { "<?xml", 5 }, { "ID3", 3 }, { "OggS", 4 },
    { "fLaC", 4 }, { "\x1a\x45\xdf\xa3", 4 }, { NULL, 0 }
};

/* ══════════════════ PATTERN TABLE (tools.skills_guard subset) ═════════════ */

/* \bsudo\b(?!\.\w)  (dotted_guard=0 gives the bare \bsudo\b token) */
static int pis_m_sudo(const char *s, pis_spans *o, int dotted_guard)
{
    int n = pis_len(s);
    o->n = 0;
    for (int i = 0; i < n; i++) {
        if (!pis_ci_at(s, i, "sudo") || !pis_before_boundary(s, i)) continue;
        int e = i + 4;
        if (!pis_after_boundary(s, e)) continue;
        if (dotted_guard && s[e] == '.' && pis_is_word((unsigned char)s[e + 1])) continue;
        pis_add(o, i, e);
        i = e - 1;
    }
    return o->n;
}

/* \bmkfs\b */
static int pis_m_mkfs(const char *s, pis_spans *o)
{
    int n = pis_len(s);
    o->n = 0;
    for (int i = 0; i < n; i++) {
        if (!pis_ci_at(s, i, "mkfs") || !pis_before_boundary(s, i)) continue;
        int e = i + 4;
        if (!pis_after_boundary(s, e)) continue;
        pis_add(o, i, e);
        i = e - 1;
    }
    return o->n;
}

/* printenv|(?<![A-Za-z0-9_])env\s*\|(?!\|)(?!\s*[{(]) */
static int pis_m_dump_env(const char *s, pis_spans *o)
{
    int n = pis_len(s);
    o->n = 0;
    for (int i = 0; i < n; i++) {
        if (pis_ci_at(s, i, "printenv")) { pis_add(o, i, i + 8); i += 7; continue; }
        if (pis_ci_at(s, i, "env") && (i == 0 || !pis_is_word((unsigned char)s[i - 1]))) {
            int j = pis_skip_ws(s, i + 3);
            if (s[j] != '|' || s[j + 1] == '|') continue;
            int k = pis_skip_ws(s, j + 1);
            if (s[k] == '{' || s[k] == '(') continue;
            pis_add(o, i, j + 1);
            i = j;
        }
    }
    return o->n;
}

/* _PATTERN_TOKEN["dump_all_env"] = printenv|env\s*\|  (no lookarounds) */
static int pis_m_dump_env_token(const char *s, pis_spans *o)
{
    int n = pis_len(s);
    o->n = 0;
    for (int i = 0; i < n; i++) {
        if (pis_ci_at(s, i, "printenv")) { pis_add(o, i, i + 8); i += 7; continue; }
        if (pis_ci_at(s, i, "env") && (i == 0 || !pis_is_word((unsigned char)s[i - 1]))) {
            int j = pis_skip_ws(s, i + 3);
            if (s[j] == '|') { pis_add(o, i, j + 1); i = j; }
        }
    }
    return o->n;
}

/* \.\./\.\./\.\. */
static int pis_m_traversal_deep(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i + 8 <= n) {
        if (!strncmp(s + i, "../../..", 8)) { pis_add(o, i, i + 8); i += 8; }
        else i++;
    }
    return o->n;
}

/* (?:\.\./)+\.\.  (the repo-root climb run; greedy with backtracking) */
static int pis_m_traversal_run(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        int ends[64], cnt = 0, j = i, matched = 0;
        while (cnt < 64 && j + 3 <= n && !strncmp(s + j, "../", 3)) { j += 3; ends[cnt++] = j; }
        for (int k = cnt - 1; k >= 0 && !matched; k--) {
            if (ends[k] + 2 <= n && !strncmp(s + ends[k], "..", 2)) {
                pis_add(o, i, ends[k] + 2);
                i = ends[k] + 2;
                matched = 1;
            }
        }
        if (!matched) i++;
    }
    return o->n;
}

/* __import__\s*\(\s*["']os["']\s*\) */
static int pis_match_import_os_at(const char *s, int i, int *end)
{
    if (!pis_ci_at(s, i, "__import__")) return 0;
    int j = pis_skip_ws(s, i + 10);
    if (s[j] != '(') return 0;
    j = pis_skip_ws(s, j + 1);
    if (s[j] != '"' && s[j] != '\'') return 0;
    char q = s[j];
    if (!(s[j + 1] == 'o' && s[j + 2] == 's' && s[j + 3] == q)) return 0;
    j = pis_skip_ws(s, j + 4);
    if (s[j] != ')') return 0;
    *end = j + 1;
    return 1;
}

static int pis_m_import_os(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0, e;
    o->n = 0;
    while (i < n) {
        if (pis_match_import_os_at(s, i, &e)) { pis_add(o, i, e); i = e; }
        else i++;
    }
    return o->n;
}

/* /etc/passwd|/etc/shadow */
static int pis_m_passwd(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (i + 11 <= n && (!strncmp(s + i, "/etc/passwd", 11) || !strncmp(s + i, "/etc/shadow", 11))) {
            pis_add(o, i, i + 11);
            i += 11;
        } else i++;
    }
    return o->n;
}

/* authorized_keys */
static int pis_m_authkeys(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (i + 15 <= n && !strncmp(s + i, "authorized_keys", 15)) { pis_add(o, i, i + 15); i += 15; }
        else i++;
    }
    return o->n;
}

/* \d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d{2,5} */
static int pis_d13_dot(const char *s, int i, int *next)
{
    int n = pis_len(s), run = 0;
    while (i + run < n && pis_is_digit((unsigned char)s[i + run])) run++;
    for (int k = (run > 3 ? 3 : run); k >= 1; k--)
        if (s[i + k] == '.') { *next = i + k + 1; return k; }
    return 0;
}

static int pis_ip_port_at(const char *s, int i, int *end)
{
    int n = pis_len(s), p = i;
    for (int g = 0; g < 3; g++)
        if (!pis_d13_dot(s, p, &p)) return 0;
    int run = 0;
    while (p + run < n && pis_is_digit((unsigned char)s[p + run])) run++;
    int hit = -1;
    for (int t = (run > 3 ? 3 : run); t >= 1; t--)
        if (s[p + t] == ':') { hit = t; break; }
    if (hit < 0) return 0;
    p += hit + 1;
    run = 0;
    while (p + run < n && pis_is_digit((unsigned char)s[p + run])) run++;
    if (run < 2) return 0;
    *end = p + (run > 5 ? 5 : run);
    return 1;
}

static int pis_m_ip_port(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0, e;
    o->n = 0;
    while (i < n) {
        if (pis_ip_port_at(s, i, &e)) { pis_add(o, i, e); i = e; }
        else i++;
    }
    return o->n;
}

/* pip\s+install\b  (token form; the pattern form also needs \s+ (?!-r\s) (?!.*==)) */
static int pis_pip_install_at(const char *s, int i, int full)
{
    if (!pis_ci_at(s, i, "pip") || (i > 0 && pis_is_word((unsigned char)s[i - 1]))) return 0;
    int j = pis_eat_ws(s, i + 3);
    if (j < 0 || !pis_ci_at(s, j, "install")) return 0;
    j += 7;
    if (!pis_after_boundary(s, j)) return 0;
    if (!full) return j;
    j = pis_eat_ws(s, j);
    if (j < 0) return 0;
    if (s[j] == '-' && (s[j + 1] == 'r' || s[j + 1] == 'R') && pis_is_space((unsigned char)s[j + 2]))
        return 0;
    for (int k = j; s[k]; k++)
        if (s[k] == '=' && s[k + 1] == '=') return 0;
    return j;
}

static int pis_m_pip(const char *s, pis_spans *o, int full)
{
    int n = pis_len(s), i = 0, e;
    o->n = 0;
    while (i < n) {
        if ((e = pis_pip_install_at(s, i, full)) > 0) { pis_add(o, i, e); i = e; }
        else i++;
    }
    return o->n;
}

/* \\x[0-9a-fA-F]{2} */
static int pis_m_hex_escape(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i + 4 <= n) {
        if (s[i] == '\\' && s[i + 1] == 'x' && strchr("0123456789abcdefABCDEF", s[i + 2]) &&
            strchr("0123456789abcdefABCDEF", s[i + 3])) {
            pis_add(o, i, i + 4);
            i += 4;
        } else i++;
    }
    return o->n;
}

/* \\xHH.*\\xHH.*\\xHH  (greedy: first escape through last escape) */
static int pis_m_hex_chain(const char *s, pis_spans *o)
{
    pis_spans e;
    pis_m_hex_escape(s, &e);
    o->n = 0;
    if (e.n < 3) return 0;
    pis_add(o, e.v[0].a, e.v[e.n - 1].b);
    return o->n;
}

/* xmrig|stratum\+tcp|monero|coinhive|cryptonight */
static int pis_m_crypto(const char *s, pis_spans *o)
{
    static const char *const alts[] = { "xmrig", "stratum+tcp", "monero", "coinhive", "cryptonight", NULL };
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        int hit = 0;
        for (int k = 0; alts[k]; k++) {
            size_t l = strlen(alts[k]);
            if ((size_t)i + l <= (size_t)n && pis_ci_at(s, i, alts[k])) {
                pis_add(o, i, i + (int)l);
                i += (int)l;
                hit = 1;
                break;
            }
        }
        if (!hit) i++;
    }
    return o->n;
}

/* (?:api[_-]?key|token|secret|password)\s*[=:]\s*["']<guards>[A-Za-z0-9+/=_-]{20,} */
static int pis_kw_secret_at(const char *s, int i, int *end)
{
    int p = -1;
    if (pis_ci_at(s, i, "api")) {
        int j = i + 3;
        if (s[j] == '_' || s[j] == '-') j++;
        if (pis_ci_at(s, j, "key")) p = j + 3;
    }
    if (p < 0 && pis_ci_at(s, i, "token")) p = i + 5;
    if (p < 0 && pis_ci_at(s, i, "secret")) p = i + 6;
    if (p < 0 && pis_ci_at(s, i, "password")) p = i + 8;
    if (p < 0) return 0;
    int j = pis_skip_ws(s, p);
    if (s[j] != '=' && s[j] != ':') return 0;
    j = pis_skip_ws(s, j + 1);
    if (s[j] != '"' && s[j] != '\'') return 0;
    char q = s[j];
    int v = j + 1;
    /* (?!(?-i:[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+)["']) : an env-var NAME, not a value */
    if (s[v] >= 'A' && s[v] <= 'Z') {
        int k = v + 1;
        while ((s[k] >= 'A' && s[k] <= 'Z') || (s[k] >= '0' && s[k] <= '9')) k++;
        int us = 0;
        while (s[k] == '_' && s[k + 1] && (((s[k + 1] >= 'A') && (s[k + 1] <= 'Z')) ||
                                          ((s[k + 1] >= '0') && (s[k + 1] <= '9')))) {
            us = 1;
            k++;
            while ((s[k] >= 'A' && s[k] <= 'Z') || (s[k] >= '0' && s[k] <= '9')) k++;
        }
        if (us && s[k] == q) return 0;
    }
    /* (?!(?-i:<public solana program ids>)["']) */
    {
        static const char *const ids[] = {
            "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
            "TokenzQdBNbLqP5VEhdkAS6EPFLC1PHnBqCXEpPxuEb",
            "ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL",
            "MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr",
            "11111111111111111111111111111111",
            "metaqbxxUerdq28cj1RbAWkYQm3ybzjb6a8bt518x1s", NULL
        };
        for (int k = 0; ids[k]; k++) {
            size_t l = strlen(ids[k]);
            if (!strncmp(s + v, ids[k], l) && (s[v + (int)l] == '"' || s[v + (int)l] == '\''))
                return 0;
        }
    }
    {
        int r = 0;
        while (s[v + r] && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=_-",
                                  s[v + r]))
            r++;
        if (r < 20) return 0;
        *end = v + r;
    }
    return 1;
}

static int pis_m_secret(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0, e;
    o->n = 0;
    while (i < n) {
        if (pis_kw_secret_at(s, i, &e)) { pis_add(o, i, e); i = e; }
        else i++;
    }
    return o->n;
}

/* /proc/self|/proc/\d+/ */
static int pis_m_proc_access(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (i + 10 <= n && !strncmp(s + i, "/proc/self", 10)) { pis_add(o, i, i + 10); i += 10; }
        else if (i + 6 <= n && !strncmp(s + i, "/proc/", 6)) {
            int j = i + 6, run = 0;
            while (pis_is_digit((unsigned char)s[j + run])) run++;
            if (run > 0 && s[j + run] == '/') { pis_add(o, i, j + run + 1); i = j + run + 1; }
            else i++;
        } else i++;
    }
    return o->n;
}

/* curl/wget ... \${KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL} */
static int pis_m_env_exfil(const char *s, pis_spans *o, int wget)
{
    static const char *const kws[] = { "KEY", "TOKEN", "SECRET", "PASSWORD", "CREDENTIAL", NULL };
    static const char *const loop_hosts[] = { "localhost", "127.0.0.1", "[::1]", NULL };
    const char *verb = wget ? "wget" : "curl";
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (!pis_ci_at(s, i, verb) || (i > 0 && pis_is_word((unsigned char)s[i - 1]))) { i++; continue; }
        int p = pis_eat_ws(s, i + 4);
        if (p < 0) { i++; continue; }
        int bad = 0;
        for (int k = p; k < n && !bad; k++) {
            int off = -1;
            if (pis_ci_at(s, k, "https://")) off = 8;
            else if (pis_ci_at(s, k, "http://")) off = 7;
            if (off < 0) continue;
            for (int h = 0; loop_hosts[h]; h++)
                if (pis_ci_at(s, k + off, loop_hosts[h])) { bad = 1; break; }
        }
        if (bad) { i++; continue; }
        int hit = 0, hit_end = 0;
        for (int d = p; d < n && !hit; d++) {
            if (s[d] != '$') continue;
            int r = d + 1;
            if (s[r] == '{') r++;
            int run = r;
            while (pis_is_word((unsigned char)s[run])) run++;
            for (int t = r; t < run && !hit; t++) {
                for (int k = 0; kws[k]; k++) {
                    size_t l = strlen(kws[k]);
                    if (!pis_ci_at(s, t, kws[k])) continue;
                    int e = t + (int)l;
                    if (e == run || ((s[e] == 'S' || s[e] == 's') && e + 1 == run)) {
                        hit = 1;
                        hit_end = e;
                    }
                    break;
                }
            }
        }
        if (hit) { pis_add(o, i, hit_end); i = hit_end > i ? hit_end : i + 1; }
        else i++;
    }
    return o->n;
}

/* ignore\s+(?:\w+\s+)*(previous|all|above|prior)\s+instructions */
static int pis_m_inject_ignore(const char *s, pis_spans *o)
{
    static const char *const mids[] = { "previous", "all", "above", "prior", NULL };
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (!pis_ci_at(s, i, "ignore")) { i++; continue; }
        int j = pis_eat_ws(s, i + 6), done = 0, e = 0;
        while (j > 0) {
            for (int k = 0; mids[k]; k++) {
                size_t l = strlen(mids[k]);
                if (!pis_ci_at(s, j, mids[k])) continue;
                int q = pis_eat_ws(s, j + (int)l);
                if (q > 0 && pis_ci_at(s, q, "instructions")) { e = q + 12; done = 1; }
                break;
            }
            if (done) break;
            int r = j;
            while (pis_is_word((unsigned char)s[r])) r++;
            if (r == j) break;
            j = pis_eat_ws(s, r);
        }
        if (done) { pis_add(o, i, e); i = e; }
        else i++;
    }
    return o->n;
}

/* disregard\s+(?:\w+\s+)*(your|all|any)\s+(?:\w+\s+)*(instructions|rules|guidelines) */
static int pis_m_disregard(const char *s, pis_spans *o)
{
    static const char *const mids[] = { "your", "all", "any", NULL };
    static const char *const tails[] = { "instructions", "rules", "guidelines", NULL };
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (!pis_ci_at(s, i, "disregard")) { i++; continue; }
        int j = pis_eat_ws(s, i + 9), done = 0, e = 0;
        while (j > 0) {
            for (int k = 0; mids[k]; k++) {
                size_t l = strlen(mids[k]);
                if (!pis_ci_at(s, j, mids[k])) continue;
                int q = pis_eat_ws(s, j + (int)l);
                while (q > 0) {
                    for (int t = 0; tails[t]; t++) {
                        if (pis_ci_at(s, q, tails[t])) { e = q + (int)strlen(tails[t]); done = 1; break; }
                    }
                    if (done) break;
                    int r = q;
                    while (pis_is_word((unsigned char)s[r])) r++;
                    if (r == q) break;
                    q = pis_eat_ws(s, r);
                }
                break;
            }
            if (done) break;
            int r = j;
            while (pis_is_word((unsigned char)s[r])) r++;
            if (r == j) break;
            j = pis_eat_ws(s, r);
        }
        if (done) { pis_add(o, i, e); i = e; }
        else i++;
    }
    return o->n;
}

/* forget_rules prefix guard: (?! (?:do|does|...|had)n['’]?t | cannot | not | never ) [ \t] */
static int pis_forget_neg_at(const char *s, int i)
{
    static const char *const plain[] = { "cannot", "not", "never", NULL };
    static const char *const stems[] = { "do", "does", "did", "ca", "could", "would", "should", "wo",
                                         "must", "need", "is", "are", "was", "were", "ai", "has",
                                         "have", "had", NULL };
    for (int k = 0; plain[k]; k++) {
        size_t l = strlen(plain[k]);
        if (pis_ci_at(s, i, plain[k]) && pis_is_hspace((unsigned char)s[i + (int)l])) return 1;
    }
    for (int k = 0; stems[k]; k++) {
        size_t l = strlen(stems[k]);
        if (!pis_ci_at(s, i, stems[k])) continue;
        int e = i + (int)l;
        if (s[e] == 'n' && (s[e + 1] == '\'' || (unsigned char)s[e + 1] == 0x92)) {
            int q = e + 2;
            if (s[q] == 't' || s[q] == 'T') q++;
            if (pis_is_hspace((unsigned char)s[q])) return 1;
        }
        if (s[e] == 'n' && (s[e + 1] == 't' || s[e + 1] == 'T') && pis_is_hspace((unsigned char)s[e + 2]))
            return 1;
    }
    return 0;
}

/* forget\s+(?:about\s+)?(your|all|any|previous|prior|above|earlier)\s+
 * (?:word\s+){0,3}(instructions|rules|guidelines)(?![^\W_]) */
static int pis_forget_tail(const char *s, int at, int *end)
{
    static const char *const mids[] = { "your", "all", "any", "previous", "prior", "above", "earlier", NULL };
    static const char *const tails[] = { "instructions", "rules", "guidelines", NULL };
    int j = pis_eat_ws(s, at);
    if (j < 0) return 0;
    if (pis_ci_at(s, j, "about")) {
        int q = pis_eat_ws(s, j + 5);
        if (q > 0) j = q;
    }
    for (int k = 0; mids[k]; k++) {
        size_t l = strlen(mids[k]);
        if (!pis_ci_at(s, j, mids[k])) continue;
        int q = pis_eat_ws(s, j + (int)l);
        for (int words = 0; words <= 3 && q > 0; words++) {
            for (int t = 0; tails[t]; t++) {
                if (!pis_ci_at(s, q, tails[t])) continue;
                int e = q + (int)strlen(tails[t]);
                if (!pis_is_alnum((unsigned char)s[e])) { *end = e; return 1; }
                break;
            }
            int r = q;
            while (pis_is_word((unsigned char)s[r])) r++;
            if (r == q) break;
            q = pis_eat_ws(s, r);
        }
        break;
    }
    return 0;
}

static int pis_m_forget(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        /* branch A: <word>([ \t]+)forget ... */
        if ((i == 0 || (!pis_is_alnum((unsigned char)s[i - 1]) && s[i - 1] != '\'' &&
                        (unsigned char)s[i - 1] != 0x92)) &&
            pis_is_alnum((unsigned char)s[i]) && !pis_forget_neg_at(s, i)) {
            int r = i;
            while (pis_is_alnum((unsigned char)s[r]) || s[r] == '\'' || (unsigned char)s[r] == 0x92) r++;
            int q = pis_eat_hspace(s, r);
            if (q > 0 && pis_ci_at(s, q, "forget")) {
                int e;
                if (pis_forget_tail(s, q + 6, &e)) { pis_add(o, i, e); i = e; continue; }
            }
        }
        /* branch B: (?:^|[^\w’' \t]|_)[ \t]* forget ... */
        {
            int ok = i == 0;
            if (!ok) {
                char c = s[i];
                if (!pis_is_word((unsigned char)c) && c != '\'' && (unsigned char)c != 0x92 &&
                    !pis_is_hspace((unsigned char)c))
                    ok = 1;
                else if (c == '_')
                    ok = 1;
                else if ((c == '\'' || (unsigned char)c == 0x92) && !pis_is_word((unsigned char)s[i - 1]))
                    ok = 1;
            }
            if (ok) {
                int q = i;
                while (pis_is_hspace((unsigned char)s[q])) q++;
                if (q > i && pis_ci_at(s, q, "forget")) {
                    int e;
                    if (pis_forget_tail(s, q + 6, &e)) { pis_add(o, i, e); i = e; continue; }
                }
            }
        }
        i++;
    }
    return o->n;
}

/* \[(?:[^\]\\\n"'`]|\\.)*\] */
static int pis_m_char_class(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        if (s[i] != '[') { i++; continue; }
        int j = i + 1, closed = -1;
        while (j < n) {
            char c = s[j];
            if (c == ']') { closed = j; break; }
            if (c == '\\') {
                if (j + 1 < n && s[j + 1] != '\n') { j += 2; continue; }
                break;
            }
            if (c == '\n' || c == '"' || c == '\'' || c == '`') break;
            j++;
        }
        if (closed >= 0) { pis_add(o, i, closed + 1); i = closed + 1; }
        else i++;
    }
    return o->n;
}

/* "[^"\n]*"|'[^'\n]*'|`[^`\n]*` */
static int pis_m_quoted_span(const char *s, pis_spans *o)
{
    int n = pis_len(s), i = 0;
    o->n = 0;
    while (i < n) {
        char c = s[i];
        if (c == '"' || c == '\'' || c == '`') {
            int j = i + 1;
            while (j < n && s[j] != c && s[j] != '\n') j++;
            if (j < n && s[j] == c) { pis_add(o, i, j + 1); i = j + 1; continue; }
        }
        i++;
    }
    return o->n;
}

/* https?://([^\s/"'`)]+)([^\s"'`)]*) */
typedef struct { int host_a, host_b, path_a, path_b, end; } pis_url;

static int pis_url_at(const char *s, int i, pis_url *u)
{
    int n = pis_len(s), off = -1;
    if (i + 8 <= n && !strncmp(s + i, "https://", 8)) off = 8;
    else if (i + 7 <= n && !strncmp(s + i, "http://", 7)) off = 7;
    if (off < 0) return 0;
    int j = i + off, a = j;
    while (s[j] && !pis_is_space((unsigned char)s[j]) && !strchr("/\"'`)]", s[j])) j++;
    if (j == a) return 0;
    u->host_a = a;
    u->host_b = j;
    u->path_a = j;
    while (s[j] && !pis_is_space((unsigned char)s[j]) && !strchr("\"'`)]", s[j])) j++;
    u->path_b = j;
    u->end = j;
    return 1;
}

static int pis_m_urls(const char *s, pis_url *out, int max)
{
    int n = pis_len(s), i = 0, cnt = 0;
    while (i < n && cnt < max) {
        pis_url u;
        if (pis_url_at(s, i, &u)) { out[cnt++] = u; i = u.end > i ? u.end : i + 1; }
        else i++;
    }
    return cnt;
}

/* https?://(\[[0-9a-fA-F:]+\]|[^\s/:"'`)\]]+) */
static int pis_url_hosts(const char *s, pis_url *out, int max)
{
    int n = pis_len(s), i = 0, cnt = 0;
    while (i < n && cnt < max) {
        int off = -1;
        if (i + 8 <= n && !strncmp(s + i, "https://", 8)) off = 8;
        else if (i + 7 <= n && !strncmp(s + i, "http://", 7)) off = 7;
        if (off < 0) { i++; continue; }
        int j = i + off, a = j;
        if (s[j] == '[') {
            int k = j + 1;
            while (s[k] && strchr("0123456789abcdefABCDEF:", s[k])) k++;
            if (s[k] != ']') { i++; continue; }
            j = k + 1;
        } else {
            while (s[j] && !pis_is_space((unsigned char)s[j]) && !strchr("/:\"'`)]", s[j])) j++;
        }
        if (j == a) { i++; continue; }
        pis_url u;
        memset(&u, 0, sizeof(u));
        u.host_a = a;
        u.host_b = j;
        u.end = j;
        out[cnt++] = u;
        i = j;
    }
    return cnt;
}

/* xmrig|stratum|coinhive|cryptonight|randomx|cpuminer|minerd|nicehash|hashrate|
 * donate-level|\bmin(?:e[sd]?|ers?|ing)\b|--(?:coin|algo)\b */
static int pis_mining_machinery(const char *s)
{
    static const char *const plain[] = { "xmrig", "stratum", "coinhive", "cryptonight", "randomx",
                                         "cpuminer", "minerd", "nicehash", "hashrate", "donate-level", NULL };
    static const char *const sfx[] = { "e", "es", "ed", "er", "ers", "ing", NULL };
    for (int k = 0; plain[k]; k++)
        if (pis_has_ci(s, plain[k])) return 1;
    int n = pis_len(s);
    for (int i = 0; i < n; i++) {
        if (pis_ci_at(s, i, "min") && pis_before_boundary(s, i)) {
            for (int k = 0; sfx[k]; k++) {
                size_t l = strlen(sfx[k]);
                if (pis_ci_at(s, i + 3, sfx[k]) && pis_after_boundary(s, i + 3 + (int)l)) return 1;
            }
        }
        if (pis_ci_at(s, i, "--coin") && pis_after_boundary(s, i + 6)) return 1;
        if (pis_ci_at(s, i, "--algo") && pis_after_boundary(s, i + 6)) return 1;
    }
    return 0;
}

/* _PROC_WRITE */
static int pis_proc_write(const char *s)
{
    int n = pis_len(s), i = 0;
    if (pis_has_ci(s, "write_text") || pis_has_ci(s, "write_bytes")) return 1;
    while (i < n) {
        if (pis_at(s, i, ".write") && s[pis_skip_ws(s, i + 6)] == '(') return 1;
        if (pis_ci_at(s, i, "O_WRONLY") || pis_ci_at(s, i, "O_RDWR")) return 1;
        if (s[i] == '>') {
            int j = pis_skip_ws(s, i + 1);
            if (s[j] == '"' || s[j] == '\'') j++;
            if (pis_at(s, j, "/proc/")) return 1;
        }
        if (pis_ci_at(s, i, "tee") && pis_before_boundary(s, i) && pis_after_boundary(s, i + 3))
            return 1;
        if (pis_ci_at(s, i, "open")) {
            int j = pis_skip_ws(s, i + 4);
            if (s[j] == '(') {
                j = pis_skip_ws(s, j + 1);
                for (int k = j; s[k] && k < j + 300; k++) {
                    if (s[k] == ')') break;
                    if (s[k] == '"' || s[k] == '\'') {
                        char q = s[k];
                        int m = k + 1, ok = 1, mode = 0;
                        while (s[m] && s[m] != q) m++;
                        if (s[m] != q) break;          /* unterminated literal */
                        for (int t = k + 1; t < m; t++) {
                            if (!strchr("rbtwax+", s[t])) { ok = 0; break; }
                            if (strchr("wax+", s[t])) mode = 1;
                        }
                        if (ok && mode) return 1;      /* a write/append mode string */
                        k = m;                         /* skip it, keep scanning */
                    }
                }
            }
        }
        i++;
    }
    return 0;
}

/* _DANGEROUS_OS_ON_LINE */
static int pis_dangerous_os(const char *s)
{
    static const char *const verbs[] = { "system", "popen", "remove", "unlink", "rmdir", "removedirs",
                                         "exec", "spawn", NULL };
    int n = pis_len(s);
    for (int i = 0; i < n; i++) {
        int target = -1;
        if (pis_ci_at(s, i, "os") && pis_before_boundary(s, i)) {
            int j = pis_skip_ws(s, i + 2);
            if (s[j] == '.') target = pis_skip_ws(s, j + 1);
        }
        if (target < 0) {
            int e;
            if (pis_match_import_os_at(s, i, &e)) {
                int j = pis_skip_ws(s, e);
                if (s[j] == '.') target = pis_skip_ws(s, j + 1);
            }
        }
        if (target < 0) continue;
        for (int k = 0; verbs[k]; k++) {
            const char *v = verbs[k];
            if (!pis_ci_at(s, target, v)) continue;
            int e = target + (int)strlen(v);
            if (strcmp(v, "exec") && strcmp(v, "spawn")) {
                if (pis_after_boundary(s, e)) return 1;
            } else {
                if (pis_after_boundary(s, e)) return 1;
                int r = e;
                while (pis_is_word((unsigned char)s[r])) r++;
                if (pis_after_boundary(s, r)) return 1;
            }
        }
    }
    return 0;
}

/* _AUTHORIZED_KEYS_WRITE */
static int pis_authkeys_write(const char *s)
{
    int n = pis_len(s), i = 0;
    while (i < n) {
        if (s[i] == '>') return 1;
        if ((pis_ci_at(s, i, "tee") && pis_before_boundary(s, i) && pis_after_boundary(s, i + 3)) ||
            (pis_ci_at(s, i, "cp") && pis_before_boundary(s, i) && pis_after_boundary(s, i + 2)) ||
            (pis_ci_at(s, i, "mv") && pis_before_boundary(s, i) && pis_after_boundary(s, i + 2)) ||
            (pis_ci_at(s, i, "cat") && pis_before_boundary(s, i) && pis_after_boundary(s, i + 3)) ||
            pis_ci_at(s, i, "writeFileSync") || pis_ci_at(s, i, "appendFileSync"))
            return 1;
        if (pis_ci_at(s, i, "sed")) {
            int j = pis_eat_ws(s, i + 3);
            if (j > 0 && s[j] == '-' && pis_is_alpha((unsigned char)s[j + 1])) {
                int k = j + 1, has_i = 0;
                while (pis_is_alpha((unsigned char)s[k])) {
                    if (s[k] == 'i' || s[k] == 'I') has_i = 1;
                    k++;
                }
                if (has_i) return 1;
            }
        }
        if (s[i] == '.' && (pis_ci_at(s, i + 1, "write") || pis_ci_at(s, i + 1, "append"))) {
            int k = i + 1;
            while (pis_is_word((unsigned char)s[k])) k++;
            if (s[pis_skip_ws(s, k)] == '(') return 1;
        }
        i++;
    }
    return 0;
}

/* ─────────────────────── pattern-id dispatch ───────────────────────────── */
static int pis_match_pid(const char *pid, const char *line, pis_spans *o)
{
    o->n = 0;
    if (!pid || !line) return 0;
    if (!strcmp(pid, "sudo_usage")) return pis_m_sudo(line, o, 1);
    if (!strcmp(pid, "dump_all_env")) return pis_m_dump_env(line, o);
    if (!strcmp(pid, "format_filesystem")) return pis_m_mkfs(line, o);
    if (!strcmp(pid, "path_traversal_deep")) return pis_m_traversal_deep(line, o);
    if (!strcmp(pid, "python_import_os")) return pis_m_import_os(line, o);
    if (!strcmp(pid, "system_passwd_access")) return pis_m_passwd(line, o);
    if (!strcmp(pid, "ssh_backdoor")) return pis_m_authkeys(line, o);
    if (!strcmp(pid, "hardcoded_ip_port")) return pis_m_ip_port(line, o);
    if (!strcmp(pid, "unpinned_pip_install")) return pis_m_pip(line, o, 1);
    if (!strcmp(pid, "hex_encoded_string")) return pis_m_hex_chain(line, o);
    if (!strcmp(pid, "crypto_mining")) return pis_m_crypto(line, o);
    if (!strcmp(pid, "hardcoded_secret")) return pis_m_secret(line, o);
    if (!strcmp(pid, "proc_access")) return pis_m_proc_access(line, o);
    if (!strcmp(pid, "env_exfil_curl")) return pis_m_env_exfil(line, o, 0);
    if (!strcmp(pid, "env_exfil_wget")) return pis_m_env_exfil(line, o, 1);
    if (!strcmp(pid, "prompt_injection_ignore")) return pis_m_inject_ignore(line, o);
    if (!strcmp(pid, "disregard_rules")) return pis_m_disregard(line, o);
    if (!strcmp(pid, "forget_rules")) return pis_m_forget(line, o);
    return 0;   /* not carried: no hits, so the predicate answers "not inert" */
}

static int pis_token_of_pid(const char *pid, const char *line, pis_spans *o)
{
    o->n = 0;
    if (!pid || !line) return 0;
    if (!strcmp(pid, "sudo_usage")) return pis_m_sudo(line, o, 0);
    if (!strcmp(pid, "dump_all_env")) return pis_m_dump_env_token(line, o);
    if (!strcmp(pid, "format_filesystem")) return pis_m_mkfs(line, o);
    return 0;
}

/* ─────────────────────── shared classification sets ────────────────────── */
static int pis_in_set(const char *v, const char *const *set)
{
    if (!v) return 0;
    for (int k = 0; set[k]; k++)
        if (!strcmp(v, set[k])) return 1;
    return 0;
}

static const char *const PIS_PROSE_FULL_CATEGORIES[] = { "injection", "credential_exposure", NULL };
static const char *const PIS_PROSE_FULL_IDS[] = {
    "context_exfil", "send_to_url", "md_image_exfil", "md_link_exfil", "ssh_backdoor",
    "curl_pipe_shell", "wget_pipe_shell", "curl_pipe_python",
    "agent_config_mod", "agent_config_mod_shell", "agent_config_contract", "agent_config_ref",
    "hermes_config_mod", "hermes_config_mod_shell", "hermes_config_ref",
    "other_agent_config_mod", "other_agent_config_mod_shell", "other_agent_config_ref", NULL
};
static const char *const PIS_CATALOG_STEP_IDS[] = {
    "agent_config_mod", "agent_config_mod_shell", "agent_config_contract", "agent_config_ref",
    "hermes_config_mod", "hermes_config_mod_shell", "hermes_config_ref",
    "other_agent_config_mod", "other_agent_config_mod_shell", "other_agent_config_ref", NULL
};
static const char *const PIS_PIPE_IDS[] = { "curl_pipe_shell", "wget_pipe_shell", "curl_pipe_python", NULL };
static const char *const PIS_AGENT_INSTRUCTION_FILES[] = { "skill.md", "after-install.md", NULL };
static const char *const PIS_AGENT_INSTRUCTION_DIRS[] = { "skills", "optional-skills", NULL };
static const char *const PIS_DOC_SUFFIXES[] = { ".md", ".txt", ".rst", ".html", NULL };
static const char *const PIS_TEST_TREE_DIRS[] = { "tests", "test", "testing", "spec", "specs", "fixtures", NULL };
static const char *const PIS_TEST_DIRS_ANY[] = { "__tests__", "__fixtures__", "__mocks__", NULL };
static const char *const PIS_TRUSTED_HOSTS[] = {
    "hermes-agent.nousresearch.com", "ollama.com", "bun.sh", "get.docker.com",
    "sh.rustup.rs", "deno.land", "astral.sh", "app.factory.ai", NULL
};
static const char *const PIS_SENSITIVE_LEAVES[] = {
    "etc", "passwd", "shadow", ".ssh", ".aws", ".env", ".hermes", ".gnupg", ".kube",
    ".docker", ".netrc", ".npmrc", ".pypirc", ".config", "proc", "root", "home", "var",
    "usr", "sys", NULL
};
static const char *const PIS_BENIGN_PROC_LEAVES[] = {
    "cgroup", "mountinfo", "mounts", "stat", "statm", "status", "limits", "io", "comm",
    "cpuset", "sched", "schedstat", "oom_score", NULL
};
static const char *const PIS_BENIGN_OS_MEMBERS[] = {
    "environ", "getenv", "fsync", "fspath", "path", "sep", "linesep", "name", "getcwd", "getpid",
    "getppid", "urandom", "cpu_count", "uname", "stat", "listdir", "scandir", "walk", "access",
    "isatty", "get_terminal_size", "devnull", "curdir", "pardir", "extsep", "altsep", "pathsep", NULL
};

/* ══════════════════════════ gate p1 ═══════════════════════════════════════ */

int noc_pis_p1_is_doc_prose(const char *rel_path)
{
    pis_path p;
    pis_path_init(&p, rel_path);
    if (!pis_suffix_in(&p, PIS_DOC_SUFFIXES)) return 0;
    for (int k = 0; PIS_AGENT_INSTRUCTION_FILES[k]; k++)
        if (pis_name_is(&p, PIS_AGENT_INSTRUCTION_FILES[k])) return 0;
    for (int i = 0; i < p.n - 1; i++)
        for (int k = 0; PIS_AGENT_INSTRUCTION_DIRS[k]; k++)
            if (pis_seg_is(&p, i, PIS_AGENT_INSTRUCTION_DIRS[k])) return 0;
    return 1;
}

int noc_pis_p1_is_ci_workflow(const char *rel_path)
{
    pis_path p;
    pis_path_init(&p, rel_path);
    if (pis_name_is(&p, ".gitlab-ci.yml")) return 1;
    return p.n == 3 && pis_seg_is(&p, 0, ".github") && pis_seg_is(&p, 1, "workflows") &&
           (pis_suf_is(&p, ".yml") || pis_suf_is(&p, ".yaml"));
}

int noc_pis_p1_is_agent_facing(const noc_finding_t *f)
{
    if (!f) return 0;
    return pis_in_set(f->category, PIS_PROSE_FULL_CATEGORIES) || pis_in_set(f->pattern_id, PIS_PROSE_FULL_IDS);
}

const char *noc_pis_p1_prose_cap(const noc_finding_t *f)
{
    if (noc_pis_p1_is_agent_facing(f)) return NULL;
    if (!f || !f->severity) return NULL;
    if (!strcmp(f->severity, "critical")) return "high";
    if (!strcmp(f->severity, "high")) return "medium";
    return NULL;
}

static int pis_trusted_installer_url(const char *host, int hlen, const char *path, int plen)
{
    char buf[256];
    if (hlen <= 0 || hlen >= (int)sizeof(buf)) return 0;
    for (int k = 0; k < hlen; k++) buf[k] = (char)pis_lc((unsigned char)host[k]);
    buf[hlen] = '\0';
    for (int k = 0; PIS_TRUSTED_HOSTS[k]; k++)
        if (!strcmp(buf, PIS_TRUSTED_HOSTS[k])) return 1;
    if (!strcmp(buf, "raw.githubusercontent.com")) {
        char pbuf[1024];
        int n = plen < (int)sizeof(pbuf) - 1 ? plen : (int)sizeof(pbuf) - 1;
        for (int k = 0; k < n; k++) pbuf[k] = (char)pis_lc((unsigned char)path[k]);
        pbuf[n] = '\0';
        return pis_has_ci(pbuf, "install");
    }
    return 0;
}

int noc_pis_p1_is_trusted_installer_pipe(const noc_finding_t *f, const char *line)
{
    const char *s = line ? line : "";
    pis_url urls[32];
    if (!f || !pis_in_set(f->pattern_id, PIS_PIPE_IDS)) return 0;
    int n = pis_m_urls(s, urls, 32);
    if (n == 0) return 0;
    for (int k = 0; k < n; k++)
        if (!pis_trusted_installer_url(s + urls[k].host_a, urls[k].host_b - urls[k].host_a,
                                      s + urls[k].path_a, urls[k].path_b - urls[k].path_a))
            return 0;
    return 1;
}

int noc_pis_p1_is_locale_catalog(const char *rel_path)
{
    pis_path p;
    pis_path_init(&p, rel_path);
    return p.n == 2 && pis_seg_is(&p, 0, "locales") &&
           (pis_suf_is(&p, ".yaml") || pis_suf_is(&p, ".yml"));
}

const char *noc_pis_p1_catalog_cap(const noc_finding_t *f)
{
    if (!f) return NULL;
    if (pis_in_set(f->pattern_id, PIS_CATALOG_STEP_IDS)) {
        const char *sev = f->severity ? f->severity : "";
        int rank = 0;
        if (!strcmp(sev, "medium")) rank = 1;
        else if (!strcmp(sev, "high")) rank = 2;
        else if (!strcmp(sev, "critical")) rank = 3;
        return rank <= 1 ? sev : "medium";
    }
    return noc_pis_p1_prose_cap(f);
}

/* one ~/.hermes/<segments>/<leaf> operand, optionally quoted */
static int pis_uninstall_operand(const char *s, int i, int *end)
{
    int q = 0;
    if (s[i] == '"' || s[i] == '\'') { q = s[i]; i++; }
    if (s[i] == '$' && !strncmp(s + i, "$HOME/.hermes", 13)) i += 13;
    else if (!strncmp(s + i, "${HOME}/.hermes", 15)) i += 15;
    else if (s[i] == '~' && !strncmp(s + i, "~/.hermes", 9)) i += 9;
    else return 0;
    int segs = 0;
    for (;;) {
        if (s[i] != '/') break;
        int j = i + 1;
        if (!pis_is_alnum((unsigned char)s[j])) break;
        j++;
        while (pis_is_alnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '_' || s[j] == '-') j++;
        i = j;
        segs++;
    }
    if (!segs) return 0;
    if (s[i] == '/') i++;
    if (q) {
        if (s[i] != q) return 0;
        i++;
    }
    *end = i;
    return 1;
}

/* ^(?:\$\s*)?rm\s+(?:-[a-zA-Z]+\s+)*(operand)(?:\s+operand)*\s*(?:#.*)?$ */
static int pis_self_uninstall_rm(const char *s, int start, int stop, int require_end)
{
    int i = start;
    if (i < stop && s[i] == '$') { i++; i = pis_skip_ws(s, i); }
    if (i + 2 > stop || strncmp(s + i, "rm", 2)) return 0;
    if (i > 0 && pis_is_word((unsigned char)s[i - 1])) return 0;
    i += 2;
    if (!pis_is_space((unsigned char)s[i])) return 0;
    i = pis_skip_ws(s, i);
    for (;;) {
        if (s[i] != '-' || !pis_is_alpha((unsigned char)s[i + 1])) break;
        int j = i + 1;
        while (pis_is_alpha((unsigned char)s[j])) j++;
        if (!pis_is_space((unsigned char)s[j])) break;
        i = pis_skip_ws(s, j);
    }
    if (!pis_uninstall_operand(s, i, &i)) return 0;
    for (;;) {
        if (!pis_is_space((unsigned char)s[i])) break;
        int after;
        if (!pis_uninstall_operand(s, pis_skip_ws(s, i), &after)) break;
        i = after;
    }
    i = pis_skip_ws(s, i);
    if (s[i] == '#') while (i < stop && s[i]) i++;
    if (require_end) return i >= stop;
    return 1;
}

int noc_pis_p1_is_self_uninstall_doc(const noc_finding_t *f, const char *line)
{
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "destructive_home_rm")) return 0;
    if (!line) return 0;
    int n = pis_len(line), a = 0, b = n;
    while (a < b && pis_is_space((unsigned char)line[a])) a++;
    while (b > a && pis_is_space((unsigned char)line[b - 1])) b--;
    if (pis_self_uninstall_rm(line, a, b, 1)) return 1;
    /* the same shape inside a backtick code span mid-prose */
    for (int i = 0; i < n; i++) {
        if (line[i] != '`') continue;
        int j = i + 1;
        while (j < n && line[j] != '`' && line[j] != '\n') j++;
        if (j >= n || line[j] != '`') continue;
        if (pis_self_uninstall_rm(line, i + 1, j, 1)) return 1;
        i = j;
    }
    return 0;
}

int noc_pis_p1_is_test_tree(const char *rel_path)
{
    pis_path p;
    pis_path_init(&p, rel_path);
    if (p.n > 1)
        for (int k = 0; PIS_TEST_TREE_DIRS[k]; k++)
            if (pis_seg_is(&p, 0, PIS_TEST_TREE_DIRS[k])) return 1;
    for (int i = 0; i < p.n - 1; i++)
        for (int k = 0; PIS_TEST_DIRS_ANY[k]; k++)
            if (pis_seg_is(&p, i, PIS_TEST_DIRS_ANY[k])) return 1;
    /* ^(?:tests?_[^/]*|[^/]*_tests?\.[^./]+|[^/]*\.(?:test|spec)\.[^./]+)$ */
    if (p.namelen > 5 && (pis_ci_at(p.name, 0, "test_") || pis_ci_at(p.name, 0, "tests_"))) return 1;
    int dot = -1;
    for (int k = p.namelen - 1; k >= 0; k--)
        if (p.name[k] == '.') { dot = k; break; }
    if (dot > 0 && dot < p.namelen - 1) {
        int has_dot = 0;
        for (int k = dot + 1; k < p.namelen; k++)
            if (p.name[k] == '.') has_dot = 1;
        if (!has_dot) {
            static const char *const tails[] = { "_test", "_tests", ".test", ".spec", NULL };
            for (int k = 0; tails[k]; k++) {
                int l = (int)strlen(tails[k]);
                if (dot >= l && pis_ci_at(p.name, dot - l, tails[k])) return 1;
            }
        }
    }
    return 0;
}

int noc_pis_p1_is_disclosed_config_note(const noc_finding_t *f, const char *rel_path)
{
    pis_path p;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "hermes_config_mod")) return 0;
    pis_path_init(&p, rel_path);
    return pis_suf_is(&p, ".md") || pis_name_is(&p, "after-install.md");
}

/* ^\s*(?:"(?P<k>...)"\s*:\s*)?"(?P<v>...)"\s*,?\s*$ */
typedef struct { int has_key, k_a, k_b, v_a, v_b; } pis_json_pair;

static int pis_json_str_at(const char *s, int i, int *ca, int *cb, int *end)
{
    if (s[i] != '"') return 0;
    int j = i + 1;
    while (s[j]) {
        if (s[j] == '\\') {
            if (!s[j + 1]) return 0;
            j += 2;
            continue;
        }
        if (s[j] == '"') {
            *ca = i + 1;
            *cb = j;
            *end = j + 1;
            return 1;
        }
        j++;
    }
    return 0;
}

static int pis_json_pair_parse(const char *s, pis_json_pair *p)
{
    int i = pis_skip_ws(s, 0), end;
    memset(p, 0, sizeof(*p));
    /* the "key": prefix is optional, so a failed key attempt retries as a value */
    for (int attempt = 0; attempt < 2; attempt++) {
        int k = i, has_key = 0, ka = 0, kb = 0;
        if (attempt == 0) {
            if (s[k] != '"' || !pis_json_str_at(s, k, &ka, &kb, &end)) continue;
            k = pis_skip_ws(s, end);
            if (s[k] != ':') continue;
            k = pis_skip_ws(s, k + 1);
            has_key = 1;
        }
        if (!pis_json_str_at(s, k, &p->v_a, &p->v_b, &end)) continue;
        k = pis_skip_ws(s, end);
        if (s[k] == ',') k = pis_skip_ws(s, k + 1);
        if (s[k] != '\0') continue;
        p->has_key = has_key;
        p->k_a = ka;
        p->k_b = kb;
        return 1;
    }
    return 0;
}

static int pis_has_hspace(const char *s, int a, int b)
{
    for (int i = a; i < b; i++)
        if (pis_is_space((unsigned char)s[i])) return 1;
    return 0;
}

static int pis_json_command_key(const char *s, int a, int b)
{
    static const char *const kws[] = { "cmd", "command", "script", "exec", "run", "shell", "install",
                                       "hook", "arg", "entry", "bin", "start", "setup", NULL };
    for (int i = a; i < b; i++)
        for (int k = 0; kws[k]; k++) {
            size_t l = strlen(kws[k]);
            if (i + (int)l <= b && pis_ci_at(s, i, kws[k])) return 1;
        }
    return 0;
}

/* (?:^|[;&|`(]|\$\(|\b(?:sudo|exec|eval|do|then|xargs|bash|sh|-c))\s*$ */
static int pis_command_position_end(const char *s, int a, int b)
{
    static const char *const words[] = { "sudo", "exec", "eval", "do", "then", "xargs", "bash", "sh", "-c", NULL };
    int e = b;
    while (e > a && pis_is_space((unsigned char)s[e - 1])) e--;
    if (e == a) return 1;                       /* ^\s*$ */
    if (strchr(";&|`(", s[e - 1])) return 1;    /* marker, then \s*$ */
    for (int k = 0; words[k]; k++) {
        int l = (int)strlen(words[k]);
        if (e - l < a || !pis_ci_at(s, e - l, words[k])) continue;
        if (words[k][0] != '-' && !pis_before_boundary(s, e - l)) continue;
        return 1;
    }
    return 0;
}

int noc_pis_p1_is_json_prose_value(const noc_finding_t *f, const char *rel_path, const char *line)
{
    pis_path p;
    pis_json_pair pr;
    pis_spans hits;
    if (!f || !f->severity || strcmp(f->severity, "high")) return 0;
    pis_path_init(&p, rel_path);
    if (!pis_suf_is(&p, ".json") || pis_name_is(&p, "package.json")) return 0;
    if (!pis_json_pair_parse(line ? line : "", &pr)) return 0;
    pis_match_pid(f->pattern_id, line, &hits);
    if (hits.n == 0) return 0;
    if (pr.has_key && !pis_has_hspace(line, pr.k_a, pr.k_b) &&
        pis_json_command_key(line, pr.k_a, pr.k_b))
        return 0;
    for (int k = 0; k < hits.n; k++) {
        int part_a = -1, part_b = -1;
        if (pr.has_key && pr.k_a <= hits.v[k].a && hits.v[k].b <= pr.k_b) { part_a = pr.k_a; part_b = pr.k_b; }
        else if (pr.v_a <= hits.v[k].a && hits.v[k].b <= pr.v_b) { part_a = pr.v_a; part_b = pr.v_b; }
        (void)part_b;
        if (part_a < 0) return 0;
        if (pis_command_position_end(line, part_a, hits.v[k].a)) return 0;
    }
    return 1;
}

static int pis_sensitive_after(const char *s)
{
    int i = 0;
    while (s[i] == '/' || s[i] == '\\') i++;
    for (int k = 0; PIS_SENSITIVE_LEAVES[k]; k++) {
        const char *kw = PIS_SENSITIVE_LEAVES[k];
        if (pis_ci_at(s, i, kw) && pis_after_boundary(s, i + (int)strlen(kw))) return 1;
    }
    return 0;
}

int noc_pis_p1_is_repo_root_traversal(const noc_finding_t *f, const char *line)
{
    pis_spans runs;
    const char *s = line ? line : "";
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "path_traversal_deep")) return 0;
    pis_m_traversal_run(s, &runs);
    if (runs.n == 0) return 0;
    for (int k = 0; k < runs.n; k++) {
        const char *rest = s + runs.v[k].b;
        int i = 0;
        for (;;) {
            if (rest[i] == '/' && rest[i + 1] == '.' && rest[i + 2] == '.') { i += 3; continue; }
            if (rest[i] == '.' && rest[i + 1] == '.') { i += 2; continue; }
            break;
        }
        if (rest[i] == '/') i++;
        if (pis_sensitive_after(rest + i)) return 0;
    }
    return 1;
}

/* (?:^|[;&|`(]|\b(?:uv|pipx|sudo|python[\d.]*\s+-m))\s*$ */
static int pis_pip_command_position_end(const char *s, int a, int b)
{
    static const char *const words[] = { "uv", "pipx", "sudo", NULL };
    int e = b;
    while (e > a && pis_is_space((unsigned char)s[e - 1])) e--;
    if (e == a) return 1;
    if (strchr(";&|`(", s[e - 1])) return 1;
    for (int k = 0; words[k]; k++) {
        int l = (int)strlen(words[k]);
        if (e - l >= a && pis_ci_at(s, e - l, words[k]) && pis_before_boundary(s, e - l)) return 1;
    }
    if (e - 2 >= a && s[e - 2] == '-' && (s[e - 1] == 'm' || s[e - 1] == 'M')) {
        int w = e - 2;
        while (w > a && pis_is_space((unsigned char)s[w - 1])) w--;
        int st = w;
        while (st > a && (pis_is_alnum((unsigned char)s[st - 1]) || s[st - 1] == '.')) st--;
        if (st < w && pis_ci_at(s, st, "python") && pis_before_boundary(s, st)) {
            int k = st + 6;
            while (k < w && (pis_is_digit((unsigned char)s[k]) || s[k] == '.')) k++;
            if (k == w) return 1;
        }
    }
    return 0;
}

int noc_pis_p1_is_pip_install_in_prose_literal(const noc_finding_t *f, const char *line)
{
    pis_spans spans, hits;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "unpinned_pip_install")) return 0;
    if (pis_exec_on_line(line ? line : "")) return 0;
    pis_literal_spans(line, &spans);
    pis_m_pip(line, &hits, 0);
    if (hits.n == 0) return 0;
    for (int k = 0; k < hits.n; k++) {
        int idx = pis_span_of(&spans, hits.v[k].a, hits.v[k].b);
        if (idx < 0) return 0;
        int a = spans.v[idx].a, b = spans.v[idx].b, content_start = a;
        for (int i = a; i < b; i++)
            if (strchr("\"'`/", line[i])) { content_start = i + 1; break; }
        if (pis_pip_command_position_end(line, content_start, hits.v[k].a)) return 0;
    }
    return 1;
}

int noc_pis_p1_is_hex_in_char_class(const noc_finding_t *f, const char *line)
{
    pis_spans classes, hits;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "hex_encoded_string")) return 0;
    pis_m_char_class(line ? line : "", &classes);
    pis_m_hex_escape(line, &hits);
    if (hits.n == 0) return 0;
    for (int k = 0; k < hits.n; k++)
        if (!pis_in_spans(&classes, hits.v[k].a, hits.v[k].b)) return 0;
    return 1;
}

json_t *noc_pis_p1_logical_line(const json_t *lines, int index)
{
    const char *parts[9];
    int len = lines ? json_len(lines) : 0, count = 0;
    if (index < 0 || index >= len) return json_null();
    json_t *cur = json_get(lines, index);
    parts[count++] = (cur && cur->type == JSON_STRING) ? cur->str_val : "";
    int i = index;
    while (i + 1 < len && count <= 8) {
        const char *t = parts[count - 1];
        int j = (int)strlen(t);
        while (j > 0 && pis_is_space((unsigned char)t[j - 1])) j--;
        if (j == 0 || t[j - 1] != '\\') break;
        i++;
        json_t *nx = json_get(lines, i);
        parts[count++] = (nx && nx->type == JSON_STRING) ? nx->str_val : "";
    }
    size_t total = 1;
    for (int k = 0; k < count; k++) total += strlen(parts[k]) + 1;
    char *buf = (char *)malloc(total);
    if (!buf) return json_null();
    char *w = buf;
    for (int k = 0; k < count; k++) {
        if (k) *w++ = ' ';
        const char *t = parts[k];
        int j = (int)strlen(t);
        while (j > 0 && pis_is_space((unsigned char)t[j - 1])) j--;
        while (j > 0 && t[j - 1] == '\\') j--;
        memcpy(w, t, (size_t)j);
        w += j;
    }
    *w = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

/* ══════════════════════════ gate p2 ═══════════════════════════════════════ */

int noc_pis_p2_is_inert_fixture_line(const noc_finding_t *f, const char *line, int is_code,
                                     const char *rel_path)
{
    pis_spans hits, spans, calls, exec_args;
    if (!is_code) return 1;
    pis_match_pid(f ? f->pattern_id : NULL, line ? line : "", &hits);
    if (hits.n == 0) return 0;
    pis_literal_spans(line, &spans);
    pis_exec_call_open_spans(line, &calls);
    exec_args.n = 0;
    for (int k = 0; k < calls.n; k++) {
        int idx = pis_span_starting_at(&spans, calls.v[k].b);
        if (idx >= 0) pis_add(&exec_args, spans.v[idx].a, spans.v[idx].b);
    }
    int non_literal_exec = pis_executes(line, rel_path) && calls.n == 0;
    for (int k = 0; k < hits.n; k++) {
        int idx = pis_span_of(&spans, hits.v[k].a, hits.v[k].b);
        if (idx < 0) return 0;
        for (int m = 0; m < exec_args.n; m++)
            if (exec_args.v[m].a == spans.v[idx].a && exec_args.v[m].b == spans.v[idx].b) return 0;
    }
    return !non_literal_exec;
}

int noc_pis_p2_is_base64_media(const char *line)
{
    int n = pis_len(line), i = 0;
    if (!line) return 0;
    while (i < n) {
        int start = -1, pad = 0;
        if (i + 7 <= n && !strncmp(line + i, "base64,", 7)) start = i + 7;
        else start = i;
        int j = start;
        while (line[j] && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
                                 line[j]))
            j++;
        if (j - start >= 24) {
            while (pad < 2 && line[j + pad] == '=') pad++;
            int blob_len = j - start + pad;
            int head = blob_len > 16 ? 16 : blob_len;
            head -= head % 4;
            if (head > 0) {
                unsigned char dec[32];
                int dn = pis_b64decode_strict(line + start, head, dec, (int)sizeof(dec));
                if (dn > 0) {
                    for (int k = 0; PIS_MEDIA_MAGIC[k].p; k++) {
                        if (dn >= PIS_MEDIA_MAGIC[k].len &&
                            !memcmp(dec, PIS_MEDIA_MAGIC[k].p, (size_t)PIS_MEDIA_MAGIC[k].len))
                            return 1;
                    }
                }
            }
            i = j + pad > i ? j + pad : i + 1;
        } else {
            i = (start > i) ? start : i + 1;
        }
    }
    return 0;
}

int noc_pis_p2_is_base64_archive_roundtrip(const noc_finding_t *f, const char *line)
{
    int n, tar_ok = 0;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "base64_decode_pipe")) return 0;
    if (!line) return 0;
    n = pis_len(line);

    /* _TAR_CREATE: \btar\s+(?:-?-?create\b|-?-?c[a-z]*f?\b|c[a-z]*f?\b) */
    for (int i = 0; i + 3 <= n; i++) {
        if (!pis_ci_at(line, i, "tar") || !pis_before_boundary(line, i)) continue;
        int j = pis_eat_ws(line, i + 3);
        if (j < 0) continue;
        int k = j;
        while (line[k] == '-') k++;
        int start = k;
        while (pis_is_alpha((unsigned char)line[k])) k++;
        if (k == start) continue;
        if (k - start == 6 && pis_ci_at(line, start, "create")) { tar_ok = 1; break; }
        if (line[start] == 'c' || line[start] == 'C') { tar_ok = 1; break; }
    }
    if (!tar_ok) return 0;

    /* _B64_ROUNDTRIP: base64 (no -d before the next | ; &) ... | ... base64 -d */
    for (int i = 0; i + 6 <= n; i++) {
        if (!pis_ci_at(line, i, "base64")) continue;
        if (i > 0 && pis_is_word((unsigned char)line[i - 1])) continue;
        if (!pis_after_boundary(line, i + 6)) continue;
        int stop = i + 6, decode_in_prefix = 0;
        for (; stop < n; stop++) {
            if (line[stop] == '|' || line[stop] == ';' || line[stop] == '&') break;
            if (line[stop] == '-') {
                int k = stop + 1, has_d = 0;
                while (pis_is_alpha((unsigned char)line[k])) {
                    if (line[k] == 'd' || line[k] == 'D') has_d = 1;
                    k++;
                }
                if (has_d) decode_in_prefix = 1;
            }
        }
        if (decode_in_prefix) continue;
        int bar = -1;
        for (int k = stop; k < n; k++)
            if (line[k] == '|') { bar = k; break; }
        if (bar < 0) continue;
        for (int k = bar + 1; k + 6 <= n; k++) {
            if (!pis_ci_at(line, k, "base64") || !pis_before_boundary(line, k)) continue;
            int q = pis_eat_ws(line, k + 6);
            while (q > 0) {
                if (line[q] != '-') break;
                if (line[q + 1] == '-' && pis_ci_at(line, q, "--decode") &&
                    pis_after_boundary(line, q + 8))
                    return 1;
                int m = q + 1, has_d = 0;
                while (pis_is_alpha((unsigned char)line[m])) {
                    if (line[m] == 'd' || line[m] == 'D') has_d = 1;
                    m++;
                }
                if (has_d && pis_after_boundary(line, m)) return 1;
                if (line[m] && !pis_is_space((unsigned char)line[m]) && line[m] != '|') {
                    int e = m;
                    while (line[e] && !pis_is_space((unsigned char)line[e]) && line[e] != '|') e++;
                    m = e;
                }
                q = pis_eat_ws(line, m);
            }
        }
    }
    return 0;
}

int noc_pis_p2_is_regex_alternation_token(const noc_finding_t *f, const char *line)
{
    pis_spans tokens, spans;
    int n;
    if (pis_token_of_pid(f ? f->pattern_id : NULL, line ? line : "", &tokens) == 0) return 0;
    n = pis_len(line);
    pis_literal_spans(line, &spans);
    int executes = pis_exec_on_line(line);
    for (int k = 0; k < tokens.n; k++) {
        int a = tokens.v[k].a, b = tokens.v[k].b;
        if (memchr(line + a, ' ', (size_t)(b - a))) return 0;     /* " " in group(0) */
        int idx = pis_span_of(&spans, a, b);
        if (idx < 0) return 0;
        /* Python's `before in "|("` / `after in "|)"` are true for the empty
         * string, i.e. at the start and the end of the line. */
        char before = a > 0 ? line[a - 1] : '\0';
        char after = b < n ? line[b] : '\0';
        int alt = a == 0 || b == n || before == '|' || before == '(' || after == '|' || after == ')';
        int whole = (a == spans.v[idx].a + 1 && b == spans.v[idx].b - 1 &&
                     strchr("\"'`", line[spans.v[idx].a]) != NULL && !executes);
        if (!alt && !whole) return 0;
    }
    return 1;
}

/* _sudo_in_safety_clause: a negation governs the word on its clause */
static int pis_sudo_in_safety_clause(const char *line, int start)
{
    static const char *const negs[] = { "never", "not", "no", "without", "avoid", NULL };
    int a = 0;
    for (int i = start - 1; i >= 0; i--)
        if (line[i] == '.' || line[i] == ';') { a = i + 1; break; }
    for (int i = a; i < start; i++) {
        for (int k = 0; negs[k]; k++) {
            size_t l = strlen(negs[k]);
            if (!pis_ci_at(line, i, negs[k])) continue;
            if (!pis_before_boundary(line, i) || !pis_after_boundary(line, i + (int)l)) continue;
            return 1;
        }
        if (!pis_before_boundary(line, i)) continue;
        if (pis_ci_at(line, i, "don") && (line[i + 3] == '\'' || (unsigned char)line[i + 3] == 0x92) &&
            (line[i + 4] == 't' || line[i + 4] == 'T') && pis_after_boundary(line, i + 5))
            return 1;
        if (pis_ci_at(line, i, "doesn") && (line[i + 5] == '\'' || (unsigned char)line[i + 5] == 0x92) &&
            (line[i + 6] == 't' || line[i + 6] == 'T') && pis_after_boundary(line, i + 7))
            return 1;
        if (pis_ci_at(line, i, "cannot") && pis_after_boundary(line, i + 6)) return 1;
        if (pis_ci_at(line, i, "can") && (line[i + 3] == '\'' || (unsigned char)line[i + 3] == 0x92) &&
            (line[i + 4] == 't' || line[i + 4] == 'T') && pis_after_boundary(line, i + 5))
            return 1;
        if ((pis_ci_at(line, i, "refus") || pis_ci_at(line, i, "forbid") ||
             pis_ci_at(line, i, "disallow")) &&
            pis_after_boundary(line, i + 6))
            return 1;
    }
    return 0;
}

/* \bsudo\s+(?:-[A-Za-z]+\b\s+)*(?:apt|apt-get|snap|...|npm|gem|-u\b|-E\b|-H\b|-n\b) */
static int pis_sudo_admin_hint_starts(const char *line, pis_spans *o)
{
    static const char *const tools[] = {
        "apt-get", "apt", "snap", "dnf", "yum", "pacman", "zypper", "apk", "brew", "port",
        "emerge", "systemctl", "service", "launchctl", "npm", "gem", "-u", "-E", "-H", "-n", NULL
    };
    int n = pis_len(line), i = 0;
    o->n = 0;
    while (i < n) {
        if (!pis_ci_at(line, i, "sudo") || !pis_before_boundary(line, i) ||
            !pis_after_boundary(line, i + 4)) { i++; continue; }
        int j = pis_eat_ws(line, i + 4), ok = 0;
        while (j > 0) {
            for (int k = 0; tools[k]; k++) {
                size_t l = strlen(tools[k]);
                if (pis_ci_at(line, j, tools[k]) && pis_after_boundary(line, j + (int)l)) { ok = 1; break; }
            }
            if (ok) break;
            if (pis_ci_at(line, j, "pip")) {
                int k = j + 3;
                while (pis_is_digit((unsigned char)line[k])) k++;
                if (pis_after_boundary(line, k)) { ok = 1; break; }
            }
            if (line[j] == '-' && pis_is_alpha((unsigned char)line[j + 1])) {
                int k = j + 1;
                while (pis_is_alpha((unsigned char)line[k])) k++;
                if (!pis_after_boundary(line, k)) break;
                int q = pis_eat_ws(line, k);
                if (q < 0) break;
                j = q;
                continue;
            }
            break;
        }
        if (ok) { pis_add(o, i, j); i = j > i ? j : i + 1; }
        else i++;
    }
    return o->n;
}

int noc_pis_p2_is_sudo_inert(const noc_finding_t *f, const char *line, const char *rel_path)
{
    pis_spans hits, spans, hints;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "sudo_usage")) return 0;
    pis_m_sudo(line ? line : "", &hits, 1);
    if (hits.n == 0) return 0;
    pis_literal_spans(line, &spans);
    pis_sudo_admin_hint_starts(line, &hints);
    int executes = pis_executes(line, rel_path);
    int in_regex = 0;
    for (int i = 0, n = pis_len(line); i < n; i++) {
        if (line[i] != '(') continue;
        int j = i + 1;
        if (line[j] == '?') {
            j++;
            if (line[j] == ':') j++;       /* (?: ... : the group is (?:  */
        }
        j = pis_skip_ws(line, j);
        if (pis_at(line, j, "sudo\\s")) { in_regex = 1; break; }
    }
    for (int k = 0; k < hits.n; k++) {
        int in_lit = pis_in_spans(&spans, hits.v[k].a, hits.v[k].b), is_hint = 0;
        for (int m = 0; m < hints.n; m++)
            if (hints.v[m].a == hits.v[k].a) { is_hint = 1; break; }
        if (in_lit && !executes && (is_hint || in_regex)) continue;
        if (pis_sudo_in_safety_clause(line, hits.v[k].a)) continue;
        return 0;
    }
    return 1;
}

int noc_pis_p2_is_env_dump_in_safety_clause(const noc_finding_t *f, const char *line)
{
    pis_spans hits;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "dump_all_env")) return 0;
    pis_m_dump_env(line ? line : "", &hits);
    if (hits.n == 0) return 0;
    for (int k = 0; k < hits.n; k++)
        if (!pis_sudo_in_safety_clause(line, hits.v[k].a)) return 0;
    return 1;
}

int noc_pis_p2_is_quoted_injection_phrase(const noc_finding_t *f, const char *line)
{
    pis_spans hits, quotes;
    if (!f || !f->pattern_id) return 0;
    if (strcmp(f->pattern_id, "prompt_injection_ignore") && strcmp(f->pattern_id, "disregard_rules") &&
        strcmp(f->pattern_id, "forget_rules"))
        return 0;
    pis_match_pid(f->pattern_id, line ? line : "", &hits);
    if (hits.n == 0) return 0;
    pis_m_quoted_span(line, &quotes);
    for (int k = 0; k < hits.n; k++)
        if (!pis_in_spans(&quotes, hits.v[k].a, hits.v[k].b)) return 0;
    return 1;
}

int noc_pis_p2_is_benign_lazy_os_import(const noc_finding_t *f, const char *line)
{
    int n, i = 0, members = 0;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "python_import_os")) return 0;
    n = pis_len(line);
    while (i < n) {
        int e;
        if (!pis_match_import_os_at(line, i, &e)) { i++; continue; }
        int j = pis_skip_ws(line, e);
        if (line[j] != '.') { i = e; continue; }
        j = pis_skip_ws(line, j + 1);
        int a = j;
        while (pis_is_word((unsigned char)line[j])) j++;
        if (j == a) { i = e; continue; }
        char name[64];
        int l = j - a < (int)sizeof(name) - 1 ? j - a : (int)sizeof(name) - 1;
        memcpy(name, line + a, (size_t)l);
        name[l] = '\0';
        if (!pis_in_set(name, PIS_BENIGN_OS_MEMBERS)) return 0;
        members++;
        i = j;
    }
    if (members == 0) return 0;
    if (pis_dangerous_os(line) || pis_exec_on_line(line)) return 0;
    return 1;
}

int noc_pis_p2_is_authorized_keys_prose(const noc_finding_t *f, const char *line, int is_doc)
{
    pis_spans spans;
    (void)is_doc;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "ssh_backdoor")) return 0;
    if (!line || pis_authkeys_write(line)) return 0;
    int pos = pis_find_ci(line, 0, "authorized_keys");
    if (pos < 0) return 0;
    pis_literal_spans(line, &spans);
    return pis_in_spans(&spans, pos, pos + 15);
}

int noc_pis_p2_is_loopback_only(const noc_finding_t *f, const char *line)
{
    pis_spans hits;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "hardcoded_ip_port")) return 0;
    pis_m_ip_port(line ? line : "", &hits);
    if (hits.n == 0) return 0;
    for (int k = 0; k < hits.n; k++) {
        int st = hits.v[k].a, e;
        if (!pis_before_boundary(line, st) || !pis_ci_at(line, st, "127.")) return 0;
        if (!pis_ip_port_at(line, st, &e)) return 0;
    }
    return 1;
}

int noc_pis_p2_is_loopback_continuation(const noc_finding_t *f, const char *line, const char *joined)
{
    pis_spans hits;
    pis_url hosts[32];
    if (!f || !f->pattern_id) return 0;
    if (strcmp(f->pattern_id, "env_exfil_curl") && strcmp(f->pattern_id, "env_exfil_wget")) return 0;
    if (!line) return 0;
    int j = pis_len(line);
    while (j > 0 && pis_is_space((unsigned char)line[j - 1])) j--;
    if (j == 0 || line[j - 1] != '\\') return 0;
    const char *jn = joined ? joined : "";
    pis_match_pid(f->pattern_id, jn, &hits);
    if (hits.n != 0) return 0;
    int cnt = pis_url_hosts(jn, hosts, 32);
    if (cnt == 0) return 0;
    for (int k = 0; k < cnt; k++) {
        int a = hosts[k].host_a, len = hosts[k].host_b - a;
        char host[128];
        if (len <= 0 || len >= (int)sizeof(host)) return 0;
        for (int m = 0; m < len; m++) host[m] = (char)pis_lc((unsigned char)jn[a + m]);
        host[len] = '\0';
        if (strcmp(host, "localhost") && strcmp(host, "127.0.0.1") && strcmp(host, "[::1]")) return 0;
    }
    return 1;
}

int noc_pis_p2_is_coin_name_only(const noc_finding_t *f, const char *line)
{
    pis_spans hits;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "crypto_mining")) return 0;
    if (!line || pis_mining_machinery(line)) return 0;
    pis_m_crypto(line, &hits);
    if (hits.n == 0) return 0;
    for (int k = 0; k < hits.n; k++) {
        int a = hits.v[k].a, b = hits.v[k].b;
        if (b - a != 6 || !pis_ci_at(line, a, "monero")) return 0;
        if (a > 0 && pis_is_alnum((unsigned char)line[a - 1])) return 0;
        if (line[b] && pis_is_alnum((unsigned char)line[b])) return 0;
    }
    return 1;
}

int noc_pis_p2_is_public_client_key(const noc_finding_t *f, const char *line)
{
    pis_spans hits;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "hardcoded_secret")) return 0;
    pis_m_secret(line ? line : "", &hits);
    if (hits.n == 0) return 0;
    for (int k = 0; k < hits.n; k++) {
        int a = hits.v[k].a, b = hits.v[k].b, found = 0;
        for (int i = a; i < b; i++) {
            if (line[i] != '=' && line[i] != ':') continue;
            int j = pis_skip_ws(line, i + 1);
            if (line[j] != '"' && line[j] != '\'') continue;
            j++;
            int r = 0;
            while (line[j + r] &&
                   strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=_-",
                          line[j + r]))
                r++;
            if (r < 20) break;
            if (!strncmp(line + j, "GOCSPX-", 7) || !strncmp(line + j, "phc_", 4)) found = 1;
            break;
        }
        if (!found) return 0;
    }
    return 1;
}

int noc_pis_p2_is_own_proc_read(const noc_finding_t *f, const char *line)
{
    int n, i = 0, seen = 0;
    if (!f || !f->pattern_id || strcmp(f->pattern_id, "proc_access")) return 0;
    if (!line) return 0;
    n = pis_len(line);
    while (i < n) {
        int j = -1;
        if (i + 10 <= n && !strncmp(line + i, "/proc/self", 10)) j = i + 10;
        else if (i + 6 <= n && !strncmp(line + i, "/proc/", 6)) {
            int k = i + 6, run = 0;
            while (pis_is_digit((unsigned char)line[k + run])) run++;
            if (run > 0) j = k + run;
        }
        if (j < 0) { i++; continue; }
        int a = j, b = j;
        if (line[j] == '/') {
            a = j + 1;
            b = a;
            while (line[b] && (pis_is_word((unsigned char)line[b]) || line[b] == '.' || line[b] == '-')) b++;
        }
        char leaf[128];
        int l = b - a < (int)sizeof(leaf) - 1 ? b - a : (int)sizeof(leaf) - 1;
        memcpy(leaf, line + a, (size_t)l);
        leaf[l] = '\0';
        if (!pis_in_set(leaf, PIS_BENIGN_PROC_LEAVES)) return 0;
        seen++;
        i = b > i ? b : i + 1;
    }
    if (!seen) return 0;
    return !pis_proc_write(line);
}
