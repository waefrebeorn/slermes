#ifndef NOC_SESSCRON_H
#define NOC_SESSCRON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: sesscron_cron_read @ gateway/session_cron_commands.py:cron_read
 *
 * Faithful port of cron_read(tokens):
 *   - no tokens            -> "overview"
 *   - tokens[0].lower()=="list" and every remaining token in {"--all","-a"}
 *                          -> "list-all" when there are extra tokens, else "list"
 *   - otherwise            -> NULL (a write form of /cron)
 * The returned string is a static literal; callers must not free it.
 */
const char *sesscron_cron_read(const char *const *tokens, size_t n_tokens);

#ifdef __cplusplus
}
#endif

#endif /* NOC_SESSCRON_H */
