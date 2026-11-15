/* port_display.c -- pure helpers ported from agent/display.py.
 *
 * PoP: st_oneline                    @ agent/display.py:_oneline
 * PoP: st_tail_trunc                 @ agent/display.py:_tail_trunc
 * PoP: st_clip                       @ agent/display.py:_clip
 * PoP: st_shell_basename             @ agent/display.py:_shell_basename
 * PoP: st_split_shell_words          @ agent/display.py:_split_shell_words
 * PoP: st_strip_shell_pipe_tail      @ agent/display.py:_strip_shell_pipe_tail
 * PoP: st_split_shell_compound       @ agent/display.py:_split_shell_compound
 * PoP: st_shell_head_word            @ agent/display.py:_shell_head_word
 * PoP: st_clean_shell_segment        @ agent/display.py:_clean_shell_segment
 * PoP: st_is_shell_boundary_echo     @ agent/display.py:_is_shell_boundary_echo
 * PoP: st_summarize_shell_command    @ agent/display.py:summarize_shell_command
 *
 * The ``t()`` call at the tail of summarize_shell_command is a seam: the two
 * locale templates are passed in verbatim (locales/en.yaml:3506-3507), with
 * {first}/{count} substituted here. Everything else is self-contained string
 * logic -- no i18n, no I/O.
 */
#include "port_display.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void copy_out(char *out, size_t cap, const char *s)
{
    if (!cap)
        return;
    snprintf(out, cap, "%s", s ? s : "");
}

/* ── _oneline ───────────────────────────────────────────────────────────── */
void st_oneline(const char *text, char *out, size_t cap)
{
    size_t w = 0;
    int pending_space = 0;
    if (!cap)
        return;
    for (const unsigned char *p = (const unsigned char *)(text ? text : ""); *p; p++) {
        if (isspace(*p)) {
            pending_space = (w > 0);
            continue;
        }
        if (pending_space) {
            if (w + 1 >= cap)
                break;
            out[w++] = ' ';
            pending_space = 0;
        }
        if (w + 1 >= cap)
            break;
        out[w++] = (char)*p;
    }
    out[w] = '\0';
}

/* ── _tail_trunc ────────────────────────────────────────────────────────── */
void st_tail_trunc(const char *text, long limit, char *out, size_t cap)
{
    if (!cap)
        return;
    size_t len = text ? strlen(text) : 0;
    if (limit <= 0 || (size_t)limit >= len) {
        copy_out(out, cap, text);
        return;
    }
    if (limit <= 3) {
        size_t n = (size_t)limit;
        if (n >= cap)
            n = cap - 1;
        memset(out, '.', n);
        out[n] = '\0';
        return;
    }
    size_t keep = (size_t)limit - 3;
    if (keep >= cap)
        keep = cap - 4 > 0 ? cap - 4 : 0;
    memcpy(out, text, keep);
    snprintf(out + keep, cap - keep, "...");
}

/* ── _clip ──────────────────────────────────────────────────────────────── */
void st_clip(const char *text, size_t n, char *out, size_t cap)
{
    if (!cap)
        return;
    size_t len = text ? strlen(text) : 0;
    size_t keep = len > n ? n : len;
    if (keep >= cap)
        keep = cap - 1;
    memcpy(out, text ? text : "", keep);
    out[keep] = '\0';
    if (len > n)
        snprintf(out + keep, cap - keep, "...");
}

/* ── _shell_basename ────────────────────────────────────────────────────── */
static void shell_basename(const char *head, char *out, size_t cap)
{
    if (!head || !*head) {
        copy_out(out, cap, "");
        return;
    }
    const char *slash = strrchr(head, '/');
    copy_out(out, cap, slash ? slash + 1 : head);
}

/* ── quote scan (shared by every splitter) ──────────────────────────────── */
typedef struct {
    const char *text;
    size_t len;
} scanner;

