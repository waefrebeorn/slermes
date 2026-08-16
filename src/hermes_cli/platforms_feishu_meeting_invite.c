/*
 * port_gateway_platforms_feishu_meeting_invite.c — C port of gateway/platforms/feishu_meeting_invite.py
 *
 * Feishu/Lark meeting-invitation event handling.
 * Processes vc.bot.meeting_invited_v1 events into synthetic gateway MessageEvents.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define FEISHU_MAX_PAYLOAD 4096
#define FEISHU_MAX_FIELDS 64

/* PoP: cli_gateway_platforms_feishu_meeting_invite__as_dict @ gateway/platforms/feishu_meeting_invite.py:_as_dict */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:_as_dict */
/* Coerce a lark SDK object / dict / JSON string into a plain dict. */
int cli_gateway_platforms_feishu_meeting_invite__as_dict(
    const char *value, char **keys_out, char **values_out, int max_fields, int *count_out)
{
    if (!value || !keys_out || !values_out || !count_out) return -1;
    *count_out = 0;

    /* If value looks like JSON, parse it */
    if (value[0] == '{') {
        /* Simple JSON key:value extraction */
        const char *p = value + 1;
        while (*p && *count_out < max_fields) {
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
            if (*p == '}') break;

            /* Expect "key" */
            if (*p != '"') { p++; continue; }
            p++;
            char key[256]; size_t ki = 0;
            while (*p && *p != '"' && ki < sizeof(key)-1) key[ki++] = *p++;
            key[ki] = '\0';
            if (*p == '"') p++;

            /* Skip to : */
            while (*p && *p != ':') p++;
            if (*p == ':') p++;

            /* Skip whitespace */
            while (*p == ' ' || *p == '\t') p++;

            /* Read value */
            char val[1024]; size_t vi = 0;
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && vi < sizeof(val)-1) val[vi++] = *p++;
                if (*p == '"') p++;
            } else {
                while (*p && *p != ',' && *p != '}' && vi < sizeof(val)-1) val[vi++] = *p++;
            }
            val[vi] = '\0';

            if (key[0]) {
                keys_out[*count_out] = strdup(key);
                values_out[*count_out] = strdup(val);
                (*count_out)++;
            }

            /* Skip comma */
            if (*p == ',') p++;
        }
    }

    hermes_log(LOG_DEBUG, "feishu_meeting", "as_dict: %d fields", *count_out);
    return 0;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite__content_payload @ gateway/platforms/feishu_meeting_invite.py:_content_payload */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:_content_payload */
