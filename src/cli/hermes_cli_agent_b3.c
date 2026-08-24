/*
 * hermes_cli_agent_b3.c — HermesCLI batch 3 methods (cli.py class HermesCLI).
 * Model normalization, reasoning preview/streaming, user-message preview,
 * busy slash-command state, and inline paste expansion.
 */
#include "cli_hermes_cli.h"
#include "model_normalize.h"
#include "model_catalog.h"
#include "port_models_helpers.h"   /* normalize_opencode_model_id / opencode_model_api_mode */
#include "port_models_pure.h"      /* models_copilot_model_api_mode */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

/* Defined in src/hermes_cli/helpers2.c (PoP: cli.py:_accent_hex). */
char *cl2_accent_hex(void);

/* Optional seam: hermes_cli/codex_models.get_codex_model_ids(). Weak: when the
 * codex catalog is not linked in, Python's `except Exception` path applies
 * (fallback model id is used). */
__attribute__((weak)) char **codex_models_get_ids(const char *api_key)
{
    (void)api_key;
    return NULL;
}

#define B3_DIM "\x1b[2m"
#define B3_RST "\x1b[0m"
#define B3_YELLOW "\x1b[33m"

static char *b3_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static char *b3_strndup_safe(const char *s, size_t n)
{
    char *d = malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static void b3_free(char **p)
{
    free(*p);
    *p = NULL;
}

static void b3_invalidate(hcli_t *self)
{
    /* Python: self._invalidate(min_interval=0.0) — schedule a TUI repaint. */
    if (self && self->invalidate_fn)
        self->invalidate_fn(self, 0.0);
}

static int b3_terminal_width(void)
{
    /* shutil.get_terminal_size().columns with the same fallback of 80. */
    struct winsize ws;
    const char *cols = getenv("COLUMNS");
    if (cols && atoi(cols) > 0)
        return atoi(cols);
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/* rich.markup.escape — escape [ ] * for rich markup. Caller frees. */
static char *b3_markup_escape(const char *s)
{
    size_t n = strlen(s), j = 0;
    char *out = malloc(n * 2 + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '[' || s[i] == ']' || s[i] == '*')
            out[j++] = '\\';
        out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}

/* ── hcli_normalize_model_for_provider ──────────────────────────────────── */

/* PoP: opencode_provider_family @ hermes_cli/models.py:opencode_provider_family
 * Returns the family slug or NULL. Static table mirrors the built-in set plus
 * prefix extension matching; canonical resolution via model_normalize_provider. */
static const char *b3_opencode_provider_family(const char *provider_id)
{
    static const char *fams[] = { "opencode-zen", "opencode-go", "opencode-free", NULL };
    if (!provider_id) return NULL;
    char raw[256];
    snprintf(raw, sizeof(raw), "%s", provider_id);
    for (char *p = raw; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* trim */
    char *s = raw;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
    if (!*s) return NULL;

    const char *canonical = model_normalize_provider(provider_id);
    for (int i = 0; fams[i]; i++)
        if (canonical && !strcmp(canonical, fams[i]))
            return fams[i];
    if (!strncmp(s, "opencode-free", 13)) return "opencode-free";
    if (!strncmp(s, "opencode-go", 11))   return "opencode-go";
    if (!strncmp(s, "opencode-zen", 12))  return "opencode-zen";
    return NULL;
}

/* _AGGREGATOR_PROVIDERS from hermes_cli/model_normalize.py. */
static bool b3_is_aggregator_provider(const char *p)
{
    return p && (!strcmp(p, "openrouter") || !strcmp(p, "nous") || !strcmp(p, "kilocode"));
}

static void b3_warn_normalized(hcli_t *self, const char *fmt,
                               const char *old_m, const char *new_m,
                               const char *provider)
{
    if (self->model_is_default) return;
    printf("%s⚠️  ", B3_YELLOW);
    printf(fmt, old_m, new_m, provider ? provider : "");
    printf("%s\n", B3_RST);
}

/* PoP: hcli_normalize_model_for_provider @ cli.py:_normalize_model_for_provider */
bool hcli_normalize_model_for_provider(hcli_t *self, const char *resolved_provider)
{
    if (!self) return false;

    char *current_model = NULL;
    bool changed = false;

    /* current_model = str(self.model or "").strip(); dict form → split default. */
    if (self->model) {
        current_model = b3_strdup(self->model);
    } else {
        current_model = b3_strdup("");
    }
    if (!current_model) return false;
    {
        char *s = current_model;
        while (*s == ' ' || *s == '\t') s++;
        char *dup = b3_strdup(s);
        free(current_model);
        current_model = dup;
        if (!current_model) return false;
        char *e = current_model + strlen(current_model);
        while (e > current_model && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
    }

    if (!b3_is_aggregator_provider(resolved_provider)) {
        char *normalized = model_normalize_for_provider(current_model, resolved_provider);
        if (normalized && normalized[0] && strcmp(normalized, current_model) != 0) {
            b3_warn_normalized(self,
                "Normalized model '%s' to '%s' for %s.",
                current_model, normalized, resolved_provider);
            free(self->model);
            self->model = b3_strdup(normalized);
            b3_free(&current_model);
            current_model = b3_strdup(normalized);
            changed = true;
        }
        free(normalized);
    }

    if (resolved_provider && !strcmp(resolved_provider, "copilot")) {
        char *canonical = model_normalize_copilot_model_id(current_model);
        if (canonical && canonical[0] && strcmp(canonical, current_model) != 0) {
            b3_warn_normalized(self,
                "Normalized Copilot model '%s' to '%s'.",
                current_model, canonical, NULL);
            free(self->model);
            self->model = b3_strdup(canonical);
            b3_free(&current_model);
            current_model = b3_strdup(canonical);
            changed = true;
        }
        free(canonical);

        char *mode = models_copilot_model_api_mode(NULL, NULL, current_model, NULL,
                                                   self->api_key);
        if (mode) {
            if ((!self->api_mode || strcmp(mode, self->api_mode) != 0)) {
                free(self->api_mode);
                self->api_mode = b3_strdup(mode);
                changed = true;
            }
            free(mode);
        }
        b3_free(&current_model);
        return changed;
    }

    if (b3_opencode_provider_family(resolved_provider)) {
        char *canonical = normalize_opencode_model_id(resolved_provider, current_model);
        if (canonical && canonical[0] && strcmp(canonical, current_model) != 0) {
            b3_warn_normalized(self,
                "Stripped provider prefix from '%s'; using '%s' for %s.",
                current_model, canonical, resolved_provider);
            free(self->model);
            self->model = b3_strdup(canonical);
            b3_free(&current_model);
            current_model = b3_strdup(canonical);
            changed = true;
        }
        free(canonical);

        char *mode = opencode_model_api_mode(resolved_provider, current_model);
        if (mode) {
            if ((!self->api_mode || strcmp(mode, self->api_mode) != 0)) {
                free(self->api_mode);
                self->api_mode = b3_strdup(mode);
                changed = true;
            }
            free(mode);
        }
        b3_free(&current_model);
        return changed;
    }

    if (!resolved_provider || strcmp(resolved_provider, "openai-codex") != 0) {
        b3_free(&current_model);
        return changed;
    }

    /* 1. Strip provider prefix ("openai/gpt-5.4" → "gpt-5.4"). */
    char *slash = strchr(current_model, '/');
    if (slash) {
        const char *slug = slash + 1;
        if (!self->model_is_default)
            printf("%s⚠️  Stripped provider prefix from '%s'; using '%s' for OpenAI Codex.%s\n",
                   B3_YELLOW, current_model, slug, B3_RST);
        char *slug_copy = b3_strdup(slug);
        free(self->model);
        self->model = slug_copy;
        b3_free(&current_model);
        current_model = b3_strdup(slug_copy);
        changed = true;
    }

    /* 2. Replace untouched default with a Codex model. */
    if (self->model_is_default) {
        const char *fallback_model = "gpt-5.3-codex";
        char **available = codex_models_get_ids(self->api_key ? self->api_key : NULL);
        if (available && available[0])
            fallback_model = available[0];

        if (!current_model || strcmp(current_model, fallback_model) != 0) {
            free(self->model);
            self->model = b3_strdup(fallback_model);
            changed = true;
        }
        if (available) {
            for (char **it = available; *it; it++) free(*it);
            free(available);
        }
    }

    b3_free(&current_model);
    return changed;
}

/* ── _emit_reasoning_preview (helper used by flush) ─────────────────────── */

/* Split into paragraphs on blank lines and compact each to one line.
 * Appends compacted paragraph strings to a caller array. */
static char **b3_split_paragraphs(const char *text, size_t *out_n)
{
    size_t cap = 8, n = 0;
    char **paras = malloc(cap * sizeof(char *));
    const char *p = text;
    if (!paras) { *out_n = 0; return NULL; }

    while (*p) {
        while (*p == '\n' || *p == ' ' || *p == '\t' || *p == '\r') p++;
        if (!*p) break;
        /* One paragraph runs to the next blank line. */
        size_t len = 0;
        while (p[len]) {
            if (p[len] == '\n') {
                size_t k = len + 1;
                while (p[k] == ' ' || p[k] == '\t' || p[k] == '\r') k++;
                if (p[k] == '\n' || !p[k]) break;
            }
            len++;
        }
        /* Compact internal whitespace runs to single spaces. */
        char *comp = malloc(len + 1);
        if (!comp) break;
        size_t j = 0;
        bool at_space = true;
        for (size_t i = 0; i < len; i++) {
            char c = p[i] == '\r' ? ' ' : p[i];
            if (c == '\n' || c == '\t') c = ' ';
            if (c == ' ') {
                if (at_space) continue;
                at_space = true;
            } else {
                at_space = false;
            }
            comp[j++] = c;
        }
        while (j > 0 && comp[j - 1] == ' ') j--;
        comp[j] = '\0';
        if (j > 0) {
            if (n == cap) {
                cap *= 2;
                paras = realloc(paras, cap * sizeof(char *));
            }
            paras[n++] = comp;
        } else {
            free(comp);
        }
        p += len;
    }
    *out_n = n;
    return paras;
}

/* PoP: emit_reasoning_preview @ cli.py:_emit_reasoning_preview */
static void b3_emit_reasoning_preview(hcli_t *self, const char *reasoning_text)
{
    if (!reasoning_text) return;
    char *preview = b3_strdup(reasoning_text);
    if (!preview) return;
    /* strip */
    char *s = preview;
    while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\t' || e[-1] == '\r')) *--e = '\0';
    if (!*s) { free(preview); return; }

    int term_width = b3_terminal_width();
    const char *prefix = "  [thinking] ";
    int wrap_width = term_width - (int)strlen(prefix) - 2;
    if (wrap_width < 30) wrap_width = 30;

    size_t nparas = 0;
    char **paras = b3_split_paragraphs(s, &nparas);

    /* textwrap.fill per paragraph (greedy wrap). */
    size_t total = 1;
    for (size_t i = 0; i < nparas; i++) {
        size_t L = strlen(paras[i]);
        total += L + (L / (size_t)wrap_width + 2) * (size_t)wrap_width + 4;
    }
    char *wrapped = malloc(total);
    size_t w = 0;
    if (wrapped) {
        for (size_t i = 0; i < nparas; i++) {
            const char *para = paras[i];
            size_t plen = strlen(para), start = 0;
            if (i > 0) wrapped[w++] = '\n';
            while (start < plen) {
                if (start > 0 && w > 0) wrapped[w++] = '\n';
                size_t avail = (size_t)wrap_width;
                if (plen - start <= avail) {
                    memcpy(wrapped + w, para + start, plen - start);
                    w += plen - start;
                    break;
                }
                size_t cut = start + avail;
                while (cut > start && para[cut] != ' ') cut--;
                if (cut == start) cut = start + avail;
                memcpy(wrapped + w, para + start, cut - start);
                w += cut - start;
                start = cut;
                while (para[start] == ' ') start++;
            }
        }
        wrapped[w] = '\0';
    }

    if (self->verbose) {
        printf("  %s[thinking] %s%s\n", B3_DIM, wrapped ? wrapped : s, B3_RST);
        goto done;
    }

    {
        /* Cap preview at 5 lines with a "... (N more lines)" tail. */
        int lines = 1;
        for (const char *q = (wrapped ? wrapped : s); *q; q++)
            if (*q == '\n') lines++;
        const char *body = wrapped ? wrapped : s;
        if (lines > 5) {
            printf("  %s[thinking] ", B3_DIM);
            int shown = 0;
            for (const char *q = body; *q && shown < 5; q++) {
                if (*q == '\n') { shown++; if (shown >= 5) break; }
                putchar(*q);
            }
            printf("\n  ... (%d more lines)%s\n", lines - 5, B3_RST);
        } else {
            printf("  %s[thinking] %s%s\n", B3_DIM, body, B3_RST);
        }
    }

done:
    for (size_t i = 0; i < nparas; i++) free(paras[i]);
    free(paras);
    free(wrapped);
    free(preview);
}

/* ── hcli_flush_reasoning_preview ───────────────────────────────────────── */

/* PoP: hcli_flush_reasoning_preview @ cli.py:_flush_reasoning_preview */
void hcli_flush_reasoning_preview(hcli_t *self, bool force)
{
    if (!self || !self->reasoning_preview_buf || !self->reasoning_preview_buf[0])
        return;

    int term_width = b3_terminal_width();
    const char *prefix = "  [thinking] ";
    int target_width = term_width - (int)strlen(prefix) - 4;
    if (target_width < 40) target_width = 40;

    char *buf = self->reasoning_preview_buf;
    size_t blen = strlen(buf);
    char *flush_text = NULL;
    char *rest = NULL;

    if (force) {
        flush_text = b3_strdup(buf);
        rest = b3_strdup("");
    } else {
        char *lb = buf[0] ? strrchr(buf, '\n') : NULL; /* rfind("\n") over whole buf */
        int min_newline_flush = target_width / 3;
        if (min_newline_flush < 16) min_newline_flush = 16;
        size_t lb_pos = lb ? (size_t)(lb - buf) : (size_t)-1;
        bool endsnl2 = blen >= 2 && buf[blen - 1] == '\n' && buf[blen - 2] == '\n';
        bool ends_sent = blen >= 2 && buf[blen - 1] == '\n' &&
                         strchr(".!?:", buf[blen - 2]) && buf[blen - 2] != '\n';

        if (lb && ((long)lb_pos >= min_newline_flush || endsnl2 || ends_sent)) {
            flush_text = b3_strndup_safe(buf, lb_pos + 1);
            rest = b3_strdup(buf + lb_pos + 1);
        } else if ((long)blen >= target_width) {
            size_t search_start = (size_t)(target_width / 2);
            if (search_start < 20) search_start = 20;
            size_t search_end = blen;
            size_t lim = (size_t)target_width + (size_t)(target_width / 3);
            size_t lim2 = (size_t)target_width + 8;
            if (lim2 > lim) lim = lim2;
            if (search_end > lim) search_end = lim;
            long cut = -1;
            const char *boundaries = " \t.!?,;:";
            for (const char *b = boundaries; *b; b++) {
                /* rfind(boundary, search_start, search_end) */
                for (size_t i = search_end; i-- > 0;) {
                    if (i < search_start) break;
                    if (buf[i] == *b) { if ((long)i > cut) cut = (long)i; break; }
                }
            }
            if (cut >= 0) {
                flush_text = b3_strndup_safe(buf, (size_t)cut + 1);
                rest = b3_strdup(buf + cut + 1);
            }
        }
    }

    if (flush_text && rest) {
        /* buf.lstrip() when something was flushed. */
        char *r = rest;
        while (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') r++;
        free(self->reasoning_preview_buf);
        self->reasoning_preview_buf = b3_strdup(r);
        b3_emit_reasoning_preview(self, flush_text);
    }
    free(flush_text);
    free(rest);
}

/* ── _format_submitted_user_message_preview helper ──────────────────────── */

/* PoP: format_submitted_user_message_preview @ cli.py:_format_submitted_user_message_preview
 * Returns a malloc'd rich-markup string (caller frees). */
static char *b3_format_user_preview(hcli_t *self, const char *user_input)
{
    char ts_suffix[128];
    ts_suffix[0] = '\0';
    if (self->show_timestamps) {
        char fmt[64];
        snprintf(fmt, sizeof(fmt), "%s", self->timestamp_format ? self->timestamp_format : "%H:%M");
        char ts[64];
        time_t now = time(NULL);
        struct tm tmv;
        localtime_r(&now, &tmv);
        strftime(ts, sizeof(ts), fmt, &tmv);
        snprintf(ts_suffix, sizeof(ts_suffix), " [dim]%s[/]", ts);
    }

    /* Split into lines. */
    size_t nlines = 1;
    for (const char *p = user_input; *p; p++)
        if (*p == '\n') nlines++;
    char **lines = malloc(nlines * sizeof(char *));
    if (!lines) return NULL;
    {
        size_t i = 0;
        const char *start = user_input;
        for (const char *p = user_input;; p++) {
            if (*p == '\n' || !*p) {
                lines[i++] = b3_strndup_safe(start, (size_t)(p - start));
                if (!*p) break;
                start = p + 1;
            }
        }
    }

    char *accent = cl2_accent_hex();
    const char *acc = accent ? accent : "#00b7eb";

    /* Python: first=max(1,int(getattr(self,'..._first_lines',2)));
     *         last =max(0,int(getattr(self,'..._last_lines',2))). */
    int first_lines = self->user_message_preview_first_lines > 0
                          ? self->user_message_preview_first_lines : 1;
    int last_lines_raw = self->user_message_preview_last_lines != 0
                             ? self->user_message_preview_last_lines : 2;
    int last_lines = last_lines_raw > 0 ? last_lines_raw : 0;

    size_t head_n = (size_t)(first_lines < 1 ? 1 : first_lines);
    if (head_n > nlines) head_n = nlines;
    size_t remaining_after_head = nlines - head_n;
    size_t tail_count = (size_t)(last_lines > 0 ? last_lines : 0);
    if (tail_count > remaining_after_head) tail_count = remaining_after_head;
    long hidden_middle = (long)nlines - (long)head_n - (long)tail_count;
    if (hidden_middle < 0) { hidden_middle = 0; tail_count = 0; }

    /* Assemble with a growing buffer. */
    size_t cap = strlen(user_input) * 2 + 256, len = 0;
    char *out = malloc(cap);

#define B3_APPF(...) do { \
        char _tmp[1024]; \
        int _n = snprintf(_tmp, sizeof(_tmp), __VA_ARGS__); \
        if (_n > 0) { \
            while (len + (size_t)_n + 1 > cap) { cap *= 2; out = realloc(out, cap); } \
            memcpy(out + len, _tmp, (size_t)_n); len += (size_t)_n; out[len] = '\0'; \
        } \
    } while (0)

    out[0] = '\0';
    {
        char *esc = b3_markup_escape(lines[0]);
        B3_APPF("[bold %s]\xe2\x97\x8f[/] [bold]%s[/]%s", acc, esc, ts_suffix);
        free(esc);
    }
    for (size_t i = 1; i < head_n; i++) {
        char *esc = b3_markup_escape(lines[i]);
        B3_APPF("\n[bold]%s[/]", esc);
        free(esc);
    }
    if (hidden_middle > 0) {
        const char *noun = hidden_middle == 1 ? "line" : "lines";
        B3_APPF("\n[dim]... (+%ld more %s)[/]", hidden_middle, noun);
    }
    for (size_t i = nlines - tail_count; i < nlines; i++) {
        char *esc = b3_markup_escape(lines[i]);
        B3_APPF("\n[bold]%s[/]", esc);
        free(esc);
    }
#undef B3_APPF

    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
    free(accent);
    return out;
}

/* ── hcli_print_user_message_preview ────────────────────────────────────── */

/* PoP: hcli_print_user_message_preview @ cli.py:_print_user_message_preview */
void hcli_print_user_message_preview(hcli_t *self, const char *user_input)
{
    char rule[41];
    memset(rule, '-', 40);
    rule[40] = '\0';
    printf("%s%s%s\n", B3_DIM, rule, B3_RST);   /* ChatConsole accent rule line */

    const char *text = user_input ? user_input : "";
    if (strchr(text, '\n')) {
        char *markup = b3_format_user_preview(self, text);
        if (markup) {
            fputs(markup, stdout);
            putchar('\n');
            free(markup);
        }
    } else {
        char *esc = b3_markup_escape(text);
        char *accent2 = cl2_accent_hex();
        printf("[bold %s]\xe2\x97\x8f[/] [bold]%s[/]\n",
               accent2 ? accent2 : "#00b7eb", esc ? esc : "");
        free(accent2);
        free(esc);
    }
}

/* ── hcli_stream_reasoning_delta ────────────────────────────────────────── */

/* PoP: hcli_stream_reasoning_delta @ cli.py:_stream_reasoning_delta */
void hcli_stream_reasoning_delta(hcli_t *self, const char *text)
{
    if (!self || !text || !text[0])
        return;
    self->reasoning_shown_this_turn = true;
    if (self->stream_box_opened)
        return;

    /* Open reasoning box on first reasoning token. */
    if (!self->reasoning_box_opened) {
        self->reasoning_box_opened = true;
        int w = cli_scrollback_box_width(0);
        const char *r_label = " Reasoning ";
        int r_fill = w - 2 - (int)strlen(r_label);
        int dashes = r_fill - 1 > 0 ? r_fill - 1 : 0;
        char *fill = malloc((size_t)dashes * 3 + 1);
        if (fill) {
            fill[0] = '\0';
            for (int i = 0; i < dashes; i++)
                strcat(fill, "\xe2\x94\x80");           /* ─ */
            printf("\n%s\xe2\x94\x8c\xe2\x94\x80%s%s\xe2\x94\x90%s\n",
                   B3_DIM, r_label, fill, "", B3_RST);   /* ┌─<label>───┐ */
            free(fill);
        }
    }

    /* Append to buffer. */
    size_t old_len = self->reasoning_buf ? strlen(self->reasoning_buf) : 0;
    size_t add_len = strlen(text);
    char *nb = realloc(self->reasoning_buf, old_len + add_len + 1);
    if (!nb) return;
    self->reasoning_buf = nb;
    memcpy(self->reasoning_buf + old_len, text, add_len + 1);

    /* Emit complete lines. */
    char *nl;
    while ((nl = strchr(self->reasoning_buf, '\n')) != NULL) {
        char *line = b3_strndup_safe(self->reasoning_buf, (size_t)(nl - self->reasoning_buf));
        memmove(self->reasoning_buf, nl + 1, strlen(nl + 1) + 1);
        printf("%s%s%s\n", B3_DIM, line ? line : "", B3_RST);
        free(line);
    }
    if (strlen(self->reasoning_buf) > 80) {
        printf("%s%s%s\n", B3_DIM, self->reasoning_buf, B3_RST);
        free(self->reasoning_buf);
        self->reasoning_buf = b3_strdup("");
    }
}

/* ── hcli_busy_command / hcli_busy_command_end ──────────────────────────── */

static bool b3_prev_blocks_input_slot_init = false;
static bool b3_prev_blocks_input;   /* previous_blocks_input saved across pair */

/* PoP: hcli_busy_command @ cli.py:_busy_command
 * Enter part of the generator context manager; pair with
 * hcli_busy_command_end() in the caller's cleanup path (`finally`). */
void hcli_busy_command(hcli_t *self, const char *status, bool blocks_input)
{
    if (!self) return;
    b3_prev_blocks_input = self->command_blocks_input;
    b3_prev_blocks_input_slot_init = true;
    self->command_running = true;
    self->command_blocks_input = blocks_input;
    free(self->command_status);
    self->command_status = b3_strdup(status ? status : "");
    b3_invalidate(self);
    printf("\xe2\x8f\xb3 %s\n", status ? status : "");
}

/* PoP: hcli_busy_command_end @ cli.py:_busy_command (finally branch) */
void hcli_busy_command_end(hcli_t *self)
{
    if (!self || !b3_prev_blocks_input_slot_init) return;
    self->command_running = false;
    self->command_blocks_input = b3_prev_blocks_input;
    b3_prev_blocks_input_slot_init = false;
    free(self->command_status);
    self->command_status = b3_strdup("");
    b3_invalidate(self);
}

/* ── paste-reference expansion (_expand_paste_references) ───────────────── */

/* Expand "[Pasted text #N: M lines → path]" placeholders into file contents.
 * Returns malloc'd expanded text (caller frees). */
static char *b3_expand_paste_references(const char *text)
{
    static const char *marker = "[Pasted text #";
    if (!text || !strstr(text, marker))
        return b3_strdup(text ? text : "");

    size_t cap = strlen(text) + 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    const char *p = text;
    while (*p) {
        const char *m = strstr(p, marker);
        if (!m) break;
        /* Copy up to marker. */
        while (len + (size_t)(m - p) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
        memcpy(out + len, p, (size_t)(m - p)); len += (size_t)(m - p); out[len] = '\0';

        /* Parse "[Pasted text #N: M lines → path]" strictly. */
        const char *q = m + strlen(marker);
        char *endp;
        (void)strtol(q, &endp, 10);          /* #N */
        q = endp;
        unsigned long nlines;
        {
            char *e2;
            if (*q != ':' || *++q != ' ') goto literal;
            nlines = strtoul(q + 1, &e2, 10);
            (void)nlines;
            q = e2;
            static const char *suffix = " lines \xe2\x86\x92 ";
            if (strncmp(q, suffix, strlen(suffix)) != 0) goto literal;
            q += strlen(suffix);
            const char *close = strchr(q, ']');
            if (!close || close == q) goto literal;
            char *path = b3_strndup_safe(q, (size_t)(close - q));
            FILE *f = fopen(path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long fl = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (fl >= 0) {
                    while (len + (size_t)fl + 1 > cap) { cap *= 2; out = realloc(out, cap); }
                    size_t got = fread(out + len, 1, (size_t)fl, f);
                    len += got; out[len] = '\0';
                }
                fclose(f);
            } else {
                /* Paste file gone/unreadable → keep placeholder (#17666). */
                while (len + (size_t)(close - m + 1) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
                memcpy(out + len, m, (size_t)(close - m + 1));
                len += (size_t)(close - m + 1);
                out[len] = '\0';
            }
            free(path);
            p = close + 1;
            continue;
literal:
            /* Not a valid ref — copy the marker literally and rescan after it. */
            while (len + strlen(marker) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + len, m, strlen(marker)); len += strlen(marker); out[len] = '\0';
            p = m + strlen(marker);
            continue;
        }
    }
    while (len + strlen(p) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
    memcpy(out + len, p, strlen(p) + 1);
    return out;
}

/* ── hcli_inline_pastes ─────────────────────────────────────────────────── */

/* PoP: hcli_inline_pastes @ cli.py:_inline_pastes */
void hcli_inline_pastes(hcli_t *self, char **buffer_text, size_t *cursor_position)
{
    if (!self || !buffer_text || !*buffer_text)
        return;
    char *existing = *buffer_text;
    char *expanded = b3_expand_paste_references(existing);
    if (expanded && strcmp(expanded, existing) != 0) {
        self->skip_paste_collapse = true;
        free(existing);
        *buffer_text = expanded;
        if (cursor_position)
            *cursor_position = strlen(expanded);
    } else {
        free(expanded);
    }
}
