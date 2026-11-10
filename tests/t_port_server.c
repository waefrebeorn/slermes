/*
 * t_port_server.c -- oracle harness for port_server.c
 */
#include "port_server.h"

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

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_frame(const char *label, const char *out)
{
    emit_str("_event_frame", label, out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char buf[SV_FRAME_MAX];
    char *s;

    /* ---- _load_busy_input_mode ---- */
    s = sv_busy_input_mode("queue");     emit_str("_load_busy_input_mode", "queue", s);     free(s);
    s = sv_busy_input_mode("STEER");     emit_str("_load_busy_input_mode", "steer", s);     free(s);
    s = sv_busy_input_mode("interrupt"); emit_str("_load_busy_input_mode", "interrupt", s); free(s);
    s = sv_busy_input_mode("bogus");     emit_str("_load_busy_input_mode", "bogus", s);     free(s);
    s = sv_busy_input_mode("  Queue  "); emit_str("_load_busy_input_mode", "padded", s);     free(s);
    s = sv_busy_input_mode("");          emit_str("_load_busy_input_mode", "empty", s);      free(s);
    s = sv_busy_input_mode(NULL);        emit_str("_load_busy_input_mode", "null", s);       free(s);

    /* ---- _canonical_profile_request ---- */
    s = sv_canonical_profile("hermes", 1);   emit_str("_canonical_profile_request", "hermes-dir", s);   free(s);
    s = sv_canonical_profile("hermes", 0);   emit_str("_canonical_profile_request", "hermes-nodir", s); free(s);
    s = sv_canonical_profile("HERMES", 0);   emit_str("_canonical_profile_request", "HERMES", s);       free(s);
    s = sv_canonical_profile(".hermes", 0);  emit_str("_canonical_profile_request", "dot-hermes", s);   free(s);
    s = sv_canonical_profile("work", 0);     emit_str("_canonical_profile_request", "other", s);        free(s);
    s = sv_canonical_profile(NULL, 0);       emit_str("_canonical_profile_request", "null", s);         free(s);

    /* ---- _current_profile_name ---- */
    s = sv_current_profile("work");  emit_str("_current_profile_name", "set", s);   free(s);
    s = sv_current_profile("");      emit_str("_current_profile_name", "empty", s); free(s);
    s = sv_current_profile(NULL);    emit_str("_current_profile_name", "null", s);  free(s);

    /* ---- _event_frame ---- */
    sv_event_frame(buf, sizeof(buf), "delta", "sess-1", NULL);
    emit_frame("no-payload", buf);
    sv_event_frame(buf, sizeof(buf), "delta", "sess-1", "{\"a\":1}");
    emit_frame("with-payload", buf);
    sv_event_frame(buf, sizeof(buf), "done", "s2", "");
    emit_frame("empty-payload", buf);
    sv_event_frame(buf, sizeof(buf), "q\"uote", "s3", "v\\x");
    emit_frame("escapes", buf);

    return 0;
}
