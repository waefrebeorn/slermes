/*
 * skills_hub_clawhub.c — C home for upstream Hermes tools/skills_hub_clawhub.py.
 * PoP: w11_skillshub_a953b1_query_terms @ tools/skills_hub_clawhub.py:_query_terms
 * PoP: w11_skillshub_a953b1_search_score @ tools/skills_hub_clawhub.py:_search_score
 * PoP: w11_skillshub_a953b1_normalize_tags @ tools/skills_hub_clawhub.py:ClawHubSource._normalize_tags
 * PoP: w11_skillshub_a953b1_owner_from_payload @ tools/skills_hub_clawhub.py:ClawHubSource._owner_from_payload
 * PoP: w11_skillshub_a953b1_owner_matches @ tools/skills_hub_clawhub.py:ClawHubSource._owner_matches
 * PoP: w11_skillshub_a953b1_parse_identifier @ tools/skills_hub_clawhub.py:ClawHubSource._parse_identifier
 * PoP: w11_skillshub_a953b1_catalog_identifier @ tools/skills_hub_clawhub.py:ClawHubSource.catalog_identifier
 * PoP: w11_skillshub_a953b1_first_str @ tools/skills_hub_clawhub.py:_first_str
 * PoP: w11_skillshub_a953b1_coerce_skill_payload @ tools/skills_hub_clawhub.py:ClawHubSource._coerce_skill_payload
 *
 * HTTP catalog walking / ZIP download stay in Python; these are the pure
 * identifier, ranking and payload-shape helpers used by search/inspect/fetch.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_skillshub_a953b1.h"

static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static const char *jstr_or(json_t *v, const char *d){ const char *s=jstr(v); return s ? s : d; }

static int is_space(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
/* str.strip().lower() on the ASCII domain. Always malloc'd. */
static char *strip_lower(const char *s) {
    s = s ? s : "";
    size_t n = strlen(s);
    while (n && is_space((unsigned char)*s)) { s++; n--; }
    while (n && is_space((unsigned char)s[n-1])) n--;
    char *out = (char *)malloc(n+1);
    for (size_t i=0;i<n;i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n]='\0';
    return out;
}
/* str.lower() (no strip). Always malloc'd. */
static char *lower_dup(const char *s) {
    s = s ? s : "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n+1);
    for (size_t i=0;i<n;i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n]='\0';
    return out;
}

/* Python str() for the JSON scalar domain used by fixtures. */
static char *py_str(json_t *v) {
    char buf[64];
    if (!v) return (char *)calloc(1,1);
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *o = (char *)malloc(strlen(s)+1); strcpy(o,s); return o;
    }
    if (v->type == JSON_BOOL) {
        const char *s = v->bool_val ? "True" : "False";
        char *o = (char *)malloc(strlen(s)+1); strcpy(o,s); return o;
    }
    if (v->type == JSON_NULL) { char *o=(char*)malloc(5); strcpy(o,"None"); return o; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf,sizeof(buf),"%lld",(long long)v->num_val);
        else
            snprintf(buf,sizeof(buf),"%g",v->num_val);
        char *o=(char*)malloc(strlen(buf)+1); strcpy(o,buf); return o;
    }
    char *d = json_dumps(v,0);
    if (!d) return (char *)calloc(1,1);
    return d;
}

