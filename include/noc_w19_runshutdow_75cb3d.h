/* noc_w19_runshutdow_75cb3d.h - public C API for gateway/run_shutdown_home_notice.py port. */
#ifndef NOC_W19_RUNSHUTDOW_75CB3D_H
#define NOC_W19_RUNSHUTDOW_75CB3D_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_runshutdow_75cb3d_broadcast_skip_reason @ gateway/run_shutdown_home_notice.py:_home_shutdown_broadcast_skip_reason
 * Why this stop sends no home-channel notice, in the source's order: "in-chat restart",
 * "planned --replace takeover", "drain marker (suppress_notification=true)", then the
 * restart loop (which stamps the muted outcome). Answers
 * {"skip_reason":str|null,"notice":str|null,"loop_checked":bool}. */
json_t *w19_runshutdow_75cb3d_broadcast_skip_reason(int restart_requested,
                                                   const char *restart_command_source,
                                                   int planned_takeover, int drain_suppressed,
                                                   int loop_tripped);

/* PoP: w19_runshutdow_75cb3d_notice_muted_by_loop @ gateway/run_shutdown_home_notice.py:_home_shutdown_notice_muted_by_loop
 * Records this broadcast in the notice chain (loop_checked True) and answers
 * {"muted":bool,"loop_checked":true} from restart_loop_guard.notice_loop_tripped. */
json_t *w19_runshutdow_75cb3d_notice_muted_by_loop(int loop_tripped);

/* PoP: w19_runshutdow_75cb3d_got_active_session_notice @ gateway/run_shutdown_home_notice.py:_home_got_active_session_notice
 * True when the active-session pass already told a served home chat: one entry of
 * `configs` ([{profile,platform,chat_id,thread_id}]) whose delivery key is in `delivered`
 * ([{platform,chat_id,thread_id,profile}]). Keys follow run_shutdown._delivery_target_key. */
json_t *w19_runshutdow_75cb3d_got_active_session_notice(json_t *configs, json_t *delivered);

/* PoP: w19_runshutdow_75cb3d_settle_notice_debt @ gateway/run_shutdown_home_notice.py:settle_home_online_notice_debt
 * The plan the settle makes for the next boot: {"drop_marker":bool,"write_marker":bool,
 * "record_delivered":bool,"loop_checked":false} from this stop's notice outcome, the
 * restart flag and the loop-check flag. */
json_t *w19_runshutdow_75cb3d_settle_notice_debt(const char *notice, int restart_requested,
                                                 int loop_checked);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_RUNSHUTDOW_75CB3D_H */
