/* t_port_whatsapp_identity.c */
#include <stdio.h>
#include <stdlib.h>
#include "port_gateway_whatsapp_identity.h"

static const char *js(const char *s){static char b[2][512];static int cur=0;char*q=b[cur];cur^=1;char*base=q;*q++='"';for(const char*p=s;*p&&(q-base)<500;p++){if(*p=='"'||*p=='\\')*q++='\\';*q++=*p;}*q++='"';*q=0;return base;}
static void emit(const char *v){
    char *o = gateway_whatsapp_to_jid(v);
    printf("{\"in\":%s,\"out\":%s}\n", js(v), js(o?o:""));
    free(o);
}
int main(void){
    emit("+50766715226"); emit("50766715226"); emit("50766715226@s.whatsapp.net");
    emit("group-id@g.us"); emit("130631430344750@lid"); emit("user:device@s.whatsapp.net");
    emit(""); emit("  "); emit("+1 (800) 555-0199"); emit("not a phone"); emit("waid:abc@s.whatsapp.net");
    return 0;
}
