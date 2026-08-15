/* port_prompt_stash.c — C11 port of hermes_cli/prompt_stash.py
 *
 * See port_prompt_stash.h for the faithful-port contract.
 */

#include "port_prompt_stash.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>

/* forward */
static int prompt_stash_clamp_cursor(prompt_stash_t *ps, int value);

/* Default monotonic clock (seconds). */
static double default_clock(void) {
    /* struct timespec -> seconds as double. */
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    ts.tv_sec = time(NULL); ts.tv_nsec = 0;
#endif
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Build a preview: collapse newlines/tabs into a single line, ellipsize. */
/* PoP: build_preview @ hermes_cli/prompt_stash.py:build_preview */
char *prompt_stash_build_preview(const char *text, int width) {
    if (!text || !*text) return strdup("");
    /* flatten */
    size_t n = strlen(text);
    char *flat = malloc(n * 3 + 1);
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        if (c == '\r') {
            if (i + 1 < n && text[i + 1] == '\n') i++;
            /* -> newline handled below */
            if (w + 3 < n * 3) { flat[w++] = ' '; flat[w++] = 0xE2; flat[w++] = 0x8F; flat[w++] = 0x8E; flat[w++] = ' '; }
            continue;
        }
        if (c == '\n') {
            if (w + 3 < n * 3) { flat[w++] = ' '; flat[w++] = 0xE2; flat[w++] = 0x8F; flat[w++] = 0x8E; flat[w++] = ' '; }
            continue;
        }
        if (c == '\t') { flat[w++] = ' '; continue; }
        flat[w++] = c;
    }
    flat[w] = '\0';
    /* collapse runs of whitespace to single space */
    char *collapsed = malloc(w + 1);
    size_t cw = 0;
    bool in_ws = false;
    for (size_t i = 0; i < w; i++) {
        if (flat[i] == ' ' || flat[i] == '\t') {
            if (!in_ws) { collapsed[cw++] = ' '; in_ws = true; }
        } else {
            collapsed[cw++] = flat[i];
            in_ws = false;
        }
    }
    collapsed[cw] = '\0';
    free(flat);
    if (width > 1 && cw > (size_t)width) {
        char *out = malloc((size_t)width + 4);
        memcpy(out, collapsed, (size_t)(width - 1));
        out[width - 1] = 0xE2; out[width] = 0x80; out[width + 1] = 0xA6; /* … */
        out[width + 2] = '\0';
        free(collapsed);
        return out;
    }
    return collapsed;
}

/* PoP: stash_entry_t @ hermes_cli/prompt_stash.py:StashEntry */

/* PoP: prompt_stash_t @ hermes_cli/prompt_stash.py:PromptStash */
struct prompt_stash {
    stash_entry_t **items;   /* newest-first; items[0] most recent */
    size_t        count;
    size_t        cap;       /* allocated slots */
    int           max_items;
    prompt_stash_clock_fn clock;
    bool          panel_open;
    int           panel_cursor;
};

/* PoP: prompt_stash_new @ hermes_cli/prompt_stash.py:PromptStash.__init__ */
prompt_stash_t *prompt_stash_new(int max_items, prompt_stash_clock_fn clock) {
    prompt_stash_t *ps = calloc(1, sizeof(*ps));
    if (!ps) return NULL;
    ps->max_items = max_items < 1 ? 1 : max_items;
    ps->clock = clock ? clock : default_clock;
    ps->panel_open = false;
    ps->panel_cursor = 0;
    ps->cap = 8;
    ps->items = calloc(ps->cap, sizeof(stash_entry_t *));
    return ps;
}

static void entry_free(stash_entry_t *e) {
    if (!e) return;
    free(e->text);
    if (e->images) json_free(e->images);
    free(e->preview);
    free(e);
}

/* PoP: prompt_stash_len @ hermes_cli/prompt_stash.py:PromptStash.__len__ */
size_t prompt_stash_len(const prompt_stash_t *ps) { return ps ? ps->count : 0; }
/* PoP: prompt_stash_bool @ hermes_cli/prompt_stash.py:PromptStash.__bool__ */
bool prompt_stash_bool(const prompt_stash_t *ps) { return ps && ps->count > 0; }
size_t prompt_stash_count(const prompt_stash_t *ps) { return ps ? ps->count : 0; }

static void ensure_cap(prompt_stash_t *ps, size_t need) {
    if (ps->cap >= need) return;
    size_t nc = ps->cap ? ps->cap * 2 : 8;
    while (nc < need) nc *= 2;
    ps->items = realloc(ps->items, nc * sizeof(stash_entry_t *));
    ps->cap = nc;
}

/* PoP: prompt_stash_items @ hermes_cli/prompt_stash.py:PromptStash.items */
stash_entry_t **prompt_stash_items(const prompt_stash_t *ps, size_t *out_n) {
    size_t n = ps ? ps->count : 0;
    stash_entry_t **out = calloc(n ? n : 1, sizeof(stash_entry_t *));
    for (size_t i = 0; i < n; i++) out[i] = ps->items[i];
    *out_n = n;
    return out;
}

