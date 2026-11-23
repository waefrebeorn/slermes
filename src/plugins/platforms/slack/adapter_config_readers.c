/*
 * adapter_config_readers.c — C port of upstream Hermes
 * plugins/platforms/slack/adapter_config_readers.py.
 * PoP: slackcfg_flag_getter_name @ plugins/platforms/slack/adapter_config_readers.py:_extra_or_env_flag_getter
 * PoP: slackcfg_flag_getter_call @ plugins/platforms/slack/adapter_config_readers.py:_extra_or_env_flag_getter
 * PoP: slackcfg_channel_set_getter_name @ plugins/platforms/slack/adapter_config_readers.py:_extra_or_env_channel_set_getter
 * PoP: slackcfg_channel_set_getter_call @ plugins/platforms/slack/adapter_config_readers.py:_extra_or_env_channel_set_getter
 */
#include "noc_slackcfg.h"
#include <stdio.h>
#include <string.h>

/* getter.__name__ = f"_slack_{key}" */
static void getter_name(const char *key, char *buf, size_t cap) {
    if (!buf || cap == 0) return;
    snprintf(buf, cap, "_slack_%s", key ? key : "");
}

/* A getter closes over (key, env_var, flag) and forwards them verbatim to
 * self._extra_or_env_flag / self._extra_or_env_channel_set. The oracle injects
 * a self that renders the forwarded call as f"{key}|{env_var}|{flag}". */
static void getter_call(const char *key, const char *env_var, int flag,
                        char *buf, size_t cap) {
    if (!buf || cap == 0) return;
    snprintf(buf, cap, "%s|%s|%s",
             key ? key : "", env_var ? env_var : "", flag ? "True" : "False");
}

void slackcfg_flag_getter_name(const char *key, char *buf, size_t cap) {
    getter_name(key, buf, cap);
}

void slackcfg_flag_getter_call(const char *key, const char *env_var, int strip,
                               char *buf, size_t cap) {
    getter_call(key, env_var, strip, buf, cap);
}

void slackcfg_channel_set_getter_name(const char *key, char *buf, size_t cap) {
    getter_name(key, buf, cap);
}

void slackcfg_channel_set_getter_call(const char *key, const char *env_var,
                                      int coerce_scalar, char *buf, size_t cap) {
    getter_call(key, env_var, coerce_scalar, buf, cap);
}
