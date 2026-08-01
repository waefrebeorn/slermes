/* display_diff.c - Diff rendering subsystem (extracted from display_core.c).
 * Self-contained: skin-aware ANSI diff colors + unified-diff split/render/summarize
 * pipeline. Ported from agent/display.py. C11, opaque-friendly, minimal includes.
 */
#include "hermes_display.h"
#include "hermes_json.h"
#include "skin.h"
#include "ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* g_display_skin is owned by display_core.c and shared here for skin-aware
 * ANSI diff colors. Declared extern to avoid duplicating global display state. */
extern skin_t *g_display_skin;

/* Static buffers for skin-aware ANSI diff colors */
static char g_ansi_diff_dim[64];
static char g_ansi_diff_file[64];
static char g_ansi_diff_hunk[64];
static char g_ansi_diff_minus[64];
static char g_ansi_diff_plus[64];
static int g_ansi_diff_inited = 0;

/* Initialize skin-aware ANSI colors for unified diff display.
 * Port of Python display.py:_diff_ansi(). Queries active skin once and caches. */
static void diff_ansi_init(void) {
    if (g_ansi_diff_inited) return;
    g_ansi_diff_inited = 1;

    /* Defaults for dark terminals */
    snprintf(g_ansi_diff_dim, sizeof(g_ansi_diff_dim), "\033[38;2;150;150;150m");
    snprintf(g_ansi_diff_file, sizeof(g_ansi_diff_file), "\033[38;2;180;160;255m");
    snprintf(g_ansi_diff_hunk, sizeof(g_ansi_diff_hunk), "\033[38;2;120;120;140m");
    snprintf(g_ansi_diff_minus, sizeof(g_ansi_diff_minus), "\033[38;2;255;255;255;48;2;120;20;20m");
    snprintf(g_ansi_diff_plus, sizeof(g_ansi_diff_plus), "\033[38;2;255;255;255;48;2;20;90;20m");

    if (!g_display_skin) return;

    const char *val;

    /* banner_dim → dim */
    val = skin_get(g_display_skin, "colors.banner_dim", NULL);
    if (val && strlen(val) == 7 && val[0] == '#') {
        int r, g, b;
        if (ansi_parse_hex(val, &r, &g, &b))
            snprintf(g_ansi_diff_dim, sizeof(g_ansi_diff_dim), "\033[38;2;%d;%d;%dm", r, g, b);
    }

    /* session_label → file */
    val = skin_get(g_display_skin, "colors.session_label", NULL);
    if (val && strlen(val) == 7 && val[0] == '#') {
        int r, g, b;
        if (ansi_parse_hex(val, &r, &g, &b))
            snprintf(g_ansi_diff_file, sizeof(g_ansi_diff_file), "\033[38;2;%d;%d;%dm", r, g, b);
    }

    /* session_border → hunk */
    val = skin_get(g_display_skin, "colors.session_border", NULL);
    if (val && strlen(val) == 7 && val[0] == '#') {
        int r, g, b;
        if (ansi_parse_hex(val, &r, &g, &b))
            snprintf(g_ansi_diff_hunk, sizeof(g_ansi_diff_hunk), "\033[38;2;%d;%d;%dm", r, g, b);
    }

    /* ui_error → minus (dark tinted bg) */
    val = skin_get(g_display_skin, "colors.ui_error", NULL);
    if (val && strlen(val) == 7 && val[0] == '#') {
        int r, g, b;
        if (ansi_parse_hex(val, &r, &g, &b))
            snprintf(g_ansi_diff_minus, sizeof(g_ansi_diff_minus),
                     "\033[38;2;255;255;255;48;2;%d;%d;%dm",
                     r > 120 ? r/2 : (r > 20 ? r : 20),
                     g > 40 ? g/4 : (g > 10 ? g : 10),
                     b > 40 ? b/4 : (b > 10 ? b : 10));
    }

    /* ui_ok → plus (dark tinted bg) */
    val = skin_get(g_display_skin, "colors.ui_ok", NULL);
    if (val && strlen(val) == 7 && val[0] == '#') {
        int r, g, b;
        if (ansi_parse_hex(val, &r, &g, &b))
            snprintf(g_ansi_diff_plus, sizeof(g_ansi_diff_plus),
                     "\033[38;2;255;255;255;48;2;%d;%d;%dm",
                     r > 40 ? r/4 : (r > 10 ? r : 10),
                     g > 40 ? g/2 : (g > 20 ? g : 20),
                     b > 40 ? b/4 : (b > 10 ? b : 10));
    }
}

