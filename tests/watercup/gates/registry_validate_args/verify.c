/* Verification gate for the centralized tool-input validation (P0).
 * Mirrors src/ports/tools/registry.c::validate_args_json EXACTLY (same source)
 * and asserts the behavioral contract:
 *   - valid args              -> returns NULL (dispatcher proceeds)
 *   - non-object args         -> structured error JSON (retryable:false)
 *   - wrong-type present arg  -> structured error JSON naming key/expected
 *   - missing required arg    -> structured error JSON naming the missing key
 * Compiled against the real libjson + hermes_tool_result.
 */
#include "hermes_json.h"
#include "hermes_tool_result.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char *validate_args_json(const char *name, const char *args_json,
                                const char *schema_json)
{
    if (!args_json || !schema_json) return NULL;

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args || args->type != JSON_OBJECT) {
        free(err);
        json_free(args);
        return tool_error("tool arguments must be a JSON object",
                          "tool", name ? name : "",
                          "retryable", "false",
                          "recovery_hint", "pass a JSON object {\"key\": value, ...} as the arguments",
                          NULL);
    }
    free(err);

    json_node_t *schema = json_parse(schema_json, &err);
    if (!schema || schema->type != JSON_OBJECT) {
        free(err);
        json_free(schema);
        json_free(args);
        return NULL;
    }
    free(err);

    json_node_t *params = json_object_get(schema, "parameters");
    json_node_t *properties = params ? json_object_get(params, "properties") : NULL;

    json_node_t *required = params ? json_object_get(params, "required") : NULL;
    if (required && required->type == JSON_ARRAY) {
        size_t rc = json_len(required);
        for (size_t i = 0; i < rc; i++) {
            json_node_t *rn = json_get(required, (int)i);
            if (!rn || rn->type != JSON_STRING) continue;
            const char *kptr = json_string_value_safe(rn);
            char keybuf[96];
            snprintf(keybuf, sizeof(keybuf), "%s", kptr ? kptr : "");
            if (!json_object_get(args, keybuf)) {
                json_free(schema);
                json_free(args);
                char hint[160];
                snprintf(hint, sizeof(hint),
                         "the '%s' argument is required for %s but was missing",
                         keybuf, name ? name : "this tool");
                return tool_error("missing required argument",
                                  "tool", name ? name : "",
                                  "missing", keybuf,
                                  "retryable", "false",
                                  "recovery_hint", hint,
                                  NULL);
            }
        }
    }

    if (properties && properties->type == JSON_OBJECT) {
        size_t pc = json_object_size(properties);
        for (size_t pi = 0; pi < pc; pi++) {
            const char *key = json_object_get_key_at(properties, pi);
            if (!key) continue;
            json_node_t *prop = json_object_get_at(properties, pi);
            if (!prop || prop->type != JSON_OBJECT) continue;
            json_node_t *arg_val = json_object_get(args, key);
            if (!arg_val) continue;  /* absent -> required check handles it */

            const char *type_str = json_get_str(prop, "type", "");
            const char *expected = type_str;
            char union_buf[64];
            json_node_t *type_node = json_object_get(prop, "type");
            if (type_node && type_node->type == JSON_ARRAY) {
                json_node_t *first = json_get(type_node, 0);
                const char *fst = (first && first->type == JSON_STRING)
                                      ? json_string_value_safe(first) : "";
                snprintf(union_buf, sizeof(union_buf), "%s", fst ? fst : "");
                expected = union_buf;
            }
            if (!expected[0]) continue;

            bool ok = true;
            if (strcmp(expected, "string") == 0) {
                ok = (arg_val->type == JSON_STRING);
            } else if (strcmp(expected, "integer") == 0 || strcmp(expected, "number") == 0) {
                if (arg_val->type == JSON_STRING) {
                    const char *sv = json_string_value_safe(arg_val);
                    char *end = NULL;
                    strtod(sv ? sv : "", &end);
                    ok = (end && *end == '\0' && end != sv);
                } else {
                    ok = (arg_val->type == JSON_NUMBER);
                }
            } else if (strcmp(expected, "boolean") == 0) {
                if (arg_val->type == JSON_STRING) {
                    const char *sv = json_string_value_safe(arg_val);
                    ok = (sv && (strcasecmp(sv, "true") == 0 ||
                                 strcasecmp(sv, "false") == 0 ||
                                 strcmp(sv, "1") == 0 ||
                                 strcmp(sv, "0") == 0));
                } else {
                    ok = (arg_val->type == JSON_BOOL);
                }
            } else if (strcmp(expected, "array") == 0) {
                if (arg_val->type == JSON_STRING) {
                    const char *sv = json_string_value_safe(arg_val);
                    char *ae = NULL;
                    json_node_t *p = sv ? json_parse(sv, &ae) : NULL;
                    free(ae);
                    ok = (p && p->type == JSON_ARRAY);
                    json_free(p);
                } else {
                    ok = (arg_val->type == JSON_ARRAY);
                }
            } else if (strcmp(expected, "object") == 0) {
                ok = (arg_val->type == JSON_OBJECT);
            }

            if (!ok) {
                char keybuf[96];
                snprintf(keybuf, sizeof(keybuf), "%s", key ? key : "");
                char expbuf[64];
                snprintf(expbuf, sizeof(expbuf), "%s", expected ? expected : "");
                json_free(schema);
                json_free(args);
                char hint[256];
                snprintf(hint, sizeof(hint),
                         "argument '%s' for %s must be of type %s; the provided value is not",
                         keybuf, name ? name : "this tool", expbuf);
                return tool_error("argument type mismatch",
                                  "tool", name ? name : "",
                                  "argument", keybuf,
                                  "expected", expbuf,
                                  "retryable", "false",
                                  "recovery_hint", hint,
                                  NULL);
            }
        }
    }

    json_free(schema);
    json_free(args);
    return NULL;
}

