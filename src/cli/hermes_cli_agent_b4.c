/*
 * hermes_cli_agent_b4.c — HermesCLI batch 4 methods (cli.py class HermesCLI).
 * Approval modal handling, computer_use verdict translation, secret capture
 * submit/cancel, exit summary, TUI prompt text / layout children, stash panel.
 */
#include "cli_hermes_cli.h"
#include "profile_store.h"
#include "hermes_state_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* hermes_state.py:get_session_title (SessionDB method port, hermes_state_misc.c)
 * — not yet declared in a public header; declare here with the real signature. */
extern char *hermes_state_get_session_title(hermes_state_db_t *db,
                                            const char *session_id);

/* ── shared helpers ─────────────────────────────────────────────────────── */

static double b4_now_monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static double b4_now_wall(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static char *b4_strdup(const char *s)
{
    return s ? strdup(s) : NULL;
}

/* Fill buf with n box-drawing dashes ('─' is 3 UTF-8 bytes). */
static void b4_fill_dashes(char *buf, int n, size_t bufsz)
{
    static const char D[] = "─";
    size_t pos = 0;
    for (int i = 0; i < n && pos + sizeof(D) <= bufsz; i++) {
        memcpy(buf + pos, D, sizeof(D));   /* includes NUL each time; harmless */
        pos += 3;
    }
    if (pos < bufsz) buf[pos] = '\0';
}

/* Free an approval-state struct owned by the hcli_t. */
static void b4_approval_state_free(hcli_approval_state_t *st)
{
    if (!st) return;
    for (size_t i = 0; i < st->choice_count; i++) free(st->choices[i]);
    free(st->choices);
    free(st->command);
    free(st->description);
    free(st);
}

/* ── hcli_computer_use_approval_callback ────────────────────────────────── */

/* PoP: hcli_computer_use_approval_callback @ cli.py:_computer_use_approval_callback */
char *hcli_computer_use_approval_callback(hcli_t *self, const char *action,
                                          const char *args_json,
                                          const char *summary)
{
    (void)args_json;   /* Python accepts `args` but never reads it */
    if (!self || !self->approval_prompt_fn)
        return strdup("deny");   /* no UI bound → cannot approve → deny */

    /* Build a command-ish string so the existing UI renders something
     * meaningful. `summary` is already a one-line human description. */
    char *command = malloc(strlen("computer_use: ") +
                           strlen(summary ? summary : "") + 1);
    char *description = malloc(strlen("Allow computer_use to perform ``?")
                               + strlen(action ? action : "") + 1);
    if (!command || !description) { free(command); free(description); return strdup("deny"); }
    sprintf(command, "computer_use: %s", summary ? summary : "");
    sprintf(description, "Allow computer_use to perform `%s`?", action ? action : "");

    char *verdict = self->approval_prompt_fn(self, command, description);
    free(command);
    free(description);
    if (!verdict)
        return strdup("deny");   /* dict.get(verdict, "deny") — miss → deny */

    const char *mapped;
    if      (!strcmp(verdict, "once"))    mapped = "approve_once";
    else if (!strcmp(verdict, "session")) mapped = "approve_session";
    else if (!strcmp(verdict, "always"))  mapped = "always_approve";
    else if (!strcmp(verdict, "timeout")) mapped = "timeout";
    else                                  mapped = "deny";   /* "deny" + unknown */
    free(verdict);
    return strdup(mapped);
}

/* _invalidate / _paint_now live in sibling batch files that may not be
 * linked yet; bind weakly so this object links standalone. Same for
 * hcli_fragments_free (declared in the shared header, owned by a sibling). */
extern void hcli_invalidate(hcli_t *self) __attribute__((weak));
extern void hcli_paint_now(hcli_t *self) __attribute__((weak));
extern void hcli_fragments_free(hcli_fragment_t *frags, size_t len)
    __attribute__((weak));

/* Weak-safe fragment free. */
static void b4_fragments_free(hcli_fragment_t *frags, size_t len)
{
    if (!frags)
        return;
    if (hcli_fragments_free) {
        b4_fragments_free(frags, len);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        free(frags[i].style);
        free(frags[i].text);
    }
    free(frags);
}

/* ── hcli_handle_approval_selection ─────────────────────────────────────── */

/* PoP: hcli_handle_approval_selection @ cli.py:_handle_approval_selection */
void hcli_handle_approval_selection(hcli_t *self)
{
    if (!self) return;
    hcli_approval_state_t *state = self->approval_state;
    if (!state)
        return;

    int selected = state->selected;
    size_t n = state->choice_count;

    /* if not (0 <= selected < len(choices)): return */
    if (selected < 0 || (size_t)selected >= n)
        return;

    const char *chosen = state->choices[selected] ? state->choices[selected] : "";

    if (!strcmp(chosen, "view")) {
        state->show_full = true;
        /* choices = [c for c in choices if c != "view"] */
        size_t w = 0;
        for (size_t i = 0; i < n; i++) {
            if (state->choices[i] && strcmp(state->choices[i], "view") == 0) {
                free(state->choices[i]);
                continue;
            }
            state->choices[w++] = state->choices[i];
        }
        state->choice_count = w;
        if ((size_t)state->selected >= w)
            state->selected = w > 0 ? (int)w - 1 : 0;
        if (hcli_invalidate) hcli_invalidate(self);
        return;
    }

    if (state->response_queue && state->queue_put)
        state->queue_put(state->response_queue, chosen);
    b4_approval_state_free(state);
    self->approval_state = NULL;
    if (hcli_invalidate) hcli_invalidate(self);
}

/* ── hcli_submit_secret_response / hcli_cancel_secret_capture ───────────── */

/* PoP: hcli_submit_secret_response @ cli.py:_submit_secret_response */
void hcli_submit_secret_response(hcli_t *self, const char *value)
{
    if (!self || !self->secret_state)
        return;
    if (self->secret_state->response_queue && self->secret_state->queue_put)
        self->secret_state->queue_put(self->secret_state->response_queue,
                                      value ? value : "");
    free(self->secret_state);
    self->secret_state = NULL;
    self->secret_deadline = 0;
    /* Modal teardown — paint directly so the secret panel clears at once and
     * isn't held by the _invalidate throttle/resize guard (#41098). */
    if (hcli_paint_now) hcli_paint_now(self);
}

/* PoP: hcli_cancel_secret_capture @ cli.py:_cancel_secret_capture */
void hcli_cancel_secret_capture(hcli_t *self)
{
    hcli_submit_secret_response(self, "");
}

/* ── hcli_print_exit_summary ────────────────────────────────────────────── */

/* PoP: hcli_print_exit_summary @ cli.py:_print_exit_summary */
void hcli_print_exit_summary(hcli_t *self, bool clear_screen)
{
    if (!self) return;
    if (clear_screen && self->clear_terminal_fn)
        self->clear_terminal_fn(self);
    printf("\n");

    size_t msg_count = self->history_len;
    if (msg_count > 0) {
        size_t user_msgs = 0, tool_calls = 0;
        for (size_t i = 0; i < msg_count; i++) {
            const char *role = self->history[i].role ? self->history[i].role : "";
            /* m.get("role") == "user" */
            if (!strcmp(role, "user"))
                user_msgs++;
            /* m.get("role") == "tool" or m.get("tool_calls") — the in-memory
             * hcli_msg_t carries no tool_calls field; role=="tool" rows are
             * the tool-call transcript entries this struct models. */
            if (!strcmp(role, "tool"))
                tool_calls++;
        }

        double elapsed = 0.0;
        if (self->has_session_start)
            elapsed = b4_now_wall() - self->session_start_epoch;
        long total = elapsed > 0 ? (long)elapsed : 0;
        long hours = total / 3600, minutes = (total % 3600) / 60, seconds = total % 60;
        char duration_str[32];
        if (hours > 0)
            snprintf(duration_str, sizeof(duration_str), "%ldh %ldm %lds", hours, minutes, seconds);
        else if (minutes > 0)
            snprintf(duration_str, sizeof(duration_str), "%ldm %lds", minutes, seconds);
        else
            snprintf(duration_str, sizeof(duration_str), "%lds", seconds);

        /* Look up session title for resume-by-name hint */
        char *session_title = NULL;
        if (self->session_db && self->session_id)
            session_title = hermes_state_get_session_title(self->session_db,
                                                           self->session_id);

        printf("Resume this session with:\n");
        /* Session IDs are profile-constrained → include `-p <profile>` for
         * non-default profiles ("default"/"custom" need no flag). */
        char *active_profile = profile_get_active_name();
        if (!active_profile)
            active_profile = strdup("default");
        int is_default = !strcmp(active_profile, "default") ||
                         !strcmp(active_profile, "custom");
        /* " -p <profile>" suffix for non-default profiles, else "" */
        char profile_flag[128] = "";
        if (!is_default)
            snprintf(profile_flag, sizeof(profile_flag), " -p %s", active_profile);
        printf("  hermes --resume %s%s\n",
               self->session_id ? self->session_id : "", profile_flag);
        if (session_title && self->session_id)
            printf("  hermes -c \"%s\"%s\n", session_title, profile_flag);
        printf("\n");
        printf("Session:        %s\n", self->session_id ? self->session_id : "");
        if (session_title)
            printf("Title:          %s\n", session_title);
        printf("Duration:       %s\n", duration_str);
        printf("Messages:       %zu (%zu user, %zu tool calls)\n",
               msg_count, user_msgs, tool_calls);
        free(session_title);
        free(active_profile);
    } else {
        if (self->goodbye_fn)
            self->goodbye_fn("Goodbye! ⚕");
        else
            printf("Goodbye! ⚕\n");
    }
}

/* ── hcli_get_tui_prompt_text ───────────────────────────────────────────── */

/* PoP: hcli_get_tui_prompt_text @ cli.py:_get_tui_prompt_text */
char *hcli_get_tui_prompt_text(hcli_t *self)
{
    if (!self || !self->tui_prompt_fragments_fn)
        return strdup("");
    size_t len = 0;
    hcli_fragment_t *frags = self->tui_prompt_fragments_fn(self, &len);
    if (!frags)
        return strdup("");
    size_t total = 1;
    for (size_t i = 0; i < len; i++)
        total += frags[i].text ? strlen(frags[i].text) : 0;
    char *out = malloc(total);
    if (!out) { b4_fragments_free(frags, len); return strdup(""); }
    out[0] = '\0';
    for (size_t i = 0; i < len; i++)
        if (frags[i].text) strcat(out, frags[i].text);
    b4_fragments_free(frags, len);
    return out;
}

/* ── hcli_build_tui_layout_children ─────────────────────────────────────── */

/* PoP: hcli_build_tui_layout_children @ cli.py:_build_tui_layout_children */
char **hcli_build_tui_layout_children(
    hcli_t *self,
    const char *sudo_widget,
    const char *secret_widget,
    const char *approval_widget,
    const char *slash_confirm_widget,
    const char *clarify_widget,
    const char *model_picker_widget,
    const char *command_palette_widget,
    const char *spinner_widget,
    const char *spacer,
    const char *status_bar,
    const char *input_rule_top,
    const char *image_bar,
    const char *input_area,
    const char *input_rule_bot,
    const char *voice_status_bar,
    const char *completions_menu,
    size_t extra_count, char *const *extra_widgets,
    size_t *out_len)
{
    (void)self;
    if (out_len) *out_len = 0;

    /* Ordered candidate list exactly as in the Python list literal; NULL
     * entries model `None` and are filtered by the final comprehension. */
    const char *items[24];
    size_t n = 0;
    items[n++] = "Window(height=0)";
    items[n++] = sudo_widget;
    items[n++] = secret_widget;
    items[n++] = approval_widget;
    items[n++] = slash_confirm_widget;
    items[n++] = clarify_widget;
    items[n++] = model_picker_widget;
    items[n++] = command_palette_widget;
    items[n++] = spinner_widget;
    items[n++] = spacer;
    for (size_t i = 0; i < extra_count; i++)           /* *_get_extra_tui_widgets */
        items[n++] = extra_widgets ? extra_widgets[i] : NULL;
    items[n++] = self && self->has_pet_widget ? "_pet_widget" : NULL;
    items[n++] = self && self->has_stash_panel_widget ? "_stash_panel_widget" : NULL;
    items[n++] = status_bar;
    items[n++] = input_rule_top;
    items[n++] = image_bar;
    items[n++] = input_area;
    items[n++] = input_rule_bot;
    items[n++] = voice_status_bar;
    items[n++] = completions_menu;

    size_t keep = 0;
    for (size_t i = 0; i < n; i++)
        if (items[i]) keep++;

    char **out = malloc(sizeof(char *) * (keep > 0 ? keep : 1));
    if (!out) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        if (!items[i]) continue;
        out[w] = strdup(items[i]);
        if (!out[w]) {
            for (size_t j = 0; j < w; j++) free(out[j]);
            free(out);
            return NULL;
        }
        w++;
    }
    if (out_len) *out_len = w;
    return out;
}

