/* port_session_title.c — faithful C11 port of the session-title surface of
 * hermes_state.py (SessionDB), backed by libdb (the C tree's canonical
 * session store).
 *
 * Ports: sanitize_title, get_session_title, _set_session_title (with
 * only_if_empty predicate, uniqueness check, and the compression-ancestor
 * title transfer), set_session_title, set_auto_title_if_empty,
 * get_next_title_in_lineage.
 *
 * Store mapping: the Python sqlite `sessions.title` column maps to libdb's
 * sidecar session_meta_t.title; NULL maps to the empty string. The
 * unique-title index is enforced by scanning db_list_with_meta (the C store
 * is a per-session sidecar layout, not one sqlite table). The compression
 * continuation edge is parent.end_reason == "compression" via meta.parent_id
 * (mirror of _COMPRESSION_CHILD_SQL).
 */

#include "session_title.h"
#include "db.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TITLE_LENGTH 100   /* PoP mirror: SessionDB.MAX_TITLE_LENGTH */

/* --- UTF-8 helpers (for the Unicode control-char scrub + length rule) --- */

/* Decode one UTF-8 code point at p; stores it in *cp, returns byte length
 * (1..4), or 1 with *cp = byte value for invalid sequences (pass-through). */
static size_t utf8_decode(const unsigned char *p, unsigned int *cp) {
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) |
              (p[2] & 0x3F);
        return 3;
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12) |
              ((unsigned)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }
    *cp = p[0];
    return 1;
}

/* ASCII control chars to REMOVE (keep \t \n \r for whitespace collapsing):
 * [\x00-\x08\x0b\x0c\x0e-\x1f\x7f] */
static int is_removed_ascii_control(unsigned int cp) {
    if (cp == 0x7F) return 1;
    if (cp <= 0x08) return 1;
    if (cp == 0x0B || cp == 0x0C) return 1;
    if (cp >= 0x0E && cp <= 0x1F) return 1;
    return 0;
}

/* Problematic Unicode controls:
 * [\u200b-\u200f\u2028-\u202e\u2060-\u2069\ufeff\ufffc\ufff9-\ufffb] */
static int is_removed_unicode_control(unsigned int cp) {
    if (cp >= 0x200B && cp <= 0x200F) return 1;
    if (cp >= 0x2028 && cp <= 0x202E) return 1;
    if (cp >= 0x2060 && cp <= 0x2069) return 1;
    if (cp == 0xFEFF || cp == 0xFFFC) return 1;
    if (cp >= 0xFFF9 && cp <= 0xFFFB) return 1;
    return 0;
}

/* Python \s for the collapse step (ASCII whitespace; the wider Unicode set
 * is out of scope for the store's UTF-8 titles — \t \n \r \f \v space). */
static int is_ws(unsigned int cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
           cp == '\f' || cp == '\v';
}

/* PoP: session_title_sanitize @ hermes_state.py:sanitize_title */
char *session_title_sanitize(const char *title, bool *invalid) {
    if (invalid) *invalid = false;
    if (!title || !*title) return NULL;   /* falsy -> None */

    size_t slen = strlen(title);
    char *out = malloc(slen + 1);
    if (!out) return NULL;

    /* Single pass: drop removed controls, collapse whitespace runs to one
     * space, strip leading/trailing (the Python does re.sub removals, then
     * re.sub(r'\s+', ' ') + strip — order-equivalent because removed chars
     * are never whitespace). */
    size_t oi = 0;
    size_t cp_count = 0;      /* code points written (for MAX_TITLE_LENGTH) */
    int pending_space = 0;    /* a ws run seen since last emitted char */
    const unsigned char *p = (const unsigned char *)title;
    while (*p) {
        unsigned int cp;
        size_t adv = utf8_decode(p, &cp);
        if (is_removed_ascii_control(cp) || is_removed_unicode_control(cp)) {
            p += adv;
            continue;
        }
        if (is_ws(cp)) { pending_space = 1; p += adv; continue; }
        if (pending_space && oi > 0) { out[oi++] = ' '; cp_count++; }
        pending_space = 0;
        memcpy(out + oi, p, adv);
        oi += adv;
        cp_count++;
        p += adv;
    }
    out[oi] = '\0';

    if (oi == 0) { free(out); return NULL; }   /* empty after cleaning -> None */

    if (cp_count > MAX_TITLE_LENGTH) {
        free(out);
        if (invalid) *invalid = true;   /* Python raises ValueError */
        return NULL;
    }
    return out;
}

/* PoP: session_title_get @ hermes_state.py:get_session_title */
char *session_title_get(db_t *db, const char *session_id) {
    if (!db || !session_id || !*session_id) return NULL;
    session_meta_t meta;
    if (!db_load_meta(db, session_id, &meta)) return NULL;   /* no row -> None */
    if (!meta.title[0]) return NULL;                          /* NULL title */
    return strdup(meta.title);
}

/* Walk the compression-continuation parent chain up from descendant_id and
 * report whether ancestor_id is reached. Mirror of _is_compression_ancestor:
 * an edge counts only when the parent ended with end_reason='compression'. */
