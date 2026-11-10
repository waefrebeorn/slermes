/*
 * computer_use_resp.c - computer-use JSON response builders.
 *
 * Split from computer_use.c (soul.md: self-contained modules). Builds the
 * text / capture / error response objects. No file-scope state.
 */
#include "computer_use_resp.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <windows.h>


/* ======================================================================
 *  JSON response helpers (plain C, no libjson dependency here)
 * ====================================================================== */ char *make_text_response(const cu_action_t *act) {
    json_t *root = json_object();
    json_set(root, "ok", json_bool(act->ok));
    json_set(root, "action", json_string(act->action));
    if (act->message[0])
        json_set(root, "message", json_string(act->message));
    char *str = json_serialize(root);
    json_free(root);
    return str ? str : strdup("{\"error\":\"OOM\"}");
}
 char *make_capture_response(const cu_capture_t *cap,
                                    int max_elements) {
    json_t *root = json_object();
    json_set(root, "mode", json_string(cap->mode));
    json_set(root, "width", json_number((double)cap->width));
    json_set(root, "height", json_number((double)cap->height));
    if (cap->app[0])
        json_set(root, "app", json_string(cap->app));
    if (cap->window_title[0])
        json_set(root, "window_title", json_string(cap->window_title));

    int n = cap->element_count;
    int visible = (max_elements > 0 && n > max_elements) ? max_elements : n;
    json_t *elements = json_array();
    for (int i = 0; i < visible; i++) {
        json_t *e = json_object();
        json_set(e, "index", json_number((double)cap->elements[i].index));
        json_set(e, "role", json_string(cap->elements[i].role));
        if (cap->elements[i].label[0])
            json_set(e, "label", json_string(cap->elements[i].label));
        json_append(elements, e);
    }
    json_set(root, "elements", elements);
    json_set(root, "total_elements", json_number((double)n));

    /* Build summary text */
    char summary[4096];
    int pos = snprintf(summary, sizeof(summary),
        "capture mode=%s %dx%d",
        cap->mode, cap->width, cap->height);
    if (cap->app[0])
        pos += snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                        " app=%s", cap->app);
    pos += snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                    "\n%d interactable element(s)", n);
    for (int i = 0; i < visible && i < 50 && (size_t)pos < sizeof(summary) - 120; i++) {
        pos += snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                        "\n  [@e%d] %s%s%s",
                        cap->elements[i].index,
                        cap->elements[i].role,
                        cap->elements[i].label[0] ? ": " : "",
                        cap->elements[i].label[0] ? cap->elements[i].label : "");
    }
    if (n > visible) {
        snprintf(summary + pos, sizeof(summary) - (size_t)pos,
                 "\n  (response truncated to %d of %d elements)",
                 visible, n);
    }
    json_set(root, "summary", json_string(summary));

    if (cap->png_b64 && strcmp(cap->mode, "ax") != 0) {
        json_set(root, "screenshot_available", json_bool(true));
        json_set(root, "png_bytes", json_number((double)cap->png_bytes));
    }

    char *str = json_serialize(root);
    json_free(root);
    return str ? str : strdup("{\"error\":\"OOM\"}");
}
 char *make_error_response(const char *fmt, ...) {
    json_t *root = json_object();
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    json_set(root, "error", json_string(msg));
    char *str = json_serialize(root);
    json_free(root);
    return str ? str : strdup("{\"error\":\"OOM\"}");
}
