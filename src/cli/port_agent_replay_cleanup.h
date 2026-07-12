#ifndef PORT_AGENT_REPLAY_CLEANUP_H
#define PORT_AGENT_REPLAY_CLEANUP_H

#include <stdbool.h>
#include <stddef.h>

/* C port of agent/replay_cleanup.py — pure replay-history sanitizers. */

typedef struct {
    char role[16];
    char *content;
} replay_msg_t;

typedef struct {
    replay_msg_t *msgs;
    size_t count;
    size_t cap;
} replay_hist_t;

void replay_init(replay_hist_t *h);
void replay_free(replay_hist_t *h);
int  replay_push(replay_hist_t *h, const char *role, const char *content);

/* True if a tool result indicates the tool was interrupted. */
bool replay_cleanup_is_interrupted(const char *content);
/* Strip interrupted assistant->tool blocks. Returns new count. */
size_t replay_cleanup_strip_tails(replay_hist_t *h);
/* Strip a trailing dangling assistant(tool_calls) tail. Returns new count. */
size_t replay_cleanup_strip_dangling(replay_hist_t *h);
/* Apply both strippers in canonical order. Returns new count. */
size_t replay_cleanup_sanitize(replay_hist_t *h);

#endif /* PORT_AGENT_REPLAY_CLEANUP_H */
