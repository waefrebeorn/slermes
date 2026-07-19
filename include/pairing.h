/*
 * pairing.h — DM pairing system (faithful C11 port of gateway/pairing.py).
 *
 * Code-based approval flow for authorizing new users on messaging platforms.
 * Backed by per-platform JSON files under <dir>/ (e.g. ~/.hermes/pairing/):
 *   - {platform}-pending.json   : pending requests {entry_id: {hash,salt,...}}
 *   - {platform}-approved.json  : approved users   {user_id: {user_name,approved_at}}
 *   - _rate_limits.json         : rate-limit / lockout / failure tracking
 *
 * Security model (OWASP/NIST): 8-char codes from an unambiguous 32-char
 * alphabet, salted SHA-256 hashes stored (never plaintext), 1h TTL, max 3
 * pending/platform, 1 request/user/10min, lockout after 5 failed approvals.
 *
 * Time is INJECTABLE (pass `now` explicitly) so the logic is unit-testable.
 */

#ifndef PAIRING_H
#define PAIRING_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pairing_store pairing_store_t;

/* Approved user entry. */
typedef struct {
    char *user_id;
    char *user_name;
    double approved_at;
} pairing_approved_t;

/* Pending entry (admin view; code shown as first 8 hash hex). */
typedef struct {
    char *platform;
    char *code_display;   /* first 8 hex of hash, or "legacy" */
    char *user_id;
    char *user_name;
    int   age_minutes;
} pairing_pending_t;

/* approve_code result. */
typedef struct {
    char *user_id;
    char *user_name;
} pairing_result_t;

/* Timing/limit constants (mirror Python). */
#define PAIRING_CODE_LENGTH 8
#define PAIRING_CODE_TTL_SECONDS 3600
#define PAIRING_RATE_LIMIT_SECONDS 600
#define PAIRING_LOCKOUT_SECONDS 3600
#define PAIRING_MAX_PENDING_PER_PLATFORM 3
#define PAIRING_MAX_FAILED_ATTEMPTS 5
#define PAIRING_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

pairing_store_t *pairing_store_open(const char *dir);
void pairing_store_close(pairing_store_t *st);

/* Approved users. */
bool pairing_is_approved(pairing_store_t *st, const char *platform, const char *user_id);
int  pairing_list_approved(pairing_store_t *st, const char *platform, /* NULL = all */
                            pairing_approved_t **out);
bool pairing_approve_user(pairing_store_t *st, const char *platform,
                          const char *user_id, const char *user_name, double now);
bool pairing_revoke(pairing_store_t *st, const char *platform, const char *user_id);

/* Pending codes. */
char *pairing_generate_code(pairing_store_t *st, const char *platform,
                            const char *user_id, const char *user_name, double now);
pairing_result_t *pairing_approve_code(pairing_store_t *st, const char *platform,
                                       const char *code, double now);
int  pairing_list_pending(pairing_store_t *st, const char *platform, double now,
                          pairing_pending_t **out);
int  pairing_clear_pending(pairing_store_t *st, const char *platform);

/* Lockout inspection. */
bool pairing_is_locked_out(pairing_store_t *st, const char *platform, double now);

/* Free helpers. */
void pairing_free_approved(pairing_approved_t *arr, int n);
void pairing_free_pending(pairing_pending_t *arr, int n);
void pairing_free_result(pairing_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_H */
