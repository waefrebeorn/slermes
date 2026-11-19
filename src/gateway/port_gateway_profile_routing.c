/*
 * port_gateway_profile_routing.c — pure discriminator helpers from
 * gateway/profile_routing.py.
 */

#include "port_gateway_profile_routing.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* PoP: pr_specificity @ gateway/profile_routing.py:specificity */
int pr_specificity(int guild, int chat, int thread, int user)
{
    return 2 * (guild != 0) + 4 * (chat != 0) + 8 * (thread != 0) + 16 * (user != 0);
}

/* PoP: pr_bot_profile_key @ gateway/profile_routing.py:_bot_profile_key */
char *pr_bot_profile_key(const char *name)
{
    if (!name) return NULL;
    size_t len = strlen(name), s = 0, e = len;
    while (s < e && isspace((unsigned char)name[s])) s++;
    while (e > s && isspace((unsigned char)name[e - 1])) e--;
    if (s == e || (e - s == 7 && strncmp(name + s, "default", 7) == 0))
        return NULL;
    char *o = dup_(name + s);
    if (o) o[e - s] = '\0'; /* bounded: strip() removes the trailing space too */
    return o;
}

/* PoP: pr_coerce_route_id @ gateway/profile_routing.py:_coerce_route_id */
char *pr_coerce_route_id(const json_t *value)
{
    if (!value || value->type == JSON_NULL)
        return NULL;
    if (value->type == JSON_STRING)
        return dup_(value->str_val ? value->str_val : "");
    if (value->type == JSON_BOOL)
        return dup_(value->bool_val ? "True" : "False");
    if (value->type == JSON_NUMBER) {
        char buf[40];
        if (value->num_val == (double)(long long)value->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)value->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", value->num_val);
        return dup_(buf);
    }
    return NULL;
}

/* PoP: pr_is_whatsapp_non_user_chat @ gateway/profile_routing.py:_is_whatsapp_non_user_chat */
int pr_is_whatsapp_non_user_chat(const char *chat_id)
{
    if (!chat_id || !chat_id[0])
        return 0;
    /* bool(chat_id) and str(chat_id).strip().lower().endswith(suffix) */
    size_t len = strlen(chat_id), s = 0, e = len;
    while (s < e && isspace((unsigned char)chat_id[s])) s++;
    while (e > s && isspace((unsigned char)chat_id[e - 1])) e--;
    if (s == e)
        return 0;
    char *low = (char *)malloc(e - s + 1);
    if (!low) return 0;
    for (size_t i = s; i < e; i++) low[i - s] = (char)tolower((unsigned char)chat_id[i]);
    low[e - s] = '\0';
    size_t llen = e - s;
    static const char *const SUFFIXES[] = { "@g.us", "@broadcast", "@newsletter" };
    int match = 0;
    for (size_t i = 0; i < sizeof(SUFFIXES) / sizeof(SUFFIXES[0]); i++) {
        size_t sl = strlen(SUFFIXES[i]);
        if (llen >= sl && strcmp(low + llen - sl, SUFFIXES[i]) == 0) {
            match = 1;
            break;
        }
    }
    free(low);
    return match;
}
