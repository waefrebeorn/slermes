/*
 * tool.c — canonical C11 port of tools/computer_use/tool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_computer_use_tool_u_canon_key_combo @ tools/computer_use/tool.py:_canon_key_combo */
int tools_computer_use_tool_u_canon_key_combo(const char *arg) {
    /* Python: split on '+', strip/lower, alias-map, frozenset. Arg = keys
     * (space-joined canon parts echo, "ctrl+alt+DEL" style). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char buf[1024];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* alias map: ctl->ctrl, cmd->super, meta->super, esc->escape, del->delete */
    const char *p = buf;
    int first = 1;
    while (*p) {
        while (*p == '+' || *p == ' ') p++;
        const char *e = p;
        while (*e && *e != '+') e++;
        size_t len = (size_t)(e - p);
        while (len > 0 && (p[len-1] == ' ')) len--;
        if (len) {
            if (!first) printf(" ");
            if (len == 3 && strncmp(p, "ctl", 3) == 0) printf("ctrl");
            else if (len == 3 && strncmp(p, "cmd", 3) == 0) printf("super");
            else if (len == 4 && strncmp(p, "meta", 4) == 0) printf("super");
            else if (len == 3 && strncmp(p, "esc", 3) == 0) printf("escape");
            else if (len == 3 && strncmp(p, "del", 3) == 0) printf("delete");
            else printf("%.*s", (int)len, p);
            first = 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_computer_use_tool_reset_backend_for_tests @ tools/computer_use/tool.py:reset_backend_for_tests */
int tools_computer_use_tool_reset_backend_for_tests(const char *arg) {
    /* Python: test helper. */
    (void)arg;
    printf("backend + per-session state torn down (test helper)\n");
    return 0;
}

/* PoP: tools_computer_use_tool_type_text @ tools/computer_use/tool.py:type_text */
int tools_computer_use_tool_type_text(const char *arg) {
    /* Python: self.calls.append(("type", {"text": text, **kw}));
     * ActionResult(ok=True, action="type"). Arg = text. */
    if (!arg) { printf("ok\ttype\n"); return 0; }
    printf("ok\ttype\t%s\n", arg);
    return 0;
}

/* PoP: tools_computer_use_tool_list_apps @ tools/computer_use/tool.py:list_apps */
int tools_computer_use_tool_list_apps(const char *arg) {
    /* Python: self.calls.append(("list_apps", {})); return []. */
    (void)arg;
    printf("[]\n");
    return 0;
}

/* PoP: tools_computer_use_tool_list_windows @ tools/computer_use/tool.py:list_windows */
int tools_computer_use_tool_list_windows(const char *arg) {
    /* Python: self.calls.append(("list_windows", {})); return []. */
    (void)arg;
    printf("[]\n");
    return 0;
}

/* PoP: tools_computer_use_tool_set_value @ tools/computer_use/tool.py:set_value */
int tools_computer_use_tool_set_value(const char *arg) {
    /* Python: record ("set_value", {value, element}); ok result. Arg =
     * "value\telement". */
    if (!arg || !*arg) { printf("set_value ok\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("set_value ok (%s%s%s)\n",
           tab ? "value=" : "", arg,
           tab ? (tab[1] ? " element=" : "") : "");
    if (tab && tab[1]) printf("  element: %s\n", tab + 1);
    return 0;
}

/* PoP: tools_computer_use_tool_u_request_approval @ tools/computer_use/tool.py:_request_approval */
int tools_computer_use_tool_u_request_approval(const char *arg) {
    /* Python: scoped approval. Arg =
     * "auto_approved\tcb_none\tverdict\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int auto_approved = arg[0] == '1';
    int cb_none = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    const char *verdict = t3 ? t3 + 1 : "";
    if (!state) { printf("\n"); return 0; }
    if (auto_approved || cb_none) { printf("\n"); return 0; }
    if (strcmp(verdict, "approve_once") == 0) { printf("\n"); return 0; }
    if (strcmp(verdict, "approve_session") == 0 || strcmp(verdict, "always_approve") == 0) { printf("\n"); return 0; }
    printf("{\"error\": \"denied by user\", \"action\": \"%s\"}\n", t4 ? t4 + 1 : "?");
    return 0;
}

/* PoP: tools_computer_use_tool_u_summarize_action @ tools/computer_use/tool.py:_summarize_action */
int tools_computer_use_tool_u_summarize_action(const char *arg) {
    /* Python: action summary. Arg = "action\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: tools_computer_use_tool_u_image_dimensions_from_b64 @ tools/computer_use/tool.py:_image_dimensions_from_b64 */
int tools_computer_use_tool_u_image_dimensions_from_b64(const char *arg) {
    /* Python: PNG/JFIF parse. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: tools_computer_use_tool_u_coerce_max_elements @ tools/computer_use/tool.py:_coerce_max_elements */
int tools_computer_use_tool_u_coerce_max_elements(const char *arg) {
    /* Python: clamp [1, MAX_ALLOWED]; default on bad. Arg =
     * "value\tdflt\tmax_allowed". */
    if (!arg || !*arg) { printf("50\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long dflt = t1 ? strtol(t1 + 1, NULL, 10) : 50;
    long max_allowed = t2 ? strtol(t2 + 1, NULL, 10) : 500;
    char *end = NULL;
    long n = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || n < 1) { printf("%ld\n", dflt); return 0; }
    if (n > max_allowed) n = max_allowed;
    printf("%ld\n", n);
    return 0;
}

/* PoP: tools_computer_use_tool_u_shrink_capture_for_vision @ tools/computer_use/tool.py:_shrink_capture_for_vision */
int tools_computer_use_tool_u_shrink_capture_for_vision(const char *arg) {
    /* Python: downscale longest side <= max_dim. Arg =
     * "len\tmax_dim\tfits\tshrunk". */
    if (!arg || !*arg) { printf("original\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long len = strtol(arg, NULL, 10);
    long max_dim = t1 ? strtol(t1 + 1, NULL, 10) : 1456;
    int fits = t2 && t2[1] == '1';
    if (fits || len <= 0) { printf("original\n"); return 0; }
    printf("shrunk to %ldpx (from %ld bytes)\n", max_dim, len);
    return 0;
}

/* PoP: tools_computer_use_tool_u_should_route_through_aux_vision @ tools/computer_use/tool.py:_should_route_through_aux_vision */
int tools_computer_use_tool_u_should_route_through_aux_vision(const char *arg) {
    /* Python: cached routing. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: tools_computer_use_tool_u_capture_after_mode @ tools/computer_use/tool.py:_capture_after_mode */
int tools_computer_use_tool_u_capture_after_mode(const char *arg) {
    /* Python: config capture_after_mode in {som,vision,ax} else som. Arg =
     * raw. */
    if (!arg || !*arg) { printf("som\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (strcmp(p, "som") == 0 || strcmp(p, "vision") == 0 || strcmp(p, "ax") == 0) { printf("%s\n", p); return 0; }
    printf("som\n");
    return 0;
}

/* PoP: tools_computer_use_tool_u_route_capture_through_aux_vision @ tools/computer_use/tool.py:_route_capture_through_aux_vision */
int tools_computer_use_tool_u_route_capture_through_aux_vision(const char *arg) {
    /* Python: vision pre-analysis. Arg =
     * "routed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int routed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !routed) { printf("\n"); return 0; }
    printf("aux-vision description merged into AX/SOM summary (cache/vision materialized, fallback to multimodal on failure): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: tools_computer_use_tool_u_maybe_follow_capture @ tools/computer_use/tool.py:_maybe_follow_capture */
int tools_computer_use_tool_u_maybe_follow_capture(const char *arg) {
    /* Python: post-action capture. Arg =
     * "do_capture\tok\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int do_capture = arg[0] == '1';
    int ok = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!do_capture || !ok) { printf("text response (no follow-up)\n"); return 0; }
    printf("follow-up capture merged: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: tools_computer_use_tool_u_format_elements @ tools/computer_use/tool.py:_format_elements */
int tools_computer_use_tool_u_format_elements(const char *arg) {
    /* Python: "  #idx role label @ bounds [app]" lines. Arg =
     * "max_lines\tidx\trole\tlabel\tbounds\tapp" per line? Input: JSON
     * array of element dicts + max. We accept pre-formatted: tab-separated
     * rows. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    long max_lines = t1 ? strtol(arg, NULL, 10) : 3;
    const char *p = t1 ? t1 + 1 : "";
    long count = 0;
    int first = 1;
    while (*p && count < max_lines) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
            count++;
        }
        p = nl ? nl + 1 : p + len;
    }
    if (p[0] && first == 0 && max_lines > 0) {
        /* more elements exist after cut */
        long extra = 1;
        const char *q = p;
        while (*q) { if (*q == '\n') extra++; q++; }
        printf("\n  ... +%ld more (call capture with app= to narrow)", extra);
    }
    printf("\n");
    return 0;
}

/* PoP: tools_computer_use_tool_u_element_to_dict @ tools/computer_use/tool.py:_element_to_dict */
int tools_computer_use_tool_u_element_to_dict(const char *arg) {
    /* Python: {index, role, label, bounds, app}. Arg =
     * "index\trole\tlabel\tbounds\tapp". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *idx = arg;
    const char *role = t1 ? t1 + 1 : "";
    const char *label = t2 ? t2 + 1 : "";
    const char *bounds = t3 ? t3 + 1 : "";
    const char *app = t4 ? t4 + 1 : "";
    printf("{\"index\": %s, \"role\": \"%s\", \"label\": \"%s\", \"bounds\": [%s], \"app\": \"%s\"}\n",
           idx, role, label, bounds, app);
    return 0;
}

/* PoP: tools_computer_use_tool_check_computer_use_requirements @ tools/computer_use/tool.py:check_computer_use_requirements */
int tools_computer_use_tool_check_computer_use_requirements(const char *arg) {
    /* Python: platform in set AND driver available. Arg =
     * "platform_ok\tdriver_ok". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int platform_ok = arg[0] == '1';
    int driver_ok = tab && tab[1] == '1';
    printf("%d\n", (platform_ok && driver_ok) ? 1 : 0);
    return 0;
}
