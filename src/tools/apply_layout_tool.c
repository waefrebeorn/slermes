/*
 * apply_layout_tool.c — Port of tools/apply_layout_tool.py.
 * Schema + thin dispatcher over the platform-injected desktop_ui emit bridge.
 */
#define _POSIX_C_SOURCE 200809L

#include "apply_layout_tool.h"
#include "hermes_json.h"
#include "hermes_tool_result.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* PoP: apply_layout_tool @ tools/apply_layout_tool.py:apply_layout_tool */
char *apply_layout_tool(const char *preset, apply_layout_emit_fn emit, void *emit_ctx)
{
    /* name = (preset or "").strip() */
    const char *p = preset ? preset : "";
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                       p[len - 1] == '\n' || p[len - 1] == '\r'))
        len--;
    if (len == 0)
        return tool_error("preset is required — a layout preset id, e.g. 'default' or 'focus'.", NULL);

    char *name = malloc(len + 1);
    if (!name)
        return tool_error("out of memory", NULL);
    memcpy(name, p, len);
    name[len] = '\0';

    int ok;
    if (!emit) {
        free(name);
        /* Python: callback absent -> desktop_ui.emit unavailable is the
         * desktop-only guard; mirror the not-ok branch's timeout note. */
        return tool_error(APPLY_LAYOUT_TIMEOUT_NOTE, NULL);
    }

    /* payload = {"preset": name} */
    json_t *payload = json_new_object();
    json_object_set(payload, "preset", json_string(name));
    char *payload_json = json_serialize(payload);
    json_free(payload);

    ok = emit("layout.apply", payload_json, emit_ctx);
    free(payload_json);

    if (ok < 0) {
        /* except Exception as exc: tool_error(f"Failed to apply layout '{name}': {exc}") */
        size_t need = snprintf(NULL, 0,
            "Failed to apply layout '%s': desktop_ui bridge raised", name) + 1;
        char *msg = malloc(need);
        snprintf(msg, need, "Failed to apply layout '%s': desktop_ui bridge raised", name);
        char *err = tool_error(msg, NULL);
        free(msg);
        free(name);
        return err;
    }
    if (!ok) {
        free(name);
        return tool_error(APPLY_LAYOUT_TIMEOUT_NOTE, NULL);
    }

    /* return json.dumps({"success": True, "preset": name}, ensure_ascii=False) */
    json_t *o = json_new_object();
    json_object_set(o, "success", json_bool(1));
    json_object_set(o, "preset", json_string(name));
    char *out = json_serialize(o);
    json_free(o);
    free(name);
    return out;
}

/* PoP: APPLY_LAYOUT_SCHEMA @ tools/apply_layout_tool.py */
char *apply_layout_schema(void)
{
    static const char *schema =
"{\"name\":\"apply_layout\","
"\"description\":\"Apply a saved layout preset to the Hermes desktop app when the user asks to "
"rearrange the workspace — e.g. \\\"set up my layout for coding\\\", \\\"give me a "
"focused view\\\", \\\"put the terminal front and center\\\". Built-in presets: "
"default (chat + sidebars), focus (chat only), terminal-deck (terminal "
"forward), quad (four zones). Plugin and user-saved presets are addressed by "
"their id. To reveal a single pane without rearranging everything, use "
"focus_pane instead.\","
"\"parameters\":{\"type\":\"object\",\"properties\":{\"preset\":{\"type\":\"string\","
"\"description\":\"Layout preset id to apply (e.g. 'default', 'focus', 'terminal-deck', 'quad', or a user/plugin preset id).\"}},"
"\"required\":[\"preset\"]}}";
    return strdup(schema);
}
