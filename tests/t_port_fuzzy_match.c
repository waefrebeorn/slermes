/* t_port_fuzzy_match.c */
#include <stdio.h>
#include <stdlib.h>
#include "port_tools_fuzzy_match.h"

static const char *js(const char *s){static char b[2][2048];static int t=0;char*q=b[t];t^=1;*q++='"';for(const char*p=s;*p&&q-b[t^1]<1900;p++){unsigned char c=*p;if(c=='"'||c=='\\'){*q++='\\';*q++=c;}else if(c=='\t'){*q++='\\';*q++='t';}else if(c=='\n'){*q++='\\';*q++='n';}else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}else *q++=c;}*q++='"';*q=0;return b[t^1];}

int main(void){
    /* original has extra/tab whitespace; normalized collapses to single spaces */
    const char *orig = "foo    bar\tbaz";
    const char *norm = "foo bar baz";
    /* matches in normalized coords: "foo" (0,3), "bar" (4,7) */
    span_t ms[] = {{0,3},{4,7}};
    int n;
    span_t *r = fuzzy_map_normalized_positions(orig, norm, ms, 2, &n);
    printf("{\"original\":%s,\"normalized\":%s,\"matches\":[[0,3],[4,7]],\"out\":[",
           js(orig), js(norm));
    for (int i=0;i<n;i++) printf("%s[%d,%d]", i?",":"", r[i].start, r[i].end);
    printf("]}\n");
    free(r);

    /* trailing-space expansion case (issue #52491) */
    const char *o2 = "hello world ";
    const char *n2 = "hello world";
    span_t ms2[] = {{0,11}};
    span_t *r2 = fuzzy_map_normalized_positions(o2, n2, ms2, 1, &n);
    printf("{\"original\":%s,\"normalized\":%s,\"matches\":[[0,11]],\"out\":[",
           js(o2), js(n2));
    for (int i=0;i<n;i++) printf("%s[%d,%d]", i?",":"", r2[i].start, r2[i].end);
    printf("]}\n");
    free(r2);

    /* empty matches */
    span_t *r3 = fuzzy_map_normalized_positions("abc", "abc", NULL, 0, &n);
    printf("{\"original\":\"abc\",\"normalized\":\"abc\",\"matches\":[],\"out\":[");
    for (int i=0;i<n;i++) printf("%s[%d,%d]", i?",":"", r3[i].start, r3[i].end);
    printf("]}\n");
    free(r3);
    return 0;
}
