/*
 * port_approvals_suggest_proposals.h — Opaque Proposal struct + methods
 * from hermes_cli/approvals_suggest.py.
 */
#ifndef PORT_APPROVALS_SUGGEST_PROPOSALS_H
#define PORT_APPROVALS_SUGGEST_PROPOSALS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Proposal (Python: class Proposal) */
typedef struct proposal proposal_t;

/* PoP: Proposal.__init__ @ hermes_cli/approvals_suggest.py:Proposal */
proposal_t *proposal_create(const char *pattern, const char *kind);

/* PoP: Proposal.__del__ — not tracked by scanner, destructor */
void proposal_free(proposal_t *p);

/* PoP: add_example @ hermes_cli/approvals_suggest.py:Proposal.add_example */
int proposal_add_example(proposal_t *p, const char *command);

/* Accessors for oracle serialization */
const char  *proposal_pattern     (const proposal_t *p);
const char  *proposal_kind        (const proposal_t *p);
int          proposal_count       (const proposal_t *p);
const char **proposal_classes     (const proposal_t *p, size_t *len);
const char **proposal_examples    (const proposal_t *p, size_t *len);

/* PoP: build_proposals @ hermes_cli/approvals_suggest.py:build_proposals */
/* records: array of (command, description) pairs (2-char* arrays).
 * existing: set of pattern strings to exclude (or NULL, 0).
 * min_count / limit: see Python.
 * out_len: receives number of proposals returned.
 * Returns malloc'd array of proposal_t* — caller frees each + the array. */
proposal_t **build_proposals(
    const char ***records,
    size_t n_records,
    const char **existing,
    size_t n_existing,
    int min_count,
    int limit,
    size_t *out_len
);

#ifdef __cplusplus
}
#endif
#endif
