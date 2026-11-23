#ifndef NOC_W6_RUNTOPICS_D5AA88_H
#define NOC_W6_RUNTOPICS_D5AA88_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C port of gateway/run_topics.py (gate w6_runtopics_d5aa88).
 * Only the module-level pure helpers and the static/shape predicates of
 * GatewayTopicThreadsMixin are ported; the gateway I/O methods are not.
 * The i18n default title is the literal "DEFAULT_TITLE" (the oracle pins
 * t("gateway.topic.default_title") to that value). Source fields are passed
 * explicitly: profile, chat_id, thread_id, platform, chat_type, flags. */

/* PoP: w6_runtopics_d5aa88_collapse_title @ gateway/run_topics.py:_collapse_title */
char *w6_runtopics_d5aa88_collapse_title(const char *title);
/* PoP: w6_runtopics_d5aa88_sanitize_telegram_topic_title @ gateway/run_topics.py:_sanitize_telegram_topic_title */
char *w6_runtopics_d5aa88_sanitize_telegram_topic_title(const char *title);
/* PoP: w6_runtopics_d5aa88_sanitize_discord_thread_title @ gateway/run_topics.py:_sanitize_discord_thread_title */
char *w6_runtopics_d5aa88_sanitize_discord_thread_title(const char *title);
/* PoP: w6_runtopics_d5aa88_telegram_topic_profile_name @ gateway/run_topics.py:_telegram_topic_profile_name */
char *w6_runtopics_d5aa88_telegram_topic_profile_name(const char *profile);
/* PoP: w6_runtopics_d5aa88_telegram_topic_key @ gateway/run_topics.py:_telegram_topic_key */
struct json_t *w6_runtopics_d5aa88_telegram_topic_key(const char *chat_id, const char *thread_id);
/* PoP: w6_runtopics_d5aa88_telegram_topic_cooldown_key @ gateway/run_topics.py:_telegram_topic_cooldown_key */
char *w6_runtopics_d5aa88_telegram_topic_cooldown_key(const char *profile, const char *chat_id);
/* PoP: w6_runtopics_d5aa88_is_telegram_dm @ gateway/run_topics.py:_is_telegram_dm */
int w6_runtopics_d5aa88_is_telegram_dm(const char *platform, const char *chat_type);
/* PoP: w6_runtopics_d5aa88_is_discord_auto_thread_lane @ gateway/run_topics.py:_is_discord_auto_thread_lane */
int w6_runtopics_d5aa88_is_discord_auto_thread_lane(const char *platform, const char *chat_type,
                                    int auto_thread_created, const char *thread_id,
                                    const char *auto_thread_initial_name);
/* PoP: w6_runtopics_d5aa88_is_relay_discord_channel_lane @ gateway/run_topics.py:_is_relay_discord_channel_lane */
int w6_runtopics_d5aa88_is_relay_discord_channel_lane(const char *platform, const char *chat_id,
                                      const char *thread_id, const char *chat_type,
                                      int delivered_via_upstream_relay);
/* PoP: w6_runtopics_d5aa88_topic_restore_steps @ gateway/run_topics.py:_topic_restore_steps */
struct json_t *w6_runtopics_d5aa88_topic_restore_steps(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_RUNTOPICS_D5AA88_H */
