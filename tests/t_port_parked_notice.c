/* t_port_parked_notice.c — oracle harness for _notice_due's pure state machine. */
#include "port_parked_notice.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    /* (sent entries, now, profile, platform, chat) */
    struct { const char *sent; double now; const char *p, *pl, *ch; } C[] = {
        /* course: old entry evicted -> due */
        {"[{\"profile\":\"A\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":10.0}],", 100.0, "A", "tg", "c1"},
        /* fresh entry present -> not due, unchanged */
        {"[{\"profile\":\"A\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":90.0}]", 100.0, "A", "tg", "c1"},
        /* absent -> due, marks now */
        {"[]", 50.0, "X", "disc", "g9"},
        /* evicts stale, keeps fresh, target fresh present -> not due */
        {"[{\"profile\":\"A\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":5.0},{\"profile\":\"A\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":99.0}]", 100.0, "A", "tg", "c1"},
        /* one chat, two profiles: only the target is fresh, target different key -> due+mark */
        {"[{\"profile\":\"B\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":99.0}]", 100.0, "A", "tg", "c1"},
        /* exactly at window boundary (now-ts == 60) -> evicted (>=) -> due */
        {"[{\"profile\":\"A\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":40.0}]", 100.0, "A", "tg", "c1"},
        {"null", 100.0, "A", "tg", "c1"},
    };
    /* NOTE: the first case string has a stray comma inside — fix below. */
    C[0].sent = "[{\"profile\":\"A\",\"platform\":\"tg\",\"chat\":\"c1\",\"ts\":10.0}]";
    for (int i = 0; i < 7; i++) {
        char *o = prr_notice_due(C[i].sent, C[i].now, C[i].p, C[i].pl, C[i].ch);
        printf("{\"family\":\"due\",\"case\":%d,\"sent\":%s,\"now\":%f,\"p\":\"%s\","
               "\"pl\":\"%s\",\"ch\":\"%s\",\"out\":%s}\n",
               i, C[i].sent ? C[i].sent : "null", C[i].now,
               C[i].p, C[i].pl, C[i].ch, o ? o : "null");
        free(o);
    }
    return 0;
}
