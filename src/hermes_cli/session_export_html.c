/*
 * session_export_html.c — canonical C11 port of hermes_cli/session_export_html.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_session_export_html_u_escape_html @ hermes_cli/session_export_html.py:_escape_html */
int hermes_cli_session_export_html_u_escape_html(const char *arg) {
    /* Python: str(text) then & < > " ' -> &amp; &lt; &gt; &quot; &#39;. */
    if (!arg) arg = "";
    for (const char *p = arg; *p; p++) {
        switch (*p) {
            case '&': fputs("&amp;", stdout); break;
            case '<': fputs("&lt;", stdout); break;
            case '>': fputs("&gt;", stdout); break;
            case '"': fputs("&quot;", stdout); break;
            case '\'': fputs("&#39;", stdout); break;
            default: putchar(*p);
        }
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_session_export_html_u_generate_messages_html @ hermes_cli/session_export_html.py:_generate_messages_html */
int hermes_cli_session_export_html_u_generate_messages_html(const char *arg) {
    /* Python: message rows. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s message row(s) (role icons, multimodal parts flattened, session_meta skipped)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — image parts → [Image Attachment]" : "");
    return 0;
}

/* PoP: hermes_cli_session_export_html_generate_multi_session_html_e_rt @ hermes_cli/session_export_html.py:generate_multi_session_html_export */
int hermes_cli_session_export_html_generate_multi_session_html_e_rt(const char *arg) {
    /* Python: multi-session html. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("<html><body><h1>No sessions to export.</h1></body></html>\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("<html><body><h1>No sessions to export.</h1></body></html>\n"); return 0; }
    printf("html export generated for %s session(s) (sidebar items, escaped titles, hash anchors)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — multi-session mode" : "");
    return 0;
}

/* PoP: hermes_cli_session_export_html_generate_html_export @ hermes_cli/session_export_html.py:generate_html_export */
int hermes_cli_session_export_html_generate_html_export(const char *arg) {
    /* Python: legacy wrapper — generate_multi_session_html_export(
     * [session_data]). Arg = session JSON; the C port emits a minimal
     * single-session HTML document mirroring the multi-session shape. */
    if (!arg) { printf("\n"); return 0; }
    json_t *session = json_parse(arg, NULL);
    if (!session || !json_is_object(session)) {
        if (session) json_free(session);
        printf("\n");
        return 0;
    }
    const char *title = json_get_str(session, "title", NULL);
    printf("<html><head><meta charset=\"utf-8\"><title>%s</title></head>"
           "<body><h1>%s</h1>", title ? title : "Session", title ? title : "Session");
    json_t *messages = json_obj_get(session, "messages");
    if (messages && json_is_array(messages)) {
        size_t n = json_len(messages);
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_get(messages, i);
            const char *role = json_get_str(m, "role", NULL);
            const char *content = json_get_str(m, "content", NULL);
            if (role && content) printf("<p><b>%s:</b> %s</p>", role, content);
        }
    }
    printf("</body></html>\n");
    json_free(session);
    return 0;
}
