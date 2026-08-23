/* gate: agent_import_extract_markdown_entries — faithful traced copy of
 * src/hermes_cli/agent_import.c::ai_extract_markdown_entries
 * (LIVE twin: hermes_cli/agent_import.py::extract_markdown_entries).
 *
 * Branch ladder per input line (must match twin.py EXACTLY):
 *   LOOP per line
 *   B1: stripped.startswith("```")   -> toggle code, flush
 *   B2: in_code_block                -> skip line
 *   B3: heading  ^#{1,6}\s+\S.*      -> flush, push heading
 *   B4: bullet   ^\s*([-*]|\d+\.)\s  -> flush, emit entry
 *   B5: blank line                   -> flush
 *   B6: table |...|                  -> flush
 *   else paragraph accumulate
 *   V on every emitted entry; final dedup loop (LOOP + dup branch) with
 *   V on each kept entry.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

#define MAXL 4096

static char **ents; static size_t n_ents;
static char *heads[32]; static size_t n_heads;
static char *para[256]; static size_t n_para;

static bool banned_heading(const char *h) {
    static const char *const kws[] = {"MEMORY","USER","SOUL","AGENTS","TOOLS",
                                      "IDENTITY","CLAUDE",NULL};
    for (int k = 0; kws[k]; k++) {
        size_t kl = strlen(kws[k]);
        const char *p = h;
        while ((p = strstr(p, kws[k])) != NULL) {
            bool l_ok = (p == h) || !(isalnum((unsigned char)p[-1]) || p[-1]=='_');
            bool md = (p[kl]=='.') && tolower((unsigned char)p[kl+1])=='m' &&
                      tolower((unsigned char)p[kl+2])=='d';
            bool r_ok = (p[kl+3]=='\0') ||
                        !(isalnum((unsigned char)p[kl+3]) || p[kl+3]=='_');
            if (l_ok && md && r_ok) return true;
            p += kl;
        }
    }
    return false;
}

static void emit_entry(wcup_t *wc, const char *prefix, const char *block) {
    char buf[MAXL * 2];
    if (prefix && *prefix) snprintf(buf, sizeof buf, "%s: %s", prefix, block);
    else snprintf(buf, sizeof buf, "%s", block);
    wcup_valstr(wc, buf);
    ents = realloc(ents, (n_ents + 1) * sizeof(char *));
    ents[n_ents] = strdup(buf);
    n_ents++;
}

static void context_prefix(char *out, size_t outsz) {
    out[0] = '\0';
    size_t n = 0;
    for (size_t i = 0; i < n_heads; i++) {
        if (banned_heading(heads[i])) continue;
        if (n == 0) snprintf(out, outsz, "%s", heads[i]);
        else { strncat(out, " > ", outsz - strlen(out) - 1);
               strncat(out, heads[i], outsz - strlen(out) - 1); }
        n++;
    }
}

static void flush_paragraph(wcup_t *wc) {
    if (n_para == 0) return;
    char block[MAXL * 2]; block[0] = '\0';
    for (size_t i = 0; i < n_para; i++) {
        if (i) strcat(block, " ");
        strcat(block, para[i]);
    }
    /* strip */
    char *s = block;
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    for (size_t i = 0; i < n_para; i++) free(para[i]);
    n_para = 0;
    if (!*s) return;
    char prefix[MAXL];
    context_prefix(prefix, sizeof prefix);
    emit_entry(wc, prefix, s);
}

static void normalize_text(const char *in, char *out, size_t outsz) {
    size_t n = strlen(in), j = 0;
    size_t start = 0, end = n;
    while (start < end && isspace((unsigned char)in[start])) start++;
    while (end > start && isspace((unsigned char)in[end-1])) end--;
    bool in_space = false;
    for (size_t i = start; i < end && j + 1 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isspace(c)) { if (!in_space) { out[j++] = ' '; in_space = true; } }
        else { out[j++] = (char)tolower(c); in_space = false; }
    }
    while (j > 0 && out[j-1] == ' ') j--;
    out[j] = '\0';
}