/* ── hcli_render_stash_panel ────────────────────────────────────────────── */

/* Python _fmt_stash_age: human-readable age for a monotonic stashed_at. */
static void b4_fmt_stash_age(double stashed_at, char *buf, size_t bufsz)
{
    long secs = (long)(b4_now_monotonic() - stashed_at);
    if (secs < 0) secs = 0;
    if (secs < 10)     snprintf(buf, bufsz, "just now");
    else if (secs < 90) snprintf(buf, bufsz, "%lds ago", secs);
    else {
        long mins = secs / 60;
        if (mins < 60) snprintf(buf, bufsz, "%ld min ago", mins);
        else           snprintf(buf, bufsz, "%ldh ago", mins / 60);
    }
}

static bool b4_frag_push(hcli_fragment_t **frags, size_t *n, size_t *cap,
                         const char *style, const char *text)
{
    if (*n == *cap) {
        size_t ncap = *cap ? *cap * 2 : 16;
        hcli_fragment_t *grown = realloc(*frags, sizeof(**frags) * ncap);
        if (!grown) return false;
        *frags = grown;
        *cap = ncap;
    }
    (*frags)[*n].style = strdup(style);
    (*frags)[*n].text = strdup(text);
    if (!(*frags)[*n].style || !(*frags)[*n].text) return false;
    (*n)++;
    return true;
}

