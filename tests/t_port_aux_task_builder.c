/* t_port_aux_task_builder.c — oracle harness for build_auxiliary_task_entry. */
#include "port_aux_task_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    /* (me, owner, key, dn, desc, defaults, inherit, registered, builtin) */
    /* defaults/registered are raw JSON object tokens; builtin is a JSON array. */
    struct { const char *me, *owner, *key, *dn, *desc, *defaults, *inh, *reg, *bi; } C[] = {
        {"p", "plug", "my_task", "My Task", "does x",
         "{\"model\":\"m\",\"timeout\":30}", NULL, "{}", "[\"session_start\"]"},
        {"p", "plug", "OK", "N", "D", NULL, NULL, "{}", "[]"},
        {"p", "plug", "", "N", "D", NULL, NULL, "{}", "[]"}, /* empty key -> invalid */
        {"p", "plug", "bad-key!", "N", "D", NULL, NULL, "{}", "[]"}, /* non-alnum */
        {"p", "plug", "session_start", "N", "D", NULL, NULL, "{}",
         "[\"session_start\",\"done\"]"}, /* reserved */
        {"p", "plug", "taken", "N", "D", NULL, NULL,
         "{\"taken\":{\"plugin\":\"other\"}}", "[]"}, /* owned by other */
        {"p", "plug", "mine", "N", "D", NULL, "my_base", "{}", "[\"s\"]"},
        {"p", "plug", "self", "N", "D", NULL, "self", "{}", "[]"}, /* self-inherit->reset */
        {"p", "plug", "ghost", "N", "D", "{\"provider\":\"x\"}", "nope", "{}",
         "[\"s\"]"}, /* inherit not builtin/registered -> reset, uses defaults */
        {"p", "plug", "reg", "N", "D", "{\"k\":1}", NULL, "{\"reg\":{\"plugin\":\"plug\"}}",
         "[]"}, /* same owner ok */
    };
    for (int i = 0; i < 10; i++) {
        char itok[256];
        char *o = bat_aux_task_entry(C[i].me, C[i].owner, C[i].key, C[i].dn,
                                     C[i].desc, C[i].defaults, C[i].inh,
                                     C[i].reg, C[i].bi);
        if (C[i].inh) snprintf(itok, sizeof itok, "\"%s\"", C[i].inh);
        else snprintf(itok, sizeof itok, "null");
        printf("{\"iwas\":\"%s\",\"me\":\"%s\",\"owner\":\"%s\",\"key\":\"%s\","
               "\"dn\":\"%s\",\"desc\":\"%s\",\"defaults\":%s,\"inh\":%s,"
               "\"reg\":%s,\"bi\":%s,\"out\":%s}\n",
               C[i].key ? "k" : "", C[i].me, C[i].owner, C[i].key ? C[i].key : "",
               C[i].dn, C[i].desc, C[i].defaults ? C[i].defaults : "null",
               itok, C[i].reg ? C[i].reg : "{}", C[i].bi,
               o ? o : "null");
        free(o);
    }
    (void)strlen;
    return 0;
}
