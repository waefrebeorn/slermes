/*
 * port_nous_account.h -- pure helpers ported from hermes_cli/nous_account.py.
 *
 * The module's JWT decoding, urllib calls and dataclass construction stay in
 * Python; only the dependency-free decisions are here.
 */
#ifndef PORT_NOUS_ACCOUNT_H
#define PORT_NOUS_ACCOUNT_H

#include <stddef.h>

/* nous_account.py:_normalize_tier -- strip + lowercase a tier claim; "" for a
 * non-string or NULL (Python's isinstance guard). Caller frees. */
char *nous_normalize_tier(const char *value);

/* nous_account.py:_is_anonymous_tier -- both facts must hold. */
int nous_is_anonymous_tier(int has_account_info, int is_anonymous_tier);

/* nous_account.py:nous_policy_notice -- the one-line notice, or "".
 * policy_present: 1 = True, 0 = False, -1 = unknown (None). Only a definite
 * True counts. Caller frees (including the ""). */
char *nous_policy_notice(int removed, int policy_present);

#endif /* PORT_NOUS_ACCOUNT_H */