/* ─── _diff_* accessor wrappers (port of Python display.py _diff_dim et al.) ─── */
/* Each returns the cached ANSI escape from diff_ansi_init(). */

/* PoP: cli_agent_display__diff_dim @ agent/display.py:_diff_dim */
/* Port of Python display.py:_diff_dim().
 * Return cached ANSI escape for dim diff lines (context).
 * Resolves from the active skin engine with fallback to defaults. */
const char *display_diff_dim(void)
{
    diff_ansi_init();
    if (!g_ansi_diff_dim[0]) {
        /* Fallback: use skin-resolved dim color or compile-time default */
        strncpy(g_ansi_diff_dim, "\033[38;2;150;150;150m", sizeof(g_ansi_diff_dim) - 1);
        g_ansi_diff_dim[sizeof(g_ansi_diff_dim) - 1] = '\0';
    }
    return g_ansi_diff_dim;
}

/* PoP: cli_agent_display__diff_file @ agent/display.py:_diff_file */
/* Port of Python display.py:_diff_file().
 * Return cached ANSI escape for file headers (---/+++).
 * Resolves from the active skin engine with fallback to defaults. */
const char *display_diff_file(void)
{
    diff_ansi_init();
    if (!g_ansi_diff_file[0]) {
        strncpy(g_ansi_diff_file, "[38;2;180;160;255m", sizeof(g_ansi_diff_file) - 1);
        g_ansi_diff_file[sizeof(g_ansi_diff_file) - 1] = '\0';
    }
    return g_ansi_diff_file;
}

/* PoP: cli_agent_display__diff_hunk @ agent/display.py:_diff_hunk */
/* Port of Python display.py:_diff_hunk().
 * Return cached ANSI escape for hunk headers (@@ ... @@).
 * Resolves from the active skin engine with fallback to defaults. */
const char *display_diff_hunk(void)
{
    diff_ansi_init();
    if (!g_ansi_diff_hunk[0]) {
        strncpy(g_ansi_diff_hunk, "[38;2;120;120;140m", sizeof(g_ansi_diff_hunk) - 1);
        g_ansi_diff_hunk[sizeof(g_ansi_diff_hunk) - 1] = '\0';
    }
    return g_ansi_diff_hunk;
}

/* PoP: cli_agent_display__diff_minus @ agent/display.py:_diff_minus */
/* Port of Python display.py:_diff_minus().
 * Return cached ANSI escape for removed lines (-).
 * Resolves from the active skin engine with fallback to defaults. */
const char *display_diff_minus(void)
{
    diff_ansi_init();
    if (!g_ansi_diff_minus[0]) {
        strncpy(g_ansi_diff_minus, "[38;2;255;255;255;48;2;120;20;20m", sizeof(g_ansi_diff_minus) - 1);
        g_ansi_diff_minus[sizeof(g_ansi_diff_minus) - 1] = '\0';
    }
    return g_ansi_diff_minus;
}

/* PoP: cli_agent_display__diff_plus @ agent/display.py:_diff_plus */
/* Port of Python display.py:_diff_plus().
 * Return cached ANSI escape for added lines (+).
 * Resolves from the active skin engine with fallback to defaults. */
const char *display_diff_plus(void)
{
    diff_ansi_init();
    if (!g_ansi_diff_plus[0]) {
        strncpy(g_ansi_diff_plus, "[38;2;255;255;255;48;2;20;90;20m", sizeof(g_ansi_diff_plus) - 1);
        g_ansi_diff_plus[sizeof(g_ansi_diff_plus) - 1] = '\0';
    }
    return g_ansi_diff_plus;
}

/* ─── Diff pipeline helpers (port of Python display.py) ─── */

