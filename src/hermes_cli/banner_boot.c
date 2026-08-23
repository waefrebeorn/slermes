/* banner_boot.c — LIVE Hermes TUI boot-appearance parity.
 *
 * Faithful C11 port of the Python boot banner:
 *   PoP: show_banner           @ cli.py:8220         (compact/full dispatch,
 *                                                   >=95-col logo block)
 *   PoP: build_welcome_banner  @ hermes_cli/banner.py:926
 *   PoP: _build_compact_banner @ cli.py:4580
 *
 * Every visual element matches LIVE: HERMES_AGENT_LOGO block art above the
 * panel at >=95 columns, caduceus hero on the left column, model line with
 * context length + "· Nous Research", YOLO warning, cwd, session id, right
 * column with Available Tools grouped per toolset (sorted, max 8, 42-char
 * truncation with "..."), MCP Servers section (configured state at boot),
 * Available Skills grouped by category with width-adaptive "+N more"
 * truncation, profile line, and the summary line. Compact banner under 80
 * columns or SLERMES_FAST_STARTUP_BANNER/HERMES_FAST_STARTUP_BANNER=1.
 * Colors come from the skin engine with the LIVE defaults (#FFD700 gold
 * title, #FFBF00 accent, #B8860B dim, #FFF8DC text, #CD7F32 border).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "banner_boot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>

#include "hermes_core_types.h"          /* HERMES_VERSION */
#include "hermes_display.h"             /* display_width() */
#include "registry.h"
#include "skills_parser.h"
#include "banner_pure.h"                /* banner_format_context_length */
#include "port_hermes_cli_slash_exec.h" /* format_banner_version_label() */
#include "../../lib/libskin/skin.h"
#include "../../lib/libyaml/yaml.h"

#include "banner_boot_art.h"            /* generated exact art from banner.py */

/* ================================================================
 *  Skin color resolution (PoP: _skin_color @ banner.py)
 * ================================================================ */

#define COL_TITLE   "FFD700"
#define COL_ACCENT  "FFBF00"
#define COL_DIM     "B8860B"
#define COL_TEXT    "FFF8DC"
#define COL_BORDER  "CD7F32"
#define COL_SESSION "8B8682"
#define COL_RED     "FF0000"
#define COL_YELLOW  "FFFF00"

static const char *skin_hex(const skin_t *skin, const char *key, const char *fallback) {
    if (!skin) return fallback;
    const char *v = skin_get(skin, key, NULL);
    return (v && v[0] == '#' && strlen(v) >= 7) ? v + 1 : fallback;
}

static void hex_rgb(const char *hex6, int *r, int *g, int *b) {
    char buf[8];
    for (int i = 0; i < 6 && hex6[i]; i++)
        buf[i] = (char)tolower((unsigned char)hex6[i]);
    buf[6] = '\0';
    int v = (int)strtol(buf, NULL, 16);
    *r = (v >> 16) & 0xFF;
    *g = (v >> 8) & 0xFF;
    *b = v & 0xFF;
}

/* ================================================================
 *  Colored segments (rich-markup equivalent)
 * ================================================================ */

typedef struct { char *text; char hex[8]; int bold; } seg_t;

#define MAX_SEGS 64

typedef struct { seg_t segs[MAX_SEGS]; int n; } line_t;

static void seg_add(line_t *ln, const char *hex, int bold, const char *fmt, ...)
        __attribute__((format(printf, 4, 5)));
static void seg_add(line_t *ln, const char *hex, int bold, const char *fmt, ...) {
    if (ln->n >= MAX_SEGS) return;
    va_list ap;
    va_start(ap, fmt);
    char *s = NULL;
    if (vasprintf(&s, fmt, ap) < 0) { va_end(ap); return; }
    va_end(ap);
    seg_t *sg = &ln->segs[ln->n];
    sg->text = s;
    snprintf(sg->hex, sizeof(sg->hex), "%s", hex);
    sg->bold = bold;
    ln->n++;
}