void prompt_stash_free_entries(stash_entry_t **items, size_t n) { free(items); }

/* PoP: prompt_stash_entry_as_dict @ hermes_cli/prompt_stash.py:StashEntry.as_dict */
json_t *prompt_stash_entry_as_dict(const stash_entry_t *e) {
    json_t *d = json_object();
    json_set(d, "text", json_string(e->text ? e->text : ""));
    json_set(d, "images", e->images ? e->images : json_array());
    json_set(d, "stashed_at", json_number(e->stashed_at));
    json_set(d, "preview", json_string(e->preview ? e->preview : ""));
    return d;
}

/* PoP: prompt_stash_panel_rows @ hermes_cli/prompt_stash.py:PromptStash.panel_rows */
json_t *prompt_stash_panel_rows(const prompt_stash_t *ps) {
    json_t *arr = json_array();
    for (size_t i = 0; i < ps->count; i++) {
        json_append(arr, prompt_stash_entry_as_dict(ps->items[i]));
    }
    return arr;
}

/* PoP: prompt_stash_indicator @ hermes_cli/prompt_stash.py:PromptStash.indicator */
char *prompt_stash_indicator(const prompt_stash_t *ps) {
    size_t n = ps ? ps->count : 0;
    if (!n) return strdup("");
    size_t need = 32;
    char *buf = malloc(need);
    if (ps->panel_open) snprintf(buf, need, "\xF0\x9F\x93\x8C %zu \xE2\x96\xB2", n);
    else              snprintf(buf, need, "\xF0\x9F\x93\x8C %zu", n);
    return buf;
}

/* PoP: prompt_stash_placeholder_hint @ hermes_cli/prompt_stash.py:PromptStash.placeholder_hint */
char *prompt_stash_placeholder_hint(const prompt_stash_t *ps) {
    size_t n = ps ? ps->count : 0;
    if (!n) return strdup("");
    if (n == 1) {
        const char *prev = ps->items[0]->preview ? ps->items[0]->preview : "";
        size_t need = strlen(prev) + 32;
        char *buf = malloc(need);
        snprintf(buf, need, "Ctrl+S to restore: %s", prev);
        return buf;
    }
    size_t need = 48;
    char *buf = malloc(need);
    snprintf(buf, need, "Ctrl+S to browse %zu stashed drafts", n);
    return buf;
}

/* PoP: prompt_stash_stash @ hermes_cli/prompt_stash.py:PromptStash.stash */
bool prompt_stash_stash(prompt_stash_t *ps, const char *text, json_t *images) {
    bool has_images = images && json_len(images) > 0;
    /* Python: if not (text or "").strip() and not has_images: return False */
    const char *t = text ? text : "";
    bool blank = true;
    for (const char *p = t; *p; p++) {
        if (!isspace((unsigned char)*p)) { blank = false; break; }
    }
    if (blank && !has_images) return false;

    stash_entry_t *e = calloc(1, sizeof(*e));
    e->text = strdup(t);
    e->images = images ? images : NULL; /* take ownership when provided */
    e->stashed_at = ps->clock();
    char *pv = prompt_stash_build_preview(t, PROMPT_STASH_PREVIEW_WIDTH);
    e->preview = pv && *pv ? pv : strdup("(images only)");

    /* insert at front (newest-first) */
    ensure_cap(ps, ps->count + 1);
    memmove(&ps->items[1], &ps->items[0], ps->count * sizeof(stash_entry_t *));
    ps->items[0] = e;
    ps->count++;
    /* trim oldest past the cap */
    while (ps->count > (size_t)ps->max_items) {
        entry_free(ps->items[ps->count - 1]);
        ps->items[ps->count - 1] = NULL;
        ps->count--;
    }
    ps->panel_open = false;
    ps->panel_cursor = 0;
    return true;
}

/* PoP: prompt_stash_pop @ hermes_cli/prompt_stash.py:PromptStash.pop */
bool prompt_stash_pop(prompt_stash_t *ps, int index,
                      char **out_text, json_t **out_images) {
    *out_text = NULL; *out_images = NULL;
    if (!ps->count) return false;
    if (!(0 <= index && (size_t)index < ps->count)) return false;
    stash_entry_t *e = ps->items[index];
    *out_text = e->text; e->text = NULL;
    *out_images = e->images; e->images = NULL;
    /* shift down */
    memmove(&ps->items[index], &ps->items[index + 1],
            (ps->count - index - 1) * sizeof(stash_entry_t *));
    ps->count--;
    entry_free(e);
    if (!ps->count) { ps->panel_open = false; }
    ps->panel_cursor = prompt_stash_clamp_cursor(ps, ps->panel_cursor);
    return true;
}

