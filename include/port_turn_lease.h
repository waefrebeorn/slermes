/*
 * port_turn_lease.h — pure formatting/serialization helpers from
 * gateway/turn_lease.py. The asyncio lease registry (acquire/rebind/release/
 * evict) stays unported; these take the token fields as parameters.
 */
#ifndef PORT_TURN_LEASE_H
#define PORT_TURN_LEASE_H

/* _holder_desc(holder): JSON array ["<owner_key>",<generation>] for a held
 * lease, or ["?","?"]. present=0 means no holder. Returns malloc'd JSON. */
char *tls_holder_desc(const char *owner_key, long long generation, int present);

/* TurnLeaseToken.__repr__: the exact Python repr string, with session_id and
 * owner_key Python-repr'd (single-quoted). released is Python bool (True/False).
 * Returns malloc'd string. Caller frees. */
char *tls_token_repr(const char *session_id, const char *owner_key,
                     long long generation, int released);

/* TurnLeaseTimeoutError message: "turn lease wait timed out after {wait:.0f}s
 * on session {sid} for routing key {key} (gen {gen})" with banker's rounding
 * (round-half-even) on wait_seconds. Returns malloc'd string. Caller frees. */
char *tls_timeout_msg(const char *session_id, const char *owner_key,
                      long long generation, double wait_seconds);

#endif /* PORT_TURN_LEASE_H */