static void line_reset(line_t *ln) {
    for (int i = 0; i < ln->n; i++) free(ln->segs[i].text);
    ln->n = 0;
}

/* Emit one segment as ANSI truecolor text to stdout. */
static void emit_seg(const seg_t *sg) {
    int r, g, b;
    hex_rgb(sg->hex, &r, &g, &b);
    printf("\x1B[%d;38;2;%d;%d;%dm%s\x1B[0m",
           sg->bold ? 1 : 0, r, g, b, sg->text);
}

/* Visible width of a string (UTF-8 codepoint count; art is 1-col glyphs). */
static size_t str_cols(const char *s) {
    size_t w = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == 0x1B) {                       /* skip embedded ANSI */
            while (*p && *p != 'm') p++;
            if (!*p) break;
            continue;
        }
        if ((*p & 0xC0) != 0x80) w++;
    }
    return w;
}

static size_t line_cols(const line_t *ln) {
    size_t w = 0;
    for (int i = 0; i < ln->n; i++) w += str_cols(ln->segs[i].text);
    return w;
}

static void line_print(const line_t *ln) {
    for (int i = 0; i < ln->n; i++) emit_seg(&ln->segs[i]);
}

static void pad_spaces(size_t cols) {
    for (size_t i = 0; i < cols; i++) putchar(' ');
}

static int cmp_strp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Truncate to at most max_cols visible codepoints. */
static void trunc_cols(char *dst, size_t dstsz, const char *src, size_t max_cols) {
    size_t w = 0, o = 0;
    for (const unsigned char *p = (const unsigned char *)src;
         *p && o + 5 < dstsz; ) {
        size_t adv = 1;
        if ((p[0] & 0xE0) == 0xC0) adv = 2;
        else if ((p[0] & 0xF0) == 0xE0) adv = 3;
        else if ((p[0] & 0xF8) == 0xF0) adv = 4;
        if (w >= max_cols) break;
        memcpy(dst + o, p, adv); o += adv; w++;
        p += adv;
    }
    dst[o] = '\0';
}

/* Pad-with-spaces helper matching Python str.ljust(content_width). */
static void ljust_cols(char *dst, size_t dstsz, const char *src, int width) {
    char tmp[256];
    trunc_cols(tmp, sizeof(tmp), src, (size_t)(width > 0 ? width : 0));
    size_t w = str_cols(tmp);
    snprintf(dst, dstsz, "%s%*s", tmp,
             (int)((size_t)width > w ? width - (int)w : 0), "");
}

/* PoP: _display_toolset_name @ banner.py — strip trailing "_tools". */
static void strip_tools_suffix(char *dst, size_t dstsz, const char *src) {
    char *disp = banner_display_toolset_name(src);
    snprintf(dst, dstsz, "%s", disp ? disp : src);
    free(disp);
}

/* ================================================================
 *  Logo block (PoP: show_banner >=95-col branch + HERMES_AGENT_LOGO)
 * ================================================================ */

#define N_ART(a) (sizeof(a) / sizeof((a)[0]))

static void print_logo_block(void) {
    for (size_t i = 0; i < N_ART(BANNER_LOGO_ART); i++) {
        const banner_art_line_t *l = &BANNER_LOGO_ART[i];
        int r, g, b;
        hex_rgb(l->hex, &r, &g, &b);
        printf("\x1B[%d;38;2;%d;%d;%dm%s\x1B[0m\n",
               l->bold ? 1 : 0, r, g, b, l->line);
    }
    putchar('\n');
}

/* PoP: HERMES_CADUCEUS — hero art on the panel's left column. */
static void caduceus_into(line_t lines[], int max_lines, int *out_n) {
    int n = 0;
    for (size_t i = 0; i < N_ART(BANNER_CADUCEUS_ART) && n < max_lines; i++, n++) {
        const banner_art_line_t *l = &BANNER_CADUCEUS_ART[i];
        seg_add(&lines[n], l->hex, l->bold, "%s", l->line);
    }
    *out_n = n;
}

