/*
 * noc_w21_targets_e73d84.h -- public C API for the port of upstream Hermes
 *   gateway/outbound/targets.py (gate w21_targets_e73d84).
 *
 * Ported: the whole explicit-target syntax ladder of _parse_target_ref (plugin
 * parser, E.164 phone numbers for the phone platforms, bare numeric ids,
 * Matrix !room/@user, XMPP JIDs) and the whole resolution ladder of
 * resolve_send_target (plugin parser, plugin validator, channel directory,
 * unknown-platform error, the pass_unresolved_references hand-off, and the
 * three failure hints). The plugin registry entry, Platform membership and the
 * channel-directory lookup are seams supplied as `world`.
 */
#ifndef NOC_W21_TARGETS_E73D84_H
#define NOC_W21_TARGETS_E73D84_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _parse_target_ref(platform_name, target_ref) -> [chat_id, thread_id, explicit].
 * `parser` is the plugin's target_ref_syntax_fn result:
 *   null                       -- the platform has no parser
 *   {"kind":"not_explicit"}    -- the NOT_EXPLICIT sentinel
 *   {"kind":"none"}            -- parser returned None (fall through to the id rules)
 *   {"kind":"tuple","chat":"...","thread":null|"..."} -- the parsed pair */
json_t *w21_targets_e73d84_parse_ref(const char *platform_name, const char *target_ref,
                                     const json_t *parser);

/* resolve_send_target(platform_name, target_ref, pass_unresolved_references=...) ->
 * [chat_id, thread_id, error] (nulls where the live tuple has None). `world`:
 *   entry        -- null when platform_registry.get() found nothing, else
 *                   {"source":"plugin"|"builtin","parse":<parser above>,
 *                    "validate":"none"|"true"|"false"|"str:<text>"|"raises"}
 *   registered   -- platform_registry.is_registered(platform_name)
 *   builtin      -- platform_name in {member.value for member in Platform}
 *   resolve      -- resolve_channel_name(platform_name, target_ref) result (or null)
 *   resolve_raises -- the channel-directory import/lookup raised
 *   install_hint -- platform_install_hint(platform_name) */
json_t *w21_targets_e73d84_resolve(const char *platform_name, const char *target_ref,
                                   int pass_unresolved, const json_t *world);

/* unknown_platform_error(platform_name) with the install hint as a seam. */
json_t *w21_targets_e73d84_unknown_platform_error(const char *platform_name,
                                                  const char *install_hint);

/* Probe of the harness environment: {"sler":bool,"home":bool,"same":bool}. */
json_t *w21_targets_e73d84_env_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_TARGETS_E73D84_H */
