/*
 * session_cron_commands.c — C home for upstream Hermes gateway/session_cron_commands.py.
 *
 * PoP: sesscron_cron_read @ gateway/session_cron_commands.py:cron_read
 *
 * Real C11 port of the read-form classifier for the shared-gateway /cron
 * handler.  The write forms are intentionally not executed here (they run in
 * the owning profile on the classic CLI's command socket); cron_read returns
 * NULL for those, exactly as upstream does.
 */
#include "noc_sesscron.h"

#include <ctype.h>
#include <string.h>

static char ascii_lower(char c) {
    return (char)tolower((unsigned char)c);
}

static int is_list_flag(const char *s) {
    return s && (!strcmp(s, "--all") || !strcmp(s, "-a"));
}

const char *sesscron_cron_read(const char *const *tokens, size_t n_tokens) {
    if (!tokens || n_tokens == 0)
        return "overview";

    /* tokens[0].lower() == 'list' (ASCII tokens; Python .lower() on ASCII is
     * byte-for-byte identical). */
    const char *first = tokens[0];
    if (!first || strlen(first) != 4)
        return NULL;
    if (ascii_lower(first[0]) != 'l' || ascii_lower(first[1]) != 'i' ||
        ascii_lower(first[2]) != 's' || ascii_lower(first[3]) != 't')
        return NULL;

    /* set(tokens[1:]) <= {'--all','-a'} */
    for (size_t i = 1; i < n_tokens; ++i) {
        if (!is_list_flag(tokens[i]))
            return NULL;
    }

    /* 'list-all' when tokens[1:] is non-empty, 'list' otherwise. */
    return n_tokens > 1 ? "list-all" : "list";
}