/* ================================================================
 *  Compact banner (PoP: _build_compact_banner @ cli.py:4580)
 * ================================================================ */

static void print_compact_banner(bool fast, const skin_t *skin) {
    const char *agent_name = skin_get_branding(skin, "agent_name", "Hermes Agent");
    const char *border_hex = skin_hex(skin, "colors.banner_border", COL_TITLE);
    const char *title_hex  = skin_hex(skin, "colors.banner_title", COL_ACCENT);
    const char *dim_hex    = skin_hex(skin, "colors.banner_dim", COL_DIM);
    bool default_skin = (strcmp(agent_name, "Hermes Agent") == 0);

    /* default skin keeps the caduceus-staff glyph prefix */
    static const char staff[] = "\xe2\x9a\x95";               /* ⚕ */
    char line1[256], tiny[192];
    if (default_skin) {
        snprintf(line1, sizeof(line1), "%s NOUS HERMES - AI Agent Framework", staff);
        snprintf(tiny,  sizeof(tiny),  "%s NOUS HERMES", staff);
    } else {
        snprintf(line1, sizeof(line1), "%s - AI Agent Framework", agent_name);
        snprintf(tiny,  sizeof(tiny),  "%s", agent_name);
    }

    char *version_line = NULL;
    if (fast) {
        if (asprintf(&version_line, "Hermes Agent v%s", HERMES_VERSION) < 0)
            version_line = NULL;
    } else {
        version_line = format_banner_version_label();
    }

    int w = display_width() - 2;
    if (w > 88) w = 88;
    putchar('\n');
    if (w < 30) {
        int tr, tg, tb, dr, dg, db;
        hex_rgb(title_hex, &tr, &tg, &tb);
        hex_rgb(dim_hex, &dr, &dg, &db);
        printf("\x1B[38;2;%d;%d;%dm%s\x1B[0m \x1B[2;38;2;%d;%d;%dm- Nous Research\x1B[0m\n\n",
               tr, tg, tb, tiny, dr, dg, db);
        free(version_line);
        fflush(stdout);
        return;
    }

    int br, bgc, bbc, trr, tgg, tbb, drr, dgg, dbb;
    hex_rgb(border_hex, &br, &bgc, &bbc);
    hex_rgb(title_hex, &trr, &tgg, &tbb);
    hex_rgb(dim_hex, &drr, &dgg, &dbb);

    int inner = w - 2;
    int content_width = inner - 2 > 0 ? inner - 2 : 1;

    char l1[256], l2[256];
    ljust_cols(l1, sizeof(l1), line1, content_width);
    snprintf(l2, sizeof(l2), "%-*.*s", content_width, content_width,
             version_line ? version_line : "");

    /* ╔══╗ box, bold border color — exact LIVE geometry */
    printf("\x1B[1;38;2;%d;%d;%dm\xe2\x95\x94", br, bgc, bbc);
    for (int i = 0; i < w; i++) fputs("\xe2\x95\x90", stdout);
    fputs("\xe2\x95\x97\x1B[0m\n", stdout);
    printf("\x1B[1;38;2;%d;%d;%dm\xe2\x95\x91\x1B[0m \x1B[38;2;%d;%d;%dm%s\x1B[0m "
           "\x1B[1;38;2;%d;%d;%dm\xe2\x95\x91\x1B[0m\n",
           br, bgc, bbc, trr, tgg, tbb, l1, br, bgc, bbc);
    printf("\x1B[1;38;2;%d;%d;%dm\xe2\x95\x91\x1B[0m \x1B[2;38;2;%d;%d;%dm%s\x1B[0m "
           "\x1B[1;38;2;%d;%d;%dm\xe2\x95\x91\x1B[0m\n",
           br, bgc, bbc, drr, dgg, dbb, l2, br, bgc, bbc);
    printf("\x1B[1;38;2;%d;%d;%dm\xe2\x95\x9a", br, bgc, bbc);
    for (int i = 0; i < w; i++) fputs("\xe2\x95\x90", stdout);
    fputs("\xe2\x95\x9d\x1B[0m\n", stdout);
    free(version_line);
    fflush(stdout);
}

