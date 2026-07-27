/**
 * @defgroup json JSON Library
 * @brief JSON parser, builder, and serialization.
 *
 * Convenience macros and shim layer over libjson. Supports
 * object/array/string/number/bool/null types, deep copy,
 * and printf-style serialization.
 *
 * @{
 */
#ifndef HERMES_JSON_H
#define HERMES_JSON_H

#include <string.h>
#include <stdlib.h>

/*
 * hermes_json.h — Compatibility shim: maps old hermes JSON API → libjson.
 *
 * Old API used json_node_t / json_new_* / json_object_* etc.
 * New libjson uses json_t / json_* / json_obj_* etc.
 * This header provides backward-compatible typedefs + macros.
 */

#include "../lib/libjson/json.h"

/* Type alias: old json_node_t → new json_t */
typedef json_t json_node_t;

/* Builder macros: old json_new_* → new short names */
#define json_new_null()         json_null()
#define json_new_bool(v)        json_bool(v)
#define json_new_number(v)      json_number(v)
#define json_new_string(v)      json_string(v)
#define json_new_array()        json_array()
#define json_new_object()       json_object()

/* Array ops: old json_array_* → new json_* */
#define json_array_append(a,i)  json_append((a),(i))
#define json_array_get(a,i)     json_get((a),(i))
#define json_array_count(a)     json_len((a))

/* Object ops: old json_object_* → new json_obj_* */
#define json_object_set(o,k,v)  json_set((o),(k),(v))
#define json_object_get(o,k)    json_obj_get((o),(k))
#define json_object_get_string(o,k,d) json_get_str((o),(k),(d))
#define json_object_get_number(o,k,d) json_get_num((o),(k),(d))
#define json_object_get_bool(o,k,d)   json_get_bool((o),(k),(d))

/* Type checking macros for backward compatibility */
#define json_node_is_null(v)       ((v) && (v)->type == JSON_NULL)
#define json_node_is_bool(v)       ((v) && (v)->type == JSON_BOOL)
#define json_node_is_number(v)     ((v) && (v)->type == JSON_NUMBER)
#define json_node_is_string(v)     ((v) && (v)->type == JSON_STRING)
#define json_node_is_array(v)      ((v) && (v)->type == JSON_ARRAY)
#define json_node_is_object(v)     ((v) && (v)->type == JSON_OBJECT)

/* Value access macros */
#define json_node_get_string(v)    ((v) ? (v)->str_val : NULL)
#define json_node_get_bool(v)      ((v) ? (v)->bool_val : false)
#define json_node_get_int(v)       ((v) ? (int)(v)->num_val : 0)
#define json_node_get_double(v)    ((v) ? (v)->num_val : 0.0)

/* Integer builder (convenience) */
#define json_int(v)                json_number((double)(v))

/* Double builder (convenience) */
#define json_double(v)             json_number((double)(v))

/* YAML parsing — REAL libyaml-backed bridge (lib/libjson/json_yaml_bridge.c).
 * Historically this macro aliased json_parse(s, NULL), which fails on any
 * real YAML and silently returned NULL (config consumers fell back to
 * defaults). json_parse_yaml_real parses YAML via libyaml and converts to a
 * json_t tree; already-valid JSON input takes the strict json_parse path. */
json_t *json_parse_yaml_real(const char *input);
#define json_parse_yaml(s) json_parse_yaml_real(s)

/* json_node_copy -> json_copy */
#define json_node_copy json_copy

/* === Iteration helpers (missing from libjson, added for compatibility) === */

#define json_is_object(v)       ((v) && (v)->type == JSON_OBJECT)
#define json_is_array(v)        ((v) && (v)->type == JSON_ARRAY)
#define json_is_string(v)       ((v) && (v)->type == JSON_STRING)
#define json_is_number(v)       ((v) && (v)->type == JSON_NUMBER)
#define json_is_bool(v)         ((v) && (v)->type == JSON_BOOL)
#define json_is_null(v)         ((v) && (v)->type == JSON_NULL)

#define json_type(v)            ((v) ? (v)->type : JSON_NULL)
#define json_string_value(v)    ((v) ? (v)->str_val : NULL)
#define json_number_value(v)    ((v) ? (v)->num_val : 0.0)
#define json_bool_value(v)      ((v) ? (v)->bool_val : false)

/* Array iteration */
#define json_array_size(a)      json_len((a))
#define json_array_get(a, i)    json_get((a), (i))

/* Object iteration */
static inline size_t json_object_size(const json_t *obj) {
    return obj && obj->type == JSON_OBJECT ? obj->c.count : 0;
}
static inline const char *json_object_get_key_at(const json_t *obj, size_t index) {
    return (obj && obj->type == JSON_OBJECT && obj->c.keys && index < obj->c.count) ? obj->c.keys[index] : NULL;
}
static inline json_t *json_object_get_at(const json_t *obj, size_t index) {
    return (obj && obj->type == JSON_OBJECT && obj->c.items && index < obj->c.count) ? obj->c.items[index] : NULL;
}

/* Serialization with pretty printing */
#define JSON_INDENT(n)  (n)  /* ignored in this shim, use json_serialize_pretty */
#define JSON_ENSURE_ASCII(v)  (0)  /* ignored */

static inline char *json_dumps(const json_t *node, int flags) {
    if (flags > 0) return json_serialize_pretty(node, flags);
    return json_serialize(node);
}

/* Set array element at index, freeing any previous value. */
static inline void json_array_set(json_t *arr, size_t index, json_t *val) {
    if (!arr || arr->type != JSON_ARRAY) { if (val) json_free(val); return; }
    if (index >= arr->c.count) { if (val) json_free(val); return; }
    json_free(arr->c.items[index]);
    arr->c.items[index] = val;
}

/* Remove array element at index (shifts remaining left). */
static inline void json_array_remove(json_t *arr, size_t index) {
    if (!arr || arr->type != JSON_ARRAY) return;
    if (index >= arr->c.count) return;
    for (size_t i = index; i + 1 < arr->c.count; i++)
        arr->c.items[i] = arr->c.items[i + 1];
    arr->c.count--;
}

/* Delete object key — canonical implementation lives in libjson (json_obj_del). */
#define json_object_del(obj, key) ((void)json_obj_del((obj), (key)))

#define json_string_value_safe(v)  ((v) && (v)->type == JSON_STRING ? (v)->str_val : NULL)

#define json_is_true(v)         ((v) && (v)->type == JSON_BOOL && (v)->bool_val)

/* JSON_NULL constant */
#define JSON_NULL ((json_type_t)0)

/** @} */ /* end of json group */

#endif /* HERMES_JSON_H */
