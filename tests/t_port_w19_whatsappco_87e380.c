/* tests/t_port_w19_whatsappco_87e380.c - JSON-lines harness for gateway/platforms/whatsapp_common.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_whatsappco_87e380.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A key that is absent or JSON null reads as an unset carrier (NULL). */
static const char *opt_str(json_t *in, const char *key) {
    json_t *v = in ? json_obj_get(in, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "header_to_bold")) {
            out = w19_whatsappco_87e380_header_to_bold(json_get_str(in, "inner", ""));
        } else if (!strcmp(func, "sanitize_outbound_text")) {
            out = w19_whatsappco_87e380_sanitize_outbound_text(json_get_str(in, "content", ""));
        } else if (!strcmp(func, "effective_reply_prefix")) {
            out = w19_whatsappco_87e380_effective_reply_prefix(
                opt_str(in, "mode"), opt_str(in, "reply_prefix"), opt_str(in, "env_prefix"));
        } else if (!strcmp(func, "outgoing_chunk_limit")) {
            out = w19_whatsappco_87e380_outgoing_chunk_limit((int)json_get_num(in, "prefix_len", 0));
        } else if (!strcmp(func, "normalize_whatsapp_id")) {
            out = w19_whatsappco_87e380_normalize_whatsapp_id(opt_str(in, "value"));
        } else if (!strcmp(func, "is_broadcast_chat")) {
            out = json_bool(w19_whatsappco_87e380_is_broadcast_chat(opt_str(in, "chat_id")));
        } else if (!strcmp(func, "coerce_allow_list")) {
            out = w19_whatsappco_87e380_coerce_allow_list(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "select_allowlist")) {
            out = w19_whatsappco_87e380_select_allowlist(json_obj_get(in, "extra"),
                                                         json_obj_get(in, "config_keys"),
                                                         json_obj_get(in, "env_keys"),
                                                         json_obj_get(in, "env"));
        } else if (!strcmp(func, "bot_ids_from_message")) {
            out = w19_whatsappco_87e380_bot_ids_from_message(json_obj_get(in, "data"));
        } else if (!strcmp(func, "message_is_reply_to_bot")) {
            out = json_bool(w19_whatsappco_87e380_message_is_reply_to_bot(json_obj_get(in, "data")));
        } else if (!strcmp(func, "message_mentions_bot")) {
            out = json_bool(w19_whatsappco_87e380_message_mentions_bot(json_obj_get(in, "data")));
        } else if (!strcmp(func, "clean_bot_mention_text")) {
            out = w19_whatsappco_87e380_clean_bot_mention_text(json_get_str(in, "text", ""),
                                                               json_obj_get(in, "data"));
        } else if (!strcmp(func, "format_message")) {
            out = w19_whatsappco_87e380_format_message(json_get_str(in, "content", ""));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
