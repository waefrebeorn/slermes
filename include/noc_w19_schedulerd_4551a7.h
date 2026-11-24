/* noc_w19_schedulerd_4551a7.h - public C API for cron/scheduler_delivery_standalone.py. */
#ifndef NOC_W19_SCHEDULERD_4551A7_H
#define NOC_W19_SCHEDULERD_4551A7_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_schedulerd_4551a7_standalone_send @ cron/scheduler_delivery_standalone.py:_standalone_send */
json_t *w19_schedulerd_4551a7_standalone_send(const char *where, const char *content, int media_count,
                                              double send_timeout, double base_timeout,
                                              int shutting_down, const char *outcome,
                                              const char *exc_text, int request_never_left,
                                              int in_flight0, int shutdown_on_retry,
                                              json_t *ok_result);
/* PoP: w19_schedulerd_4551a7_queue_for_reconnect @ cron/scheduler_delivery_standalone.py:_queue_for_live_reconnect */
json_t *w19_schedulerd_4551a7_queue_for_reconnect(const char *where, const char *platform_name,
                                                 const char *chat_id, const char *thread_id,
                                                 const char *job_id, const char *content,
                                                 int media_count, const char *live_error,
                                                 int live_retryable, int ledger_enabled,
                                                 int has_loop, int has_schedule, int record_raises,
                                                 const char *adapter_profile);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_SCHEDULERD_4551A7_H */