/* Port of Python display.py:_split_unified_diff_sections().
 * Split a unified diff into per-file sections.
 * Returns malloc'd JSON array of section strings. */
char *display_split_diff_sections(const char *diff) {
    if (!diff || !*diff) return NULL;
    json_t *arr = json_array();
    if (!arr) return NULL;

    json_t *current_arr = NULL;
    const char *p = diff;
    const char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        /* Check for --- file header */
        if (nl - p >= 4 && strncmp(p, "--- ", 4) == 0 && current_arr) {
            /* Start new section */
            current_arr = NULL;
        }
        if (!current_arr) {
            current_arr = json_array();
            json_append(arr, current_arr);
        }
/* PoP: section @ hermes_cli/doctor.py:_section */
        /* Add line to current section (without newline) */
        size_t len = (size_t)(nl - p);
        char *line = (char *)malloc(len + 1);
        if (line) {
            memcpy(line, p, len);
            line[len] = '\0';
            json_append(current_arr, json_string(line));
            free(line);
        }
        p = nl + 1;
    }
    /* Last line (if no trailing newline) */
    if (*p) {
        if (!current_arr) {
            current_arr = json_array();
            json_append(arr, current_arr);
        }
        json_append(current_arr, json_string(p));
    }

    /* Convert each section array to a joined string */
    json_t *result_arr = json_array();
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *section_arr = arr->c.items[i];
        if (!section_arr || section_arr->type != JSON_ARRAY) continue;
        /* Calculate total length */
        size_t total = 0;
        for (size_t j = 0; j < section_arr->c.count; j++) {
            json_t *line = section_arr->c.items[j];
            if (line && line->type == JSON_STRING)
                total += strlen(line->str_val) + 1; /* +1 for \n */
        }
        char *joined = (char *)malloc(total + 1);
        if (!joined) continue;
        size_t pos = 0;
        for (size_t j = 0; j < section_arr->c.count; j++) {
            json_t *line = section_arr->c.items[j];
            if (line && line->type == JSON_STRING) {
                size_t l = strlen(line->str_val);
                memcpy(joined + pos, line->str_val, l);
                pos += l;
                joined[pos++] = '\n';
            }
        }
        joined[pos] = '\0';
        json_append(result_arr, json_string(joined));
        free(joined);
    }
    json_free(arr);

    char *out = json_serialize(result_arr);
    json_free(result_arr);
    return out;
}

/** Render a single section (lines already wrapped) to allocated buffer. */
static char *display_render_diff_section(const char *section) {
    if (!section || !*section) return NULL;
    /* Estimate output: each line roughly doubles with ANSI */
    size_t in_len = strlen(section);
    size_t cap = in_len * 2 + 1024;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;

    const char *p = section;
    const char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        size_t len = (size_t)(nl - p);
        const char *ansi = NULL;
        if (len >= 4 && strncmp(p, "--- ", 4) == 0) { p = nl + 1; continue; }
        if (len >= 4 && strncmp(p, "+++ ", 4) == 0) {
            /* File header: skip to find the filename after "b/" */
            const char *fn = p + 4;
            if (strncmp(fn, "b/", 2) == 0) fn += 2;
            ansi = g_ansi_diff_file;
            if (pos + 256 < cap) {
                pos += snprintf(out + pos, cap - pos, "%s%s → %s%s",
                                ansi, p + 4, fn, "\033[0m");
            }
            p = nl + 1; continue;
        }
        if (len >= 2 && strncmp(p, "@@", 2) == 0) ansi = g_ansi_diff_hunk;
        else if (len > 0 && p[0] == '-') ansi = g_ansi_diff_minus;
        else if (len > 0 && p[0] == '+') ansi = g_ansi_diff_plus;
        else if (len > 0 && p[0] == ' ') ansi = g_ansi_diff_dim;
        if (ansi && pos + len + 32 < cap) {
            pos += snprintf(out + pos, cap - pos, "%s", ansi);
            memcpy(out + pos, p, len);
            pos += len;
            pos += snprintf(out + pos, cap - pos, "%s", "\033[0m");
        } else if (pos + len + 2 < cap) {
            memcpy(out + pos, p, len);
            pos += len;
        }
        out[pos++] = '\n';
        p = nl + 1;
    }
    out[pos] = '\0';
    return out;
}

