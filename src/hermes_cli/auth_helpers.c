/*
 * auth_helpers.c — Pure auth/secret helpers (faithful C11 port of
 * hermes_cli/auth.py testable core). See auth_helpers.h.
 */

#include "auth_helpers.h"
#include "crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* ── placeholders ── */
static const char *PLACEHOLDERS[] = {
    "*", "**", "***", "changeme", "your_api_key", "your_api_key_here",
    "your-api-key", "placeholder", "example", "dummy", "null", "none", NULL
};

bool auth_has_usable_secret(const char *value, int min_length) {
    if (!value) return false;
    if (min_length <= 0) min_length = 4;
    /* strip */
    while (*value == ' ' || *value == '\t') value++;
    size_t L = strlen(value);
    while (L > 0 && (value[L-1]==' '||value[L-1]=='\t')) L--;
    if (L < (size_t)min_length) return false;
    /* lowercase compare against placeholders (only the significant part) */
    char *low = (char*)malloc(L+1);
    for (size_t i=0;i<L;i++) low[i]=(char)tolower((unsigned char)value[i]);
    low[L]='\0';
    bool ok = true;
    for (int i=0; PLACEHOLDERS[i]; i++) {
        if (strcmp(low, PLACEHOLDERS[i])==0){ ok=false; break; }
    }
    free(low);
    return ok;
}

/* ── kimi base url ── */
char *auth_resolve_kimi_base_url(const char *api_key, const char *default_url, const char *env_override) {
    if (env_override && env_override[0]) return strdup(env_override);
    if (!api_key || !api_key[0]) return strdup(default_url ? default_url : "");
    if (strncmp(api_key, "sk-kimi-", 8) == 0) return strdup("https://api.kimi.com/coding");
    return strdup(default_url ? default_url : "");
}

/* ── lmstudio base url ── */
char *auth_normalize_lmstudio_runtime_base_url(const char *base_url) {
    char *root = strdup(base_url ? base_url : "");
    /* strip + rstrip '/' */
    char *p = root;
    while (*p==' '||*p=='\t') p++;
    size_t L = strlen(p);
    while (L>0 && (p[L-1]=='/'||p[L-1]==' '||p[L-1]=='\t')) p[--L]='\0';
    const char *suffixes[] = {"/api/v1", "/api", "/v1", NULL};
    for (int i=0; suffixes[i]; i++) {
        size_t sl = strlen(suffixes[i]);
        if (L >= sl && strcmp(p + L - sl, suffixes[i]) == 0) {
            p[L-sl]='\0'; L-=sl;
            while (L>0 && (p[L-1]=='/'||p[L-1]==' ')) p[--L]='\0';
            break;
        }
    }
    const char *base = (p[0]) ? p : "http://127.0.0.1:1234";
    size_t need = strlen(base) + 4;
    char *out = (char*)malloc(need);
    snprintf(out, need, "%s/v1", base);
    free(root);
    return out;
}

/* ── token fingerprint ── */
char *auth_token_fingerprint(const char *token) {
    if (!token) return NULL;
    while (*token==' '||*token=='\t') token++;
    if (!token[0]) return NULL;
    unsigned char h[32];
    crypto_sha256((const unsigned char*)token, strlen(token), h);
    char *out = (char*)malloc(13);
    const char *hx="0123456789abcdef";
    for (int i=0;i<12;i++){ out[i]=hx[h[i]>>4]; out[i+1]=hx[h[i]&0xf]; }
    out[12]='\0';
    return out;
}

/* ── retry-after ── */
int auth_parse_retry_after(const char *raw_value) {
    if (!raw_value) return -1;
    while (*raw_value==' '||*raw_value=='\t') raw_value++;
    if (!raw_value[0]) return -1;
    char *end; long v = strtol(raw_value, &end, 10);
    if (end == raw_value) return -1;          /* not a number */
    if (*end && *end!=' ' && *end!='\t' && *end!='\r' && *end!='\n') return -1; /* garbage after */
    if (v < 0) return -1;
    return (int)v;
}

