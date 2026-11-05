/*
 * port_bot_loop_guard.h — pure coercion/settings helpers from
 * gateway/bot_loop_guard.py. The config read in load_settings is the unported
 * IO half; these take the raw values/block as parameters.
 */
#ifndef PORT_BOT_LOOP_GUARD_H
#define PORT_BOT_LOOP_GUARD_H

/* bot_loop_guard._as_bool(raw, default): True/False from bool/"true|1|yes|on"/
 * "false|0|no|off" words; anything else returns default. raw_json is a JSON
 * scalar (bool, string, number, null). Returns 0 or 1. */
int blg_as_bool(const char *raw_json, int default_val);

/* _as_positive(raw, default): a positive float, else default. Returns double. */
double blg_as_positive(const char *raw_json, double default_val);

/* _as_positive_int(raw, default): a whole positive int (>=1), else default. */
long long blg_as_positive_int(const char *raw_json, long long default_val);

/* settings_from_config(block): the bot_loop_guard block dict (JSON object or
 * null) -> {"enabled","max_events","window_seconds","cooldown_seconds"} with
 * the BotLoopGuardSettings defaults. Returns malloc'd JSON, caller frees. */
char *blg_settings_from_config(const char *block_json);

#endif /* PORT_BOT_LOOP_GUARD_H */
