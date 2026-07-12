#ifndef PORT_AGENT_RETRY_UTILS_H
#define PORT_AGENT_RETRY_UTILS_H

#include <stdbool.h>

/* C port of agent/retry_utils.py — pure retry/backoff helpers. */

typedef struct {
    int status_code;     /* 0 = absent */
    const char *text;    /* lowercased, may be NULL */
} retry_error_t;

/* Flatten an error object to lowercased text. */
void retry_utils_error_text(const retry_error_t *err, char *out, size_t outsz);
/* True for Z.AI Coding Plan transient overload 429s. */
bool retry_utils_is_zai_overload(const char *base_url, const char *model,
                                 const retry_error_t *err);
/* Provider-aware backoff. reason_out is malloc'd or NULL (caller frees). */
double retry_utils_adaptive_backoff(int attempt, const char *base_url,
                                    const char *model, const retry_error_t *err,
                                    double default_wait, int short_attempts,
                                    char **reason_out);

#endif /* PORT_AGENT_RETRY_UTILS_H */
