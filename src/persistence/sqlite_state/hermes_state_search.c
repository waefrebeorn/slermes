/*
 * hermes_state_search.c — organization twin of hermes_state_search.py.
 *
 * Session search (FTS5 + CJK + LIKE-fallback routing) and FTS rebuild
 * lifecycle. The core implementations were ported BEFORE the Python module
 * split, living in hermes_state_ports.c / hermes_state_open.c /
 * hermes_state_messages.c credited to hermes_state.py. This module re-exports
 * them under their search-module names for new callers
 * (extend-don't-duplicate: implementations stay where they are).
 */

#include "hermes_json.h"
#include <stddef.h>

/* Implementations live in hermes_state_ports.c */
extern char *hermes_state_search_messages(void *db, const char *query);
extern char *hermes_state_search_messages_impl(void *db, const char *query);
extern char *hermes_state_sanitize_fts5_query(const char *query);

/* PoP: hs_search_messages @ hermes_state_search.py:search_messages */
char *hs_search_messages(void *db, const char *query) {
    return hermes_state_search_messages(db, query);
}

/* PoP: hs_search_messages_impl @ hermes_state_search.py:_search_messages_impl */
char *hs_search_messages_impl(void *db, const char *query) {
    return hermes_state_search_messages_impl(db, query);
}

/*
 * Sanitize user input into a safe FTS5 query string. Ported alongside the
 * PoP: hs_search_sanitize_fts5_query @ hermes_state_search.py:_sanitize_fts5_query
 */
extern char *hs_sanitize_fts5_query(const char *raw);
char *hs_search_sanitize_fts5_query(const char *raw) {
    return hermes_state_sanitize_fts5_query(raw);
}

/* PoP: hs_search_contains_cjk @ hermes_state_search.py:_contains_cjk */
int hs_search_contains_cjk(const char *s) {
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        /* CJK Unified Ideographs + common extensions (3-byte UTF-8 ranges) */
        unsigned c = *p;
        if (c >= 0xE0) {
            /* decode 3-byte codepoint roughly */
            unsigned cp = 0;
            if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
                cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                if ((cp >= 0x4E00 && cp <= 0x9FFF) ||   /* CJK Unified */
                    (cp >= 0x3400 && cp <= 0x4DBF) ||   /* Extension A */
                    (cp >= 0xF900 && cp <= 0xFAFF))     /* Compatibility */
                    return 1;
                p += 2;
            }
        }
    }
    return 0;
}
