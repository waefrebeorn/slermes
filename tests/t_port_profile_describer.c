/* t_port_profile_describer.c — oracle harness for _sample_skills. */
#include "port_profile_describer.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const cases[] = {
        "[\"a\",\"b\",\"c\"]", "[\"x\"]", "[]",
        "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\",\"i\",\"j\"]",
        "[\"s0\",\"s1\",\"s2\",\"s3\",\"s4\",\"s5\",\"s6\",\"s7\",\"s8\",\"s9\",\"s10\",\"s11\",\"s12\",\"s13\",\"s14\",\"s15\",\"s16\",\"s17\",\"s18\",\"s19\"]",
    };
    for (int i = 0; i < 5; i++) {
        char *o = pd_sample_skills(cases[i]);
        printf("{\"family\":\"samp\",\"case\":%d,\"names\":%s,\"out\":%s}\n",
               i, cases[i] ? cases[i] : "null", o ? o : "null");
        free(o);
    }
    /* a >60 case: 65 items s0..s64 */
    {
        char buf[4096]; size_t pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
        for (int i = 0; i < 65; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\"s%d\"", i ? "," : "", i);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
        char *o = pd_sample_skills(buf);
        printf("{\"family\":\"samp\",\"case\":5,\"names\":%s,\"out\":%s}\n", buf, o ? o : "null");
        free(o);
    }
    return 0;
}