/* ================================================================
 *  Data sources — tools / skills / MCP servers
 * ================================================================ */

#define BANNER_MAX_TOOLSETS 64

typedef struct {
    char name[64];
    char **tools;
    size_t ntools, cap;
} tool_group_t;

static bool group_push(tool_group_t *g, const char *tool) {
    if (g->ntools == g->cap) {
        size_t nc = g->cap ? g->cap * 2 : 8;
        char **nt = realloc(g->tools, nc * sizeof(char *));
        if (!nt) return false;
        g->tools = nt; g->cap = nc;
    }
    g->tools[g->ntools] = strdup(tool);
    if (!g->tools[g->ntools]) return false;
    g->ntools++;
    return true;
}

/* Read MCP server names from ~/.slermes/config.yaml `mcp_servers`.
 * Returns malloc'd array of malloc'd names (may be NULL). */
static char **load_mcp_server_names(size_t *out_n) {
    *out_n = 0;
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) return NULL;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.slermes/config.yaml", home);

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) { free(err); err = NULL; }
    if (!doc) return NULL;

    size_t nkeys = 0;
    char **keys = yaml_map_keys(doc, "mcp_servers", &nkeys);
    char **names = NULL;
    size_t nn = 0;
    if (keys && nkeys) {
        names = calloc(nkeys, sizeof(char *));
        for (size_t i = 0; i < nkeys; i++) {
            if (!keys[i]) continue;
            /* structural keys are not server entries */
            if (strcmp(keys[i], "timeout") == 0 ||
                strcmp(keys[i], "enabled") == 0)
                continue;
            names[nn++] = strdup(keys[i]);
        }
    }
    free(keys);
    yaml_free(doc);
    *out_n = nn;
    if (nn == 0) { free(names); return NULL; }
    return names;
}

/* ================================================================
 *  Tool grouping (PoP: build_welcome_banner toolsets_dict loop)
 * ================================================================ */

/* Wrap one composed right-column line into visual rows of at most width.
 * Wraps on word boundaries across segments; oversized words hard-split. */
static void wrap_line(const line_t *src, size_t width,
                      line_t rows[], int max_rows, int *out_rows) {
    int nrows = 0;
    memset(&rows[0], 0, sizeof(rows[0]));
    line_t *cur = &rows[0];
    size_t cur_w = 0;

    for (int i = 0; i < src->n; i++) {
        const seg_t *sg = &src->segs[i];
        const char *p = sg->text;
        while (*p) {
            const char *start = p;
            while (*p && *p != ' ') p++;   /* word */
            while (*p == ' ') p++;         /* trailing spaces ride along */
            size_t clen = (size_t)(p - start);
            char chunk[600];
            if (clen >= sizeof(chunk)) clen = sizeof(chunk) - 1;
            memcpy(chunk, start, clen);
            chunk[clen] = '\0';
            size_t cw = str_cols(chunk);

            if (cur_w > 0 && cur_w + cw > width) {
                if (++nrows >= max_rows) goto done;
                cur = &rows[nrows];
                memset(cur, 0, sizeof(*cur));
                cur_w = 0;
            }
            seg_add(cur, sg->hex, sg->bold, "%s", chunk);
            cur_w += cw;
        }
    }
done:
    *out_rows = nrows + 1;
}

/* ================================================================
 *  Full welcome banner
 * ================================================================ */

