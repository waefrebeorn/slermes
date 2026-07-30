/*
 * gw_pollers.h — prototypes for gateway platform poller / maintenance
 * threads (gw_pollers.c, gw_notifier.c). Included by gateway/server.c to
 * populate the platform dispatch table. Self-contained: no gateway types
 * needed, thread entry points only.
 */
#ifndef GW_POLLERS_H
#define GW_POLLERS_H

/* Telegram short-poll used during approval waits (registered via
 * gw_approval_set_poll from thread_poll_telegram). */
char *telegram_poll_for_response(const char *target_chat_id);

/* Per-platform poll threads (gw_pollers.c) */
void *thread_poll_telegram(void *arg);
void *thread_poll_discord(void *arg);
void *thread_poll_slack(void *arg);
void *thread_poll_matrix(void *arg);
void *thread_poll_mattermost(void *arg);
void *thread_webhook(void *arg);
void *thread_poll_email(void *arg);
void *thread_poll_signal(void *arg);
void *thread_poll_ha(void *arg);
void *thread_poll_sms(void *arg);
void *thread_poll_feishu(void *arg);
void *thread_poll_wecom(void *arg);
void *thread_poll_dingtalk(void *arg);
void *thread_poll_qqbot(void *arg);
void *thread_poll_bluebubbles(void *arg);
void *thread_msgraph_webhook(void *arg);
void *thread_weixin(void *arg);
void *thread_yuanbao(void *arg);

/* Background maintenance threads (gw_notifier.c) */
void *thread_kanban_notifier(void *arg);
void *thread_cleanup_sessions(void *arg);

#endif /* GW_POLLERS_H */
