/* t_port_pty_clamp.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_pty_clamp_helpers.h"

static const char *js(const char *s){static char b[2][512];static int cur=0;char*q=b[cur];cur^=1;char*base=q;*q++='"';for(const char*p=s;*p&&(q-base)<500;p++){if(*p=='"'||*p=='\\')*q++='\\';*q++=*p;}*q++='"';*q=0;return base;}
static void emit(const char *fn, int value, int maximum){
    int got = (strcmp(fn,"dim")==0) ? pty_clamp_dimension(value,maximum)
                                    : pty_win_clamp(value,maximum);
    printf("{\"fn\":%s,\"value\":%d,\"maximum\":%d,\"out\":%d}\n", js(fn), value, maximum, got);
}
int main(void){
    /* valid, min-boundary, max-boundary, below-min, above-max */
    emit("dim", 80, 2000);  emit("dim", 1, 2000);   emit("dim", 2000, 2000);
    emit("dim", 0, 2000);   emit("dim", 5000, 2000); emit("dim", -5, 2000);
    emit("win", 120, 1000); emit("win", 1, 1000);   emit("win", 1000, 1000);
    emit("win", 0, 1000);   emit("win", 9999, 1000);
    return 0;
}