/* PoP: prompt_stash_peek @ hermes_cli/prompt_stash.py:PromptStash.peek */
const stash_entry_t *prompt_stash_peek(const prompt_stash_t *ps, int index) {
    if (!ps || !ps->count) return NULL;
    if (!(0 <= index && (size_t)index < ps->count)) return NULL;
    return ps->items[index];
}

/* PoP: prompt_stash_clear @ hermes_cli/prompt_stash.py:PromptStash.clear */
void prompt_stash_clear(prompt_stash_t *ps) {
    for (size_t i = 0; i < ps->count; i++) entry_free(ps->items[i]);
    ps->count = 0;
    ps->panel_open = false;
    ps->panel_cursor = 0;
}

/* clamp helper (used by cursor ops) */
/* PoP: prompt_stash_clamp_cursor @ hermes_cli/prompt_stash.py:PromptStash._clamp_cursor */
static int clamp_cursor_int(prompt_stash_t *ps, int value) {
    if (!ps->count) return 0;
    int hi = (int)ps->count - 1;
    if (value < 0) return 0;
    if (value > hi) return hi;
    return value;
}
int prompt_stash_clamp_cursor(prompt_stash_t *ps, int value) {
    return clamp_cursor_int(ps, value);
}

/* PoP: prompt_stash_open_panel @ hermes_cli/prompt_stash.py:PromptStash.open_panel */
bool prompt_stash_open_panel(prompt_stash_t *ps) {
    if (!ps->count) return false;
    ps->panel_open = true;
    ps->panel_cursor = 0;
    return true;
}

/* PoP: prompt_stash_close_panel @ hermes_cli/prompt_stash.py:PromptStash.close_panel */
void prompt_stash_close_panel(prompt_stash_t *ps) {
    ps->panel_open = false;
    ps->panel_cursor = 0;
}

/* PoP: prompt_stash_move_cursor @ hermes_cli/prompt_stash.py:PromptStash.move_cursor */
int prompt_stash_move_cursor(prompt_stash_t *ps, int delta) {
    ps->panel_cursor = clamp_cursor_int(ps, ps->panel_cursor + delta);
    return ps->panel_cursor;
}

/* PoP: prompt_stash_delete_at_cursor @ hermes_cli/prompt_stash.py:PromptStash.delete_at_cursor */
bool prompt_stash_delete_at_cursor(prompt_stash_t *ps) {
    if (!ps->count) return false;
    int idx = clamp_cursor_int(ps, ps->panel_cursor);
    entry_free(ps->items[idx]);
    memmove(&ps->items[idx], &ps->items[idx + 1],
            (ps->count - idx - 1) * sizeof(stash_entry_t *));
    ps->count--;
    if (!ps->count) {
        ps->panel_open = false;
        ps->panel_cursor = 0;
    } else {
        ps->panel_cursor = clamp_cursor_int(ps, idx);
    }
    return true;
}

/* PoP: prompt_stash_restore_at_cursor @ hermes_cli/prompt_stash.py:PromptStash.restore_at_cursor */
bool prompt_stash_restore_at_cursor(prompt_stash_t *ps,
                                   char **out_text, json_t **out_images) {
    *out_text = NULL; *out_images = NULL;
    if (!ps->count) return false;
    int idx = clamp_cursor_int(ps, ps->panel_cursor);
    bool ok = prompt_stash_pop(ps, idx, out_text, out_images);
    prompt_stash_close_panel(ps);
    return ok;
}

/* PoP: resolve_ctrl_s @ hermes_cli/prompt_stash.py:resolve_ctrl_s */
ctrl_s_action_t prompt_stash_resolve_ctrl_s(prompt_stash_t *ps,
                                            const char *buffer_text,
                                            json_t *images,
                                            char **out_text,
                                            json_t **out_images) {
    *out_text = NULL; *out_images = NULL;
    /* Panel open -> close it. */
    if (ps->panel_open) {
        prompt_stash_close_panel(ps);
        return CTRL_S_CLOSE_PANEL;
    }
    /* Non-blank buffer or images -> park it. */
    const char *b = buffer_text ? buffer_text : "";
    bool blank = true;
    for (const char *p = b; *p; p++) {
        if (!isspace((unsigned char)*p)) { blank = false; break; }
    }
    bool has_images = images && json_len(images) > 0;
    if (!blank || has_images) {
        return prompt_stash_stash(ps, b, images) ? CTRL_S_STASHED : CTRL_S_NOOP;
    }
    /* Empty buffer -> restore half. */
    size_t count = ps->count;
    if (count == 0) return CTRL_S_NOOP;
    if (count == 1) {
        if (prompt_stash_pop(ps, 0, out_text, out_images)) return CTRL_S_RESTORED;
        return CTRL_S_NOOP;
    }
    prompt_stash_open_panel(ps);
    return CTRL_S_OPEN_PANEL;
}

void prompt_stash_free(prompt_stash_t *ps) {
    if (!ps) return;
    for (size_t i = 0; i < ps->count; i++) entry_free(ps->items[i]);
    free(ps->items);
    free(ps);
}