/* split on [^a-z0-9]+ after the caller has lowercased; empties dropped. */
static void split_terms(const char *s, json_t *arr) {
    size_t i = 0, n = strlen(s);
    while (i < n) {
        if (!((s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9'))) { i++; continue; }
        size_t start = i;
        while (i<n && ((s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9'))) i++;
        char *term = (char *)malloc(i-start+1);
        memcpy(term, s+start, i-start); term[i-start]='\0';
        json_append(arr, json_string(term));
        free(term);
    }
}
json_t *w11_skillshub_a953b1_query_terms(const char *query) {
    json_t *res = json_array();
    char *lower = lower_dup(query);
    split_terms(lower, res);
    free(lower);
    return res;
}
static char *join_terms(json_t *arr) {
    size_t cap = 1, used = 0;
    for (size_t i=0;i<json_array_size(arr);i++)
        cap += (jstr(json_get(arr,i)) ? strlen(jstr(json_get(arr,i))) : 0) + 1;
    char *out = (char *)malloc(cap);
    out[0]='\0';
    for (size_t i=0;i<json_array_size(arr);i++) {
        if (used) out[used++]=' ';
        const char *t = jstr_or(json_get(arr,i),"");
        size_t n = strlen(t); memcpy(out+used,t,n); used+=n;
    }
    out[used]='\0';
    return out;
}
static int list_has(json_t *arr, const char *want) {
    for (size_t i=0;i<json_array_size(arr);i++) {
        const char *v=jstr(json_get(arr,i));
        if (v && !strcmp(v,want)) return 1;
    }
    return 0;
}
static int list_slice_eq(json_t *a, json_t *b, size_t n) {
    if (json_array_size(b) < n) return 0;
    for (size_t i=0;i<n;i++) {
        const char *x=jstr(json_get(a,i)), *y=jstr(json_get(b,i));
        if (!x || !y || strcmp(x,y)) return 0;
    }
    return 1;
}
static int starts_with(const char *s, const char *prefix) {
    return strncmp(s,prefix,strlen(prefix))==0;
}

int w11_skillshub_a953b1_search_score(const char *query, json_t *meta) {
    char *qn = strip_lower(query);
    if (!*qn) { free(qn); return 1; }
    const char *id = meta&&meta->type==JSON_OBJECT ? jstr_or(json_obj_get(meta,"identifier"),"") : "";
    const char *nm = meta&&meta->type==JSON_OBJECT ? jstr_or(json_obj_get(meta,"name"),"") : "";
    const char *ds = meta&&meta->type==JSON_OBJECT ? jstr_or(json_obj_get(meta,"description"),"") : "";
    char *idl=lower_dup(id), *nml=lower_dup(nm), *dsl=lower_dup(ds);
    json_t *qt=w11_skillshub_a953b1_query_terms(qn);
    json_t *it=w11_skillshub_a953b1_query_terms(idl);
    json_t *nt=w11_skillshub_a953b1_query_terms(nml);
    char *nid=join_terms(it), *nnm=join_terms(nt);
    int score=0;
    size_t nq=json_array_size(qt);
    if (!strcmp(qn,idl)) score+=140;
    if (!strcmp(qn,nml)) score+=130;
    if (!strcmp(nid,qn)) score+=125;
    if (!strcmp(nnm,qn)) score+=120;
    if (starts_with(nid,qn)) score+=95;
    if (starts_with(nnm,qn)) score+=90;
    if (nq && list_slice_eq(qt,it,nq)) score+=70;
    if (nq && list_slice_eq(qt,nt,nq)) score+=65;
    if (strstr(idl,qn)) score+=40;
    if (strstr(nml,qn)) score+=35;
    if (strstr(dsl,qn)) score+=10;
    for (size_t i=0;i<nq;i++) {
        const char *t = jstr_or(json_get(qt,i),"");
        if (list_has(it,t)) score+=15;
        if (list_has(nt,t)) score+=12;
        if (strstr(dsl,t)) score+=3;
    }
    free(nnm); free(nid); free(dsl); free(nml); free(idl); free(qn);
    json_free(nt); json_free(it); json_free(qt);
    return score;
}

json_t *w11_skillshub_a953b1_normalize_tags(json_t *tags) {
    json_t *res = json_array();
    if (!tags) return res;
    if (tags->type == JSON_ARRAY) {
        for (size_t i=0;i<json_array_size(tags);i++) {
            char *s = py_str(json_get(tags,i));
            json_append(res, json_string(s)); free(s);
        }
    } else if (tags->type == JSON_OBJECT) {
        for (size_t i=0;i<json_object_size(tags);i++) {
            const char *k = json_object_get_key_at(tags,i);
            if (!k || !strcmp(k,"latest")) continue;
            json_append(res, json_string(k));
        }
    }
    return res;
}

char *w11_skillshub_a953b1_owner_from_payload(json_t *data) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    json_t *owner = json_obj_get(data,"owner");
    if (owner && owner->type == JSON_OBJECT) owner = json_obj_get(owner,"handle");
    const char *s = jstr(owner);
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n && is_space((unsigned char)*s)) { s++; n--; }
    while (n && is_space((unsigned char)s[n-1])) n--;
    if (!n) return NULL;
    char *out=(char*)malloc(n+1); memcpy(out,s,n); out[n]='\0';
    return out;
}

int w11_skillshub_a953b1_owner_matches(const char *expected_owner, json_t *data) {
    if (!expected_owner || !*expected_owner) return 1;
    char *actual = w11_skillshub_a953b1_owner_from_payload(data);
    if (!actual) return 1;
    char *a=lower_dup(actual), *b=lower_dup(expected_owner);
    int eq = !strcmp(a,b);
    free(a); free(b); free(actual);
    return eq;
}

static int slug_ok(const char *s) {
    if (!s || !*s) return 0;
    if (!isalnum((unsigned char)*s)) return 0;
    for (const char *p=s+1; *p; p++)
        if (!(isalnum((unsigned char)*p)||*p=='.'||*p=='_'||*p=='-')) return 0;
    return 1;
}
static int starts_with_ci(const char *s,const char *prefix) {
    size_t n=strlen(prefix);
    if (strlen(s)<n) return 0;
    for (size_t i=0;i<n;i++) if (tolower((unsigned char)s[i])!=tolower((unsigned char)prefix[i])) return 0;
    return 1;
}
json_t *w11_skillshub_a953b1_parse_identifier(const char *identifier) {
    /* str.strip() (no lowercase). */
    const char *raw = identifier ? identifier : "";
    size_t n = strlen(raw);
    while (n && is_space((unsigned char)*raw)) { raw++; n--; }
    while (n && is_space((unsigned char)raw[n-1])) n--;
    char *buf=(char*)malloc(n+1); memcpy(buf,raw,n); buf[n]='\0';
    const char *p = buf;
    int had_at = (*p=='@');
    if (had_at) p++;
    if (starts_with(p,"clawhub/")) p += 8;
    if (*p=='@') { had_at=1; p++; }
    const char *parts[64]; size_t np=0;
    char *cur=(char*)p;
    for (char *q=(char*)p;;q++) {
        if (*q=='/' || *q=='\0') {
            if (q>cur) { if (np<64) parts[np]=cur; np++; }
            char saved=*q;
            *q='\0';                 /* terminate this component in buf */
            if (saved=='\0') break;
            cur=q+1;
        }
    }
    const char *slug=NULL, *owner=NULL;
    if (np==1) { slug=parts[0]; }
    else if (np==2 && had_at) { owner=parts[0]; slug=parts[1]; }
    else if (np==3 && starts_with_ci(parts[1],"skills")
             && strlen(parts[1])==6) { owner=parts[0]; slug=parts[2]; }
    json_t *res = json_null();
    if (slug && slug_ok(slug) && (!owner || slug_ok(owner))) {
        res = json_object();
        json_set(res,"slug",json_string(slug));
        json_set(res,"owner",owner ? json_string(owner) : json_null());
    }
    free(buf);
    return res;
}

char *w11_skillshub_a953b1_catalog_identifier(json_t *identifier_value) {
    const char *s = jstr(identifier_value);
    if (!s) return NULL;
    json_t *parsed = w11_skillshub_a953b1_parse_identifier(s);
    if (!parsed || parsed->type != JSON_OBJECT) { json_free(parsed); return NULL; }
    const char *slug = jstr(json_obj_get(parsed,"slug"));
    if (!slug) { json_free(parsed); return NULL; }
    size_t n = strlen(slug);
    char *out=(char*)malloc(9+n);
    memcpy(out,"clawhub/",8); memcpy(out+8,slug,n); out[8+n]='\0';
    json_free(parsed);
    return out;
}

char *w11_skillshub_a953b1_first_str(json_t *values) {
    if (!values || values->type != JSON_ARRAY) return NULL;
    for (size_t i=0;i<json_array_size(values);i++) {
        json_t *v=json_get(values,i);
        if (v && v->type==JSON_STRING && v->str_val && *v->str_val) {
            char *o=(char*)malloc(strlen(v->str_val)+1); strcpy(o,v->str_val); return o;
        }
    }
    return NULL;
}

json_t *w11_skillshub_a953b1_coerce_skill_payload(json_t *data) {
    if (!data || data->type != JSON_OBJECT) return json_null();
    json_t *nested = json_obj_get(data,"skill");
    if (!nested || nested->type != JSON_OBJECT) return json_copy(data);
    json_t *merged = json_copy(nested);
    json_t *latest = json_obj_get(data,"latestVersion");
    if (latest && latest->type != JSON_NULL && !json_obj_get(merged,"latestVersion"))
        json_set(merged,"latestVersion",json_copy(latest));
    json_t *owner = json_obj_get(data,"owner");
    if (owner && !json_obj_get(merged,"owner"))
        json_set(merged,"owner",json_copy(owner));
    return merged;
}
