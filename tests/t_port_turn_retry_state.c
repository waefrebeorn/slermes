/* Oracle harness for agent/turn_retry_state.py port.
 * Reads fixture (argv[1]): {"ops":[{"op":"set"/"get"/"iter",
 *                                   "field":"codex_auth_retry_attempted", "val":true}]}
 * For "set": sets the named bool field. For "get": reads it. For "iter": emits
 * the (name,value) pairs. Emits a compact JSON array of per-op output:
 *   set -> null, get -> bool, iter -> [[name,bool],...]
 * Byte-diffed against the Python oracle.
 */
#include "agent/turn_retry_state.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

/* Map a field name to its getter; returns -1 if unknown. */
static int field_index(const char *name) {
    if (!strcmp(name, "codex_auth_retry_attempted")) return 0;
    if (!strcmp(name, "anthropic_auth_retry_attempted")) return 1;
    if (!strcmp(name, "nous_auth_retry_attempted")) return 2;
    if (!strcmp(name, "nous_paid_entitlement_refresh_attempted")) return 3;
    if (!strcmp(name, "copilot_auth_retry_attempted")) return 4;
    if (!strcmp(name, "vertex_auth_retry_attempted")) return 5;
    if (!strcmp(name, "thinking_sig_retry_attempted")) return 6;
    if (!strcmp(name, "invalid_encrypted_content_retry_attempted")) return 7;
    if (!strcmp(name, "image_shrink_retry_attempted")) return 8;
    if (!strcmp(name, "multimodal_tool_content_retry_attempted")) return 9;
    if (!strcmp(name, "oauth_1m_beta_retry_attempted")) return 10;
    if (!strcmp(name, "llama_cpp_grammar_retry_attempted")) return 11;
    if (!strcmp(name, "primary_recovery_attempted")) return 12;
    if (!strcmp(name, "has_retried_429")) return 13;
    if (!strcmp(name, "auth_failover_attempted")) return 14;
    if (!strcmp(name, "restart_with_compressed_messages")) return 15;
    if (!strcmp(name, "restart_with_length_continuation")) return 16;
    if (!strcmp(name, "restart_with_rebuilt_messages")) return 17;
    return -1;
}

static bool get_field(turn_retry_state_t *s, int idx) {
    switch (idx) {
        case 0: return turn_retry_state_get_codex_auth_retry_attempted(s);
        case 1: return turn_retry_state_get_anthropic_auth_retry_attempted(s);
        case 2: return turn_retry_state_get_nous_auth_retry_attempted(s);
        case 3: return turn_retry_state_get_nous_paid_entitlement_refresh_attempted(s);
        case 4: return turn_retry_state_get_copilot_auth_retry_attempted(s);
        case 5: return turn_retry_state_get_vertex_auth_retry_attempted(s);
        case 6: return turn_retry_state_get_thinking_sig_retry_attempted(s);
        case 7: return turn_retry_state_get_invalid_encrypted_content_retry_attempted(s);
        case 8: return turn_retry_state_get_image_shrink_retry_attempted(s);
        case 9: return turn_retry_state_get_multimodal_tool_content_retry_attempted(s);
        case 10: return turn_retry_state_get_oauth_1m_beta_retry_attempted(s);
        case 11: return turn_retry_state_get_llama_cpp_grammar_retry_attempted(s);
        case 12: return turn_retry_state_get_primary_recovery_attempted(s);
        case 13: return turn_retry_state_get_has_retried_429(s);
        case 14: return turn_retry_state_get_auth_failover_attempted(s);
        case 15: return turn_retry_state_get_restart_with_compressed_messages(s);
        case 16: return turn_retry_state_get_restart_with_length_continuation(s);
        case 17: return turn_retry_state_get_restart_with_rebuilt_messages(s);
    }
    return false;
}

static void set_field(turn_retry_state_t *s, int idx, bool v) {
    switch (idx) {
        case 0: turn_retry_state_set_codex_auth_retry_attempted(s, v); break;
        case 1: turn_retry_state_set_anthropic_auth_retry_attempted(s, v); break;
        case 2: turn_retry_state_set_nous_auth_retry_attempted(s, v); break;
        case 3: turn_retry_state_set_nous_paid_entitlement_refresh_attempted(s, v); break;
        case 4: turn_retry_state_set_copilot_auth_retry_attempted(s, v); break;
        case 5: turn_retry_state_set_vertex_auth_retry_attempted(s, v); break;
        case 6: turn_retry_state_set_thinking_sig_retry_attempted(s, v); break;
        case 7: turn_retry_state_set_invalid_encrypted_content_retry_attempted(s, v); break;
        case 8: turn_retry_state_set_image_shrink_retry_attempted(s, v); break;
        case 9: turn_retry_state_set_multimodal_tool_content_retry_attempted(s, v); break;
        case 10: turn_retry_state_set_oauth_1m_beta_retry_attempted(s, v); break;
        case 11: turn_retry_state_set_llama_cpp_grammar_retry_attempted(s, v); break;
        case 12: turn_retry_state_set_primary_recovery_attempted(s, v); break;
        case 13: turn_retry_state_set_has_retried_429(s, v); break;
        case 14: turn_retry_state_set_auth_failover_attempted(s, v); break;
        case 15: turn_retry_state_set_restart_with_compressed_messages(s, v); break;
        case 16: turn_retry_state_set_restart_with_length_continuation(s, v); break;
        case 17: turn_retry_state_set_restart_with_rebuilt_messages(s, v); break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *src = read_file(argv[1]);
    if (!src) return 1;
    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    turn_retry_state_t *s = turn_retry_state_create();
    json_t *results = json_new_array();

    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < ops->c.count; i++) {
            json_t *o = ops->c.items[i];
            const char *op = json_string_value(json_object_get(o, "op"));
            const char *field = json_string_value(json_object_get(o, "field"));
            int idx = field ? field_index(field) : -1;
            if (strcmp(op, "set") == 0) {
                bool v = json_is_true(json_object_get(o, "val"));
                if (idx >= 0) set_field(s, idx, v);
                json_array_append(results, json_new_null());
            } else if (strcmp(op, "get") == 0) {
                json_array_append(results, json_new_bool(idx >= 0 ? get_field(s, idx) : false));
            } else if (strcmp(op, "iter") == 0) {
                size_t n = 0;
                turn_retry_field_t *arr = turn_retry_state_fields(s, &n);
                json_t *pairs = json_new_array();
                for (size_t k = 0; k < n; k++) {
                    json_t *pair = json_new_array();
                    json_array_append(pair, json_new_string(arr[k].name));
                    json_array_append(pair, json_new_bool(arr[k].value));
                    json_array_append(pairs, pair);
                }
                json_array_append(results, pairs);
                turn_retry_state_fields_free(arr);
            }
        }
    }

    char *out = json_serialize(results);
    printf("%s", out ? out : "[]");
    free(out);
    json_free(results);
    turn_retry_state_free(s);
    json_free(root);
    free(src);
    return 0;
}
