/*
 * session_policy_flow.h — local-client launch-policy FLOW cores of
 * gateway/session_policy.py.
 *
 * Ports the policy-evaluation DECISION chains: surface/provenance mapping,
 * profile-route resolution, config section normalization, skill validation
 * refusals, stored-client-env validation, cross-OS receipt cwd absoluteness,
 * and the full restore_policy allow/deny check sequence (every refusal names
 * its field + detail). All live I/O (forward-policy classification, the
 * adoption warning log) enters through seams; the cores perform no I/O.
 *
 * Refusals model persistence.core.errors.AuthorityRefusal: a raised flag plus
 * reason / field / detail, with the live _one_line normalization applied
 * (whitespace collapse, non-printable drop, 80/160 bounds, empty -> None,
 * emitted as JSON null). JSON values are libjson json_t.
 */
#ifndef SESSION_POLICY_FLOW_H
#define SESSION_POLICY_FLOW_H

#include "gateway_lifecycle_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A named refusal (AuthorityRefusal). field/detail buffers exceed the live
 * _one_line bounds (80/160); set_refusal normalizes into them, empty -> ""
 * which the harness emits as JSON null (live None). */
typedef struct gsp_refusal {
    bool raised;
    char reason[64];
    char field[96];
    char detail[192];
} gsp_refusal_t;

/* ── 1. session_pinned: a field's last committed session edit, else launched ──
 * config/field/launched borrowed. Returns a NEW json value (caller frees):
 * pins[field] when SESSION_PINS is a dict holding a bool for field, else a
 * copy of launched. */
/* PoP: gsp_flow_session_pinned @ gateway/session_policy.py:session_pinned */
json_t *gsp_flow_session_pinned(const json_t *config, const char *field,
                                const json_t *launched);

/* ── 2. created_surface: creation label -> agent surface provenance ─────────
 * Returns malloc'd surface name, or NULL for a non-surface label
 * (None/''/local/unknown). Caller frees. */
/* PoP: gsp_flow_created_surface @ gateway/session_policy.py:created_surface */
char *gsp_flow_created_surface(const json_t *label);

/* ── 3. profile_target: (model, provider) a profile config selects ──────────
 * Returns a NEW 2-element array [model, provider] (caller frees). 'auto'
 * provider resolves to ''. config borrowed. */
/* PoP: gsp_flow_profile_target @ gateway/session_policy.py:profile_target */
json_t *gsp_flow_profile_target(const json_t *config);

/* ── 4. present_sections: drop null sections, scalar model -> {default} ─────
 * Returns a NEW object (caller frees). config borrowed. */
/* PoP: gsp_flow_present_sections @ gateway/session_policy.py:present_sections */
json_t *gsp_flow_present_sections(const json_t *config);

/* ── 5. terminal_uses_host_cwd: local backend, or docker mounting host cwd ──
 * Pure predicate. terminal borrowed. */
/* PoP: gsp_flow_terminal_uses_host_cwd @ gateway/session_policy.py:terminal_uses_host_cwd */
bool gsp_flow_terminal_uses_host_cwd(const json_t *terminal);

/* ── 6. launch_skills: validated -s/--skills identifiers (refusal chain) ────
 * Returns a NEW array of stripped skill names (caller frees), or NULL with
 * *refusal set. params borrowed. */
/* PoP: gsp_flow_launch_skills @ gateway/session_policy.py:launch_skills */
json_t *gsp_flow_launch_skills(const json_t *params, gsp_refusal_t *refusal);

/* ── 7. _valid_client_env: stored client map / rules JSON shape check ───────
 * kind_is_list selects kind=list (rules: names -> string lists) vs kind=str
 * (map: names -> strings). value borrowed (a JSON string or null). */
/* PoP: gsp_flow_valid_client_env @ gateway/session_policy.py:_valid_client_env */
bool gsp_flow_valid_client_env(const json_t *value, bool kind_is_list);

/* ── 8. _stored_cwd_is_absolute: receipt cwd absolute in EITHER OS flavour ──
 * PurePosixPath(cwd).is_absolute() or PureWindowsPath(cwd).is_absolute().
 * cwd borrowed (a JSON string). */
/* PoP: gsp_flow_stored_cwd_is_absolute @ gateway/session_policy.py:_stored_cwd_is_absolute */
bool gsp_flow_stored_cwd_is_absolute(const char *cwd);

/* ── 9. restore_policy: the full stored-receipt allow/deny check sequence ───
 * Seams stand in for:
 *   is_forward_policy -> gateway/session_a2a.py:is_forward_policy (scripted)
 *   log               -> gateway logger (adoption warning; level + fmt + args)
 * data is the stored receipt policy object (borrowed). Returns a NEW object
 * with all LocalSessionPolicy fields (defaults applied, source relabeled for a
 * peer-refreshed 'local' receipt, toolsets as an array) on success, or NULL
 * with *refusal set. The adoption warning fires once per case (the oracle
 * clears _ADOPTED_KEY_SETS per case). */
typedef struct gsp_restore_seams {
    void *ctx;
    bool (*is_forward_policy)(void *ctx, const json_t *policy);
    void (*log)(void *ctx, int level, const char *fmt, const json_t *args);
} gsp_restore_seams_t;

/* PoP: gsp_flow_restore_policy @ gateway/session_policy.py:restore_policy */
json_t *gsp_flow_restore_policy(const json_t *data, const gsp_restore_seams_t *s,
                                gsp_refusal_t *refusal);

#ifdef __cplusplus
}
#endif

#endif /* SESSION_POLICY_FLOW_H */