static void wc_extract(const char *text) {
    WCUP_BEGIN("extract_markdown_entries");
    ents = NULL; n_ents = 0;
    n_heads = 0; n_para = 0;
    bool in_code = false;

    const char *it = text ? text : "";
    while (*it) {
        const char *eol = strchr(it, '\n');
        size_t linelen = eol ? (size_t)(eol - it) : strlen(it);
        char line[MAXL];
        memcpy(line, it, linelen < MAXL-1 ? linelen : MAXL-1);
        line[linelen < MAXL-1 ? linelen : MAXL-1] = '\0';
        it = eol ? eol + 1 : it + strlen(it);
        WCUP_LOOP_TICK();

        size_t rl = strlen(line);
        while (rl > 0 && isspace((unsigned char)line[rl-1])) rl--;
        line[rl] = '\0';                              /* rstrip */
        size_t s = 0;
        while (s < rl && isspace((unsigned char)line[s])) s++;
        const char *stripped = line + s;              /* lstrip view */

        if (strncmp(stripped, "```", 3) == 0) {       /* B1 */
            WCUP_BRANCH(1);
            flush_paragraph(&_wc);
            in_code = !in_code;
            continue;
        }
        WCUP_BRANCH(0);
        if (in_code) {                                /* B2 */
            WCUP_BRANCH(1);
            continue;
        }
        WCUP_BRANCH(0);
        bool is_head = false;
        if (stripped[0] == '#') {                     /* B3 heading */
            size_t hl = 0;
            while (hl < 6 && stripped[hl] == '#') hl++;
            is_head = hl >= 1 && hl <= 6 && stripped[hl] == ' ' &&
                      ((s + hl + 1) < rl) && stripped[hl+1];
            /* mirror C: value must contain a non-space char */
            if (is_head) {
                const char *v = stripped + hl + 1;
                const char *q = v + strlen(v) - 1;
                while (q >= v && isspace((unsigned char)*q)) q--;
                if (q < v) is_head = false;
            }
        }
        WCUP_BRANCH(is_head);
        if (is_head) {
            size_t hl = 0;
            while (hl < 6 && stripped[hl] == '#') hl++;
            flush_paragraph(&_wc);
            char val[MAXL];
            snprintf(val, sizeof val, "%s", stripped + hl + 1);
            char *e2 = val + strlen(val);
            while (e2 > val && isspace((unsigned char)e2[-1])) e2--;
            *e2 = '\0';
            while (n_heads >= hl) free(heads[--n_heads]);
            heads[n_heads++] = strdup(val);
            continue;
        }
        {   /* B4 bullet on raw line */
            size_t bi = 0;
            while (bi < rl && isspace((unsigned char)line[bi])) bi++;
            bool is_bullet = false; size_t cs = 0;
            if (bi < rl && (line[bi] == '-' || line[bi] == '*')) {
                is_bullet = true; cs = bi + 1;
            } else if (bi < rl && isdigit((unsigned char)line[bi])) {
                size_t di = bi;
                while (di < rl && isdigit((unsigned char)line[di])) di++;
                if (di < rl && line[di] == '.') { is_bullet = true; cs = di + 1; }
            }
            /* require a non-space content tail after the marker */
            if (is_bullet) {
                size_t t = rl;
                while (t > cs && isspace((unsigned char)line[t-1])) t--;
                if (t <= cs) is_bullet = false;
            }
            WCUP_BRANCH(is_bullet);
            if (is_bullet) {
                flush_paragraph(&_wc);
                while (cs < rl && isspace((unsigned char)line[cs])) cs++;
                size_t ce = rl;
                while (ce > cs && isspace((unsigned char)line[ce-1])) ce--;
                char content[MAXL];
                memcpy(content, line + cs, ce - cs); content[ce-cs] = '\0';
                char prefix[MAXL];
                context_prefix(prefix, sizeof prefix);
                emit_entry(&_wc, prefix, content);
                continue;
            }
        }
        if (stripped[0] == '\0') {                    /* B5 blank */
            WCUP_BRANCH(1);
            flush_paragraph(&_wc);
            continue;
        }
        WCUP_BRANCH(0);
        if (stripped[0] == '|' &&
            stripped[strlen(stripped)-1] == '|') {    /* B6 table */
            WCUP_BRANCH(1);
            flush_paragraph(&_wc);
            continue;
        }
        WCUP_BRANCH(0);
        para[n_para++] = strdup(stripped);
    }
    flush_paragraph(&_wc);

    /* dedup by normalize_text */
    char seen[64][MAXL]; size_t n_seen = 0;
    for (size_t i = 0; i < n_ents; i++) {
        WCUP_LOOP_TICK();
        char norm[MAXL * 2];
        normalize_text(ents[i], norm, sizeof norm);
        bool dup = (!norm[0]);
        if (!dup)
            for (size_t j = 0; j < n_seen; j++)
                if (strcmp(seen[j], norm) == 0) { dup = true; break; }
        WCUP_BRANCH(dup);
        if (!dup) {
            snprintf(seen[n_seen++], MAXL, "%s", norm);
            WCUP_VALSTR(ents[i]);
        }
    }

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("extract_markdown_entries");
    for (size_t i = 0; i < n_ents; i++) free(ents[i]);
    free(ents);
    for (size_t i = 0; i < n_heads; i++) free(heads[i]);
}

/* unescape \n sequences from argv */
static void unsescape(char *s) {
    char *w = s, *r = s;
    while (*r) {
        if (*r == '\\' && r[1] == 'n') { *w++ = '\n'; r += 2; }
        else *w++ = *r++;
    }
    *w = '\0';
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_extract <text(\\n-escaped)>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[MAXL * 4];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        unsescape(buf);
        wc_extract(buf);
    }
    return 0;
}
