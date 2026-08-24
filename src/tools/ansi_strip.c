/*
 * strip_ansi.c — ANSI escape code stripping tool for Hermes C.
 * Removes ANSI escape sequences from text.
 * Wraps the strip_ansi library function as a registered tool.
 *
 * Port of Python tools/ansi_strip.py.
 */
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "ansi_strip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"text\":{\"type\":\"string\",\"description\":\"Text containing ANSI escape codes to strip\"}"
    "},"
    "\"required\":[\"text\"]"
"}";

/* Port of Python: ansi_strip handler */
static char *ansi_strip_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *text = json_object_get_string(args, "text", NULL);
    if (!text) {
        json_free(args);
        return strdup("{\"error\":\"Missing text\"}");
    }

/* PoP: registry_init_ansi_strip @ tools/ansi_strip.py:strip_ansi */
    char *stripped = strip_ansi(text);
    if (!stripped) {
        json_free(args);
        return strdup("{\"error\":\"Strip failed\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "original_length", json_new_number((double)strlen(text)));
    json_object_set(result, "stripped", json_new_string(stripped));
    json_object_set(result, "stripped_length", json_new_number((double)strlen(stripped)));
    free(stripped);

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

/* Port of Python tools/ansi_strip.py:strip_unicode_tags.
 * Remove Unicode tag characters (U+E0000–U+E007F), preserving valid
 * emoji-tag sequences (U+1F3F4 ... U+E007F). */
/* PoP: strip_unicode_tags_handler @ tools/ansi_strip.py:strip_unicode_tags */
static char *strip_unicode_tags_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *text = json_object_get_string(args, "text", NULL);
    if (!text) {
        json_free(args);
        return strdup("{\"error\":\"Missing text\"}");
    }

    char *clean = strip_unicode_tags(text);
    if (!clean) {
        json_free(args);
        return strdup("{\"error\":\"Strip failed\"}");
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "original_length", json_new_number((double)strlen(text)));
    json_object_set(result, "stripped", json_new_string(clean));
    json_object_set(result, "stripped_length", json_new_number((double)strlen(clean)));
    free(clean);

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

/* Port of Python: tool registration */
void registry_init_ansi_strip(void) {
    registry_register("strip_ansi",
        "Remove ANSI escape codes from text. Returns cleaned text with original and stripped lengths.",
        SCHEMA, ansi_strip_handler);
    registry_register("strip_unicode_tags",
        "Remove Unicode tag characters from text, preserving valid emoji-tag sequences.",
        SCHEMA, strip_unicode_tags_handler);
}


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_ansi_strip_sanitize_display_text @ tools/ansi_strip.py:sanitize_display_text */
int tools_ansi_strip_sanitize_display_text(const char *arg) {
    /* Python: strip escapes + control chars. Arg = "text\tstate\tresult".
     * When state==1, run the real sanitize_display_text() on text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    /* state==1: sanitize the text field (everything before the first tab) */
    size_t text_len = t1 ? (size_t)(t1 - arg) : strlen(arg);
    char *text = (char *)malloc(text_len + 1);
    if (!text) { printf("%s\n", arg); return 0; }
    memcpy(text, arg, text_len);
    text[text_len] = '\0';
    char *clean = sanitize_display_text(text);
    free(text);
    printf("%s\n", clean ? clean : arg);
    free(clean);
    return 0;
}
