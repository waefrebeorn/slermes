/*
 * PoP: w9_sessiontui_48a0e5_wire_bytes @ gateway/session_tui_ops.py:_wire_bytes
 * PoP: w9_sessiontui_48a0e5_fit_frame  @ gateway/session_tui_ops.py:_fit_frame
 *
 * _wire_bytes = len(json.dumps(text, ensure_ascii=False).encode('utf-8','surrogatepass')) - 2
 * which is the byte count of the escaped string body (the two JSON quotes cancel the -2).
 */
#include "hermes_json.h"
#include "noc_w9_sessiontui_48a0e5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t u8_next(const char *s, size_t len, size_t i, unsigned *cp){
    unsigned char c=(unsigned char)s[i];
    if(c<0x80){ *cp=c; return 1; }
    size_t need=0; unsigned v=0;
    if((c&0xE0)==0xC0){ need=2; v=c&0x1F; }
    else if((c&0xF0)==0xE0){ need=3; v=c&0x0F; }
    else if((c&0xF8)==0xF0){ need=4; v=c&0x07; }
    else { *cp=c; return 1; }
    if(i+need>len){ *cp=c; return 1; }
    for(size_t j=1;j<need;j++){
        unsigned char d=(unsigned char)s[i+j];
        if((d&0xC0)!=0x80){ *cp=c; return 1; }
        v=(v<<6)|(d&0x3F);
    }
    *cp=v; return need;
}
static size_t cp_bytes(unsigned cp){
    if(cp<0x80) return 1; if(cp<0x800) return 2; if(cp<0x10000) return 3; return 4;
}
static size_t cplen(const char *s){
    size_t len=strlen(s), n=0;
    for(size_t k=0;k<len;){ unsigned cp; k+=u8_next(s,len,k,&cp); n++; }
    return n;
}
int w9_sessiontui_48a0e5_wire_bytes(const char *text){
    if(!text) return 0;
    size_t len=strlen(text), total=0;
    for(size_t k=0;k<len;){
        unsigned cp; k+=u8_next(text,len,k,&cp);
        if(cp=='"'||cp=='\\'||cp=='\b'||cp=='\f'||cp=='\n'||cp=='\r'||cp=='\t') total+=2;
        else if(cp>=0x20 && cp<0x7f) total+=1;
        else if(cp<0x20) total+=6;
        else total+=cp_bytes(cp);
    }
    return (int)total;
}
/* first n codepoints of s, malloc'd */
static char *utf8_prefix(const char *s, size_t n){
    size_t len=strlen(s), k=0, count=0;
    while(k<len && count<n){
        unsigned cp; k+=u8_next(s,len,k,&cp); count++;
    }
    char *out=(char*)malloc(k+1); if(!out)return NULL;
    memcpy(out,s,k); out[k]=0; return out;
}
char *w9_sessiontui_48a0e5_fit_frame(const char *text, long long budget){
    if(!text) text="";
    size_t n=cplen(text);
    if((long long)(n*6)<=budget ||
       ((long long)n<=budget && (long long)w9_sessiontui_48a0e5_wire_bytes(text)<=budget))
        return strdup(text);
    char *cut=utf8_prefix(text,(size_t)(budget+1));
    if(!cut) cut=strdup("");
    size_t len=strlen(cut), start=0, used=0;
    char *res=(char*)malloc(len+2); if(!res){ free(cut); return strdup(""); }
    size_t r=0;
    for(size_t i=0;i<=len;i++){
        if(i==len || cut[i]=='\n'){
            size_t end=(i<len)?i+1:i;      /* keep the trailing \n like re.split(r'(?<=\n)') */
            size_t llen=end-start;
            char *line=(char*)malloc(llen+1);
            if(line){ memcpy(line,cut+start,llen); line[llen]=0;
                int wb=w9_sessiontui_48a0e5_wire_bytes(line);
                free(line);
                if(used+(size_t)wb>(size_t)budget) break;
                memcpy(res+r,cut+start,llen); r+=llen; used+=(size_t)wb;
            }
            start=end;
        }
    }
    res[r]=0;
    /* removesuffix('\n') */
    if(r>0 && res[r-1]=='\n') res[--r]=0;
    free(cut);
    return res;
}
