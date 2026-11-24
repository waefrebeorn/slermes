/* noc_w19_runnotific_a1bcca.h - public C API for gateway/run_notifications_restart.py port. */
#ifndef NOC_W19_RUNNOTIFIC_A1BCCA_H
#define NOC_W19_RUNNOTIFIC_A1BCCA_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_runnotific_a1bcca_notice_expired @ gateway/run_notifications_restart.py:_notice_expired */
json_t *w19_runnotific_a1bcca_notice_expired(json_t *age_seconds, int stat_ok);
/* PoP: w19_runnotific_a1bcca_send_restart_notification @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._send_restart_notification */
json_t *w19_runnotific_a1bcca_send_restart_notification(json_t *spec);
/* PoP: w19_runnotific_a1bcca_deliver_restart_notification @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._deliver_restart_notification */
json_t *w19_runnotific_a1bcca_deliver_restart_notification(json_t *data, json_t *ctx);
/* PoP: w19_runnotific_a1bcca_schedule_replay_wanted @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._schedule_restart_notice_replay */
json_t *w19_runnotific_a1bcca_schedule_replay_wanted(json_t *spec);
/* PoP: w19_runnotific_a1bcca_notice_ttl_seconds @ gateway/run_notifications_restart.py:_RESTART_NOTICE_TTL_SECONDS */
json_t *w19_runnotific_a1bcca_notice_ttl_seconds(void);
/* PoP: w19_runnotific_a1bcca_note_restart_notice_boot @ gateway/run_notifications_restart.py:GatewayRestartNoticeMixin._note_restart_notice_boot */
json_t *w19_runnotific_a1bcca_note_restart_notice_boot(json_t *context_present);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_RUNNOTIFIC_A1BCCA_H */