/* Is the char at `i` inside quotes, given the scan up to it? Recomputed the
 * same way _scan_quoted does: a quote opens a run, and closes unless the
 * preceding char is a backslash. */
static int char_quoted(const char *text, size_t i)
{
    char quote = 0;
    for (size_t k = 0; k <= i; k++) {
        char ch = text[k];
        if (quote) {
            if (ch == quote && (k == 0 || text[k - 1] != '\\'))
                quote = 0;
        } else if (ch == '\'' || ch == '"') {
            quote = ch;
        }
    }
    return quote != 0;
}

/* ── _split_shell_words ─────────────────────────────────────────────────── */
int st_split_shell_words(const char *segment, char **out, size_t cap, int maxn)
{
    if (!segment)
        return 0;
    size_t n = strlen(segment);
    int count = 0;
    size_t start = 0;
    int in_word = 0;
    for (size_t i = 0; i <= n; i++) {
        int break_word = 0;
        if (i == n) {
            break_word = in_word;
        } else {
            char ch = segment[i];
            if (!char_quoted(segment, i) && isspace((unsigned char)ch))
                break_word = 1;
        }
        if (break_word) {
            if (in_word && count < maxn) {
                size_t wl = i - start;
                char *w = (char *)malloc(wl + 1);
                if (!w)
                    break;
                memcpy(w, segment + start, wl);
                w[wl] = '\0';
                out[count++] = w;
            }
            in_word = 0;
        } else if (!in_word) {
            start = i;
            in_word = 1;
        }
    }
    (void)cap;
    return count;
}

static void free_words(char **w, int n)
{
    for (int i = 0; i < n; i++)
        free(w[i]);
}

/* ── _strip_shell_pipe_tail ─────────────────────────────────────────────── */
void st_strip_shell_pipe_tail(const char *segment, char *out, size_t cap)
{
    static const char *const TAIL_HEADS[] = {"head", "tail", "wc", "sort", "uniq"};
    char *words[64];
    int n = st_split_shell_words(segment, words, 0, 64);
    int cut = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(words[i], "|") != 0)
            continue;
        char base[256];
        shell_basename((i + 1 < n) ? words[i + 1] : "", base, sizeof(base));
        for (size_t k = 0; k < sizeof(TAIL_HEADS) / sizeof(TAIL_HEADS[0]); k++) {
            if (strcmp(base, TAIL_HEADS[k]) == 0) {
                cut = i;
                break;
            }
        }
        if (cut >= 0)
            break;
    }
    if (cut < 0)
        cut = n;
    size_t w = 0;
    for (int i = 0; i < cut; i++) {
        if (i)
            w += (size_t)snprintf(out + w, cap - w, " ");
        w += (size_t)snprintf(out + w, cap - w, "%s", words[i]);
    }
    out[w] = '\0';
    free_words(words, n);
}

/* ── _split_shell_compound ──────────────────────────────────────────────── */
int st_split_shell_compound(const char *command, char **out, size_t cap, int maxn)
{
    if (!command)
        return 0;
    size_t n = strlen(command);
    int count = 0;
    size_t start = 0;
    size_t skip = 0;
    for (size_t i = 0; i < n; i++) {
        char ch = command[i];
        int split = 0;
        if (skip) {
            skip--;
        } else if (!char_quoted(command, i) &&
                   ((ch == '&' && i + 1 < n && command[i + 1] == '&') ||
                    (ch == '|' && i + 1 < n && command[i + 1] == '|'))) {
            split = 1;
            skip = 2; /* both characters belong to the operator */
        } else if (!char_quoted(command, i) && (ch == ';' || ch == '\n')) {
            split = 1;
            skip = 1;
        }
        if (split) {
            char seg[4096];
            size_t sl = i - start;
            if (sl >= sizeof(seg))
                sl = sizeof(seg) - 1;
            memcpy(seg, command + start, sl);
            seg[sl] = '\0';
            char cleaned[4096];
            st_strip_shell_pipe_tail(seg, cleaned, sizeof(cleaned));
            char *trim = cleaned;
            while (*trim == ' ')
                trim++;
            size_t tl = strlen(trim);
            while (tl && trim[tl - 1] == ' ')
                trim[--tl] = '\0';
            if (*trim && count < maxn) {
                out[count++] = strdup(trim);
                if (!out[count - 1])
                    break;
            }
            start = i + skip;
        }
    }
    /* trailing segment */
    {
        char seg[4096];
        size_t sl = n - start;
        if (sl >= sizeof(seg))
            sl = sizeof(seg) - 1;
        memcpy(seg, command + start, sl);
        seg[sl] = '\0';
        char cleaned[4096];
        st_strip_shell_pipe_tail(seg, cleaned, sizeof(cleaned));
        char *trim = cleaned;
        while (*trim == ' ')
            trim++;
        size_t tl = strlen(trim);
        while (tl && trim[tl - 1] == ' ')
            trim[--tl] = '\0';
        if (*trim && count < maxn) {
            out[count] = strdup(trim);
            if (out[count])
                count++;
        }
    }
    (void)cap;
    return count;
}

