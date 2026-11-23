/*
 * send_targets.c — faithful C11 port of upstream Hermes
 * plugins/platforms/matrix/send_targets.py (gate mstarget).
 * PoP: mstarget_parse_send_target @ plugins/platforms/matrix/send_targets.py:_parse_send_target
 */
#include "noc_mstarget.h"
#include <stdlib.h>
#include <string.h>

/* ASCII whitespace set of Python str.strip() (fixtures are ASCII). */
static int mstarget_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

int mstarget_parse_send_target(const char *ref, char **room, char **event_id) {
    const char *s, *e;
    size_t len, split, ev_len;

    if (!ref) return 0;
    s = ref;
    while (*s && mstarget_is_ws((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && mstarget_is_ws((unsigned char)*(e - 1))) e--;
    len = (size_t)(e - s);

    /* trimmed.rfind(":$") */
    split = (size_t)-1;
    if (len >= 2) {
        size_t i = len - 2;
        for (;;) {
            if (s[i] == ':' && s[i + 1] == '$') { split = i; break; }
            if (i == 0) break;
            i--;
        }
    }
    if (split == (size_t)-1 || split == 0) return 0;   /* split_idx > 0 */

    *room = (char *)malloc(split + 1);
    ev_len = len - (split + 1);
    *event_id = (char *)malloc(ev_len + 1);
    if (!*room || !*event_id) {
        free(*room); free(*event_id);
        *room = *event_id = NULL;
        return 0;
    }
    memcpy(*room, s, split);
    (*room)[split] = '\0';
    memcpy(*event_id, s + split + 1, ev_len);
    (*event_id)[ev_len] = '\0';
    return 1;
}
