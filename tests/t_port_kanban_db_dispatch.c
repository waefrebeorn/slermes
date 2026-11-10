/*
 * t_port_kanban_db_dispatch.c -- oracle harness for
 * port_kanban_db_dispatch.c
 */
#include "port_kanban_db_dispatch.h"

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

static void emit_wait(const char *label, int rc)
{
    printf("{\"fn\":\"_wait_status_from_returncode\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%d}\n", kbd_wait_status_from_returncode(rc));
}

static void emit_kind(const char *label, int code, int rl, int tp)
{
    int out_code = -1;
    int kind = kbd_exit_code_kind(code, rl, tp, &out_code);
    printf("{\"fn\":\"_exit_code_kind\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%d,\"code\":%d}\n", kind, out_code);
}

static void emit_fp(const char *label, const char *text)
{
    char *out = kbd_error_fingerprint(text);
    printf("{\"fn\":\"_error_fingerprint\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_survived(const char *label, const char *const *t)
{
    int v = kbd_worker_survived_termination(t);
    printf("{\"fn\":\"_worker_survived_termination\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _wait_status_from_returncode ---- */
    emit_wait("zero",     0);
    emit_wait("one",      1);
    emit_wait("127",      127);
    emit_wait("255",      255);
    emit_wait("256",      256);
    emit_wait("neg15",    -15);
    emit_wait("neg1",     -1);

    /* ---- _exit_code_kind ---- */
    emit_kind("clean",   0,   75, 78);
    emit_kind("rate",    75,  75, 78);
    emit_kind("terminal",78,  75, 78);
    emit_kind("other",   7,   75, 78);
    emit_kind("zero-is-not-rate", 0, 0, 0);

    /* ---- _error_fingerprint ---- */
    emit_fp("pid",        "worker pid 12345 died");
    emit_fp("pid-long",   "pid 987654321 exited");
    emit_fp("pid-word",   "rapid 12345 done");
    emit_fp("ts",         "failed at 1730000000 after");
    emit_fp("ts-short",   "failed at 1234567 after");
    emit_fp("both",       "pid 42 at 1730000000");
    emit_fp("upper",      "Worker PID 9 DIED");
    emit_fp("long-text",  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaapid 7");
    emit_fp("empty",      "");
    emit_fp("padded",     "  Something Failed  ");

    /* ---- _worker_survived_termination ---- */
    static const char *t1[] = { "host_local", "1", "termination_attempted", "1", "terminated", "0", NULL };
    static const char *t2[] = { "host_local", "1", "signal_refused", "1", "terminated", "0", NULL };
    static const char *t3[] = { "host_local", "1", "termination_attempted", "1", "terminated", "1", NULL };
    static const char *t4[] = { "host_local", "1", "termination_attempted", "0", "terminated", "0", NULL };
    static const char *t5[] = { "host_local", "0", "termination_attempted", "1", "terminated", "0", NULL };
    static const char *t6[] = { NULL };
    emit_survived("attempted",  t1);
    emit_survived("refused",    t2);
    emit_survived("terminated", t3);
    emit_survived("no-attempt", t4);
    emit_survived("not-local",  t5);
    emit_survived("empty",      t6);

    return 0;
}
