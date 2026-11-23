/*
 * skills_hub_install.c — C home for upstream Hermes tools/skills_hub_install.py.
 * PoP: w11_skillshub_e6cf0a_bundle_content_hash @ tools/skills_hub_install.py:bundle_content_hash
 * PoP: w11_skillshub_e6cf0a_source_matches @ tools/skills_hub_install.py:_source_matches
 *
 * Self-contained SHA-256 (no libcrypto dependency). Bundle contents are
 * injected by the caller; the destructive filesystem operations stay in Python.
 */
#include "hermes_json.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_skillshub_e6cf0a.h"

/* ---------- SHA-256 (same compact in-process implementation) ---------- */
static uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha256_block(uint32_t h[8], const unsigned char *b) {
    static const uint32_t K[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };
    uint32_t w[64];
    for (int i=0;i<16;i++)
        w[i]=((uint32_t)b[4*i]<<24)|((uint32_t)b[4*i+1]<<16)|((uint32_t)b[4*i+2]<<8)|((uint32_t)b[4*i+3]);
    for (int i=16;i<64;i++) {
        uint32_t s0=ror32(w[i-15],7)^ror32(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=ror32(w[i-2],17)^ror32(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=h[0],b1=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],h1=h[7];
    for (int i=0;i<64;i++) {
        uint32_t S1=ror32(e,6)^ror32(e,11)^ror32(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t t1=h1+S1+ch+K[i]+w[i];
        uint32_t S0=ror32(a,2)^ror32(a,13)^ror32(a,22);
        uint32_t maj=(a&b1)^(a&c)^(b1&c);
        uint32_t t2=S0+maj;
        h1=g;g=f;f=e;e=d+t1;d=c;c=b1;b1=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b1;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=h1;
}
/* Emits 64 lowercase hex chars into out. */
static void sha256_hex(const unsigned char *data, size_t len, char out[65]) {
    uint32_t h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                   0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    size_t i=0;
    for (; i+64<=len; i+=64) sha256_block(h,data+i);
    unsigned char blk[128]; size_t rem=len-i;
    if (rem) memcpy(blk,data+i,rem);
    blk[rem++]=0x80;
    size_t padto=(rem<=56)?56:120;
    memset(blk+rem,0,padto-rem);
    uint64_t bits=(uint64_t)len*8;
    for (int j=0;j<8;j++) blk[padto+j]=(unsigned char)(bits>>(56-8*j));
    sha256_block(h,blk);
    if (padto==120) sha256_block(h,blk+64);
    static const char hx[]="0123456789abcdef";
    for (int j=0;j<8;j++) for (int k=0;k<4;k++) {
        unsigned char byte=(unsigned char)(h[j]>>(24-8*k));
        out[8*j+2*k]=hx[byte>>4]; out[8*j+2*k+1]=hx[byte&15];
    }
    out[64]='\0';
}

static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }

char *w11_skillshub_e6cf0a_bundle_content_hash(json_t *files) {
    size_t count = (files && files->type==JSON_OBJECT) ? json_object_size(files) : 0;
    /* Normalize keys: '\' -> '/' and collect borrowed contents. */
    char **paths = count ? (char **)calloc(count,sizeof(char *)) : NULL;
    const char **contents = count ? (const char **)calloc(count,sizeof(char *)) : NULL;
    size_t used=0;
    if (files && files->type==JSON_OBJECT) {
        for (size_t i=0;i<json_object_size(files);i++) {
            const char *key=json_object_get_key_at(files,i);
            json_t *val=json_object_get_at(files,i);
            const char *content=jstr(val);
            if (!key || !content) continue;
            size_t n=strlen(key);
            char *norm=(char*)malloc(n+1);
            for (size_t j=0;j<n;j++) norm[j]=(key[j]=='\\')?'/':key[j];
            norm[n]='\0';
            paths[used]=norm; contents[used]=content; used++;
        }
    }
    /* sorted(normalized) */
    for (size_t i=0;i<used;i++) for (size_t j=i+1;j<used;j++)
        if (strcmp(paths[i],paths[j])>0) {
            char *tp=paths[i]; paths[i]=paths[j]; paths[j]=tp;
            const char *tc=contents[i]; contents[i]=contents[j]; contents[j]=tc;
        }
    /* h.update(path); h.update(b"\x00"); h.update(content). */
    size_t cap=1;
    for (size_t i=0;i<used;i++)
        cap+=strlen(paths[i])+1+strlen(contents[i]);
    unsigned char *buf=(unsigned char*)malloc(cap);
    size_t pos=0;
    for (size_t i=0;i<used;i++) {
        size_t n=strlen(paths[i]); memcpy(buf+pos,paths[i],n); pos+=n;
        buf[pos++]=0;
        n=strlen(contents[i]); memcpy(buf+pos,contents[i],n); pos+=n;
    }
    char hex[65];
    sha256_hex(buf,pos,hex);
    char *out=(char*)malloc(7+16+1);
    memcpy(out,"sha256:",7); memcpy(out+7,hex,16); out[23]='\0';
    free(buf);
    for (size_t i=0;i<used;i++) free(paths[i]);
    free(paths); free(contents);
    return out;
}

int w11_skillshub_e6cf0a_source_matches(const char *source_id, const char *source_name) {
    const char *sid=source_id?source_id:"";
    const char *name=source_name?source_name:"";
    const char *expected=!strcmp(name,"skills.sh") ? "skills-sh" : name;
    return !strcmp(sid,expected);
}
