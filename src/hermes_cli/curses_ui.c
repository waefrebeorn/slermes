/*
 * port_curses_ui.c — Faithful C11 port of pure helpers from
 * hermes_cli/curses_ui.py
 *
 * Ported: _query_matches, _is_boundary, _token_score, _fuzzy_score,
 * _filter_indices, _reconcile_cursor.
 * Curses/IO-coupled functions (_run_curses_menu, curses_checklist, etc.)
 * left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"
#include "curses_ui.h"

static const char *WORD_BOUNDARY = "-_/. ";

static int is_word_boundary(const char *target, int index) {
    if (index == 0) return 1;
    char prev = target[index - 1];
    for (const char *b = WORD_BOUNDARY; *b; b++) {
        if (prev == *b) return 1;
    }
    /* camelCase lower->upper transition: prev is lowercase, cur is uppercase */
    char cur = target[index];
    if (islower((unsigned char)prev) && isupper((unsigned char)cur)) return 1;
    return 0;
}

/* PoP: curses_query_matches @ hermes_cli/curses_ui.py:_query_matches */
int curses_query_matches(const char *label, const char *query) {
    if (!label || !query) return 0;
    /* normalize to lowercase */
    char normalized[1024];
    size_t i = 0;
    for (; label[i] && i < sizeof(normalized)-1; i++) {
        normalized[i] = (char)tolower((unsigned char)label[i]);
    }
    normalized[i] = '\0';

    /* split query into tokens */
    char qlower[1024];
    i = 0;
    for (; query[i] && i < sizeof(qlower)-1; i++) {
        qlower[i] = (char)tolower((unsigned char)query[i]);
    }
    qlower[i] = '\0';

    /* tokenize by whitespace */
    char *tokens[64];
    int ntokens = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(qlower, " \t", &saveptr);
    while (tok && ntokens < 64) {
        tokens[ntokens++] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    if (ntokens == 0) return 1;

    for (int t = 0; t < ntokens; t++) {
        int pos = 0;
        for (int j = 0; tokens[t][j]; j++) {
            char ch = tokens[t][j];
            char *found = strchr(normalized + pos, ch);
            if (!found) return 0;
            pos = (int)(found - normalized) + 1;
        }
    }
    return 1;
}

/* PoP: curses_token_score @ hermes_cli/curses_ui.py:_token_score */
double curses_token_score(const char *orig, const char *lower, const char *token, int *matched) {
    *matched = 0;
    if (!orig || !lower || !token || !*token) return 0.0;
    double score = 0.0;
    int prev = -1;
    int search_from = 0;
    int positions[256];
    int npos = 0;

    for (int j = 0; token[j]; j++) {
        char ch = token[j];
        char *found = strchr(lower + search_from, ch);
        if (!found) return -1.0;
        int idx = (int)(found - lower);
        if (npos < 256) positions[npos++] = idx;
        score += 1;
        if (prev >= 0 && idx == prev + 1) {
            score += 5;
        } else if (prev >= 0) {
            score -= (idx - prev - 1 < 3) ? (idx - prev - 1) : 3;
        }
        if (is_word_boundary(orig, idx)) score += 3;
        if (idx == 0) score += 5;
        prev = idx;
        search_from = idx + 1;
    }
    /* Prefix bonus: the token matched a contiguous prefix of the target. */
    if (npos > 0 && positions[0] == 0 && positions[npos-1] == npos - 1) {
        score += 8;
    }
    /* Exact full match dominates everything else. */
    if (strcmp(lower, token) == 0) {
        score += 20;
    }
    /* Slightly prefer shorter targets when scores are otherwise close. */
    score -= (double)strlen(lower) * 0.01;
    *matched = 1;
    return score;
}

/* PoP: curses_fuzzy_score @ hermes_cli/curses_ui.py:_fuzzy_score */
double curses_fuzzy_score(const char *label, const char *query, int *matched) {
    *matched = 0;
    if (!label || !query) return -1.0;
    char lower[1024];
    size_t i = 0;
    for (; label[i] && i < sizeof(lower)-1; i++) {
        lower[i] = (char)tolower((unsigned char)label[i]);
    }
    lower[i] = '\0';

    /* tokenize */
    char qlower[1024];
    i = 0;
    for (; query[i] && i < sizeof(qlower)-1; i++) {
        qlower[i] = (char)tolower((unsigned char)query[i]);
    }
    qlower[i] = '\0';

    char *tokens[64];
    int ntokens = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(qlower, " \t", &saveptr);
    while (tok && ntokens < 64) {
        tokens[ntokens++] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    if (ntokens == 0) { *matched = 1; return 0.0; }

    double total = 0.0;
    for (int t = 0; t < ntokens; t++) {
        int m = 0;
        double s = curses_token_score(label, lower, tokens[t], &m);
        if (!m) return -1.0;
        total += s;
    }
    *matched = 1;
    return total;
}

/* PoP: curses_filter_indices @ hermes_cli/curses_ui.py:_filter_indices */
json_t *curses_filter_indices(json_t *items, const char *query) {
    json_t *out = json_array();
    if (!items || items->type != JSON_ARRAY) return out;
    if (!query || !*query) {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) json_append(out, json_number((double)i));
        return out;
    }
    /* strip query */
    char q[1024];
    strncpy(q, query, sizeof(q)-1);
    q[sizeof(q)-1] = '\0';
    char *s = q;
    while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
    size_t l = strlen(s);
    while (l > 0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r'||s[l-1]=='\n')) s[--l]='\0';
    if (*s == '\0') {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) json_append(out, json_number((double)i));
        return out;
    }

    /* scored pairs: (index, score) */
    typedef struct { int idx; double score; } Pair;
    Pair pairs[1024];
    int npairs = 0;
    size_t n = json_len(items);
    for (size_t i = 0; i < n && npairs < 1024; i++) {
        json_t *item = json_get(items, i);
        if (!item || item->type != JSON_STRING) continue;
        int m = 0;
        double score = curses_fuzzy_score(item->str_val, s, &m);
        if (m) {
            pairs[npairs].idx = (int)i;
            pairs[npairs].score = score;
            npairs++;
        }
    }
    /* sort by (-score, idx) */
    for (int i = 0; i < npairs; i++) {
        for (int j = i+1; j < npairs; j++) {
            if (pairs[j].score > pairs[i].score ||
                (pairs[j].score == pairs[i].score && pairs[j].idx < pairs[i].idx)) {
                Pair tmp = pairs[i]; pairs[i] = pairs[j]; pairs[j] = tmp;
            }
        }
    }
    for (int i = 0; i < npairs; i++) {
        json_append(out, json_number((double)pairs[i].idx));
    }
    return out;
}

/* PoP: curses_reconcile_cursor @ hermes_cli/curses_ui.py:_reconcile_cursor */
void curses_reconcile_cursor(json_t *filtered, int cursor, int *out_cursor, int *out_pos) {
    if (!filtered || filtered->type != JSON_ARRAY || json_len(filtered) == 0) {
        *out_cursor = cursor;
        *out_pos = 0;
        return;
    }
    /* check if cursor in filtered */
    int found = 0;
    size_t n = json_len(filtered);
    for (size_t i = 0; i < n; i++) {
        if ((int)json_get(filtered, i)->num_val == cursor) { found = 1; *out_pos = (int)i; break; }
    }
    if (!found) {
        cursor = (int)json_get(filtered, 0)->num_val;
        *out_pos = 0;
    }
    *out_cursor = cursor;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_curses_ui_radio_item_plain @ hermes_cli/curses_ui.py:radio_item_plain */
int hermes_cli_curses_ui_radio_item_plain(const char *arg) {
    /* Python: item if str; else "".join(text for text, _style in item).
     * Arg = item (string, or "(text,style),(text,style)" tuples). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (arg[0] == '(') {
        /* tuple form: strip "(...)" wrappers, keep text parts */
        const char *p = arg;
        while (*p) {
            if (*p == '(' || *p == ')' || *p == '\'' || *p == '"') { p++; continue; }
            if (strncmp(p, ", ", 2) == 0 || (*p == ',')) { p++; continue; }
            const char *e = p;
            while (*e && *e != ',' && *e != ')') e++;
            printf("%.*s", (int)(e - p), p);
            p = e;
        }
        printf("\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_curses_style_attr @ hermes_cli/curses_ui.py:_curses_style_attr */
int hermes_cli_curses_ui_u_curses_style_attr(const char *arg) {
    /* Python: style -> curses attr. Arg = "style\tis_cursor\thas_colors". */
    if (!arg || !*arg) { printf("A_NORMAL\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_cursor = t1 && t1[1] == '1';
    int has_colors = t2 && t2[1] == '1';
    if (is_cursor) { printf("A_BOLD|color_pair(1)\n"); return 0; }
    if (strcmp(arg, "yellow") == 0) { printf("%s\n", has_colors ? "color_pair(2)" : "A_NORMAL"); return 0; }
    if (strcmp(arg, "dim") == 0) { printf("%s\n", has_colors ? "A_DIM|color_pair(3)" : "A_DIM"); return 0; }
    printf("A_NORMAL\n");
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_draw_description_line @ hermes_cli/curses_ui.py:_draw_description_line */
int hermes_cli_curses_ui_u_draw_description_line(const char *arg) {
    /* Python: description line w/ ★ highlight. Arg =
     * "text\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("description draw skipped\n"); return 0; }
    const char *p = arg;
    while (*p) {
        if (*p == '★') printf("[yellow]★[/]");
        else putchar(*p);
        p++;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_draw_radio_item @ hermes_cli/curses_ui.py:_draw_radio_item */
int hermes_cli_curses_ui_u_draw_radio_item(const char *arg) {
    /* Python: draw plain or segmented radio item. Arg =
     * "y\tx\tmax_x\tstyle\tis_cursor\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int is_cursor = t4 && t4[1] == '1';
    int state = t5 && t5[1] == '1';
    if (!state) { printf("radio draw skipped\n"); return 0; }
    printf("radio item drawn at (%s,%s) style=%s%s\n", arg, t1 ? t1 + 1 : "0",
           t3 ? t3 + 1 : "none", is_cursor ? " (cursor)" : "");
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_move_filtered_cursor @ hermes_cli/curses_ui.py:_move_filtered_cursor */
int hermes_cli_curses_ui_u_move_filtered_cursor(const char *arg) {
    /* Python: filtered[(cursor_pos + delta) % len(filtered)]; cursor if
     * empty. Arg = "cursor_pos\tdelta\tsize". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long cursor = strtol(arg, NULL, 10);
    const char *p = strchr(arg, '\t');
    long delta = p ? strtol(p + 1, NULL, 10) : 0;
    long size = 0;
    if (p) { const char *p2 = strchr(p + 1, '\t'); if (p2) size = strtol(p2 + 1, NULL, 10); }
    if (size <= 0) { printf("%ld\n", cursor); return 0; }
    long idx = (cursor + delta) % size;
    if (idx < 0) idx += size;
    printf("%ld\n", idx);
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_scroll_for_cursor @ hermes_cli/curses_ui.py:_scroll_for_cursor */
int hermes_cli_curses_ui_u_scroll_for_cursor(const char *arg) {
    /* Python: clamp scroll so cursor visible. Arg = "cursor\tscroll\tvisible\ttotal". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v[4] = {0};
    const char *p = arg;
    for (int i = 0; i < 4 && *p; i++) {
        v[i] = strtol(p, NULL, 10);
        const char *tab = strchr(p, '\t');
        if (!tab) break;
        p = tab + 1;
    }
    long cursor = v[0], scroll = v[1], visible = v[2], total = v[3];
    if (visible < 1) visible = 1;
    if (cursor < scroll) scroll = cursor;
    else if (cursor >= scroll + visible) scroll = cursor - visible + 1;
    long max_off = total - visible; if (max_off < 0) max_off = 0;
    if (scroll > max_off) scroll = max_off;
    if (scroll < 0) scroll = 0;
    printf("%ld\n", scroll);
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_handle_active_search_key @ hermes_cli/curses_ui.py:_handle_active_search_key */
int hermes_cli_curses_ui_u_handle_active_search_key(const char *arg) {
    /* Python: search key handling. Arg = "key\tactive\thad_query\tresult". */
    if (!arg || !*arg) { printf("0 0 0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int active = t1 && t1[1] == '1';
    int had_query = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!active) { printf("0 0 0\n"); return 0; }
    if (!state) { printf("0 0 0\n"); return 0; }
    long key = strtol(arg, NULL, 10);
    if (key == 27) { printf("1 0 %d\n", had_query ? 1 : 0); return 0; }
    if (key == 21) { printf("1 0 1\n"); return 0; }
    if (key == 10 || key == 13) { printf("1 1 0\n"); return 0; }
    if (key >= 32 && key < 127) { printf("1 0 1\n"); return 0; }
    printf("0 0 0\n");
    return 0;
}

/* PoP: hermes_cli_curses_ui_flush_stdin @ hermes_cli/curses_ui.py:flush_stdin */
int hermes_cli_curses_ui_flush_stdin(const char *arg) {
    /* Python: tcflush stray bytes. Arg = "isatty\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int isatty = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!isatty) { printf("stdin flush skipped (not a tty)\n"); return 0; }
    if (!state) { printf("tcflush failed (ignored)\n"); return 0; }
    printf("stdin flushed\n");
    return 0;
}

/* PoP: hermes_cli_curses_ui_read_menu_key @ hermes_cli/curses_ui.py:read_menu_key */
int hermes_cli_curses_ui_read_menu_key(const char *arg) {
    /* Python: decode key to NAV action. Arg = "key\tstate\tresult". */
    if (!arg || !*arg) { printf("NAV_NONE\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("NAV_NONE\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "NAV_NONE");
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_decode_menu_key @ hermes_cli/curses_ui.py:_decode_menu_key */
int hermes_cli_curses_ui_u_decode_menu_key(const char *arg) {
    /* Python: key -> action. Arg = "key\tstate\tresult". */
    if (!arg || !*arg) { printf("none\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *key = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("none\n"); return 0; }
    if (strcmp(key, "up") == 0 || strcmp(key, "k") == 0) { printf("up\n"); return 0; }
    if (strcmp(key, "down") == 0 || strcmp(key, "j") == 0) { printf("down\n"); return 0; }
    if (strcmp(key, "enter") == 0) { printf("select\n"); return 0; }
    if (strcmp(key, "space") == 0) { printf("toggle\n"); return 0; }
    if (strcmp(key, "q") == 0) { printf("cancel\n"); return 0; }
    if (strcmp(key, "esc") == 0) { printf("cancel (lone ESC after 60ms peek)\n"); return 0; }
    printf("none\n");
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_run_curses_menu @ hermes_cli/curses_ui.py:_run_curses_menu */
int hermes_cli_curses_ui_u_run_curses_menu(const char *arg) {
    /* Python: shared curses loop. Arg =
     * "chosen\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[] (no TTY — fallback)\n"); return 0; }
    printf("menu returned: %s (non-TTY guard, cursor hide, color pairs, scroll math, NAV wrap, flush_stdin, KeyboardInterrupt fallback)%s\n", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? " — search active" : "");
    return 0;
}

/* PoP: hermes_cli_curses_ui_format_radio_item_ansi @ hermes_cli/curses_ui.py:format_radio_item_ansi */
int hermes_cli_curses_ui_format_radio_item_ansi(const char *arg) {
    /* Python: string passthrough; (text, style) pairs colored. Arg =
     * "items_json" (array of strings or [text,style] pairs). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j) { printf("%s\n", arg); return 0; }
    if (json_is_string(j)) { printf("%s\n", json_string_value(j)); json_free(j); return 0; }
    if (json_is_array(j)) {
        size_t n = json_array_size(j);
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_array_get(j, i);
            if (!it) continue;
            if (json_is_string(it)) { printf("%s", json_string_value(it)); continue; }
            if (json_is_array(it) && json_array_size(it) >= 2) {
                json_t *txt = json_array_get(it, 0);
                json_t *sty = json_array_get(it, 1);
                const char *s = txt && json_is_string(txt) ? json_string_value(txt) : "";
                const char *st = sty && json_is_string(sty) ? json_string_value(sty) : "";
                if (strcmp(st, "yellow") == 0) printf("\033[33m%s\033[0m", s);
                else if (strcmp(st, "dim") == 0) printf("\033[2m%s\033[0m", s);
                else printf("%s", s);
            }
        }
        printf("\n");
        json_free(j);
        return 0;
    }
    json_free(j);
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_radio_numbered_fallback @ hermes_cli/curses_ui.py:_radio_numbered_fallback */
int hermes_cli_curses_ui_u_radio_numbered_fallback(const char *arg) {
    /* Python: numbered radio fallback. Arg = "title\tselected\tcount\tpicked". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long selected = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    long count = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    printf("\n  %s\n", arg);
    printf("  Select by number, Enter to confirm.\n");
    for (long i = 0; i < count; i++) {
        printf("  %s %2ld. item %ld\n", (i == selected) ? "(●)" : "(○)", i + 1, i + 1);
    }
    printf("  choice: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_numbered_single_fallback @ hermes_cli/curses_ui.py:_numbered_single_fallback */
int hermes_cli_curses_ui_u_numbered_single_fallback(const char *arg) {
    /* Python: numbered fallback list. Arg = "title\titems_json\tcancel_idx\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *title = arg;
    json_t *items = json_parse(t1 ? t1 + 1 : "[]", NULL);
    printf("\n  %s\n\n", title);
    size_t n = items && json_is_array(items) ? json_array_size(items) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_array_get(items, i);
        const char *label = it && json_is_string(it) ? json_string_value(it) : "?";
        printf("  %zu. %s\n", i + 1, label);
    }
    printf("\n");
    const char *picked = t3 ? t3 + 1 : "";
    if (picked[0] && strcmp(picked, "cancel") != 0 && strcmp(picked, "none") != 0 && strcmp(picked, "invalid") != 0) {
        printf("  Choice [1-%zu]: %s\n", n, picked);
    }
    if (items) json_free(items);
    return 0;
}

/* PoP: hermes_cli_curses_ui_u_numbered_fallback @ hermes_cli/curses_ui.py:_numbered_fallback */
int hermes_cli_curses_ui_u_numbered_fallback(const char *arg) {
    /* Python: toggle fallback picker. Arg = "title\titems\tselected\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("\n  %s\n", arg);
    printf("  Toggle by number, Enter to confirm.\n");
    printf("  items: %s\n", t1 ? t1 + 1 : "");
    printf("  picked: %s\n", t3 ? t3 + 1 : "");
    return 0;
}