/* Port of Python display.py:_summarize_rendered_diff_sections().
 * Summarise rendered diff sections with file/line caps.
 * Returns malloc'd JSON array of rendered lines. */
char *display_summarize_diff(const char *diff, int max_files, int max_lines) {
    if (!diff || !*diff) return strdup("[]");
    if (max_files <= 0) max_files = 6;
    if (max_lines <= 0) max_lines = 100;

    char *sections_json = display_split_diff_sections(diff);
    if (!sections_json) return strdup("[]");

    json_t *sections = json_parse(sections_json, NULL);
    free(sections_json);
    if (!sections) return strdup("[]");

    json_t *result = json_array();
    int omitted_files = 0;
    int omitted_lines = 0;

    for (size_t idx = 0; idx < sections->c.count; idx++) {
        json_t *sec = sections->c.items[idx];
        if (!sec || sec->type != JSON_STRING) continue;

        if ((int)idx >= max_files) {
            omitted_files++;
            char *rendered = display_render_diff_section(sec->str_val);
            if (rendered) {
                int lcount = 1;
                for (char *c = rendered; *c; c++) if (*c == '\n') lcount++;
                omitted_lines += lcount;
                free(rendered);
            }
            continue;
        }

        char *rendered = display_render_diff_section(sec->str_val);
        if (!rendered) continue;

        /* Count lines */
        int lcount = 1;
        for (char *c = rendered; *c; c++) if (*c == '\n') lcount++;
        int remaining = max_lines - (int)result->c.count;

        if (remaining <= 0) {
            omitted_lines += lcount;
            omitted_files++;
            free(rendered);
            if (idx + 1 < sections->c.count) {
                for (size_t r = idx + 1; r < sections->c.count; r++) {
                    json_t *rs = sections->c.items[r];
                    if (rs && rs->type == JSON_STRING) {
                        char *rr = display_render_diff_section(rs->str_val);
                        if (rr) {
                            int lc = 1;
                            for (char *c = rr; *c; c++) if (*c == '\n') lc++;
                            omitted_lines += lc;
                            free(rr);
                        }
                    }
                }
            }
            break;
        }

        /* Split rendered into lines */
        const char *rp = rendered;
        const char *rnl;
        int line_idx = 0;
        while ((rnl = strchr(rp, '\n')) != NULL && line_idx < remaining) {
            size_t len = (size_t)(rnl - rp);
            char *lstr = (char *)malloc(len + 1);
            if (lstr) {
                memcpy(lstr, rp, len);
                lstr[len] = '\0';
                json_append(result, json_string(lstr));
                free(lstr);
            }
            rp = rnl + 1;
            line_idx++;
        }
        if (line_idx >= remaining && rp && *rp) {
            omitted_lines += lcount - remaining;
            omitted_files += 1 + (int)(sections->c.count - idx - 1);
            for (size_t r = idx + 1; r < sections->c.count; r++) {
                json_t *rs = sections->c.items[r];
                if (rs && rs->type == JSON_STRING) {
                    char *rr = display_render_diff_section(rs->str_val);
                    if (rr) {
                        int lc = 1;
                        for (char *c = rr; *c; c++) if (*c == '\n') lc++;
                        omitted_lines += lc;
                        free(rr);
                    }
                }
            }
            break;
        }
        free(rendered);
    }

    /* Add summary line if files or lines were omitted */
    if (omitted_files > 0 || omitted_lines > 0) {
        char summary[256];
        int n = snprintf(summary, sizeof(summary),
            "%s… omitted %d diff line(s)%s%s",
            g_ansi_diff_hunk, omitted_lines,
            omitted_files > 0 ? " across " : "",
            omitted_files > 0 ? "additional file(s)/section(s)" : "");
        if (n > 0 && n < 256)
            json_append(result, json_string(summary));
    }

    json_free(sections);
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* Port of Python display.py:extract_edit_diff().
 * Extract a unified diff from a file-edit tool result JSON.
 * Checks "patch" → "diff" field, or generates from pre/post content. */
char *display_extract_edit_diff(const char *tool_name, const char *result_json,
                                 const char *function_args_json,
                                 const char *pre_content, const char *post_content) {
    if (!tool_name) return NULL;

    /* For "patch" tool: extract "diff" field from result JSON */
    if (strcmp(tool_name, "patch") == 0 && result_json && *result_json) {
        json_t *data = json_parse(result_json, NULL);
        if (data) {
            json_t *diff = json_obj_get(data, "diff");
            if (diff && diff->type == JSON_STRING && diff->str_val && diff->str_val[0]) {
                char *out = strdup(diff->str_val);
                json_free(data);
                return out;
            }
            json_free(data);
        }
    }

    /* Only handle write_file, patch, skill_manage */
    if (strcmp(tool_name, "write_file") != 0 &&
        strcmp(tool_name, "patch") != 0 &&
        strcmp(tool_name, "skill_manage") != 0)
        return NULL;

    /* Check if result indicates success */
    if (result_json && *result_json) {
        json_t *data = json_parse(result_json, NULL);
        if (data) {
            bool has_error = false;
            json_t *err = json_obj_get(data, "error");
            if (err && err->type == JSON_STRING && err->str_val && err->str_val[0])
                has_error = true;
            json_t *success = json_obj_get(data, "success");
            if (success && success->type == JSON_BOOL && !success->num_val)
                has_error = true;
            json_free(data);
            if (has_error) return NULL;
        }
    }

    /* Generate diff from pre/post content using difflib */
    if (!pre_content || !post_content) return NULL;

    /* Use libdifflib to generate unified diff */
    /* Split into lines */
    size_t pre_lines = 1, post_lines = 1;
    for (const char *c = pre_content; *c; c++) if (*c == '\n') pre_lines++;
    for (const char *c = post_content; *c; c++) if (*c == '\n') post_lines++;
    const char **pre_arr = (const char **)malloc(sizeof(char *) * (pre_lines + 1));
    const char **post_arr = (const char **)malloc(sizeof(char *) * (post_lines + 1));
    if (!pre_arr || !post_arr) {
        free(pre_arr); free(post_arr);
        return NULL;
    }

    /* Split pre_content */
    size_t pi = 0;
    const char *p = pre_content, *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        size_t len = (size_t)(nl - p);
        char *line = (char *)malloc(len + 2);
        if (line) { memcpy(line, p, len); line[len] = '\n'; line[len+1] = '\0'; }
        pre_arr[pi++] = (const char *)line;
        p = nl + 1;
    }
    if (*p) {
        char *line = strdup(p);
        if (line) pre_arr[pi++] = (const char *)line;
    }
    pre_arr[pi] = NULL;

    /* Split post_content */
    size_t poi = 0;
    p = post_content;
    while ((nl = strchr(p, '\n')) != NULL) {
        size_t len = (size_t)(nl - p);
        char *line = (char *)malloc(len + 2);
        if (line) { memcpy(line, p, len); line[len] = '\n'; line[len+1] = '\0'; }
        post_arr[poi++] = (const char *)line;
        p = nl + 1;
    }
    if (*p) {
        char *line = strdup(p);
        if (line) post_arr[poi++] = (const char *)line;
    }
    post_arr[poi] = NULL;

    /* Call unified_diff from libdifflib */
    /* We'll build a simple diff ourselves if libdifflib's API isn't convenient */
    /* For now, build a basic diff output */
    char *diff_text = NULL;
    size_t diff_cap = 4096;
    size_t diff_pos = 0;
    diff_text = (char *)malloc(diff_cap);
    if (!diff_text) { free(pre_arr); free(post_arr); return NULL; }

    /* Simple diff: compare lines, output unified diff format */
    bool any_diff = false;
    size_t max_lines = pi > poi ? pi : poi;
    for (size_t i = 0; i < max_lines; i++) {
        const char *a = (i < pi) ? pre_arr[i] : NULL;
        const char *b = (i < poi) ? post_arr[i] : NULL;
        if ((a && b && strcmp(a, b) == 0)) continue;
        if (!any_diff) {
            /* Output header */
            const char *fname = function_args_json ? "file" : "file";
            size_t needed = snprintf(NULL, 0, "--- a/%s\n+++ b/%s\n", fname, fname);
            if (diff_pos + needed + 1 > diff_cap) {
                diff_cap = diff_cap * 2 + needed + 1024;
                char *newd = (char *)realloc(diff_text, diff_cap);
                if (!newd) { free(pre_arr); free(post_arr); return NULL; }
                diff_text = newd;
            }
            diff_pos += snprintf(diff_text + diff_pos, diff_cap - diff_pos,
                                 "--- a/%s\n+++ b/%s\n", fname, fname);
            any_diff = true;
        }
        if (a) {
            size_t needed = strlen(a) + 2;
            if (diff_pos + needed + 2 > diff_cap) {
                diff_cap = diff_cap * 2 + needed + 1024;
                char *newd = (char *)realloc(diff_text, diff_cap);
                if (!newd) { free(pre_arr); free(post_arr); return NULL; }
                diff_text = newd;
            }
            diff_text[diff_pos++] = '-';
            memcpy(diff_text + diff_pos, a, strlen(a));
            diff_pos += strlen(a);
            if (diff_text[diff_pos - 1] != '\n') diff_text[diff_pos++] = '\n';
        }
        if (b) {
            size_t needed = strlen(b) + 2;
            if (diff_pos + needed + 2 > diff_cap) {
                diff_cap = diff_cap * 2 + needed + 1024;
                char *newd = (char *)realloc(diff_text, diff_cap);
                if (!newd) { free(pre_arr); free(post_arr); return NULL; }
                diff_text = newd;
            }
            diff_text[diff_pos++] = '+';
            memcpy(diff_text + diff_pos, b, strlen(b));
            diff_pos += strlen(b);
            if (diff_text[diff_pos - 1] != '\n') diff_text[diff_pos++] = '\n';
        }
    }
    diff_text[diff_pos] = '\0';
    if (!any_diff) { free(diff_text); diff_text = NULL; }

    /* Free split lines */
    for (size_t i = 0; i < pi; i++) free((void *)pre_arr[i]);
    for (size_t i = 0; i < poi; i++) free((void *)post_arr[i]);
    free(pre_arr);
    free(post_arr);

    return diff_text;
}