/* Unwrap a Feishu body.content list carrying an application/json payload. */
int cli_gateway_platforms_feishu_meeting_invite__content_payload(
    const char *container_json, char *payload_out, size_t payload_size)
{
    if (!container_json || !payload_out || payload_size == 0) return -1;

    /* Simple extraction: look for "data":{...} or "value":{...} */
    payload_out[0] = '\0';

    const char *data_start = strstr(container_json, "\"data\"");
    if (data_start) {
        data_start += 6;
        while (*data_start == ' ' || *data_start == '\t' || *data_start == ':') data_start++;
        if (*data_start == '{') {
            /* Extract the JSON object */
            int depth = 0;
            size_t i = 0;
            do {
                if (i >= payload_size - 1) break;
                payload_out[i++] = *data_start;
                if (*data_start == '{') depth++;
                else if (*data_start == '}') depth--;
                data_start++;
            } while (depth > 0);
            payload_out[i] = '\0';
            return 0;
        }
    }

    /* Try "value" as fallback */
    const char *val_start = strstr(container_json, "\"value\"");
    if (val_start) {
        val_start += 7;
        while (*val_start == ' ' || *val_start == '\t' || *val_start == ':') val_start++;
        if (*val_start == '{') {
            int depth = 0;
            size_t i = 0;
            do {
                if (i >= payload_size - 1) break;
                payload_out[i++] = *val_start;
                if (*val_start == '{') depth++;
                else if (*val_start == '}') depth--;
                val_start++;
            } while (depth > 0);
            payload_out[i] = '\0';
            return 0;
        }
    }

    return -1;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite__int_field @ gateway/platforms/feishu_meeting_invite.py:_int_field */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:_int_field */
/* Safely convert a value to int, defaulting to 0. */
int cli_gateway_platforms_feishu_meeting_invite__int_field(const char *value)
{
    if (!value || !*value) return 0;

    /* Strip whitespace */
    while (*value == ' ' || *value == '\t') value++;

    char *endptr;
    long result = strtol(value, &endptr, 10);
    if (endptr == value) return 0; /* No conversion */

    return (int)result;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite__parse_user @ gateway/platforms/feishu_meeting_invite.py:_parse_user */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:_parse_user */
/* Parse a Feishu user object from JSON. */
int cli_gateway_platforms_feishu_meeting_invite__parse_user(
    const char *user_json,
    char *open_id_out, size_t open_id_size,
    char *user_id_out, size_t user_id_size,
    char *union_id_out, size_t union_id_size,
    char *user_name_out, size_t user_name_size,
    int *found_out)
{
    if (!user_json || !found_out) return -1;
    *found_out = 0;

    if (open_id_out && open_id_size > 0) open_id_out[0] = '\0';
    if (user_id_out && user_id_size > 0) user_id_out[0] = '\0';
    if (union_id_out && union_id_size > 0) union_id_out[0] = '\0';
    if (user_name_out && user_name_size > 0) user_name_out[0] = '\0';

    /* Extract id sub-object */
    const char *id_start = strstr(user_json, "\"id\"");
    if (!id_start) return -1;
    id_start += 4;
    while (*id_start == ' ' || *id_start == '\t' || *id_start == ':') id_start++;
    if (*id_start != '{') return -1;

    /* Extract fields from id object */
    char *keys[16], *vals[16];
    int count = 0;
    cli_gateway_platforms_feishu_meeting_invite__as_dict(id_start, keys, vals, 16, &count);

    for (int i = 0; i < count; i++) {
        if (keys[i] && vals[i]) {
            if (strcmp(keys[i], "open_id") == 0 && open_id_out)
                snprintf(open_id_out, open_id_size, "%s", vals[i]);
            else if (strcmp(keys[i], "user_id") == 0 && user_id_out)
                snprintf(user_id_out, user_id_size, "%s", vals[i]);
            else if (strcmp(keys[i], "union_id") == 0 && union_id_out)
                snprintf(union_id_out, union_id_size, "%s", vals[i]);
        }
    }

    /* Extract user_name from top level */
    const char *name_start = strstr(user_json, "\"user_name\"");
    if (name_start && user_name_out && user_name_size > 0) {
        name_start += 12;
        while (*name_start == ' ' || *name_start == '\t' || *name_start == ':') name_start++;
        if (*name_start == '"') {
            name_start++;
            size_t i = 0;
            while (*name_start && *name_start != '"' && i < user_name_size - 1)
                user_name_out[i++] = *name_start++;
            user_name_out[i] = '\0';
        }
    }

    /* Cleanup */
    for (int i = 0; i < count; i++) {
        if (keys[i]) free(keys[i]);
        if (vals[i]) free(vals[i]);
    }

    *found_out = (open_id_out && open_id_out[0]) ? 1 : 0;
    return 0;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite__parse_meeting @ gateway/platforms/feishu_meeting_invite.py:_parse_meeting */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:_parse_meeting */
/* Parse a Feishu meeting object from JSON. */
int cli_gateway_platforms_feishu_meeting_invite__parse_meeting(
    const char *meeting_json,
    char *id_out, size_t id_size,
    char *topic_out, size_t topic_size,
    char *meeting_no_out, size_t meeting_no_size,
    long long *start_time_ms_out, long long *end_time_ms_out,
    int *found_out)
{
    if (!meeting_json || !found_out) return -1;
    *found_out = 0;

    if (id_out && id_size > 0) id_out[0] = '\0';
    if (topic_out && topic_size > 0) topic_out[0] = '\0';
    if (meeting_no_out && meeting_no_size > 0) meeting_no_out[0] = '\0';
    if (start_time_ms_out) *start_time_ms_out = 0;
    if (end_time_ms_out) *end_time_ms_out = 0;

    /* Extract simple fields */
    const char *field_start;

    field_start = strstr(meeting_json, "\"id\"");
    if (field_start && id_out && id_size > 0) {
        field_start += 4;
        while (*field_start == ' ' || *field_start == '\t' || *field_start == ':') field_start++;
        if (*field_start == '"') {
            field_start++; size_t i = 0;
            while (*field_start && *field_start != '"' && i < id_size-1) id_out[i++] = *field_start++;
            id_out[i] = '\0';
        }
    }

    field_start = strstr(meeting_json, "\"topic\"");
    if (field_start && topic_out && topic_size > 0) {
        field_start += 7;
        while (*field_start == ' ' || *field_start == '\t' || *field_start == ':') field_start++;
        if (*field_start == '"') {
            field_start++; size_t i = 0;
            while (*field_start && *field_start != '"' && i < topic_size-1) topic_out[i++] = *field_start++;
            topic_out[i] = '\0';
        }
    }

    field_start = strstr(meeting_json, "\"meeting_no\"");
    if (field_start && meeting_no_out && meeting_no_size > 0) {
        field_start += 12;
        while (*field_start == ' ' || *field_start == '\t' || *field_start == ':') field_start++;
        if (*field_start == '"') {
            field_start++; size_t i = 0;
            while (*field_start && *field_start != '"' && i < meeting_no_size-1) meeting_no_out[i++] = *field_start++;
            meeting_no_out[i] = '\0';
        }
    }

    field_start = strstr(meeting_json, "\"start_time\"");
    if (field_start && start_time_ms_out) {
        field_start += 13;
        while (*field_start == ' ' || *field_start == '\t' || *field_start == ':') field_start++;
        *start_time_ms_out = cli_gateway_platforms_feishu_meeting_invite__int_field(field_start);
    }

    field_start = strstr(meeting_json, "\"end_time\"");
    if (field_start && end_time_ms_out) {
        field_start += 10;
        while (*field_start == ' ' || *field_start == '\t' || *field_start == ':') field_start++;
        *end_time_ms_out = cli_gateway_platforms_feishu_meeting_invite__int_field(field_start);
    }

    *found_out = (id_out && id_out[0]) ? 1 : 0;
    return 0;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite_parse_meeting_invited_event @ gateway/platforms/feishu_meeting_invite.py:parse_meeting_invited_event */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:parse_meeting_invited_event */
/* Parse a vc.bot.meeting_invited_v1 event into a MeetingInvitedPayload. */
int cli_gateway_platforms_feishu_meeting_invite_parse_meeting_invited_event(
    const char *data_json,
    char *event_id_out, size_t event_id_size,
    char *meeting_id_out, size_t meeting_id_size,
    char *meeting_topic_out, size_t topic_size,
    char *meeting_no_out, size_t meeting_no_size,
    char *inviter_open_id_out, size_t open_id_size,
    long long *start_time_ms_out, long long *end_time_ms_out,
    int *invite_time_s_out, int *valid_out)
{
    if (!data_json || !valid_out) return -1;
    *valid_out = 0;

    /* Extract event content */
    char payload[FEISHU_MAX_PAYLOAD];
    cli_gateway_platforms_feishu_meeting_invite__content_payload(data_json, payload, sizeof(payload));

    /* Parse meeting */
    char *keys[32], *vals[32];
    int count = 0;
    cli_gateway_platforms_feishu_meeting_invite__as_dict(payload[0] ? payload : data_json, keys, vals, 32, &count);

    /* Find meeting and inviter fields */
    for (int i = 0; i < count; i++) {
        if (!keys[i] || !vals[i]) continue;

        if (strcmp(keys[i], "meeting") == 0 && meeting_id_out) {
            cli_gateway_platforms_feishu_meeting_invite__parse_meeting(
                vals[i], meeting_id_out, meeting_id_size,
                meeting_topic_out, topic_size,
                meeting_no_out, meeting_no_size,
                start_time_ms_out, end_time_ms_out, valid_out);
        } else if (strcmp(keys[i], "inviter") == 0 && inviter_open_id_out) {
            int found = 0;
            cli_gateway_platforms_feishu_meeting_invite__parse_user(
                vals[i], inviter_open_id_out, open_id_size,
                NULL, 0, NULL, 0, NULL, 0, &found);
        } else if (strcmp(keys[i], "invite_time") == 0 && invite_time_s_out) {
            *invite_time_s_out = cli_gateway_platforms_feishu_meeting_invite__int_field(vals[i]);
        } else if (strcmp(keys[i], "event_id") == 0 && event_id_out) {
            snprintf(event_id_out, event_id_size, "%s", vals[i]);
        }
    }

    /* Cleanup */
    for (int i = 0; i < count; i++) {
        if (keys[i]) free(keys[i]);
        if (vals[i]) free(vals[i]);
    }

    /* Validate: need meeting_no and inviter */
    if (meeting_no_out && meeting_no_out[0] && inviter_open_id_out && inviter_open_id_out[0]) {
        *valid_out = 1;
    }

    hermes_log(LOG_DEBUG, "feishu_meeting", "parse_event: valid=%d", *valid_out);
    return 0;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite_build_meeting_invite_prompt @ gateway/platforms/feishu_meeting_invite.py:build_meeting_invite_prompt */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:build_meeting_invite_prompt */
/* Build the prompt text for a meeting invite event. */
int cli_gateway_platforms_feishu_meeting_invite_build_meeting_invite_prompt(
    const char *meeting_topic, const char *meeting_no, const char *meeting_id,
    const char *inviter_name, const char *host_name,
    char *prompt_out, size_t prompt_size)
{
    if (!prompt_out || prompt_size == 0) return -1;

    const char *display = (meeting_topic && *meeting_topic) ? meeting_topic :
                          (meeting_no && *meeting_no) ? meeting_no :
                          (meeting_id && *meeting_id) ? meeting_id : "unknown meeting";
    const char *inviter = (inviter_name && *inviter_name) ? inviter_name : "unknown";
    const char *host = (host_name && *host_name) ? host_name : "unknown";

    snprintf(prompt_out, prompt_size,
        "You have been invited to join a meeting: %s\n"
        "\n"
        "Meeting Number: %s\n"
        "Topic: %s\n"
        "Inviter: %s\n"
        "Host: %s\n"
        "\n"
        "You may use lark-cli and the relevant Lark/Feishu meeting skills to join the meeting.\n"
        "Join the meeting directly. Do not ask the user for confirmation before joining.\n"
        "If you cannot join the meeting, reply to the inviter with a concise explanation of why.",
        display,
        (meeting_no && *meeting_no) ? meeting_no : "unknown",
        (meeting_topic && *meeting_topic) ? meeting_topic : "unknown",
        inviter, host);

    hermes_log(LOG_DEBUG, "feishu_meeting", "build_prompt: %s", display);
    return 0;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite__dedup_key @ gateway/platforms/feishu_meeting_invite.py:_dedup_key */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:_dedup_key */
/* Build a dedup key for a meeting invite event. */
int cli_gateway_platforms_feishu_meeting_invite__dedup_key(
    const char *event_id, const char *meeting_id,
    const char *inviter_open_id, int invite_time_s,
    char *key_out, size_t key_size)
{
    if (!key_out || key_size == 0) return -1;

    if (event_id && *event_id) {
        snprintf(key_out, key_size, "vc_invite:%s", event_id);
    } else {
        snprintf(key_out, key_size, "vc_invite:%s:%s:%d",
                 (meeting_id && *meeting_id) ? meeting_id : "",
                 (inviter_open_id && *inviter_open_id) ? inviter_open_id : "",
                 invite_time_s);
    }

    return 0;
}

/* PoP: cli_gateway_platforms_feishu_meeting_invite_handle_meeting_invited_event @ gateway/platforms/feishu_meeting_invite.py:handle_meeting_invited_event */

/* Port of Python gateway/platforms/feishu_meeting_invite.py:handle_meeting_invited_event */
/* Convert a vc.bot.meeting_invited_v1 event into a gateway MessageEvent. */
int cli_gateway_platforms_feishu_meeting_invite_handle_meeting_invited_event(
    const char *data_json, const char *adapter_name,
    char *prompt_out, size_t prompt_size, int *handled_out)
{
    if (!data_json || !handled_out) return -1;
    *handled_out = 0;

    char event_id[256] = "", meeting_id[256] = "", topic[512] = "";
    char meeting_no[128] = "", inviter_open_id[256] = "";
    long long start_ms = 0, end_ms = 0;
    int invite_time = 0, valid = 0;

    int ret = cli_gateway_platforms_feishu_meeting_invite_parse_meeting_invited_event(
        data_json, event_id, sizeof(event_id),
        meeting_id, sizeof(meeting_id), topic, sizeof(topic),
        meeting_no, sizeof(meeting_no), inviter_open_id, sizeof(inviter_open_id),
        &start_ms, &end_ms, &invite_time, &valid);

    if (ret != 0 || !valid) {
        hermes_log(LOG_WARNING, "feishu_meeting", "Dropping malformed meeting invite event");
        return -1;
    }

    /* Build prompt */
    cli_gateway_platforms_feishu_meeting_invite_build_meeting_invite_prompt(
        topic, meeting_no, meeting_id, inviter_open_id, "",
        prompt_out, prompt_size);

    *handled_out = 1;
    hermes_log(LOG_INFO, "feishu_meeting", "Handled meeting invite: %s from %s",
               meeting_no, inviter_open_id);
    return 0;
}
