#define _POSIX_C_SOURCE 200809L
/* plugin_index_pure.c — Faithful C11 port of the pure, deterministic
 * search/resolve/validation logic in hermes_cli/plugin_index.py (community
 * plugin index: fetch/cache is NOT ported — network/FS glue).
 *
 * Ports (PoP):
 *   PluginIndexEntry.install_identifier -> pindex_install_identifier
 *   _parse_entries                       -> pindex_parse_entries
 *   resolve_name                          -> pindex_resolve_name
 *   search_index + _score_entry          -> pindex_search_index
 *
 * Fuzzy score uses difflib.SequenceMatcher.ratio() == 2*LCS/(len(a)+len(b))
 * over the lowercase name — implemented exactly via LCS length (DP).
 *
 * All char* returns are malloc'd (free with free()).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

/* difflib SequenceMatcher.ratio(): 2 * M / (len(a) + len(b)), M = LCS length */
static double pindex_lcs_ratio(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0 && lb == 0) return 1.0;
    if (la == 0 || lb == 0) return 0.0;
    int *prev = (int *)calloc((size_t)(lb + 1), sizeof(int));
    int *cur = (int *)calloc((size_t)(lb + 1), sizeof(int));
    for (int i = 1; i <= la; i++) {
        for (int j = 1; j <= lb; j++) {
            if (a[i - 1] == b[j - 1]) cur[j] = prev[j - 1] + 1;
            else cur[j] = prev[j] > cur[j - 1] ? prev[j] : cur[j - 1];
        }
        int *t = prev; prev = cur; cur = t;
    }
    int m = prev[lb];
    free(prev); free(cur);
    return 2.0 * m / (la + lb);
}
/* PoP: pindex_install_identifier @ hermes_cli/plugin_index.py:PluginIndexEntry.install_identifier */

char *pindex_install_identifier(const char *repo, const char *subdir) {
    char *out = (char *)malloc(strlen(repo) + (subdir ? strlen(subdir) : 0) + 2);
    if (subdir && *subdir) snprintf(out, strlen(repo) + strlen(subdir) + 2, "%s/%s", repo, subdir);
    else strcpy(out, repo);
    return out;
}

/* Validate + normalize one raw item into a JSON object, or NULL if invalid. */
static int pindex_valid_repo(const char *repo) {
    /* exactly one '/' and both sides non-empty */
    if (!repo || !*repo) return 0;
    int slashes = 0; const char *p = repo;
    const char *first = NULL;
    while (*p) { if (*p == '/') { slashes++; if (slashes == 1) first = p; } p++; }
    if (slashes != 1) return 0;
    if (first == repo) return 0;            /* empty owner */
    if (*(first + 1) == '\0') return 0;     /* empty name */
    return 1;
}

/* _parse_entries: returns malloc'd JSON array of normalized entry objects. *//* PoP: pindex_parse_entries @ hermes_cli/plugin_index.py:_parse_entries */

char *pindex_parse_entries(const char *raw_json) {
    json_t *raw = json_parse(raw_json, NULL);
    char *out = (char *)malloc(2); out[0] = '['; out[1] = '\0';
    size_t outlen = 1;
    if (raw) {
        json_t *items = NULL;
        if (raw->type == JSON_OBJECT) {
            json_t *pl = json_object_get(raw, "plugins");
            if (pl && pl->type == JSON_ARRAY) items = pl;
        } else if (raw->type == JSON_ARRAY) {
            items = raw;
        }
        if (items) {
            size_t n = json_len(items);
            for (size_t i = 0; i < n; i++) {
                json_t *item = json_array_get(items, i);
                if (!item || item->type != JSON_OBJECT) continue;
                const char *name = json_get_str(item, "name", "");
                const char *repo = json_get_str(item, "repo", "");
                if (!name || !*name) continue;
                if (!pindex_valid_repo(repo)) continue;
                /* build normalized entry object */
                char name_e[512], repo_e[512], ref_e[512];
                snprintf(name_e, sizeof(name_e), "%s", name);
                /* strip */
                char *ns = name_e; while (*ns==' '||*ns=='\t') ns++;
                size_t L = strlen(ns); while (L>0 && (ns[L-1]==' '||ns[L-1]=='\t')) ns[--L]=0;
                char *rs = (char *)repo; while (*rs==' '||*rs=='\t') rs++;
                L = strlen(rs); while (L>0 && (rs[L-1]==' '||rs[L-1]=='\t')) rs[--L]=0;
                snprintf(repo_e, sizeof(repo_e), "%s", rs);
                const char *ref = json_get_str(item, "ref", "");
                snprintf(ref_e, sizeof(ref_e), "%s", ref ? ref : "");

                /* subdir: strip leading/trailing '/' */
                const char *sd = json_get_str(item, "subdir", NULL);
                char sd_e[512]; sd_e[0]='\0';
                if (sd && *sd) {
                    const char *s = sd; while (*s==' '||*s=='\t'||*s=='/') s++;
                    snprintf(sd_e, sizeof(sd_e), "%s", s);
                    L = strlen(sd_e); while (L>0 && (sd_e[L-1]=='/'||sd_e[L-1]==' '||sd_e[L-1]=='\t')) sd_e[--L]=0;
                }

                char entry[1024];
                if (*sd_e)
                    snprintf(entry, sizeof(entry),
                        "{\"name\":%s,\"repo\":%s,\"ref\":%s,\"subdir\":%s}",
                        json_serialize(json_string(ns)),
                        json_serialize(json_string(repo_e)),
                        json_serialize(json_string(ref_e)),
                        json_serialize(json_string(sd_e)));
                else
                    snprintf(entry, sizeof(entry),
                        "{\"name\":%s,\"repo\":%s,\"ref\":%s}",
                        json_serialize(json_string(ns)),
                        json_serialize(json_string(repo_e)),
                        json_serialize(json_string(ref_e)));
                size_t need = outlen + (outlen>1?1:0) + strlen(entry) + 1;
                out = (char *)realloc(out, need);
                strcat(out, (outlen>1)?",":"");
                strcat(out, entry);
                outlen += (outlen>1?1:0) + strlen(entry);
            }
        }
        json_free(raw);
    }
    out = (char *)realloc(out, outlen + 2);
    strcat(out, "]");
    return out;
}