/* Port of Python display.py:_emit_inline_diff().
 * Emit rendered diff text through the provided print function.
 */
bool display_emit_inline_diff(const char *diff_text, void (*print_fn)(const char *)) {
    if (!print_fn || !diff_text || !*diff_text) return false;

    print_fn("  ┊ review diff");

    /* Clone and split by newlines */
    char *dup = strdup(diff_text);
    if (!dup) return false;

    /* Strip trailing newlines */
    size_t len = strlen(dup);
    while (len > 0 && (dup[len - 1] == '\n' || dup[len - 1] == '\r')) {
        dup[--len] = '\0';
    }

    char *saveptr;
    char *line = strtok_r(dup, "\n", &saveptr);
    while (line) {
        print_fn(line);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(dup);
    return true;
}

/* Port of Python display.py:render_edit_diff_with_delta().
 * Render an edit diff inline without taking over the terminal UI.
 * Extracts diff via display_extract_edit_diff(), renders via
 * display_summarize_diff() and emits via display_emit_inline_diff().
 */
bool display_render_edit_diff(const char *tool_name, const char *result_json,
                               const char *function_args_json,
                               const char *pre_content, const char *post_content,
                               void (*print_fn)(const char *)) {
    if (!tool_name || !print_fn) return false;

    char *diff = display_extract_edit_diff(tool_name, result_json,
                                            function_args_json,
                                            pre_content, post_content);
    if (!diff) return false;

    /* Render diff through summarize (returns JSON array of ANSI lines) */
    char *rendered = display_summarize_diff(diff, 5, 80);
    free(diff);

    if (!rendered) return false;

    /* Parse JSON array and join with newlines */
    json_t *parsed = json_parse(rendered, NULL);
    if (!parsed || parsed->type != JSON_ARRAY) {
        free(rendered);
        if (parsed) json_free(parsed);
        return false;
    }

    /* Calculate total length */
    size_t total = 0;
    for (size_t i = 0; i < parsed->c.count; i++) {
        json_t *item = parsed->c.items[i];
        if (item && item->type == JSON_STRING && item->str_val) {
            total += strlen(item->str_val) + 1; /* +1 for \n */
        }
    }

    char *text = malloc(total + 1);
    if (!text) { json_free(parsed); free(rendered); return false; }

    size_t pos = 0;
    for (size_t i = 0; i < parsed->c.count; i++) {
        json_t *item = parsed->c.items[i];
        if (item && item->type == JSON_STRING && item->str_val) {
            size_t slen = strlen(item->str_val);
            memcpy(text + pos, item->str_val, slen);
            pos += slen;
            text[pos++] = '\n';
        }
    }
    text[pos] = '\0';

    json_free(parsed);
    free(rendered);

    bool ok = display_emit_inline_diff(text, print_fn);
    free(text);
    return ok;
}

/* Port of Python display.py:_render_inline_unified_diff().
 * Render a unified diff with ANSI color for inline display.
 * Returns malloc'd string (caller free). */
char *display_inline_diff(const char *diff_text) {
    if (!diff_text) return NULL;

    /* Initialize skin-aware ANSI colors on first call */
    diff_ansi_init();

    /* Calculate output buffer size (diff_text can be up to ~50KB) */
    size_t in_len = strlen(diff_text);
    size_t cap = in_len * 3 + 4096; /* ANSI expansion */
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;

    const char *p = diff_text;
    const char *nl;
    const char *from_file = NULL;
    const char *to_file = NULL;
    char from_buf[512], to_buf[512];

    while ((nl = strchr(p, '\n')) != NULL) {
        size_t line_len = (size_t)(nl - p);
        char line[4096];
        size_t lcpy = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
        memcpy(line, p, lcpy);
        line[lcpy] = '\0';

        /* Capture ---/+++ pairs and emit combined "from → to" header (port of _render_inline_unified_diff) */
        if (line_len > 3 && strncmp(line, "--- ", 4) == 0) {
            from_file = line + 4;
            while (*from_file == ' ') from_file++;
            snprintf(from_buf, sizeof(from_buf), "%s", from_file);
            from_file = from_buf;
            goto next_line;
        }
        if (line_len > 3 && strncmp(line, "+++ ", 4) == 0) {
            to_file = line + 4;
            while (*to_file == ' ') to_file++;
            snprintf(to_buf, sizeof(to_buf), "%s", to_file);
            to_file = to_buf;
            if (from_file || to_file) {
                pos += snprintf(out + pos, cap - pos, "%s%s → %s\033[0m\n",
                                g_ansi_diff_file,
                                from_file ? from_file : "a/?",
                                to_file ? to_file : "b/?");
            }
            from_file = NULL;
            to_file = NULL;
            goto next_line;
        }

        if (line_len > 0 && line[0] == '+') {
            pos += snprintf(out + pos, cap - pos, "%s%s\033[0m\n",
                            g_ansi_diff_plus, line);
        } else if (line_len > 0 && line[0] == '-') {
            pos += snprintf(out + pos, cap - pos, "%s%s\033[0m\n",
                            g_ansi_diff_minus, line);
        } else if (line_len > 1 && line[0] == '@' && line[1] == '@') {
            pos += snprintf(out + pos, cap - pos, "%s%s\033[0m\n",
                            g_ansi_diff_hunk, line);
        } else if (line_len > 0 && line[0] == ' ') {
            pos += snprintf(out + pos, cap - pos, "%s%s\033[0m\n",
                            g_ansi_diff_dim, line);
        } else {
            pos += snprintf(out + pos, cap - pos, "%s\n", line);
        }

next_line:
        if (pos >= cap - 256) break; /* safety */
        p = nl + 1;
    }

    /* Handle last line without trailing newline */
    if (*p) {
        pos += snprintf(out + pos, cap - pos, "%s%s\033[0m\n",
                        g_ansi_diff_dim, p);
    }

    if (pos == 0) { free(out); return NULL; }
    return out;
}