/* ---- test driver ---- */
static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

static const char *SCHEMA =
  "{\"parameters\":{\"properties\":{"
  "\"path\":{\"type\":\"string\"},"
  "\"limit\":{\"type\":\"integer\"},"
  "\"recursive\":{\"type\":\"boolean\"},"
  "\"tags\":{\"type\":\"array\"},"
  "\"meta\":{\"type\":\"object\"}"
  "},\"required\":[\"path\"]}}";

static const char *SCHEMA_UNION =
  "{\"parameters\":{\"properties\":{"
  "\"model\":{\"type\":[\"string\",\"null\"]}"
  "}}}";

int main(void) {
    printf("registry_validate_args verification\n");

    CHECK(validate_args_json("my_tool",
        "{\"path\":\"/tmp/x\",\"limit\":5,\"recursive\":true,\"tags\":[1,2],\"meta\":{\"a\":1}}",
        SCHEMA) == NULL, "valid args return NULL (dispatcher proceeds)");

    char *r2 = validate_args_json("my_tool", "[1,2,3]", SCHEMA);
    CHECK(r2 != NULL && strstr(r2, "\"error\"") && strstr(r2, "retryable"),
          "non-object args -> structured error with retryable=false");
    free(r2);

    char *r3 = validate_args_json("my_tool",
        "{\"path\":\"/tmp/x\",\"limit\":\"abc\"}", SCHEMA);
    CHECK(r3 != NULL && strstr(r3, "limit") && strstr(r3, "integer"),
          "non-numeric string for integer -> error naming 'limit'+'integer'");
    free(r3);

    char *r4 = validate_args_json("my_tool",
        "{\"path\":\"/tmp/x\",\"recursive\":42}", SCHEMA);
    CHECK(r4 != NULL && strstr(r4, "recursive"),
          "number where boolean expected -> error naming 'recursive'");
    free(r4);

    char *r5 = validate_args_json("my_tool", "{\"limit\":5}", SCHEMA);
    CHECK(r5 != NULL && strstr(r5, "missing") && strstr(r5, "path"),
          "missing required 'path' -> error naming it");
    free(r5);

    CHECK(validate_args_json("my_tool",
        "{\"path\":\"/tmp/x\",\"limit\":\"5\"}", SCHEMA) == NULL,
        "numeric string for integer tolerated (coercion runs after)");

    CHECK(validate_args_json("my_tool",
        "{\"model\":\"claude\"}", SCHEMA_UNION) == NULL,
        "union [string,null] accepts string");

    char *r8 = validate_args_json("my_tool", "{\"model\":42}", SCHEMA_UNION);
    CHECK(r8 != NULL && strstr(r8, "model"),
          "union [string,null] rejects integer -> error naming 'model'");
    free(r8);

    CHECK(validate_args_json("my_tool", "{\"x\":1}", NULL) == NULL,
          "NULL schema -> no validation (pass through)");

    if (failures) { printf("RESULT: %d FAILURES\n", failures); return 1; }
    printf("RESULT: ALL PASS\n");
    return 0;
}