/* ── _shell_head_word ───────────────────────────────────────────────────── */
static int is_assignment(const char *word)
{
    if (!word || !(isalpha((unsigned char)word[0]) || word[0] == '_'))
        return 0;
    for (size_t i = 1; word[i]; i++) {
        if (word[i] == '=')
            return 1;
        if (!(isalnum((unsigned char)word[i]) || word[i] == '_'))
            return 0;
    }
    return 0;
}

void st_shell_head_word(const char *segment, char *out, size_t cap)
{
    char *words[64];
    int n = st_split_shell_words(segment, words, 0, 64);
    int i = 0;
    while (i < n && is_assignment(words[i]))
        i++;
    shell_basename(i < n ? words[i] : "", out, cap);
    free_words(words, n);
}

/* ── _clean_shell_segment ───────────────────────────────────────────────── */
static int is_redir(const char *w)
{
    /* ^\d*(?:>>?|<)$ */
    size_t i = 0;
    while (isdigit((unsigned char)w[i]))
        i++;
    if (!w[i])
        return 0;
    if (w[i] == '<')
        return w[i + 1] == '\0';
    if (w[i] == '>') {
        if (w[i + 1] == '\0')
            return 1;
        return w[i + 1] == '>' && w[i + 2] == '\0';
    }
    return 0;
}

static int is_fd_redir(const char *w)
{
    /* ^\d*(?:>&|<&)\d+$ */
    size_t i = 0;
    while (isdigit((unsigned char)w[i]))
        i++;
    if (w[i] == '>') {
        if (w[i + 1] != '&')
            return 0;
        i += 2;
    } else if (w[i] == '<') {
        if (w[i + 1] != '&')
            return 0;
        i += 2;
    } else {
        return 0;
    }
    if (!isdigit((unsigned char)w[i]))
        return 0;
    while (isdigit((unsigned char)w[i]))
        i++;
    return w[i] == '\0';
}

void st_clean_shell_segment(const char *segment, char *out, size_t cap)
{
    char *words[64];
    int n = st_split_shell_words(segment, words, 0, 64);
    size_t w = 0;
    out[0] = '\0';
    int first = 1;
    for (int i = 0; i < n;) {
        if (is_redir(words[i])) {
            i += 2;
            continue;
        }
        if (is_fd_redir(words[i])) {
            i += 1;
            continue;
        }
        if (!first)
            w += (size_t)snprintf(out + w, cap - w, " ");
        w += (size_t)snprintf(out + w, cap - w, "%s", words[i]);
        first = 0;
        i++;
    }
    out[w] = '\0';
    free_words(words, n);
}