void banner_boot_show(const banner_boot_info_t *info) {
    if (!info) return;
    const skin_t *skin = info->skin;
    int term_w = display_width();
    if (term_w <= 0) term_w = 80;

    bool fast = false;
    const char *fsb = getenv("SLERMES_FAST_STARTUP_BANNER");
    if (!fsb || !*fsb) fsb = getenv("HERMES_FAST_STARTUP_BANNER");
    if (fsb && strcmp(fsb, "1") == 0) fast = true;

    /* PoP: show_banner auto-compact under 80 columns or forced */
    if (info->compact || fast || term_w < 80) {
        print_compact_banner(fast, skin);
        return;
    }

    if (term_w >= 95) print_logo_block();

    const char *accent_hex  = skin_hex(skin, "colors.banner_accent", COL_ACCENT);
    const char *dim_hex     = skin_hex(skin, "colors.banner_dim", COL_DIM);
    const char *text_hex    = skin_hex(skin, "colors.banner_text", COL_TEXT);
    const char *session_hex = skin_hex(skin, "session_border", COL_SESSION);
    const char *title_hex   = skin_hex(skin, "colors.banner_title", COL_TITLE);
    const char *border_hex  = skin_hex(skin, "colors.banner_border", COL_BORDER);

    enum { LEFT_MAX = 24, RIGHT_MAX = 160 };

    /* ---------- Left column ---------- */
    line_t left[LEFT_MAX];
    memset(left, 0, sizeof(left));
    int left_n = 0;
    left_n++;                                   /* leading blank row */
    int hero_n = 0;
    caduceus_into(left + left_n, LEFT_MAX - 6, &hero_n);
    left_n += hero_n;
    left_n++;                                   /* trailing blank row */

    /* model line (PoP: model_short truncation at 28 -> 25+"...") */
    const char *model = info->model ? info->model : "";
    if (!model[0] || strcasecmp(model, "unknown") == 0) {
        seg_add(&left[left_n], COL_RED, 1, "no model configured");
        seg_add(&left[left_n], dim_hex, 0, " \xe2\x80\x94 run /model or hermes setup");
    } else {
        char model_short[64];
        const char *slash = strrchr(model, '/');
        snprintf(model_short, sizeof(model_short), "%s", slash ? slash + 1 : model);
        size_t ml = strlen(model_short);
        if (ml > 5 && strcmp(model_short + ml - 5, ".gguf") == 0)
            model_short[ml - 5] = '\0';
        if (strlen(model_short) > 28) {
            model_short[25] = '\0';
            strcat(model_short, "...");
        }
        char *ctx = info->context_length > 0
            ? banner_format_context_length(info->context_length) : NULL;
        seg_add(&left[left_n], accent_hex, 0, "%s", model_short);
        if (ctx) {
            seg_add(&left[left_n], dim_hex, 0, " \xc2\xb7 ");
            seg_add(&left[left_n], dim_hex, 0, "%s context", ctx);
            free(ctx);
        }
        seg_add(&left[left_n], dim_hex, 0, " \xc2\xb7 ");
        seg_add(&left[left_n], dim_hex, 0, "Nous Research");
    }
    left_n++;

    if (info->yolo) {
        seg_add(&left[left_n], COL_RED, 1, "\xe2\x9a\xa0 YOLO mode");
        seg_add(&left[left_n], dim_hex, 0, " \xe2\x80\x94 all approval prompts bypassed");
        left_n++;
    }
    if (info->cwd && info->cwd[0]) {
        seg_add(&left[left_n], dim_hex, 0, "%s", info->cwd);
        left_n++;
    }
    if (info->session_id && info->session_id[0]) {
        seg_add(&left[left_n], session_hex, 0, "Session: %s", info->session_id);
        left_n++;
    }

    /* ---------- Right column ---------- */
    line_t right[RIGHT_MAX];
    memset(right, 0, sizeof(right));
    int right_n = 0;

    /* Available Tools header */
    seg_add(&right[right_n], accent_hex, 1, "Available Tools");

    /* Group registry tools per toolset (PoP: toolsets_dict loop) */
    size_t ntools_all = 0;
    char **all_tools = registry_get_all_tool_names(&ntools_all);
    tool_group_t groups[BANNER_MAX_TOOLSETS];
    memset(groups, 0, sizeof(groups));
    int ngroups = 0;
    size_t total_tools = 0;

    for (size_t i = 0; i < ntools_all; i++) {
        const char *tn = all_tools[i];
        const char *ts = registry_get_toolset_for_tool(tn);
        char disp[64];
        strip_tools_suffix(disp, sizeof(disp),
                           (ts && ts[0]) ? ts : "other");
        int gi = -1;
        for (int j = 0; j < ngroups; j++)
            if (strcmp(groups[j].name, disp) == 0) { gi = j; break; }
        if (gi < 0) {
            if (ngroups >= BANNER_MAX_TOOLSETS) continue;
            gi = ngroups++;
            snprintf(groups[gi].name, sizeof(groups[gi].name), "%s", disp);
        }
        if (group_push(&groups[gi], tn)) total_tools++;
    }
    free(all_tools);

    /* sort toolset names alphabetically */
    for (int i = 0; i + 1 < ngroups; i++)
        for (int j = i + 1; j < ngroups; j++)
            if (strcmp(groups[j].name, groups[i].name) < 0) {
                tool_group_t t = groups[i]; groups[i] = groups[j]; groups[j] = t;
            }

    int display_count = ngroups > 8 ? 8 : ngroups;
    int remaining_toolsets = ngroups - display_count;

    for (int gi = 0; gi < display_count; gi++) {
        tool_group_t *g = &groups[gi];
        qsort(g->tools, g->ntools, sizeof(char *), cmp_strp);

        /* PoP: plain joined length > 45 triggers 42-char truncation */
        size_t plain_len = 0;
        for (size_t k = 0; k < g->ntools; k++)
            plain_len += strlen(g->tools[k]) + (k ? 2 : 0);

        line_t tl; memset(&tl, 0, sizeof(tl));
        seg_add(&tl, dim_hex, 0, "%s:", g->name);

        if (plain_len > 45) {
            /* accumulate until length+len+2 would exceed 42, then "..." */
            size_t length = 0;
            int stop = 0;
            for (size_t k = 0; k < g->ntools && !stop; k++) {
                size_t nl = strlen(g->tools[k]);
                if (length + nl + 2 > 42) { stop = 1; continue; }
                if (k) seg_add(&tl, dim_hex, 0, ", ");
                seg_add(&tl, registry_is_available(g->tools[k])
                             ? text_hex : COL_RED, 0, "%s", g->tools[k]);
                length += nl + 2;
            }
            seg_add(&tl, dim_hex, 0, "...");
        } else {
            for (size_t k = 0; k < g->ntools; k++) {
                if (k) seg_add(&tl, dim_hex, 0, ", ");
                seg_add(&tl, registry_is_available(g->tools[k])
                             ? text_hex : COL_RED, 0, "%s", g->tools[k]);
            }
        }
        if (right_n < RIGHT_MAX) right[right_n++] = tl;
        else line_reset(&tl);
    }

    if (remaining_toolsets > 0)
        seg_add(&right[right_n++], dim_hex, 0,
                "(and %d more toolsets...)", remaining_toolsets);

    /* MCP Servers section (boot-time state: configured, not yet connected) */
    size_t nmcp = 0;
    char **mcp_names = load_mcp_server_names(&nmcp);
    if (nmcp) {
        seg_add(&right[right_n++], dim_hex, 0, "");
        seg_add(&right[right_n], accent_hex, 1, "MCP Servers");
        for (size_t i = 0; i < nmcp && right_n < RIGHT_MAX; i++) {
            seg_add(&right[right_n], dim_hex, 0, "%s", mcp_names[i]);
            seg_add(&right[right_n], dim_hex, 0, "(stdio)");
            seg_add(&right[right_n], dim_hex, 0, " \xe2\x80\x94 configured");
        }
        for (size_t i = 0; i < nmcp; i++) free(mcp_names[i]);
    }
    free(mcp_names);

    seg_add(&right[right_n++], dim_hex, 0, "");

    /* Available Skills (PoP: skills_by_category width-adaptive section) */
    seg_add(&right[right_n], accent_hex, 1, "Available Skills");

    skill_info_t skills[512];
    int nskills = skills_discover(skills, 512, NULL);
    long total_skills = nskills;

    int cols_right = (int)((float)term_w * 0.6f) - 10;
    if (cols_right < 30) cols_right = 30;

    if (nskills == 0) {
        seg_add(&right[right_n++], dim_hex, 0, "No skills installed");
    } else {
        /* group by category dir name */
        typedef struct { char cat[64]; char **names; size_t n, cap; } catgrp_t;
        static catgrp_t cats[128];
        int ncats = 0;
        for (int i = 0; i < nskills; i++) {
            const char *cat = skills[i].category[0] ? skills[i].category : "general";
            int ci = -1;
            for (int j = 0; j < ncats; j++)
                if (strcmp(cats[j].cat, cat) == 0) { ci = j; break; }
            if (ci < 0) {
                if (ncats >= 128) continue;
                ci = ncats++;
                snprintf(cats[ci].cat, sizeof(cats[ci].cat), "%s", cat);
                cats[ci].names = NULL; cats[ci].n = cats[ci].cap = 0;
            }
            catgrp_t *cg = &cats[ci];
            if (cg->n == cg->cap) {
                size_t nc = cg->cap ? cg->cap * 2 : 8;
                char **nn = realloc(cg->names, nc * sizeof(char *));
                if (!nn) continue;
                cg->names = nn; cg->cap = nc;
            }
            cg->names[cg->n++] = strdup(skills[i].name);
        }

        /* sort category names */
        for (int i = 0; i + 1 < ncats; i++)
            for (int j = i + 1; j < ncats; j++)
                if (strcmp(cats[j].cat, cats[i].cat) < 0) {
                    catgrp_t t = cats[i]; cats[i] = cats[j]; cats[j] = t;
                }

        for (int ci = 0; ci < ncats && right_n < RIGHT_MAX; ci++) {
            catgrp_t *cg = &cats[ci];
            qsort(cg->names, cg->n, sizeof(char *), cmp_strp);
            line_t sl; memset(&sl, 0, sizeof(sl));
            seg_add(&sl, dim_hex, 0, "%s:", cg->cat);

            /* PoP: accumulate until sep+name(+indicator) exceeds avail */
            size_t prefix_len = strlen(cg->cat) + 2;
            size_t avail = cols_right > (int)prefix_len + 20
                           ? (size_t)cols_right - prefix_len : 20;
            size_t length = 0;
            int emitted = 0;
            for (size_t i = 0; i < cg->n; i++) {
                size_t needed = (emitted ? 2 : 0) + strlen(cg->names[i]);
                size_t after = cg->n - (i + 1);
                /* len(", +<after> more") — matches Python's indicator estimate */
                char ind[32];
                snprintf(ind, sizeof(ind), ", +%zu more", after);
                size_t ind_len = after ? strlen(ind) : 0;
                if (emitted && length + needed + ind_len > avail) {
                    seg_add(&sl, text_hex, 0, ", +%zu more", after);
                    emitted = 0;
                    break;
                }
                seg_add(&sl, dim_hex, 0, emitted ? ", " : "");
                seg_add(&sl, text_hex, 0, "%s", cg->names[i]);
                length += needed;
                emitted++;
            }
            (void)emitted;
            right[right_n++] = sl;
        }
    }

    seg_add(&right[right_n++], dim_hex, 0, "");

    /* Profile line when not 'default' */
    const char *prof = getenv("SLERMES_PROFILE");
    if (prof && prof[0] && strcmp(prof, "default") != 0 && right_n < RIGHT_MAX - 2) {
        seg_add(&right[right_n], accent_hex, 1, "Profile:");
        seg_add(&right[right_n], text_hex, 0, "%s", prof);
        right_n++;
    }

    /* Summary line: "N tools · N skills · /help for commands" */
    {
        line_t sm; memset(&sm, 0, sizeof(sm));
        seg_add(&sm, dim_hex, 0, "%ld tools \xc2\xb7 %ld skills",
                (long)(total_tools ? total_tools : registry_get_count()),
                total_skills);
        seg_add(&sm, dim_hex, 0, " \xc2\xb7 /help for commands");
        right[right_n++] = sm;
    }

    /* ---------- Panel render (Rich Panel equivalent) ---------- */
    size_t left_w = 0;
    for (int i = 0; i < left_n; i++) {
        size_t w = line_cols(&left[i]);
        if (w > left_w) left_w = w;
    }

    int inner_w = term_w - 2;                 /* between │ borders */
    int content_w = inner_w - 4;              /* padding=(0,2)      */
    int right_w = content_w - (int)left_w - 2;/* 2-col grid gap     */
    if (right_w < 20) right_w = 20;

    /* wrap every right line to right_w visual columns */
    line_t rows[RIGHT_MAX * 3];
    int nrows = 0;
    for (int i = 0; i < right_n && nrows < RIGHT_MAX * 3 - 4; i++) {
        line_t wrapped[8];
        int wr = 0;
        wrap_line(&right[i], (size_t)right_w, wrapped, 8, &wr);
        for (int k = 0; k < wr && nrows < RIGHT_MAX * 3 - 4; k++)
            rows[nrows++] = wrapped[k];
    }

    int tr, tg, tb, br, bg_, bb_;
    hex_rgb(title_hex, &tr, &tg, &tb);
    hex_rgb(border_hex, &br, &bg_, &bb_);

    char *version_label = format_banner_version_label();

    putchar('\n');                            /* console.print() blank */
    /* top border with title: ╭─ Title ───╮ */
    fputs("\x1B[1;38;2;", stdout);
    printf("%d;%d;%dm\xe2\x95\xad\xe2\x94\x80 ", br, bg_, bb_);
    printf("\x1B[1;38;2;%d;%d;%dm%s\x1B[0m\x1B[38;2;%d;%d;%dm ",
           tr, tg, tb, version_label ? version_label : "", br, bg_, bb_);
    {
        int used = 4 + (int)str_cols(version_label ? version_label : "") + 1;
        int fill = inner_w - used - 1;
        for (int i = 0; i < fill; i++) fputs("\xe2\x94\x80", stdout);
    }
    printf("\xe2\x95\xae\x1B[0m\n");

    int max_rows = nrows > left_n ? nrows : left_n;
    for (int row = 0; row < max_rows; row++) {
        printf("\x1B[38;2;%d;%d;%dm\xe2\x94\x82\x1B[0m", br, bg_, bb_);
        pad_spaces(2);
        size_t lw_used = 0;
        if (row < left_n) {
            line_print(&left[row]);
            lw_used = line_cols(&left[row]);
        }
        if ((int)lw_used < (int)left_w) pad_spaces(left_w - lw_used);
        pad_spaces(2);
        if (row < nrows) {
            line_print(&rows[row]);
            size_t rw_used = line_cols(&rows[row]);
            if ((int)rw_used < right_w) pad_spaces(right_w - rw_used);
        } else {
            pad_spaces((size_t)right_w);
        }
        pad_spaces(2);
        printf("\x1B[38;2;%d;%d;%dm\xe2\x94\x82\x1B[0m\n", br, bg_, bb_);
    }

    printf("\x1B[38;2;%d;%d;%dm\xe2\x95\xb0", br, bg_, bb_);
    for (int i = 0; i < inner_w; i++) fputs("\xe2\x94\x80", stdout);
    fputs("\xe2\x95\xaf\x1B[0m\n", stdout);

    free(version_label);
    fflush(stdout);
}