/* ── iso timestamp parse ── */
double auth_parse_iso_timestamp(const char *value) {
    if (!value || !*value) return -1;
    while (*value==' '||*value=='\t') value++;
    char *buf = strdup(value);
    size_t L = strlen(buf);
    while (L>0 && (buf[L-1]==' '||buf[L-1]=='\t'||buf[L-1]=='\r'||buf[L-1]=='\n')) buf[--L]='\0';
    /* normalize trailing Z -> +00:00 */
    if (L>0 && buf[L-1]=='Z') { buf[L-1]='+'; buf[L]='0'; buf[L+1]='0'; buf[L+2]=':'; buf[L+3]='0'; buf[L+4]='0'; buf[L+5]='\0'; }
    /* parse: YYYY-MM-DDTHH:MM:SS[.fff][+HH:MM|-HH:MM|Z] */
    int Y,M,D,Hh,Mm,Ss=0; int tzsign=0, tzh=0, tzm=0;
    char sep;
    int n = sscanf(buf, "%d-%d-%d%c%d:%d:%d%c%d:%d", &Y,&M,&D,&sep,&Hh,&Mm,&Ss,&sep,&tzh,&tzm);
    free(buf);
    if (n < 7) return -1;
    /* crude epoch (UTC) computation */
    static const int days_in_mo[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    long days = 0;
    for (int y=1970; y<Y; y++) days += (y%4==0 && (y%100!=0||y%400==0))?366:365;
    for (int m=1; m<M; m++) {
        days += days_in_mo[m-1];
        if (m==2 && (Y%4==0 && (Y%100!=0||Y%400==0))) days += 1;
    }
    days += (D-1);
    long secs = ((days*24L + Hh)*60L + Mm)*60L + Ss;
    /* apply tz offset (assume +00:00 if none parsed) */
    secs -= (long)tzsign * ((long)tzh*3600 + (long)tzm*60);
    return (double)secs;
}

bool auth_is_expiring(const char *expires_at_iso, int skew_seconds) {
    double exp = auth_parse_iso_timestamp(expires_at_iso);
    if (exp < 0) return true;
    return exp <= (time(NULL) + skew_seconds);
}

int auth_coerce_ttl_seconds(const char *expires_in) {
    if (!expires_in) return 0;
    char *end; long v = strtol(expires_in, &end, 10);
    if (end == expires_in) return 0;
    if (v < 0) return 0;
    return (int)v;
}

char *auth_optional_base_url(const char *value) {
    if (!value) return NULL;
    while (*value==' '||*value=='\t') value++;
    size_t L = strlen(value);
    while (L>0 && (value[L-1]=='/'||value[L-1]==' '||value[L-1]=='\t')) L--;
    if (L==0) return NULL;
    char *out = (char*)malloc(L+1);
    memcpy(out, value, L); out[L]='\0';
    return out;
}

/* ── scope parsing ── */
char **auth_scope_values(const char *raw_scope, int *out_count) {
    char **arr=NULL; int n=0, cap=0;
    if (raw_scope && *raw_scope) {
        char *copy = strdup(raw_scope);
        /* replace commas with spaces */
        for (char *c=copy; *c; c++) if (*c==',') *c=' ';
        char *tok = strtok(copy, " \t\r\n");
        while (tok) {
            while (*tok==' ') tok++;
            size_t tl=strlen(tok);
            while (tl>0 && (tok[tl-1]==' ')) tok[--tl]=0;
            if (tl>0) {
                arr=(char**)realloc(arr,(cap=n+1)*sizeof(char*));
                arr[n++]=strdup(tok);
            }
            tok = strtok(NULL, " \t\r\n");
        }
        free(copy);
    }
    *out_count=n; return arr;
}
void auth_free_scope(char **scopes, int n) {
    if (!scopes) return;
    for (int i=0;i<n;i++) free(scopes[i]);
    free(scopes);
}

/* ── base64url decode ── */
static int b64url_val(char c) {
    if (c>='A'&&c<='Z') return c-'A';
    if (c>='a'&&c<='z') return c-'a'+26;
    if (c>='0'&&c<='9') return c-'0'+52;
    if (c=='-') return 62;
    if (c=='_') return 63;
    return -1;
}
/* decode base64url (no padding required); returns malloc'd bytes, sets *out_len. NULL on error. */
static unsigned char *b64url_decode(const char *in, size_t *out_len) {
    size_t L = strlen(in);
    /* strip any existing padding, then pad to a multiple of 4 (URL-safe JWTs
       omit padding, but the decoder needs a length that maps to whole bytes). */
    while (L>0 && in[L-1]=='=') L--;
    size_t add = (4 - L % 4) % 4;
    size_t padded_len = L + add;
    char *pad = (char*)malloc(padded_len + 1);
    memcpy(pad, in, L);
    for (size_t i=L; i<padded_len; i++) pad[i]='=';
    pad[padded_len]='\0';
    /* standard base64 length: (groups*3) - trailing padding chars */
    size_t nbytes = (padded_len / 4) * 3 - add;
    unsigned char *out = (unsigned char*)malloc(nbytes ? nbytes : 1);
    size_t oi=0; int acc=0, cnt=0;
    for (size_t i=0;i<padded_len;i++) {
        int v = b64url_val(pad[i]);
        if (v<0) continue; /* '=' padding: contributes no bits */
        acc = (acc<<6)|v; cnt+=6;
        if (cnt>=8){ cnt-=8; out[oi++]=(unsigned char)((acc>>cnt)&0xFF); }
    }
    *out_len = oi;
    free(pad);
    return out;
}

/* minimal JSON: extract a string or number field value by key.
   Returns malloc'd string for strings, or the numeric text for numbers.
   free() the result. NULL if absent. */
static char *json_get_field(const char *json, const char *key, bool want_string) {
    char pat[128];
    snprintf(pat,sizeof(pat),"\"%s\"",key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && (*p==' '||*p==':'||*p=='\t')) p++;
    if (want_string) {
        if (*p != '"') return NULL;
        p++;
        size_t cap=16, len=0; char *s=(char*)malloc(cap);
        while (*p && *p!='"') {
            char c=*p;
            if (c=='\\'){ p++; c=*p; switch(c){case 'n':c='\n';break;case 't':c='\t';break;case 'r':c='\r';break;default:break;} }
            s[len++]=c; p++;
            if (len+1>=cap){cap*=2;s=realloc(s,cap);}
        }
        s[len]='\0'; return s;
    } else {
        /* number (and possibly bool/null — treat as text) */
        const char *start=p;
        while (*p && *p!=',' && *p!='}' && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') p++;
        size_t l=(size_t)(p-start);
        if (l==0) return NULL;
        char *s=(char*)malloc(l+1); memcpy(s,start,l); s[l]='\0';
        return s;
    }
}

char *auth_decode_jwt_payload(const char *token) {
    if (!token) return NULL;
    /* count dots */
    int dots=0; const char *d=token;
    while (*d) { if (*d=='.') dots++; d++; }
    if (dots != 2) return NULL;
    const char *p1 = strchr(token, '.');
    if (!p1) return NULL;
    const char *p2 = strchr(p1+1, '.');
    if (!p2) return NULL;
    size_t plen = (size_t)(p2 - (p1+1));
    char *payload = (char*)malloc(plen+1);
    memcpy(payload, p1+1, plen); payload[plen]='\0';
    size_t dec_len; unsigned char *dec = b64url_decode(payload, &dec_len);
    free(payload);
    if (!dec) return NULL;
    char *json = (char*)malloc(dec_len+1);
    memcpy(json, dec, dec_len); json[dec_len]='\0';
    free(dec);
    /* verify it parses as an object-ish (starts with '{') */
    const char *q=json; while(*q==' ') q++;
    if (*q!='{'){ free(json); return NULL; }
    return json;
}

char *auth_jwt_get_str(const char *payload_json, const char *key) {
    if (!payload_json) return NULL;
    return json_get_field(payload_json, key, true);
}
double auth_jwt_get_num(const char *payload_json, const char *key) {
    if (!payload_json) return -1;
    char *s = json_get_field(payload_json, key, false);
    if (!s) return -1;
    char *end; double v = strtod(s, &end);
    if (end==s){ free(s); return -1; }
    free(s);
    return v;
}

/* ── auth error ── */
auth_error_t *auth_error_new(const char *message, const char *provider, const char *code, bool relogin_required) {
    auth_error_t *e = (auth_error_t*)calloc(1,sizeof(auth_error_t));
    e->message = strdup(message?message:"");
    e->provider = strdup(provider?provider:"");
    e->code = strdup(code?code:"");
    e->relogin_required = relogin_required;
    return e;
}
void auth_error_free(auth_error_t *e) {
    if (!e) return;
    free(e->message); free(e->provider); free(e->code); free(e);
}

bool auth_is_rate_limited_error(const auth_error_t *e) {
    if (!e) return false;
    return (strcmp(e->code, "codex_rate_limited")==0) && !e->relogin_required;
}

char *auth_format_error(const auth_error_t *e) {
    if (!e) return strdup("");
    if (auth_is_rate_limited_error(e)) return strdup(e->message);
    char *base = strdup(e->message);
    if (e->relogin_required) {
        size_t need = strlen(base) + 64;
        char *out=(char*)malloc(need);
        snprintf(out, need, "%s Run `hermes model` to re-authenticate.", base);
        free(base); return out;
    }
    if (strcmp(e->code,"subscription_required")==0 ||
        strcmp(e->code,"insufficient_credits")==0 ||
        strcmp(e->code,"subscription_expired")==0 ||
        strcmp(e->code,"no_usable_credits")==0 ||
        strcmp(e->code,"account_missing")==0) {
        if (strcmp(e->provider,"nous")==0) {
            size_t need = strlen(base) + 80;
            char *out=(char*)malloc(need);
            snprintf(out, need, "%s (Nous entitlement: check your Nous Research subscription/credits.)", base);
            free(base); return out;
        }
        if (strcmp(e->code,"subscription_required")==0) {
            size_t need=strlen(base)+80; char *out=(char*)malloc(need);
            snprintf(out,need,"%s Please purchase/activate a subscription, then retry.",base); free(base); return out;
        }
        if (strcmp(e->code,"insufficient_credits")==0) {
            size_t need=strlen(base)+80; char *out=(char*)malloc(need);
            snprintf(out,need,"%s Top up/renew credits, then retry.",base); free(base); return out;
        }
    }
    return base;  /* fallback: message only */
}

/* ── nous invoke jwt status ── */
char *auth_nous_invoke_jwt_status(const char *token, const char *scope, const char *expires_at, int min_ttl_seconds) {
    char *payload = auth_decode_jwt_payload(token);
    if (!payload) return strdup("access_token_not_jwt");
    int ns; char **sc = auth_scope_values(scope, &ns);
    int np; char **sp = auth_scope_values(auth_jwt_get_str(payload, "scope"), &np);
    int nc; char **cp = auth_scope_values(auth_jwt_get_str(payload, "scp"), &nc);
    bool has_scope=false;
    for (int i=0;i<ns;i++) if (strcmp(sc[i],AUTH_NOUS_INFERENCE_INVOKE_SCOPE)==0) has_scope=true;
    for (int i=0;i<np;i++) if (strcmp(sp[i],AUTH_NOUS_INFERENCE_INVOKE_SCOPE)==0) has_scope=true;
    for (int i=0;i<nc;i++) if (strcmp(cp[i],AUTH_NOUS_INFERENCE_INVOKE_SCOPE)==0) has_scope=true;
    auth_free_scope(sc,ns); auth_free_scope(sp,np); auth_free_scope(cp,nc);
    if (!has_scope) { free(payload); return strdup("missing_inference_invoke_scope"); }
    double exp = auth_jwt_get_num(payload, "exp");
    int skew = min_ttl_seconds>0?min_ttl_seconds:0;
    if (exp > 0) {
        if (exp <= (time(NULL) + skew)) { free(payload); return strdup("invoke_jwt_expiring"); }
        free(payload); return NULL;
    }
    if (auth_is_expiring(expires_at, skew)) { free(payload); return strdup("invoke_jwt_expiry_unknown_or_expiring"); }
    free(payload);
    return NULL;
}