/* resolve_name: returns JSON {"entry": name|null, "candidates":[names]} *//* PoP: pindex_resolve_name @ hermes_cli/plugin_index.py:resolve_name */

char *pindex_resolve_name(const char *entries_json, const char *name) {
    json_t *entries = json_parse(entries_json, NULL);
    char *out = (char *)malloc(128);
    char candidates[4096]; candidates[0]='\0'; size_t cl=0;
    char *entry_name = NULL;
    if (entries && entries->type == JSON_ARRAY) {
        char *lname = strdup(name); char *q = lname;
        while (*q==' '||*q=='\t') q++;
        size_t L = strlen(q); while (L>0 && (q[L-1]==' '||q[L-1]=='\t')) q[--L]=0;
        /* lowercase */
        for (int i=0; q[i]; i++) q[i]=(char)tolower((unsigned char)q[i]);
        size_t n = json_len(entries);
        int exact_count = 0; const char *exact_name = NULL;
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const char *en = json_get_str(e, "name", "");
            if (!en) continue;
            char low[512]; snprintf(low, sizeof(low), "%s", en);
            for (int k=0; low[k]; k++) low[k]=(char)tolower((unsigned char)low[k]);
            if (strcmp(low, q) == 0) {
                exact_count++;
                exact_name = en;
                /* append to candidates */
                char *esc = json_serialize(json_string(en));
                size_t need = cl + (cl?1:0) + strlen(esc?esc:"") + 1;
                if (need < sizeof(candidates)) {
                    strcat(candidates, (cl?",":"")); strcat(candidates, esc?esc:"\"\""); cl = need-1;
                }
                if (esc) free(esc);
            }
        }
        if (exact_count == 1 && exact_name) entry_name = strdup(exact_name);
        free(lname);
    }
    if (entry_name) {
        char *esc = json_serialize(json_string(entry_name));
        snprintf(out, 128, "{\"entry\":%s,\"candidates\":[%s]}", esc?esc:"\"\"", candidates);
        if (esc) free(esc);
        free(entry_name);
    } else {
        snprintf(out, 128, "{\"entry\":null,\"candidates\":[%s]}", candidates);
    }
    json_free(entries);
    return out;
}

/* search_index: returns JSON array of entry names ranked (desc score, then name) *//* PoP: pindex_search_index @ hermes_cli/plugin_index.py:search_index */

