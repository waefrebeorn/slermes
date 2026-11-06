/* t_port_approval_transport.c — oracle harness for _validate_decision.
 * Emits {"tag":"atr","case":N,"kind":"..","value_null":0|1,
 *   "reqid":"..","digest":"..","choice":"..","completed":d,"deadline":d,
 *   "req_id":"..","req_digest":"..","allowed":"..","out":<code|null>}. */
#include "port_approval_transport.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct {
        const char *kind; int vnull;
        const char *reqid, *digest, *choice;
        double completed, deadline;
        const char *rid, *rdig, *allowed;
    } cases[] = {
        /* timeout */
        {"ok", 0, "r1", "d1", "approve", 11.0, 10.0, "r1", "d1", "approve,reject"},
        /* error */
        {"error", 0, "r1", "d1", "", 1.0, 10.0, "r1", "d1", "approve,reject"},
        /* value is None -> invalid */
        {"ok", 1, "", "", "", 1.0, 10.0, "r1", "d1", "approve,reject"},
        /* stale reqid */
        {"ok", 0, "r2", "d1", "approve", 1.0, 10.0, "r1", "d1", "approve,reject"},
        /* stale digest */
        {"ok", 0, "r1", "d2", "approve", 1.0, 10.0, "r1", "d1", "approve,reject"},
        /* disallowed choice */
        {"ok", 0, "r1", "d1", "delete", 1.0, 10.0, "r1", "d1", "approve,reject"},
        /* valid */
        {"ok", 0, "r1", "d1", "reject", 1.0, 10.0, "r1", "d1", "approve,reject"},
        /* allowed contains the choice */
        {"ok", 0, "r1", "d1", "approve", 1.0, 10.0, "r1", "d1", "approve"},
        /* empty allowed -> any choice is "invalid" (choice not in "") */
        {"ok", 0, "r1", "d1", "anything", 1.0, 10.0, "r1", "d1", ""},
        /* allowed with multiple, choice in middle */
        {"ok", 0, "r1", "d1", "reject", 1.0, 10.0, "r1", "d1", "approve,reject,snooze"},
    };
    size_t n = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < n; i++) {
        double completed = cases[i].completed, deadline = cases[i].deadline;
        const char *out = atr_validate_decision(cases[i].kind, cases[i].vnull,
            cases[i].reqid, cases[i].digest, cases[i].choice,
            completed, deadline,
            cases[i].rid, cases[i].rdig, cases[i].allowed);
        printf("{\"tag\":\"atr\",\"case\":%zu,\"kind\":", i);
        jstr(cases[i].kind);
        printf(",\"value_null\":%d,\"reqid\":", cases[i].vnull);
        jstr(cases[i].reqid);
        printf(",\"digest\":");
        jstr(cases[i].digest);
        printf(",\"choice\":");
        jstr(cases[i].choice);
        printf(",\"completed\":%.1f,\"deadline\":%.1f,\"rid\":", completed, deadline);
        jstr(cases[i].rid);
        printf(",\"rdig\":");
        jstr(cases[i].rdig);
        printf(",\"allowed\":");
        jstr(cases[i].allowed);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free((void *)out);
    }
    return 0;
}
