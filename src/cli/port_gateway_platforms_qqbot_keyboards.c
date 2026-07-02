/*
 * port_gateway_platforms_qqbot_keyboards.c — C port of gateway/platforms/qqbot/keyboards.py
 *
 * QQ Bot inline keyboards + approval / update-prompt senders.
 * QQ Bot v2 supports attaching inline keyboards to outbound messages.
 *
 * button_data formats:
 *   approve:<session_key>:<decision>   — decision = allow-once|allow-always|deny
 *   update_prompt:<answer>             — answer = y|n
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Button data prefixes */
static const char *APPROVAL_PREFIX = "approve:";
static const char *UPDATE_PROMPT_PREFIX = "update_prompt:";

/* Forward declarations for functions used before their definition */
json_node_t* cli_gateway_platforms_qqbot_keyboards__build_exec_text(json_node_t *req);
json_node_t* cli_gateway_platforms_qqbot_keyboards__build_plugin_text(json_node_t *req);

/* PoP: cli_gateway_platforms_qqbot_keyboards_parse_approval_button_data */
json_node_t* cli_gateway_platforms_qqbot_keyboards_parse_approval_button_data(const char *button_data) {
    /*
     * Parse approval button_data into (session_key, decision).
     * Returns a JSON object with session_key and decision, or NULL if not an approval button.
     * Pattern: approve:<session_key>:<decision>
     * session_key may itself contain colons, so the session_key group is greedy.
     */
    if (!button_data || !button_data[0]) return NULL;
    size_t len = strlen(button_data);
    if (len < 10) return NULL; /* Minimum: "approve:x:y" */
    /* Check prefix */
    if (strncmp(button_data, APPROVAL_PREFIX, strlen(APPROVAL_PREFIX)) != 0) {
        return NULL;
    }
    /* Find the last colon (decision separator) */
    const char *last_colon = strrchr(button_data, ':');
    if (!last_colon || last_colon == button_data) return NULL;
    /* Extract decision */
    const char *decision = last_colon + 1;
    if (strcmp(decision, "allow-once") != 0 &&
        strcmp(decision, "allow-always") != 0 &&
        strcmp(decision, "deny") != 0) {
        return NULL;
    }
    /* Extract session_key (between prefix and last colon) */
    const char *key_start = button_data + strlen(APPROVAL_PREFIX);
    size_t key_len = last_colon - key_start;
    if (key_len == 0 || key_len >= 1024) return NULL;
    char *session_key = (char*)malloc(key_len + 1);
    if (!session_key) return NULL;
    memcpy(session_key, key_start, key_len);
    session_key[key_len] = '\0';
    json_node_t *result = json_new_object();
    if (result) {
        json_object_set(result, "session_key", json_new_string(session_key));
        json_object_set(result, "decision", json_new_string(decision));
    }
    hermes_log(LOG_DEBUG, "qqbot_keyboards",
               "parse_approval_button_data: session=%.30s decision=%s", session_key, decision);
    free(session_key);
    return result;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data @ gateway/platforms/qqbot/keyboards.py:parse_update_prompt_button_data */
const char* cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data(const char *button_data) {
    /*
     * Parse update_prompt button_data into 'y' or 'n'.
     * Returns "y", "n", or NULL if not an update_prompt button.
     */
    if (!button_data || !button_data[0]) return NULL;
    if (strncmp(button_data, UPDATE_PROMPT_PREFIX, strlen(UPDATE_PROMPT_PREFIX)) != 0) {
        return NULL;
    }
    const char *answer = button_data + strlen(UPDATE_PROMPT_PREFIX);
    if (strcmp(answer, "y") == 0 || strcmp(answer, "n") == 0) {
        hermes_log(LOG_DEBUG, "qqbot_keyboards", "parse_update_prompt_button_data: %s", answer);
        return answer;
    }
    return NULL;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards__make_callback_button @ gateway/platforms/qqbot/keyboards.py:_make_callback_button */
json_node_t* cli_gateway_platforms_qqbot_keyboards__make_callback_button(const char *btn_id, const char *label,
                                                                          const char *visited_label, const char *data,
                                                                          int style, const char *group_id) {
    /*
     * Build a KeyboardButton JSON object with render data and callback action.
     */
    json_node_t *button = json_new_object();
    if (!button) return NULL;
    json_object_set(button, "id", json_new_string(btn_id ? btn_id : ""));
    /* Render data */
    json_node_t *render = json_new_object();
    if (render) {
        json_object_set(render, "label", json_new_string(label ? label : ""));
        json_object_set(render, "visited_label", json_new_string(visited_label ? visited_label : ""));
        json_object_set(render, "style", json_new_number(style));
        json_object_set(button, "render_data", render);
    }
    /* Action */
    json_node_t *action = json_new_object();
    if (action) {
        json_object_set(action, "type", json_new_number(1)); /* Callback */
        json_object_set(action, "data", json_new_string(data ? data : ""));
        json_node_t *perm = json_new_object();
        if (perm) {
            json_object_set(perm, "type", json_new_number(2)); /* All users */
            json_object_set(action, "permission", perm);
        }
        json_object_set(action, "click_limit", json_new_number(1));
        json_object_set(button, "action", action);
    }
    json_object_set(button, "group_id", json_new_string(group_id ? group_id : "default"));
    return button;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards_build_approval_keyboard @ gateway/platforms/qqbot/keyboards.py:build_approval_keyboard */
json_node_t* cli_gateway_platforms_qqbot_keyboards_build_approval_keyboard(const char *session_key) {
    /*
     * Build the 3-button approval keyboard.
     * Layout: [✅ 允许一次] [⭐ 始终允许] [❌ 拒绝]
     * All three share group_id='approval' so clicking one greys out the rest.
     */
    json_node_t *keyboard = json_new_object();
    if (!keyboard) return NULL;
    json_node_t *content = json_new_object();
    if (!content) return keyboard;
    json_node_t *rows = json_new_array();
    if (!rows) return keyboard;
    /* Row 1: three buttons */
    json_node_t *row1 = json_new_object();
    if (!row1) return keyboard;
    json_node_t *buttons = json_new_array();
    if (!buttons) return keyboard;
    /* ✅ 允许一次 (allow-once) */
    json_node_t *btn_allow = cli_gateway_platforms_qqbot_keyboards__make_callback_button(
        "allow", "✅ 允许一次", "已允许",
        NULL /* data built below */, 1, "approval");
    if (btn_allow) {
        /* Set data with session_key */
        json_node_t *action = json_object_get(btn_allow, "action");
        if (action) {
            char data_buf[512];
            snprintf(data_buf, sizeof(data_buf), "%s%s:allow-once",
                     APPROVAL_PREFIX, session_key ? session_key : "");
            json_object_set(action, "data", json_new_string(data_buf));
        }
        json_array_append(buttons, btn_allow);
    }
    /* ⭐ 始终允许 (allow-always) */
    json_node_t *btn_always = cli_gateway_platforms_qqbot_keyboards__make_callback_button(
        "always", "⭐ 始终允许", "已始终允许", "", 1, "approval");
    if (btn_always) {
        json_node_t *action = json_object_get(btn_always, "action");
        if (action) {
            char data_buf[512];
            snprintf(data_buf, sizeof(data_buf), "%s%s:allow-always",
                     APPROVAL_PREFIX, session_key ? session_key : "");
            json_object_set(action, "data", json_new_string(data_buf));
        }
        json_array_append(buttons, btn_always);
    }
    /* ❌ 拒绝 (deny) */
    json_node_t *btn_deny = cli_gateway_platforms_qqbot_keyboards__make_callback_button(
        "deny", "❌ 拒绝", "已拒绝", "", 0, "approval");
    if (btn_deny) {
        json_node_t *action = json_object_get(btn_deny, "action");
        if (action) {
            char data_buf[512];
            snprintf(data_buf, sizeof(data_buf), "%s%s:deny",
                     APPROVAL_PREFIX, session_key ? session_key : "");
            json_object_set(action, "data", json_new_string(data_buf));
        }
        json_array_append(buttons, btn_deny);
    }
    json_object_set(row1, "buttons", buttons);
    json_array_append(rows, row1);
    json_object_set(content, "rows", rows);
    json_object_set(keyboard, "content", content);
    hermes_log(LOG_INFO, "qqbot_keyboards",
               "build_approval_keyboard: session=%.20s...", session_key ? session_key : "");
    return keyboard;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards_build_update_prompt_keyboard @ gateway/platforms/qqbot/keyboards.py:build_update_prompt_keyboard */
json_node_t* cli_gateway_platforms_qqbot_keyboards_build_update_prompt_keyboard(void) {
    /*
     * Build a Yes/No keyboard for update confirmation prompts.
     * Layout: [✓ 确认] [✗ 取消]
     */
    json_node_t *keyboard = json_new_object();
    if (!keyboard) return NULL;
    json_node_t *content = json_new_object();
    if (!content) return keyboard;
    json_node_t *rows = json_new_array();
    if (!rows) return keyboard;
    json_node_t *row1 = json_new_object();
    if (!row1) return keyboard;
    json_node_t *buttons = json_new_array();
    if (!buttons) return keyboard;
    /* ✓ 确认 (yes) */
    char yes_data[64];
    snprintf(yes_data, sizeof(yes_data), "%sy", UPDATE_PROMPT_PREFIX);
    json_node_t *btn_yes = cli_gateway_platforms_qqbot_keyboards__make_callback_button(
        "yes", "✓ 确认", "已确认", yes_data, 1, "update_prompt");
    if (btn_yes) json_array_append(buttons, btn_yes);
    /* ✗ 取消 (no) */
    char no_data[64];
    snprintf(no_data, sizeof(no_data), "%sn", UPDATE_PROMPT_PREFIX);
    json_node_t *btn_no = cli_gateway_platforms_qqbot_keyboards__make_callback_button(
        "no", "✗ 取消", "已取消", no_data, 0, "update_prompt");
    if (btn_no) json_array_append(buttons, btn_no);
    json_object_set(row1, "buttons", buttons);
    json_array_append(rows, row1);
    json_object_set(content, "rows", rows);
    json_object_set(keyboard, "content", content);
    return keyboard;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards_build_approval_text @ gateway/platforms/qqbot/keyboards.py:build_approval_text */
json_node_t* cli_gateway_platforms_qqbot_keyboards_build_approval_text(json_node_t *req) {
    /*
     * Render an ApprovalRequest into the message body (markdown).
     * Returns a JSON object with text and is_exec fields.
     */
    if (!req || !json_node_is_object(req)) return NULL;
    json_node_t *cmd_preview = json_object_get(req, "command_preview");
    json_node_t *cwd_node = json_object_get(req, "cwd");
    if (cmd_preview || cwd_node) {
        return cli_gateway_platforms_qqbot_keyboards__build_exec_text(req);
    }
    return cli_gateway_platforms_qqbot_keyboards__build_plugin_text(req);
}

/* PoP: cli_gateway_platforms_qqbot_keyboards__build_exec_text @ gateway/platforms/qqbot/keyboards.py:_build_exec_text */
json_node_t* cli_gateway_platforms_qqbot_keyboards__build_exec_text(json_node_t *req) {
    /*
     * Build the approval message text for exec approvals.
     * Returns a JSON object with text field and is_exec=true.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    if (!req || !json_node_is_object(req)) {
        json_object_set(result, "text", json_new_string("🔐 **命令执行审批**"));
        json_object_set(result, "is_exec", json_new_bool(1));
        return result;
    }
    json_node_t *cmd = json_object_get(req, "command_preview");
    json_node_t *cwd_node = json_object_get(req, "cwd");
    json_node_t *title = json_object_get(req, "title");
    json_node_t *desc = json_object_get(req, "description");
    json_node_t *timeout = json_object_get(req, "timeout_sec");
    /* Build text lines */
    json_node_t *lines = json_new_array();
    if (!lines) return result;
    json_array_append(lines, json_new_string("🔐 **命令执行审批**"));
    json_array_append(lines, json_new_string(""));
    if (cmd && json_node_is_string(cmd)) {
        const char *cmd_str = json_node_get_string(cmd);
        size_t cmd_len = strlen(cmd_str);
        if (cmd_len > 300) cmd_len = 300;
        char code_buf[512];
        snprintf(code_buf, sizeof(code_buf), "```\\n%.*s\\n```", (int)cmd_len, cmd_str);
        json_array_append(lines, json_new_string(code_buf));
    }
    if (cwd_node && json_node_is_string(cwd_node)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "📁 目录: %s", json_node_get_string(cwd_node));
        json_array_append(lines, json_new_string(buf));
    }
    if (title && json_node_is_string(title)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "📋 %s", json_node_get_string(title));
        json_array_append(lines, json_new_string(buf));
    }
    if (desc && json_node_is_string(desc)) {
        const char *d = json_node_get_string(desc);
        if (d && d[0]) {
            char buf[256];
            snprintf(buf, sizeof(buf), "📝 %s", d);
            json_array_append(lines, json_new_string(buf));
        }
    }
    json_array_append(lines, json_new_string(""));
    int timeout_sec = 120;
    if (timeout && json_node_is_number(timeout)) {
        timeout_sec = json_node_get_int(timeout);
    }
    char timeout_buf[64];
    snprintf(timeout_buf, sizeof(timeout_buf), "⏱️ 超时: %d 秒", timeout_sec);
    json_array_append(lines, json_new_string(timeout_buf));
    json_object_set(result, "lines", lines);
    json_object_set(result, "is_exec", json_new_bool(1));
    return result;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards__build_plugin_text @ gateway/platforms/qqbot/keyboards.py:_build_plugin_text */
json_node_t* cli_gateway_platforms_qqbot_keyboards__build_plugin_text(json_node_t *req) {
    /*
     * Build the approval message text for plugin tool approvals.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    json_node_t *severity = json_object_get(req, "severity");
    const char *icon = "🟡";
    if (severity && json_node_is_string(severity)) {
        const char *sev = json_node_get_string(severity);
        if (strcmp(sev, "critical") == 0) icon = "🔴";
        else if (strcmp(sev, "info") == 0) icon = "🔵";
    }
    json_node_t *lines = json_new_array();
    if (!lines) return result;
    char header[64];
    snprintf(header, sizeof(header), "%s **审批请求**", icon);
    json_array_append(lines, json_new_string(header));
    json_array_append(lines, json_new_string(""));
    json_node_t *title = json_object_get(req, "title");
    if (title && json_node_is_string(title)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "📋 %s", json_node_get_string(title));
        json_array_append(lines, json_new_string(buf));
    }
    json_node_t *desc = json_object_get(req, "description");
    if (desc && json_node_is_string(desc)) {
        const char *d = json_node_get_string(desc);
        if (d && d[0]) {
            char buf[256];
            snprintf(buf, sizeof(buf), "📝 %s", d);
            json_array_append(lines, json_new_string(buf));
        }
    }
    json_node_t *tool = json_object_get(req, "tool_name");
    if (tool && json_node_is_string(tool)) {
        const char *t = json_node_get_string(tool);
        if (t && t[0]) {
            char buf[256];
            snprintf(buf, sizeof(buf), "🔧 工具: %s", t);
            json_array_append(lines, json_new_string(buf));
        }
    }
    json_array_append(lines, json_new_string(""));
    json_node_t *timeout = json_object_get(req, "timeout_sec");
    int timeout_sec = 120;
    if (timeout && json_node_is_number(timeout)) timeout_sec = json_node_get_int(timeout);
    char timeout_buf[64];
    snprintf(timeout_buf, sizeof(timeout_buf), "⏱️ 超时: %d 秒", timeout_sec);
    json_array_append(lines, json_new_string(timeout_buf));
    json_object_set(result, "lines", lines);
    json_object_set(result, "is_exec", json_new_bool(0));
    return result;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards_operator_openid @ gateway/platforms/qqbot/keyboards.py:operator_openid */
const char* cli_gateway_platforms_qqbot_keyboards_operator_openid(json_node_t *event) {
    /*
     * Best available operator openid (group -> member; c2c -> user).
     * Returns the operator's openid string or empty string.
     */
    if (!event || !json_node_is_object(event)) return "";
    const char *openid = "";
    /* Try group_member_openid first */
    json_node_t *gmid = json_object_get(event, "group_member_openid");
    if (gmid && json_node_is_string(gmid)) {
        openid = json_node_get_string(gmid);
        if (openid && openid[0]) return openid;
    }
    /* Try user_openid */
    json_node_t *uid = json_object_get(event, "user_openid");
    if (uid && json_node_is_string(uid)) {
        openid = json_node_get_string(uid);
        if (openid && openid[0]) return openid;
    }
    /* Try resolver_user_id */
    json_node_t *rid = json_object_get(event, "resolver_user_id");
    if (rid && json_node_is_string(rid)) {
        openid = json_node_get_string(rid);
    }
    return openid;
}

/* PoP: cli_gateway_platforms_qqbot_keyboards_parse_interaction_event @ gateway/platforms/qqbot/keyboards.py:parse_interaction_event */
json_node_t* cli_gateway_platforms_qqbot_keyboards_parse_interaction_event(json_node_t *raw) {
    /*
     * Parse a raw INTERACTION_CREATE dispatch payload (d field).
     * Returns a JSON object with all parsed event fields.
     */
    if (!raw || !json_node_is_object(raw)) return NULL;
    json_node_t *event = json_new_object();
    if (!event) return NULL;
    /* Top-level fields */
    json_node_t *id = json_object_get(raw, "id");
    json_object_set(event, "id", id ? id : json_new_string(""));
    json_node_t *chat_type = json_object_get(raw, "chat_type");
    int scene_code = chat_type && json_node_is_number(chat_type) ? json_node_get_int(chat_type) : 0;
    json_object_set(event, "chat_type", json_new_number(scene_code));
    const char *scene;
    switch (scene_code) {
        case 0: scene = "guild"; break;
        case 1: scene = "group"; break;
        case 2: scene = "c2c"; break;
        default: scene = ""; break;
    }
    json_object_set(event, "scene", json_new_string(scene));
    /* data.resolved fields */
    json_node_t *data = json_object_get(raw, "data");
    json_node_t *resolved = data && json_node_is_object(data) ? json_object_get(data, "resolved") : NULL;
    json_node_t *button_data = resolved && json_node_is_object(resolved) ? json_object_get(resolved, "button_data") : NULL;
    json_object_set(event, "button_data", button_data && json_node_is_string(button_data) ? button_data : json_new_string(""));
    json_node_t *button_id = resolved && json_node_is_object(resolved) ? json_object_get(resolved, "button_id") : NULL;
    json_object_set(event, "button_id", button_id && json_node_is_string(button_id) ? button_id : json_new_string(""));
    /* Other fields */
    json_node_t *goid = json_object_get(raw, "group_openid");
    json_object_set(event, "group_openid", goid && json_node_is_string(goid) ? goid : json_new_string(""));
    json_node_t *gmid = json_object_get(raw, "group_member_openid");
    json_object_set(event, "group_member_openid", gmid && json_node_is_string(gmid) ? gmid : json_new_string(""));
    json_node_t *uid = json_object_get(raw, "user_openid");
    json_object_set(event, "user_openid", uid && json_node_is_string(uid) ? uid : json_new_string(""));
    hermes_log(LOG_INFO, "qqbot_keyboards",
               "parse_interaction_event: scene=%s button=%s",
               scene, button_data && json_node_is_string(button_data) ? json_node_get_string(button_data) : "");
    return event;
}
