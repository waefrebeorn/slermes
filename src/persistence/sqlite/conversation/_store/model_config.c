/*
 * model_config.c -- real C11 port of
 *   persistence/sqlite/conversation/_store/model_config.py.
 *
 * PoP: w22_modelconfi_0c8013_update_config @ persistence/sqlite/conversation/_store/model_config.py:update_conversation_model_config_value
 * PoP: w22_modelconfi_0c8013_json_extract @ persistence/sqlite/_schema_ddl.py:_sql_json_extract
 * PoP: w22_modelconfi_0c8013_select_ids_sql @ persistence/sqlite/conversation/_store/model_config.py:session_ids_with_model_config_key
 *
 * The transaction body's DB seam (StoreHolder, `_compression_tip_on`, the row
 * cursor) stays upstream; the decode/apply decision and the SQL text are pure
 * and live here.  `update` is modelled by its result: the fixture supplies the
 * value it returned, and Python's None means "remove the key".
 */
#include "hermes_json.h"
#include "noc_w22_modelconfi_0c8013.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() truthiness: a string of only whitespace is empty. */
static int mc08013_blank(const char *s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* _parse_model_config: JSON text or dict -> dict copy; anything else -> {}. */
static json_t *mc08013_parse_model_config(const char *raw) {
    json_t *parsed;
    if (raw && !mc08013_blank(raw)) {
        parsed = json_parse(raw, NULL);
        if (!parsed) return json_object();
        if (parsed->type == JSON_OBJECT) return parsed;
        json_free(parsed);
        return json_object();
    }
    return json_object();
}

json_t *w22_modelconfi_0c8013_update_config(const char *row_config,
                                            const char *key,
                                            const json_t *new_value) {
    const char *k = key ? key : "";
    json_t *config = mc08013_parse_model_config(row_config);
    if (!config) return NULL;
    if (!new_value || new_value->type == JSON_NULL) {
        if (json_obj_get(config, k)) json_obj_del(config, k);
    } else {
        json_t *copy = json_copy(new_value);
        if (!copy) {
            json_free(config);
            return NULL;
        }
        json_set(config, k, copy);
    }
    return config;
}

/* "'" + text.replace("'", "''") + "'" */
static char *mc08013_sql_literal(const char *text) {
    size_t n, extra = 0, i, j = 0;
    char *out;
    if (!text) text = "";
    n = strlen(text);
    for (i = 0; i < n; i++) if (text[i] == '\'') extra++;
    out = (char *)malloc(n + extra + 3);
    if (!out) return NULL;
    out[j++] = '\'';
    for (i = 0; i < n; i++) {
        out[j++] = text[i];
        if (text[i] == '\'') out[j++] = '\'';
    }
    out[j++] = '\'';
    out[j] = '\0';
    return out;
}

char *w22_modelconfi_0c8013_json_extract(const char *expression, const char *path) {
    const char *expr = expression ? expression : "";
    char *literal = mc08013_sql_literal(path ? path : "");
    const char *prefix = "json_extract((CASE WHEN json_valid(";
    const char *middle = ") THEN ";
    const char *suffix = " ELSE json_object() END), ";
    size_t len;
    char *out;
    if (!literal) return NULL;
    len = strlen(prefix) + strlen(expr) + strlen(middle) + strlen(expr) +
          strlen(suffix) + strlen(literal) + 2;
    out = (char *)malloc(len);
    if (!out) {
        free(literal);
        return NULL;
    }
    snprintf(out, len, "%s%s%s%s%s%s)", prefix, expr, middle, expr, suffix, literal);
    free(literal);
    return out;
}

char *w22_modelconfi_0c8013_select_ids_sql(const char *key) {
    char *path;
    char *extract;
    const char *head = "SELECT id FROM sessions WHERE ";
    const char *tail = " IS NOT NULL ORDER BY started_at DESC";
    size_t plen, len;
    char *out;
    const char *k = key ? key : "";
    plen = strlen(k) + 3;
    path = (char *)malloc(plen);
    if (!path) return NULL;
    snprintf(path, plen, "$.%s", k);
    extract = w22_modelconfi_0c8013_json_extract("model_config", path);
    free(path);
    if (!extract) return NULL;
    len = strlen(head) + strlen(extract) + strlen(tail) + 1;
    out = (char *)malloc(len);
    if (!out) {
        free(extract);
        return NULL;
    }
    snprintf(out, len, "%s%s%s", head, extract, tail);
    free(extract);
    return out;
}
