#ifndef NOC_W20_TELEGRAMSE_1853C7_H
#define NOC_W20_TELEGRAMSE_1853C7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_send_path_recovery.py
 * (gate w20_telegramse_1853c7).
 *
 * The adapter, its runner and the event loop are seams: each call takes a
 * scripted adapter ({"degraded","running","fatal","has_replay",
 * "has_redeliver","task_running","name","rerun"}) and answers the ordered
 * action trace the Python edge performs on it.
 */

/* PoP: w20_telegramse_1853c7_finalize @ plugins/platforms/telegram/telegram_send_path_recovery.py:TelegramSendPathRecoveryMixin._finalize_delivery_obligation
 * `result` = {"success":...,"error":...}; the super call always runs, the
 * sweep is scheduled only for a degraded refusal on a live recovery adapter. */
json_t *w20_telegramse_1853c7_finalize(const json_t *result, const json_t *adapter);

/* PoP: w20_telegramse_1853c7_mark_recovered @ plugins/platforms/telegram/telegram_send_path_recovery.py:mark_send_path_recovered
 * The degraded -> healthy edge: clear the flag, publish "connected", schedule
 * the sweep and replay the restart notices when the adapter can. */
json_t *w20_telegramse_1853c7_mark_recovered(const json_t *adapter);

/* PoP: w20_telegramse_1853c7_schedule_sweep @ plugins/platforms/telegram/telegram_send_path_recovery.py:_schedule_recovery_sweep
 * One coalesced sweep per adapter; an edge during a running sweep sets rerun. */
json_t *w20_telegramse_1853c7_schedule_sweep(const json_t *adapter);

/* PoP: w20_telegramse_1853c7_sweep_loop @ plugins/platforms/telegram/telegram_send_path_recovery.py:_schedule_recovery_sweep
 * The sweep coroutine's loop: settle, re-check, redeliver, and rerun once more
 * when an edge arrived during the sweep. `delivered` and `rerun_after` script
 * each redelivery call. */
json_t *w20_telegramse_1853c7_sweep_loop(const json_t *adapter,
                                        const json_t *delivered,
                                        const json_t *rerun_after);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_TELEGRAMSE_1853C7_H */
