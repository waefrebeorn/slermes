#ifndef NOC_W9_CLISNAPSHOT_337C7C_H
#define NOC_W9_CLISNAPSHOT_337C7C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_clisnapshot_337c7c_rejoin_history @ hermes_cli/cli_snapshot.py:_rejoin_cli
 * The conversation-history rebuild from a restored snapshot: drop archived
 * messages and strip each message's row_id key, preserving the rest. */
struct json_t *w9_clisnapshot_337c7c_rejoin_history(const struct json_t *messages);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_CLISNAPSHOT_337C7C_H */
