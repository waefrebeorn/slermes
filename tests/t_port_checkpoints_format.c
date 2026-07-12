/* Oracle harness: hermes_cli/checkpoints.py pure formatters vs LIVE Python. */
#include <stdio.h>
#include <string.h>
#include "cli/port_checkpoints_format.h"

static const char *jstr(const char *s){
    static char b[4][1024]; static int bi=0; int idx=bi; char *q=b[idx]; bi=(bi+1)&3; *q++='"';
    for(const char *p=s;p&&*p&&(q-b[idx])<500;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else *q++=c;
    }
    *q++='"';*q='\0';return b[idx];
}

int main(void) {
    long bs[] = {0, 1, 512, 1023, 1024, 1536, 1048576, 1073741824, 5L*1024*1024*1024};
    for (int i = 0; i < 9; i++) {
        char out[64]; hermes_cli_checkpoints_fmt_bytes(bs[i], out, sizeof(out));
        printf("{\"t\":\"bytes\",\"n\":%ld,\"out\":%s}\n", bs[i], jstr(out));
    }
    double ages[][2] = {{1000,1000},{1000,1030},{1000,1600},{1000,3700},{1000,90000},{1000,9000000}};
    for (int i = 0; i < 6; i++) {
        char out[64]; hermes_cli_checkpoints_fmt_age(ages[i][0], ages[i][1], out, sizeof(out));
        printf("{\"t\":\"age\",\"ts\":%g,\"now\":%g,\"out\":%s}\n", ages[i][0], ages[i][1], jstr(out));
    }
    return 0;
}