/* ── _is_shell_boundary_echo ────────────────────────────────────────────── */
int st_is_shell_boundary_echo(const char *segment)
{
    char *words[64];
    int n = st_split_shell_words(segment, words, 0, 64);
    char base[256];
    shell_basename(n ? words[0] : "", base, sizeof(base));
    int result = 0;
    if (strcmp(base, "echo") == 0 && n > 1) {
        /* join words[1:] and match -{2,}|_exit=|(?:^|\s|=)\$[?{]|PIPESTATUS */
        char joined[4096];
        size_t w = 0;
        for (int i = 1; i < n; i++) {
            if (i > 1)
                w += (size_t)snprintf(joined + w, sizeof(joined) - w, " ");
            w += (size_t)snprintf(joined + w, sizeof(joined) - w, "%s", words[i]);
        }
        joined[w] = '\0';
        for (size_t i = 0; joined[i]; i++) {
            if (joined[i] == '-' && joined[i + 1] == '-') {
                result = 1;
                break;
            }
            if (joined[i] == '_' && strncmp(joined + i, "_exit=", 6) == 0) {
                result = 1;
                break;
            }
            if (joined[i] == 'P' && strncmp(joined + i, "PIPESTATUS", 10) == 0) {
                result = 1;
                break;
            }
            if (joined[i] == '$' &&
                (i == 0 || joined[i - 1] == ' ' || joined[i - 1] == '=') &&
                (joined[i + 1] == '?' || joined[i + 1] == '{')) {
                result = 1;
                break;
            }
        }
    }
    free_words(words, n);
    return result;
}

/* ── summarize_shell_command ────────────────────────────────────────────── */
void st_summarize_shell_command(const char *command, const char *plus_one,
                                const char *plus_many, char *out, size_t cap)
{
    static const char *const SILENT_HEADS[] = {
        "cd", "pushd", "popd", "export", "set", "unset", "source", ".", "true",
        "false", ":",
    };
    char original[8192];
    st_oneline(command, original, sizeof(original));
    if (!*original) {
        copy_out(out, cap, "");
        return;
    }

    char *segs[64];
    int ns = st_split_shell_compound(original, segs, 0, 64);

    if (ns <= 1) {
        char cleaned[4096];
        st_clean_shell_segment(ns ? segs[0] : original, cleaned, sizeof(cleaned));
        copy_out(out, cap, *cleaned ? cleaned : original);
        free_words(segs, ns);
        return;
    }

    char *core[64];
    int nc = 0;
    for (int i = 0; i < ns; i++) {
        char cleaned[4096];
        st_clean_shell_segment(segs[i], cleaned, sizeof(cleaned));
        if (!*cleaned)
            continue;
        char head[256];
        st_shell_head_word(cleaned, head, sizeof(head));
        int silent = 0;
        for (size_t k = 0; k < sizeof(SILENT_HEADS) / sizeof(SILENT_HEADS[0]); k++) {
            if (strcmp(head, SILENT_HEADS[k]) == 0) {
                silent = 1;
                break;
            }
        }
        if (silent || st_is_shell_boundary_echo(cleaned))
            continue;
        core[nc++] = strdup(cleaned);
    }
    free_words(segs, ns);

    if (nc == 0) {
        copy_out(out, cap, original);
        return;
    }
    int count = nc - 1;
    if (count == 0) {
        copy_out(out, cap, core[0]);
        free_words(core, nc);
        return;
    }

    const char *fmt = (count == 1) ? plus_one : plus_many;
    size_t w = 0;
    for (const char *p = fmt; *p && w + 1 < cap;) {
        if (p[0] == '{' && strncmp(p, "{first}", 7) == 0) {
            w += (size_t)snprintf(out + w, cap - w, "%s", core[0]);
            p += 7;
        } else if (p[0] == '{' && strncmp(p, "{count}", 7) == 0) {
            w += (size_t)snprintf(out + w, cap - w, "%d", count);
            p += 7;
        } else {
            out[w++] = *p++;
            out[w] = '\0';
        }
    }
    out[w] = '\0';
    free_words(core, nc);
}
