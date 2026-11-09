/*
 * t_port_chat_completion_helpers.c -- oracle harness for
 * port_chat_completion_helpers.c
 *
 * `kind`: 0 = not int/str, 1 = int, 2 = str.
 */
#include "port_chat_completion_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_code(const char *fn, const char *label, int code)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%d}\n", code);
}

static void emit_json_obj(const char *label, const char *text)
{
    json_t *o = cch_json_object_from_text(text);
    printf("{\"fn\":\"_json_object_from_text\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    if (!o) fputs("null", stdout);
    else {
        char *s = json_serialize(o);
        jstr(s);
        free(s);
        json_free(o);
    }
    printf("}\n");
}

static void emit_shape(const char *label, const char *payload_json)
{
    json_t *p = payload_json ? json_parse(payload_json, NULL) : NULL;
    int v = cch_payload_has_error_shape(p);
    printf("{\"fn\":\"_payload_has_error_shape\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
    if (p) json_free(p);
}

static void emit_payload_code(const char *label, const char *payload_json)
{
    json_t *p = payload_json ? json_parse(payload_json, NULL) : NULL;
    int code = cch_status_code_from_payload(p);
    printf("{\"fn\":\"_status_code_from_payload\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%d}\n", code);
    if (p) json_free(p);
}

static void emit_sse(const char *label, const char *text)
{
    int n = 0;
    json_t **ev = cch_parse_provider_sse_events(text, &n);
    printf("{\"fn\":\"_parse_provider_sse_events\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        char *s = ev ? json_serialize(ev[i]) : NULL;
        jstr(s);
        free(s);
        if (ev && ev[i]) json_free(ev[i]);
    }
    printf("]}\n");
    free(ev);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _status_code_from_value ---- */
    emit_code("_status_code_from_value", "int-ok",   cch_status_code_from_value(1, 200, NULL));
    emit_code("_status_code_from_value", "int-low",  cch_status_code_from_value(1, 99, NULL));
    emit_code("_status_code_from_value", "int-high", cch_status_code_from_value(1, 600, NULL));
    emit_code("_status_code_from_value", "int-599",  cch_status_code_from_value(1, 599, NULL));
    emit_code("_status_code_from_value", "bool",     cch_status_code_from_value(0, 0, NULL));
    emit_code("_status_code_from_value", "str-plain",cch_status_code_from_value(2, 0, "503 Service Unavailable"));
    emit_code("_status_code_from_value", "str-prefix",cch_status_code_from_value(2, 0, "HTTP_STATUS/429 rate limited"));
    emit_code("_status_code_from_value", "str-lower", cch_status_code_from_value(2, 0, "http_status/404"));
    emit_code("_status_code_from_value", "str-bad",   cch_status_code_from_value(2, 0, "no digits here"));
    emit_code("_status_code_from_value", "str-600",   cch_status_code_from_value(2, 0, "600 too big"));
    emit_code("_status_code_from_value", "null",      cch_status_code_from_value(0, 0, NULL));

    /* ---- _status_code_from_payload ---- */
    emit_payload_code("status_code", "{\"status_code\":503}");
    emit_payload_code("status",      "{\"status\":\"429\"}");
    emit_payload_code("http_status", "{\"http_status\":404}");
    emit_payload_code("nested",      "{\"error\":{\"code\":401}}");
    emit_payload_code("top-code",    "{\"code\":408}");
    emit_payload_code("none",        "{\"message\":\"hi\"}");
    emit_payload_code("not-dict",    NULL);
    emit_payload_code("bad-candidate", "{\"status_code\":\"nope\"}");

    /* ---- _json_object_from_text ---- */
    emit_json_obj("object",    "{\"a\":1}");
    emit_json_obj("array",     "[1,2,3]");
    emit_json_obj("scalar",    "\"str\"");
    emit_json_obj("padded",    "   {\"a\": 1}  ");
    emit_json_obj("invalid",   "{not json}");
    emit_json_obj("empty",     "");
    emit_json_obj("null",      NULL);

    /* ---- _payload_has_error_shape ---- */
    emit_shape("error-dict",  "{\"error\":{\"message\":\"boom\"}}");
    emit_shape("error-str",   "{\"error\":\"boom\"}");
    emit_shape("msg+code",    "{\"message\":\"boom\",\"code\":42}");
    emit_shape("msg+ecode",   "{\"message\":\"boom\",\"error_code\":\"E1\"}");
    emit_shape("msg+status",  "{\"message\":\"boom\",\"status_code\":500}");
    emit_shape("msg-only",    "{\"message\":\"boom\"}");
    emit_shape("empty-dict",  "{}");
    emit_shape("not-dict",    NULL);
    emit_shape("msg-empty",   "{\"message\":\"\",\"code\":42}");

    /* ---- _parse_provider_sse_events ---- */
    emit_sse("one-event", "event: message\ndata: hello\n\n");
    emit_sse("two-data",  "event: message\ndata: line1\ndata: line2\n\n");
    emit_sse("no-event",  "data: only data\n\n");
    emit_sse("comment",   ": status 200\ndata: hi\n\n");
    emit_sse("fields",    "data: hi\nstatus: 503\nid: 7\n\n");
    emit_sse("no-colon",  "data\n\n");
    emit_sse("multi",     "event: a\ndata: 1\n\nevent: b\ndata: 2\n\n");
    emit_sse("empty",     "");
    emit_sse("blank-mid", "data: 1\n\ndata: 2\n\n");
    emit_sse("crlf",      "event: message\r\ndata: hi\r\n\r\n");
    emit_sse("no-final-blank", "event: message\ndata: hi");
    emit_sse("space-field", "event:  message\ndata:  padded\n\n");

    return 0;
}
