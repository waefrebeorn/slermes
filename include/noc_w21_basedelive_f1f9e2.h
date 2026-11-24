/*
 * noc_w21_basedelive_f1f9e2.h -- public C API for the port of upstream Hermes
 *   gateway/platforms/base_delivery_ledger.py (gate w21_basedelive_f1f9e2).
 *
 * Ported: the whole record-before-send gate of record_final_obligation
 * (ephemeral, slash-command prefix with the typed_command_prefix fallback, the
 * ledger_enabled gate, the ledger_message_id/message_id choice and the
 * compute_obligation_id hash) and the settle ladder of
 * finalize_final_obligation (delivered; failed then reconnect redelivery vs the
 * flood-backoff timer vs nothing). The ledger store, the runner and the
 * classifier verdicts are seams supplied as `world`.
 */
#ifndef NOC_W21_BASEDELIVE_F1F9E2_H
#define NOC_W21_BASEDELIVE_F1F9E2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* record_final_obligation(...) as the id it returns or null. `world`:
 *   is_ephemeral          -- is_ephemeral_response
 *   text                  -- event.text raw value (str(... or "").lstrip())
 *   typed_command_prefix  -- adapter.typed_command_prefix (None/"" -> "!")
 *   ledger_enabled        -- ledger_enabled() on a worker thread
 *   session_key           -- session_key
 *   content               -- text_content (the ledgered body)
 *   ledger_message_id     -- getattr(event, "ledger_message_id", None)
 *   message_id            -- getattr(event, "message_id", "")
 * Returns null when nothing is saved, else the 24-char sha256 obligation id. */
json_t *w21_basedelive_f1f9e2_record_final(const json_t *world);

/* finalize_final_obligation(...) as the side effects it performs. `world`:
 *   success                  -- bool(result.success)
 *   error                    -- str(result.error or "")
 *   reconnect_only           -- gateway.delivery_ledger.is_reconnect_only(error)
 *   dead_kind                -- gateway.dead_targets.classify_dead_error(error)
 *   live_is_delivery_adapter -- adapter._final_delivery_adapter(...) is delivery_adapter
 *   redeliver_callable       -- the runner's _redeliver_failed_obligations_for_platform
 *   schedule_callable        -- the runner's _schedule_flood_redelivery
 * Returns {"mark":"delivered"|"failed","action":"none"|"redeliver"|"schedule"}. */
json_t *w21_basedelive_f1f9e2_finalize_action(const json_t *world);

/* Probe of the harness environment: {"sler":bool,"home":bool,"same":bool}. */
json_t *w21_basedelive_f1f9e2_env_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_BASEDELIVE_F1F9E2_H */
