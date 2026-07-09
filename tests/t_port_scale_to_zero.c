/* t_port_scale_to_zero.c */
#include <stdio.h>
#include <stdlib.h>
#include "port_gateway_scale_to_zero.h"

static const char *js(const char *s){static char b[2][512];static int cur=0;char*q=b[cur];cur^=1;char*base=q;*q++='"';for(const char*p=s;*p&&(q-base)<500;p++){if(*p=='"'||*p=='\\')*q++='\\';*q++=*p;}*q++='"';*q=0;return base;}
static void emit(const char *v){
    char *o = gateway_scale_to_zero__platform_name(v);
    printf("{\"in\":%s,\"out\":%s}\n", js(v), js(o?o:""));
    free(o);
}
int main(void){
    emit("Relay"); emit("TELEGRAM"); emit(" discord "); emit("Slack"); emit(""); emit("RELAY");
    return 0;
}
