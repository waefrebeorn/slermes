/*
 * status_bar_git.c — C home for upstream Hermes hermes_cli/status_bar_git.py.
 * PoP: w12_statusbar_132f96_branch_label @ hermes_cli/status_bar_git.py:current_git_branch
 *
 * Faithful port of the HEAD-text -> label computation (the pure heart of
 * current_git_branch after _resolve_git_dir + read_text().strip()).
 */
#include "hermes_json.h"
#include "noc_w12_statusbar_132f96.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static size_t sb_trim(const char *s, char *out, size_t cap)
{
    size_t a = 0, b, n;
    if (!cap) return 0;
    n = strlen(s);
    while (a < n && isspace((unsigned char)s[a])) a++;
    b = n;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    n = b - a;
    if (n >= cap) n = cap - 1;
    memcpy(out, s + a, n);
    out[n] = '\0';  /* NUL at the shortened length */
    return n;
}

static size_t sb_cp_len(const unsigned char *p)
{
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xE0) == 0xC0) return 2;
    if ((p[0] & 0xF0) == 0xE0) return 3;
    if ((p[0] & 0xF8) == 0xF0) return 4;
    return 1;
}

void w12_statusbar_132f96_branch_label(const char *head, char *out, size_t cap)
{
    char h[4096];
    size_t n, i;
    if (!out || !cap) return;
    out[0] = '\0';
    if (!head) head = "";
    n = sb_trim(head, h, sizeof(h));

    if (n >= 4 && !memcmp(h, "ref:", 4)) {
        char ref[4096];
        size_t rn, slash = 0;
        rn = sb_trim(h + 4, ref, sizeof(ref));
        if (rn >= 11 && !memcmp(ref, "refs/heads/", 11)) {
            /* label = ref[len("refs/heads/"):] */
            memmove(ref, ref + 11, rn - 11);
            ref[rn - 11] = '\0';
            snprintf(out, cap, "%s", ref);
            return;
        }
        for (i = 0; i < rn; i++) if (ref[i] == '/') slash = i + 1;
        snprintf(out, cap, "%s", ref + slash);
        return;
    }

    if (n > 0) {
        size_t pos = 0, taken = 0;
        int wrote = 0;
        while (pos < n && taken < 8 && (size_t)wrote + 4 < cap) {
            size_t cl = sb_cp_len((const unsigned char *)(h + pos));
            if (pos + cl > n) cl = 1;
            memcpy(out + wrote, h + pos, cl);
            wrote += (int)cl;
            pos += cl;
            taken++;
        }
        if ((size_t)wrote + 3 >= cap) { out[cap - 1] = '\0'; return; }
        memcpy(out + wrote, "\xe2\x80\xa6", 3);  /* U+2026 */
        wrote += 3;
        out[wrote] = '\0';
        return;
    }
    out[0] = '\0';
}