/* PoP: hcli_render_stash_panel @ cli.py:_render_stash_panel */
hcli_fragment_t *hcli_render_stash_panel(hcli_t *self,
                                         const hcli_stash_row_t *rows,
                                         size_t row_count,
                                         int cursor, int width,
                                         size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!self || !rows) return NULL;

    int (*cw)(const char *) = cli_status_bar_display_width;

    int W = width - 4;
    if (W < 12) W = 12;
    if (W > 80) W = 80;

    size_t n = row_count;
    char hdr_prefix[64];
    snprintf(hdr_prefix, sizeof(hdr_prefix),
             "╭─ 📌 Stash (%zu item%s) ", n, n != 1 ? "s" : "");
    const char *hdr_suffix = " Ctrl+S ─╮";
    const char *ftr_prefix = "╰";
    const char *ftr_suffix = " ↑↓ Enter=restore  D=delete  Esc ─╯";

    char compact_hdr[48];
    /* On narrow terminals drop to compact affordances rather than letting
     * the frame bleed past the right edge. */
    if (cw(hdr_prefix) + cw(hdr_suffix) > W) {
        snprintf(compact_hdr, sizeof(compact_hdr), "╭─ 📌 %zu ", n);
        hdr_suffix = "─╮";
    }
    const char *ftr2 = " ↑↓ ⏎ D Esc ─╯";
    if (cw(ftr_prefix) + cw(ftr_suffix) > W)
        ftr_suffix = ftr2;
    if (cw(ftr_prefix) + cw(ftr_suffix) > W)
        ftr_suffix = "─╯";

    const char *prefix_str = cw(hdr_prefix) + cw(hdr_suffix) > W
                                 ? compact_hdr : hdr_prefix;
    int hdr_dashes = W - cw(prefix_str) - cw(hdr_suffix);
    if (hdr_dashes < 0) hdr_dashes = 0;
    int ftr_dashes = W - cw(ftr_prefix) - cw(ftr_suffix);
    if (ftr_dashes < 0) ftr_dashes = 0;

    int INNER = W - 2;   /* minus the two '│' border cells */

    hcli_fragment_t *frags = NULL;
    size_t fn = 0, fcap = 0;
    char line[512];
    char dashes[128];

    /* line(): final guard trims every emitted line to the box width. */
    int nd = hdr_dashes < (int)sizeof(dashes) - 1 ? hdr_dashes : (int)sizeof(dashes) - 1;
    b4_fill_dashes(dashes, nd, sizeof(dashes));
    snprintf(line, sizeof(line), "%s%s%s\n", prefix_str, dashes, hdr_suffix);

    /* header row */
    if (!b4_frag_push(&frags, &fn, &fcap, "class:subagent-border", line))
        goto oom;

    for (size_t i = 0; i < row_count; i++) {
        char age[32];
        b4_fmt_stash_age(rows[i].stashed_at, age, sizeof(age));

        /* Row: " ► [N] {age:<10} {preview} " */
        char prefix[96];
        snprintf(prefix, sizeof(prefix), " %s [%zu] %-10s ",
                 (int)i == cursor ? "►" : " ", i + 1, age);
        if (cw(prefix) > INNER - 2)
            snprintf(prefix, sizeof(prefix), " %s [%zu] ",
                     (int)i == cursor ? "►" : " ", i + 1);

        int avail = INNER - cw(prefix) - 1;
        if (avail < 0) avail = 0;
        char *preview = cli_trim_status_bar_text(
            rows[i].preview ? rows[i].preview : "", avail);
        if (!preview) goto oom;
        /* preview = preview + " " * max(0, avail - cw(preview)) */
        int pad = avail - cw(preview);
        if (pad > 0) {
            char *grown = realloc(preview, strlen(preview) + (size_t)pad + 1);
            if (!grown) { free(preview); goto oom; }
            preview = grown;
            memset(preview + strlen(preview), ' ', (size_t)pad);
            preview[strlen(preview) + (size_t)pad] = '\0';
        }

        char row[512];
        snprintf(row, sizeof(row), "│%s%s │", prefix, preview);

        if ((int)i == cursor) {
            /* single selected-style fragment: trimmed whole row */
            char *trimmed = cli_trim_status_bar_text(row, W);
            free(preview);
            if (!trimmed) goto oom;
            char *withnl = realloc(trimmed, strlen(trimmed) + 2);
            if (!withnl) { free(trimmed); goto oom; }
            strcat(withnl, "\n");
            bool ok = b4_frag_push(&frags, &fn, &fcap,
                                   "class:subagent-selected", withnl);
            free(withnl);
            if (!ok) goto oom;
        } else {
            /* three fragments: left border, padded content, right border */
            if (!b4_frag_push(&frags, &fn, &fcap, "class:subagent-border", "│")) {
                free(preview);
                goto oom;
            }
            char content[512];
            snprintf(content, sizeof(content), "%s%s ", prefix, preview);
            bool ok = b4_frag_push(&frags, &fn, &fcap,
                                   "class:subagent-sub", content);
            free(preview);
            if (!ok) goto oom;
            if (!b4_frag_push(&frags, &fn, &fcap,
                              "class:subagent-border", "│\n"))
                goto oom;
        }
    }

    /* footer row */
    nd = ftr_dashes < (int)sizeof(dashes) - 1 ? ftr_dashes : (int)sizeof(dashes) - 1;
    b4_fill_dashes(dashes, nd, sizeof(dashes));
    snprintf(line, sizeof(line), "%s%s%s\n", ftr_prefix, dashes, ftr_suffix);
    if (!b4_frag_push(&frags, &fn, &fcap, "class:subagent-border", line))
        goto oom;

    if (out_len) *out_len = fn;
    return frags;

oom:
    if (frags) b4_fragments_free(frags, fn);
    return NULL;
}
