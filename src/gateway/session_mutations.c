/*
 * session_mutations.c — C home for upstream Hermes gateway/session_mutations.py.
 *
 * PoP: w12_sessionmut_83f0c3_runtime_owns_authority @ gateway/session_mutations.py:_runtime_owns_authority
 */
#include "noc_w12_sessionmut_83f0c3.h"

int w12_sessionmut_83f0c3_runtime_owns_authority(int has_registry,
                                                  int registry_contains_authority,
                                                  int session_authority_is_authority) {
    if (has_registry)
        return registry_contains_authority ? 1 : 0;
    return session_authority_is_authority ? 1 : 0;
}