char *pindex_search_index(const char *entries_json, const char *term, const char *capability) {
    json_t *entries = json_parse(entries_json, NULL);
    char *out = (char *)malloc(2); out[0]='['; out[1]='\0';
    size_t outlen = 1;
    if (entries && entries->type == JSON_ARRAY) {
        size_t n = json_len(entries);
        /* collect (name, score) */
        char **names = (char **)calloc(n?n:1, sizeof(char*));
        double *scores = (double *)calloc(n?n:1, sizeof(double));
        size_t m = 0;
        char *lt = strdup(term?term:""); char *qt = lt;
        while (*qt==' '||*qt=='\t') qt++;
        size_t L = strlen(qt); while (L>0 && (qt[L-1]==' '||qt[L-1]=='\t')) qt[--L]=0;
        for (int i=0; qt[i]; i++) qt[i]=(char)tolower((unsigned char)qt[i]);
        char *lcap = capability ? strdup(capability) : NULL;
        if (lcap) for (int i=0; lcap[i]; i++) lcap[i]=(char)tolower((unsigned char)lcap[i]);

        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const char *name = json_get_str(e, "name", "");
            if (!name) continue;
            /* capability filter */
            if (lcap && *lcap) {
                json_t *caps = json_object_get(e, "capabilities");
                int ok = 0;
                if (caps && caps->type == JSON_ARRAY) {
                    size_t cn = json_len(caps);
                    for (size_t j=0; j<cn && !ok; j++) {
                        json_t *c = json_array_get(caps, j);
                        if (c && c->type == JSON_STRING && c->str_val) {
                            char lc[256]; snprintf(lc, sizeof(lc), "%s", c->str_val);
                            for (int k=0; lc[k]; k++) lc[k]=(char)tolower((unsigned char)lc[k]);
                            if (strcmp(lc, lcap)==0) ok=1;
                        }
                    }
                }
                if (!ok) continue;
            }
            char lname[512]; snprintf(lname, sizeof(lname), "%s", name);
            for (int k=0; lname[k]; k++) lname[k]=(char)tolower((unsigned char)lname[k]);
            const char *desc = json_get_str(e, "description", "");
            char ldesc[1024]; snprintf(ldesc, sizeof(ldesc), "%s", desc?desc:"");
            for (int k=0; ldesc[k]; k++) ldesc[k]=(char)tolower((unsigned char)ldesc[k]);
            const char *author = json_get_str(e, "author", "");
            char lauth[512]; snprintf(lauth, sizeof(lauth), "%s", author?author:"");
            for (int k=0; lauth[k]; k++) lauth[k]=(char)tolower((unsigned char)lauth[k]);

            double score = 0.0;
            if (strcmp(qt, lname) == 0) score = 100.0;
            if (strstr(lname, qt)) score = score>80.0?score:80.0;
            /* tags */
            json_t *tags = json_object_get(e, "tags");
            if (tags && tags->type == JSON_ARRAY) {
                size_t tn = json_len(tags);
                for (size_t j=0; j<tn; j++) {
                    json_t *t = json_array_get(tags, j);
                    if (!t || t->type != JSON_STRING || !t->str_val) continue;
                    char lt2[512]; snprintf(lt2, sizeof(lt2), "%s", t->str_val);
                    for (int k=0; lt2[k]; k++) lt2[k]=(char)tolower((unsigned char)lt2[k]);
                    if (strcmp(qt, lt2)==0) score = score>70.0?score:70.0;
                    if (strstr(lt2, qt)) score = score>55.0?score:55.0;
                }
            }
            if (strstr(ldesc, qt)) score = score>50.0?score:50.0;
            if (strstr(lauth, qt)) score = score>40.0?score:40.0;
            double ratio = pindex_lcs_ratio(qt, lname);
            if (ratio >= 0.6) {
                double rscore = ratio * 60.0;
                if (rscore > score) score = rscore;
            }
            if (*qt && score <= 0.0) continue;  /* empty term => browse all */
            names[m] = strdup(name);
            scores[m] = score;
            m++;
        }
        /* sort by (-score, name) */
        for (size_t i = 1; i < m; i++) {
            char *kn = names[i]; double ks = scores[i];
            size_t j = i;
            while (j > 0 && (scores[j-1] < ks - 1e-9 || (scores[j-1] <= ks + 1e-9 && strcmp(names[j-1], kn) > 0))) {
                names[j] = names[j-1]; scores[j] = scores[j-1]; j--;
            }
            names[j] = kn; scores[j] = ks;
        }
        for (size_t i = 0; i < m; i++) {
            char *esc = json_serialize(json_string(names[i]));
            size_t need = outlen + (outlen>1?1:0) + strlen(esc?esc:"") + 1;
            out = (char *)realloc(out, need);
            strcat(out, (outlen>1)?",":""); strcat(out, esc?esc:"\"\"");
            outlen += (outlen>1?1:0)+strlen(esc?esc:"");
            if (esc) free(esc); free(names[i]);
        }
        free(names); free(scores); free(lt); if (lcap) free(lcap);
    }
    out = (char *)realloc(out, outlen + 2);
    strcat(out, "]");
    json_free(entries);
    return out;
}