/* PoP: is_compression_ancestor @ hermes_state.py:_is_compression_ancestor */
static bool is_compression_ancestor(db_t *db, const char *ancestor_id,
                                    const char *descendant_id) {
    if (!ancestor_id || !descendant_id || !*ancestor_id || !*descendant_id)
        return false;
    if (strcmp(ancestor_id, descendant_id) == 0) return false;

    char cur[64];
    snprintf(cur, sizeof(cur), "%s", descendant_id);
    for (int depth = 0; depth < 64; depth++) {   /* cycle guard */
        session_meta_t child;
        if (!db_load_meta(db, cur, &child)) return false;
        if (!child.parent_id[0]) return false;
        session_meta_t parent;
        if (!db_load_meta(db, child.parent_id, &parent)) return false;
        if (strcmp(parent.end_reason, "compression") != 0) return false;
        if (strcmp(child.parent_id, ancestor_id) == 0) return true;
        snprintf(cur, sizeof(cur), "%s", child.parent_id);
    }
    return false;
}

/* PoP: session_title_set_impl @ hermes_state.py:_set_session_title */
static session_title_result_t set_title_impl(db_t *db, const char *session_id,
                                             const char *title,
                                             bool only_if_empty) {
    if (!db || !session_id || !*session_id) return SESSION_TITLE_NOT_FOUND;

    bool invalid = false;
    char *clean = session_title_sanitize(title, &invalid);
    if (invalid) return SESSION_TITLE_INVALID;

    session_meta_t meta;
    if (!db_load_meta(db, session_id, &meta)) {
        free(clean);
        return SESSION_TITLE_NOT_FOUND;
    }
    if (only_if_empty && meta.title[0]) {
        /* Predicate failed: a title appeared while generation was in flight
         * (manual rename wins). */
        free(clean);
        return SESSION_TITLE_SKIPPED;
    }

    if (clean && *clean) {
        /* Uniqueness: scan the store (mirror of the unique-title index). */
        size_t count = 0;
        db_session_entry_t *all = db_list_with_meta(db, &count);
        const char *conflict_id = NULL;
        static char conflict_buf[64];
        if (all) {
            for (size_t i = 0; i < count; i++) {
                if (strcmp(all[i].id, session_id) == 0) continue;
                if (strcmp(all[i].meta.title, clean) == 0) {
                    snprintf(conflict_buf, sizeof(conflict_buf), "%s", all[i].id);
                    conflict_id = conflict_buf;
                    break;
                }
            }
            free(all);
        }
        if (conflict_id) {
            if (is_compression_ancestor(db, conflict_id, session_id)) {
                /* Transfer: move the title off the hidden ancestor onto the
                 * live continuation (uniqueness preserved). */
                session_meta_t anc;
                if (db_load_meta(db, conflict_id, &anc)) {
                    anc.title[0] = '\0';
                    db_save_meta(db, conflict_id, &anc);
                }
            } else {
                free(clean);
                return SESSION_TITLE_CONFLICT;   /* Python raises ValueError */
            }
        }
    }

    snprintf(meta.title, sizeof(meta.title), "%s", clean ? clean : "");
    free(clean);
    if (!db_save_meta(db, session_id, &meta)) return SESSION_TITLE_NOT_FOUND;
    return SESSION_TITLE_OK;
}

/* PoP: session_title_set @ hermes_state.py:set_session_title */
session_title_result_t session_title_set(db_t *db, const char *session_id,
                                         const char *title) {
    return set_title_impl(db, session_id, title, false);
}

/* PoP: session_title_set_auto_if_empty @ hermes_state.py:set_auto_title_if_empty */
session_title_result_t session_title_set_auto_if_empty(db_t *db,
                                                        const char *session_id,
                                                        const char *title) {
    return set_title_impl(db, session_id, title, true);
}

/* Match a trailing " #N" suffix; returns the base length (without suffix)
 * and stores N, or returns full length with *num = 0 when absent. */
static size_t split_hash_suffix(const char *t, long *num) {
    *num = 0;
    size_t len = strlen(t);
    if (len < 3) return len;
    size_t i = len;
    while (i > 0 && isdigit((unsigned char)t[i - 1])) i--;
    if (i == len) return len;                 /* no trailing digits */
    if (i < 2 || t[i - 1] != '#' || t[i - 2] != ' ') return len;
    *num = strtol(t + i, NULL, 10);
    return i - 2;                              /* strip " #N" */
}

/* PoP: session_title_next_in_lineage @ hermes_state.py:get_next_title_in_lineage */
char *session_title_next_in_lineage(db_t *db, const char *base_title) {
    if (!base_title) return NULL;

    long n = 0;
    size_t base_len = split_hash_suffix(base_title, &n);
    char *base = malloc(base_len + 1);
    if (!base) return NULL;
    memcpy(base, base_title, base_len);
    base[base_len] = '\0';

    /* Scan existing titles: exact base or "base #N" variants. */
    bool any = false;
    long max_num = 1;   /* the unnumbered original counts as #1 */
    size_t count = 0;
    db_session_entry_t *all = db ? db_list_with_meta(db, &count) : NULL;
    if (all) {
        for (size_t i = 0; i < count; i++) {
            const char *t = all[i].meta.title;
            if (!t[0]) continue;
            if (strcmp(t, base) == 0) { any = true; continue; }
            long vn = 0;
            size_t vb = split_hash_suffix(t, &vn);
            if (vn > 0 && vb == base_len && strncmp(t, base, base_len) == 0) {
                any = true;
                if (vn > max_num) max_num = vn;
            }
        }
        free(all);
    }

    if (!any) return base;   /* no conflict: use the base name as-is */

    size_t need = base_len + 32;
    char *out = malloc(need);
    if (!out) { free(base); return NULL; }
    snprintf(out, need, "%s #%ld", base, max_num + 1);
    free(base);
    return out;
}
