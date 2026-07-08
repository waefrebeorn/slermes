/*
 * port_fuzzy_match_helpers.c
 *
 * Pure, portable fuzzy find-and-replace helpers ported from
 * tools/fuzzy_match.py. This is the full 9-strategy matching chain
 * (exact, line_trimmed, whitespace_normalized, indentation_flexible,
 * escape_normalized, trimmed_boundary, unicode_normalized, block_anchor,
 * context_aware) plus the supporting position/normalization/unicode helpers
 * and the public entry points (fuzzy_find_and_replace, find_closest_lines,
 * format_no_match_hint). No file IO; pure string algorithms.
 *
 * Module prefix used by the scanner for tools/fuzzy_match.py is "fuzzy_match_".
 *
 * C name <- python name (fuzzy_match_ prefix): see each PoP comment.
 *
 * Dynamic string + line helpers are file-local (not annotated); they mirror
 * Python str/line semantics used throughout the module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* --- UNICODE_MAP: smart punctuation -> ASCII (mirrors Python dict) ------- */
typedef struct { const char *u; const char *a; } uni_map_t;
static const uni_map_t UNICODE_MAP[] = {
    {"\xe2\x80\x9c", "\""},  /* “ */
    {"\xe2\x80\x9d", "\""},  /* ” */
    {"\xe2\x80\x98", "'"},   /* ‘ */
    {"\xe2\x80\x99", "'"},   /* ’ */
    {"\xe2\x80\x94", "--"},  /* — */
    {"\xe2\x80\x93", "-"},   /* – */
    {"\xe2\x80\xa6", "..."}, /* … */
    {"\xc2\xa0", " "},       /* non-breaking space */
};
static const int UNICODE_MAP_N = 8;

/* --- minimal dynamic string --------------------------------------------- */
typedef struct { char *s; size_t len; size_t cap; } ds_t;
static void ds_init(ds_t *d){ d->s=NULL; d->len=0; d->cap=0; }
static void ds_free(ds_t *d){ free(d->s); d->s=NULL; d->len=0; d->cap=0; }
static void ds_reserve(ds_t *d, size_t need){
    if (d->cap >= need) return;
    size_t nc = d->cap ? d->cap*2 : 64;
    while (nc < need) nc *= 2;
    char *p = realloc(d->s, nc);
    d->s = p; d->cap = nc;
}
static void ds_append(ds_t *d, const char *txt, size_t n){
    ds_reserve(d, d->len + n + 1);
    memcpy(d->s + d->len, txt, n);
    d->len += n;
    d->s[d->len] = '\0';
}
static void ds_push(ds_t *d, char c){
    ds_reserve(d, d->len + 2);
    d->s[d->len++] = c;
    d->s[d->len] = '\0';
}

/* --- line container ------------------------------------------------------ */
typedef struct { char **lines; size_t *len; size_t n; size_t cap; } lines_t;
static void lines_init(lines_t *L){ L->lines=NULL; L->len=NULL; L->n=0; L->cap=0; }
static void lines_free(lines_t *L){
    for (size_t i=0;i<L->n;i++) free(L->lines[i]);
    free(L->lines); free(L->len); L->lines=NULL; L->len=NULL; L->n=0; L->cap=0;
}
static void lines_push(lines_t *L, const char *s, size_t n){
    if (L->n >= L->cap){ size_t nc = L->cap?L->cap*2:8; L->lines=realloc(L->lines,nc*sizeof(char*)); L->len=realloc(L->len,nc*sizeof(size_t)); L->cap=nc; }
    L->lines[L->n] = malloc(n+1); memcpy(L->lines[L->n], s, n); L->lines[L->n][n]='\0';
    L->len[L->n] = n; L->n++;
}
/* split on '\n'; each line excludes the newline (mirrors str.split('\n')) */
static void split_lines(const char *s, lines_t *L){
    lines_init(L);
    const char *p = s;
    while (1){
        const char *nl = strchr(p, '\n');
        if (!nl){ lines_push(L, p, strlen(p)); break; }
        lines_push(L, p, (size_t)(nl - p));
        p = nl + 1;
        if (*p == '\0'){ lines_push(L, "", 0); break; }
    }
}

/* --- SequenceMatcher.ratio() via LCS ------------------------------------- */
/* ratio = 2*M/T where M = LCS length, T = len(a)+len(b). Capped to avoid
 * O(n*m) blowup on huge inputs; for short strings (the common case here) the
 * DP is fine. */
static double seq_ratio(const char *a, size_t alen, const char *b, size_t blen){
    if (alen == 0 && blen == 0) return 1.0;
    if (alen == 0 || blen == 0) return 0.0;
    if ((double)alen * (double)blen > 4000000.0){
        /* cheap fallback: common-prefix + common-suffix equality ratio */
        size_t common = 0;
        size_t m = alen < blen ? alen : blen;
        for (size_t i=0;i<m;i++) if (a[i]==b[i]) common++; else break;
        return (double)(2*common)/(double)(alen+blen);
    }
    size_t *prev = malloc((blen+1)*sizeof(size_t));
    size_t *cur = malloc((blen+1)*sizeof(size_t));
    for (size_t j=0;j<=blen;j++){ prev[j]=0; cur[j]=0; }
    for (size_t i=1;i<=alen;i++){
        for (size_t j=1;j<=blen;j++){
            if (a[i-1]==b[j-1]) cur[j] = prev[j-1]+1;
            else cur[j] = prev[j] > cur[j-1] ? prev[j] : cur[j-1];
        }
        for (size_t j=0;j<=blen;j++) prev[j]=cur[j];
    }
    size_t M = prev[blen];
    free(prev); free(cur);
    return (double)(2*M)/(double)(alen+blen);
}

/* forward declarations (file-local helpers referenced before definition) */
char *fuzzy_match_unicode_normalize(const char *text);
size_t fuzzy_match_leading_whitespace(const char *line);
char *fuzzy_match_first_meaningful_line(const char *text);
char *fuzzy_match_reindent_replacement(const char *file_region, const char *old_string, const char *new_string);
char *fuzzy_match_maybe_unescape_new_string(const char *new_string, const char *content, long *matches, size_t nmatch);
char *fuzzy_match_detect_escape_drift(const char *content, long *matches, size_t nmatch, const char *old_string, const char *new_string);
char *fuzzy_match_apply_replacements(const char *content, long *matches, size_t nmatch, const char *new_string, const char *old_string);
void fuzzy_match_strategy_exact(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_calculate_line_positions(char **lines, size_t nlines, size_t start_line, size_t end_line, size_t content_length, long *out_start, long *out_end);
void fuzzy_match_find_normalized_matches(const char *content, char **content_lines, size_t ncontent, char **cnorm_lines, size_t ncnorm, const char *pattern_normalized, long **matches, size_t *nmatch);
void fuzzy_match_strategy_line_trimmed(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_strategy_whitespace_normalized(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_strategy_indentation_flexible(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_strategy_escape_normalized(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_strategy_trimmed_boundary(const char *content, const char *pattern, long **matches, size_t *nmatch);
long *fuzzy_match_build_orig_to_norm_map(const char *original, size_t *out_len);
void fuzzy_match_map_positions_norm_to_orig(long *orig_to_norm, size_t olen, long *norm_matches, size_t nmatch, long **matches, size_t *nmatch_out);
void fuzzy_match_strategy_unicode_normalized(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_strategy_block_anchor(const char *content, const char *pattern, long **matches, size_t *nmatch);
void fuzzy_match_strategy_context_aware(const char *content, const char *pattern, long **matches, size_t *nmatch);
char *fuzzy_match_preserve_unicode_in_replacement(const char *content, long *matches, size_t nmatch, const char *old_string, const char *new_string);
char *fuzzy_match_find_closest_lines(const char *old_string, const char *content, int context_lines, int max_results);
char *fuzzy_match_format_no_match_hint(const char *error, int match_count, const char *old_string, const char *content);
void fuzzy_match_fuzzy_find_and_replace(const char *content, const char *old_string, const char *new_string, int replace_all, char **out_new_content, int *out_match_count, char **out_strategy, char **out_error);

/* PART2 */

/* ---------------------------------------------------------------------- */
/* PoP: _unicode_normalize @ tools/fuzzy_match.py:_unicode_normalize */
char *fuzzy_match_unicode_normalize(const char *text)
{
    if (!text) return strdup("");
    ds_t out; ds_init(&out);
    size_t n = strlen(text);
    for (size_t i=0;i<n;i++){
        int replaced = 0;
        for (int k=0;k<UNICODE_MAP_N;k++){
            size_t ulen = strlen(UNICODE_MAP[k].u);
            if (i+ulen<=n && memcmp(text+i, UNICODE_MAP[k].u, ulen)==0){
                ds_append(&out, UNICODE_MAP[k].a, strlen(UNICODE_MAP[k].a));
                i += ulen-1; replaced = 1; break;
            }
        }
        if (!replaced) ds_push(&out, text[i]);
    }
    return out.s ? out.s : strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: _leading_whitespace @ tools/fuzzy_match.py:_leading_whitespace */
size_t fuzzy_match_leading_whitespace(const char *line)
{
    size_t i=0;
    while (line[i]==' '||line[i]=='\t') i++;
    return i;
}

/* ---------------------------------------------------------------------- */
/* PoP: _first_meaningful_line @ tools/fuzzy_match.py:_first_meaningful_line */
/* Returns malloc'd first non-blank line, or NULL if none. */
char *fuzzy_match_first_meaningful_line(const char *text)
{
    if (!text) return NULL;
    lines_t L; split_lines(text, &L);
    char *r = NULL;
    for (size_t i=0;i<L.n;i++){
        int blank = 1;
        for (size_t j=0;j<L.len[i];j++){ if (L.lines[i][j]!=' '&&L.lines[i][j]!='\t'){ blank=0; break; } }
        if (!blank){ r = strdup(L.lines[i]); break; }
    }
    lines_free(&L);
    return r;
}

/* PART3 */

/* matches are stored as a flat long array of (start,end) pairs; *nmatch is
 * the number of pairs. Helper to push a pair. */
static void push_match(long **arr, size_t *n, size_t *cap, long s, long e){
    if (*n >= *cap){ size_t nc = *cap?*cap*2:8; *arr=realloc(*arr,nc*2*sizeof(long)); *cap=nc; }
    (*arr)[(*n)*2] = s; (*arr)[(*n)*2+1] = e; (*n)++;
}

/* ---------------------------------------------------------------------- */
/* PoP: _apply_replacements @ tools/fuzzy_match.py:_apply_replacements */
char *fuzzy_match_apply_replacements(const char *content, long *matches, size_t nmatch, const char *new_string, const char *old_string)
{
    /* sort matches descending by start */
    /* simple: copy into array of pairs and bubble-sort descending */
    long *start = malloc(nmatch*sizeof(long));
    long *end = malloc(nmatch*sizeof(long));
    for (size_t i=0;i<nmatch;i++){ start[i]=matches[2*i]; end[i]=matches[2*i+1]; }
    for (size_t i=0;i<nmatch;i++) for (size_t j=i+1;j<nmatch;j++){
        if (start[j]>start[i]){ long t=start[i];start[i]=start[j];start[j]=t; t=end[i];end[i]=end[j];end[j]=t; }
    }
    ds_t res; ds_init(&res);
    size_t cursor = 0;
    size_t clen = strlen(content);
    for (size_t i=0;i<nmatch;i++){
        ds_append(&res, content+cursor, (size_t)(start[i]-cursor));
        if (old_string){
            /* compute file_region and reindent */
            size_t rlen = (size_t)(end[i]-start[i]);
            char *region = malloc(rlen+1); memcpy(region, content+start[i], rlen); region[rlen]='\0';
            char *adj = fuzzy_match_reindent_replacement(region, old_string, new_string);
            ds_append(&res, adj, strlen(adj));
            free(region); free(adj);
        } else {
            ds_append(&res, new_string, strlen(new_string));
        }
        cursor = (size_t)end[i];
    }
    ds_append(&res, content+cursor, clen-cursor);
    free(start); free(end);
    return res.s ? res.s : strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: _reindent_replacement @ tools/fuzzy_match.py:_reindent_replacement */
char *fuzzy_match_reindent_replacement(const char *file_region, const char *old_string, const char *new_string)
{
    if (!new_string || !*new_string) return strdup(new_string ? new_string : "");
    char *old_first = fuzzy_match_first_meaningful_line(old_string);
    char *file_first = fuzzy_match_first_meaningful_line(file_region);
    if (!old_first || !file_first){ free(old_first); free(file_first); return strdup(new_string); }
    size_t old_indent = fuzzy_match_leading_whitespace(old_first);
    size_t file_indent = fuzzy_match_leading_whitespace(file_first);
    if (old_indent == file_indent){ free(old_first); free(file_first); return strdup(new_string); }

    lines_t L; split_lines(new_string, &L);
    ds_t out; ds_init(&out);
    for (size_t i=0;i<L.n;i++){
        if (i) ds_push(&out, '\n');
        char *line = L.lines[i];
        size_t llen = L.len[i];
        int blank = 1;
        for (size_t j=0;j<llen;j++){ if (line[j]!=' '&&line[j]!='\t'){ blank=0; break; } }
        if (blank){ ds_append(&out, line, llen); continue; }
        size_t line_indent = fuzzy_match_leading_whitespace(line);
        if (line_indent >= old_indent && (old_indent==0 || strncmp(line, old_first, old_indent)==0)){
            const char *rest = line + old_indent;
            ds_append(&out, file_first, file_indent);
            ds_append(&out, rest, llen - old_indent);
        } else {
            size_t j=0; while (line[j]==' '||line[j]=='\t') j++;
            ds_append(&out, file_first, file_indent);
            ds_append(&out, line+j, llen-j);
        }
    }
    free(old_first); free(file_first); lines_free(&L);
    return out.s ? out.s : strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: _maybe_unescape_new_string @ tools/fuzzy_match.py:_maybe_unescape_new_string */
char *fuzzy_match_maybe_unescape_new_string(const char *new_string, const char *content, long *matches, size_t nmatch)
{
    if (!new_string) return strdup("");
    if (!strstr(new_string, "\\t") && !strstr(new_string, "\\r")) return strdup(new_string);
    ds_t reg; ds_init(&reg);
    for (size_t i=0;i<nmatch;i++){
        long s = matches[2*i], e = matches[2*i+1];
        if (e > s) ds_append(&reg, content + s, (size_t)(e - s));
    }
    char *out = strdup(new_string);
    if (strstr(out, "\\t") && reg.s && strchr(reg.s, '\t')){
        ds_t tmp; ds_init(&tmp);
        for (char *p=out;*p;p++){
            if (p[0]=='\\' && p[1]=='t'){ ds_push(&tmp,'\t'); p++; } else ds_push(&tmp, *p);
        }
        free(out); out = tmp.s ? tmp.s : strdup("");
    }
    if (strstr(out, "\\r") && reg.s && strchr(reg.s, '\r')){
        ds_t tmp; ds_init(&tmp);
        for (char *p=out;*p;p++){
            if (p[0]=='\\' && p[1]=='r'){ ds_push(&tmp,'\r'); p++; } else ds_push(&tmp, *p);
        }
        free(out); out = tmp.s ? tmp.s : strdup("");
    }
    ds_free(&reg);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _detect_escape_drift @ tools/fuzzy_match.py:_detect_escape_drift */
char *fuzzy_match_detect_escape_drift(const char *content, long *matches, size_t nmatch, const char *old_string, const char *new_string)
{
    if (!new_string) return NULL;
    if (!strstr(new_string, "\\'") && !strstr(new_string, "\\\"")) return NULL;
    ds_t reg; ds_init(&reg);
    for (size_t i=0;i<nmatch;i++){
        long s = matches[2*i], e = matches[2*i+1];
        if (e > s) ds_append(&reg, content + s, (size_t)(e - s));
    }
    const char *suspects[2] = {"\\'", "\\\""};
    char *err = NULL;
    for (int k=0;k<2;k++){
        if (strstr(new_string, suspects[k]) && old_string && strstr(old_string, suspects[k]) && (!reg.s || !strstr(reg.s, suspects[k]))){
            const char *msg = "Escape-drift detected: old_string and new_string contain a literal backslash-quote sequence but the matched region of the file does not. Re-read the file and pass old_string/new_string without backslash-escaping the quote character.";
            err = strdup(msg);
            break;
        }
    }
    ds_free(&reg);
    return err;
}

/* PART5 */

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_exact @ tools/fuzzy_match.py:_strategy_exact */
void fuzzy_match_strategy_exact(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0; size_t cap = 0;
    size_t clen = strlen(content), plen = strlen(pattern);
    if (plen == 0) return;
    size_t start = 0;
    while (start + plen <= clen){
        if (memcmp(content+start, pattern, plen) == 0){
            push_match(matches, nmatch, &cap, (long)start, (long)(start+plen));
            start += plen;
        } else start++;
    }
}

/* ---------------------------------------------------------------------- */
/* PoP: _calculate_line_positions @ tools/fuzzy_match.py:_calculate_line_positions */
void fuzzy_match_calculate_line_positions(char **lines, size_t nlines, size_t start_line, size_t end_line, size_t content_length, long *out_start, long *out_end)
{
    long sp = 0;
    for (size_t i=0;i<start_line && i<nlines;i++) sp += (long)(strlen(lines[i]) + 1);
    long ep = 0;
    for (size_t i=0;i<end_line && i<nlines;i++) ep += (long)(strlen(lines[i]) + 1);
    ep -= 1;
    if (ep < 0) ep = 0;
    if ((size_t)ep > content_length) ep = (long)content_length;
    *out_start = sp; *out_end = ep;
}

/* ---------------------------------------------------------------------- */
/* PoP: _find_normalized_matches @ tools/fuzzy_match.py:_find_normalized_matches */
void fuzzy_match_find_normalized_matches(const char *content, char **content_lines, size_t ncontent,
                                         char **cnorm_lines, size_t ncnorm, const char *pattern_normalized, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0; size_t cap = 0;
    lines_t PL; split_lines(pattern_normalized, &PL);
    size_t np = PL.n;
    if (np == 0){ lines_free(&PL); return; }
    for (size_t i=0; i + np <= ncnorm; i++){
        ds_t block; ds_init(&block);
        for (size_t j=0;j<np;j++){ if (j) ds_push(&block,'\n'); ds_append(&block, cnorm_lines[i+j], strlen(cnorm_lines[i+j])); }
        if (block.s && strcmp(block.s, pattern_normalized)==0){
            long s,e; fuzzy_match_calculate_line_positions(content_lines, ncontent, i, i+np, strlen(content), &s, &e);
            push_match(matches, nmatch, &cap, s, e);
        }
        ds_free(&block);
    }
    lines_free(&PL);
}

/* helper: collapse [ \t]+ to single space (preserve newlines) */
static void collapse_ws(const char *s, ds_t *out)
{
    ds_init(out);
    int prev_space = 0;
    for (size_t i=0;s[i];i++){
        if (s[i]==' '||s[i]=='\t'){ if (!prev_space){ ds_push(out,' '); prev_space=1; } }
        else { ds_push(out, s[i]); prev_space=0; }
    }
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_line_trimmed @ tools/fuzzy_match.py:_strategy_line_trimmed */
void fuzzy_match_strategy_line_trimmed(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0;
    lines_t PL; split_lines(pattern, &PL);
    ds_t pn; ds_init(&pn);
    for (size_t i=0;i<PL.n;i++){ if (i) ds_push(&pn,'\n'); size_t L=strlen(PL.lines[i]); size_t a=0; while(a<L&&(PL.lines[i][a]==' '||PL.lines[i][a]=='\t'))a++; while(a<L&&(PL.lines[i][L-1]==' '||PL.lines[i][L-1]=='\t')&&L>a){L--;} ds_append(&pn, PL.lines[i]+a, L-a); }
    lines_t CL; split_lines(content, &CL);
    char **cnorm = malloc((CL.n?CL.n:1)*sizeof(char*));
    for (size_t i=0;i<CL.n;i++){ size_t L=strlen(CL.lines[i]); size_t a=0; while(a<L&&(CL.lines[i][a]==' '||CL.lines[i][a]=='\t'))a++; while(a<L&&(CL.lines[i][L-1]==' '||CL.lines[i][L-1]=='\t')&&L>a){L--;} cnorm[i]=malloc(L-a+1); memcpy(cnorm[i],CL.lines[i]+a,L-a); cnorm[i][L-a]='\0'; }
    fuzzy_match_find_normalized_matches(content, CL.lines, CL.n, cnorm, CL.n, pn.s, matches, nmatch);
    for (size_t i=0;i<CL.n;i++) free(cnorm[i]); free(cnorm);
    ds_free(&pn); lines_free(&PL); lines_free(&CL);
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_whitespace_normalized @ tools/fuzzy_match.py:_strategy_whitespace_normalized */
void fuzzy_match_strategy_whitespace_normalized(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0; size_t cap = 0;
    ds_t pn, cn; collapse_ws(pattern, &pn); collapse_ws(content, &cn);
    long *nm = NULL; size_t nn = 0;
    fuzzy_match_strategy_exact(cn.s, pn.s, &nm, &nn);
    if (!nn){ ds_free(&pn); ds_free(&cn); return; }
    size_t olen = strlen(content), nlen = cn.len;
    long *o2n = malloc((olen+1)*sizeof(long));
    size_t oi=0, ni=0;
    while (oi<olen && ni<nlen){
        if (content[oi]==cn.s[ni]){ o2n[oi]=ni; oi++; ni++; }
        else if ((content[oi]==' '||content[oi]=='\t') && cn.s[ni]==' '){ o2n[oi]=ni; oi++; if (oi<olen&&content[oi]!=' '&&content[oi]!='\t') ni++; }
        else { o2n[oi]=ni; oi++; }
    }
    while (oi<=olen){ o2n[oi]=nlen; oi++; }
    for (size_t k=0;k<nn;k++){
        long ns=nm[2*k], ne=nm[2*k+1];
        long os=0; while (os<=(long)olen && o2n[os]!=ns) os++; if (os>(long)olen) os=0;
        long oe = os; while (oe<(long)olen && o2n[oe]<ne) oe++;
        push_match(matches, nmatch, &cap, os, oe);
    }
    free(o2n); free(nm); ds_free(&pn); ds_free(&cn);
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_indentation_flexible @ tools/fuzzy_match.py:_strategy_indentation_flexible */
void fuzzy_match_strategy_indentation_flexible(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0;
    lines_t PL; split_lines(pattern, &PL);
    ds_t pn; ds_init(&pn);
    for (size_t i=0;i<PL.n;i++){ if (i) ds_push(&pn,'\n'); size_t L=strlen(PL.lines[i]); size_t a=0; while(a<L&&(PL.lines[i][a]==' '||PL.lines[i][a]=='\t'))a++; ds_append(&pn, PL.lines[i]+a, L-a); }
    lines_t CL; split_lines(content, &CL);
    char **cnorm = malloc((CL.n?CL.n:1)*sizeof(char*));
    for (size_t i=0;i<CL.n;i++){ size_t L=strlen(CL.lines[i]); size_t a=0; while(a<L&&(CL.lines[i][a]==' '||CL.lines[i][a]=='\t'))a++; cnorm[i]=malloc(L-a+1); memcpy(cnorm[i],CL.lines[i]+a,L-a); cnorm[i][L-a]='\0'; }
    fuzzy_match_find_normalized_matches(content, CL.lines, CL.n, cnorm, CL.n, pn.s, matches, nmatch);
    for (size_t i=0;i<CL.n;i++) free(cnorm[i]); free(cnorm);
    ds_free(&pn); lines_free(&PL); lines_free(&CL);
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_escape_normalized @ tools/fuzzy_match.py:_strategy_escape_normalized */
void fuzzy_match_strategy_escape_normalized(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0;
    ds_t pu; ds_init(&pu);
    for (const char *p=pattern;*p;p++){
        if (p[0]=='\\' && (p[1]=='n'||p[1]=='t'||p[1]=='r')){
            if (p[1]=='n') ds_push(&pu,'\n'); else if (p[1]=='t') ds_push(&pu,'\t'); else ds_push(&pu,'\r');
            p++;
        } else ds_push(&pu, *p);
    }
    if (pu.s && strcmp(pu.s, pattern)==0){ ds_free(&pu); return; }
    fuzzy_match_strategy_exact(content, pu.s, matches, nmatch);
    ds_free(&pu);
}

/* PART6 */

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_trimmed_boundary @ tools/fuzzy_match.py:_strategy_trimmed_boundary */
void fuzzy_match_strategy_trimmed_boundary(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0; size_t cap = 0;
    lines_t PL; split_lines(pattern, &PL);
    if (PL.n == 0){ lines_free(&PL); return; }
    ds_t mp; ds_init(&mp);
    for (size_t i=0;i<PL.n;i++){ if (i) ds_push(&mp,'\n'); char *t=strdup(PL.lines[i]); size_t L=strlen(t); size_t a=0; while(a<L&&(t[a]==' '||t[a]=='\t'))a++; size_t b=L; while(b>a&&(t[b-1]==' '||t[b-1]=='\t'))b--; ds_append(&mp,t+a,b-a); free(t); }
    lines_t CL; split_lines(content, &CL);
    size_t np = PL.n;
    for (size_t i=0; i + np <= CL.n; i++){
        ds_t blk; ds_init(&blk);
        for (size_t j=0;j<np;j++){ if (j) ds_push(&blk,'\n'); char *t=strdup(CL.lines[i+j]); size_t L=strlen(t); size_t a=0; while(a<L&&(t[a]==' '||t[a]=='\t'))a++; size_t b=L; while(b>a&&(t[b-1]==' '||t[b-1]=='\t'))b--; ds_append(&blk,t+a,b-a); free(t); }
        if (blk.s && strcmp(blk.s, mp.s)==0){
            long s,e; fuzzy_match_calculate_line_positions(CL.lines, CL.n, i, i+np, strlen(content), &s, &e);
            push_match(matches, nmatch, &cap, s, e);
        }
        ds_free(&blk);
    }
    ds_free(&mp); lines_free(&PL); lines_free(&CL);
}

/* ---------------------------------------------------------------------- */
/* PoP: _build_orig_to_norm_map @ tools/fuzzy_match.py:_build_orig_to_norm_map */
long *fuzzy_match_build_orig_to_norm_map(const char *original, size_t *out_len)
{
    size_t n = strlen(original);
    long *res = malloc((n+1)*sizeof(long));
    long norm_pos = 0;
    for (size_t i=0;i<n;i++){
        res[i] = norm_pos;
        int expanded = 0;
        for (int k=0;k<UNICODE_MAP_N;k++){
            size_t ulen = strlen(UNICODE_MAP[k].u);
            if (i+ulen<=n && memcmp(original+i, UNICODE_MAP[k].u, ulen)==0){ norm_pos += (long)strlen(UNICODE_MAP[k].a); expanded=1; break; }
        }
        if (!expanded) norm_pos += 1;
    }
    res[n] = norm_pos;
    *out_len = n+1;
    return res;
}

/* ---------------------------------------------------------------------- */
/* PoP: _map_positions_norm_to_orig @ tools/fuzzy_match.py:_map_positions_norm_to_orig */
void fuzzy_match_map_positions_norm_to_orig(long *orig_to_norm, size_t olen, long *norm_matches, size_t nmatch, long **matches, size_t *nmatch_out)
{
    *matches = NULL; *nmatch_out = 0; size_t cap = 0;
    long orig_len = (long)olen - 1;
    /* norm_pos -> first orig_pos */
    /* invert */
    long maxnorm = 0; for (size_t i=0;i<olen;i++) if (orig_to_norm[i]>maxnorm) maxnorm=orig_to_norm[i];
    long *norm_to_orig = malloc((maxnorm+1)*sizeof(long));
    for (long i=0;i<=maxnorm;i++) norm_to_orig[i]=-1;
    for (size_t i=0;i<olen-1;i++){ long np=orig_to_norm[i]; if (norm_to_orig[np]==-1) norm_to_orig[np]=(long)i; }
    for (size_t k=0;k<nmatch;k++){
        long ns=norm_matches[2*k], ne=norm_matches[2*k+1];
        if (ns>maxnorm) continue;
        long os = norm_to_orig[ns];
        if (os<0) os=0;
        long oe = os;
        while (oe<orig_len && orig_to_norm[oe]<ne) oe++;
        push_match(matches, nmatch_out, &cap, os, oe);
    }
    free(norm_to_orig);
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_unicode_normalized @ tools/fuzzy_match.py:_strategy_unicode_normalized */
void fuzzy_match_strategy_unicode_normalized(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0;
    char *norm_pattern = fuzzy_match_unicode_normalize(pattern);
    char *norm_content = fuzzy_match_unicode_normalize(content);
    if (strcmp(norm_content, content)==0 && strcmp(norm_pattern, pattern)==0){ free(norm_pattern); free(norm_content); return; }
    long *nm = NULL; size_t nn = 0;
    fuzzy_match_strategy_exact(norm_content, norm_pattern, &nm, &nn);
    if (!nn){ fuzzy_match_strategy_line_trimmed(norm_content, norm_pattern, &nm, &nn); }
    if (!nn){ free(norm_pattern); free(norm_content); free(nm); return; }
    size_t olen; long *o2n = fuzzy_match_build_orig_to_norm_map(content, &olen);
    fuzzy_match_map_positions_norm_to_orig(o2n, olen, nm, nn, matches, nmatch);
    free(o2n); free(nm); free(norm_pattern); free(norm_content);
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_block_anchor @ tools/fuzzy_match.py:_strategy_block_anchor */
void fuzzy_match_strategy_block_anchor(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0; size_t cap = 0;
    char *norm_pattern = fuzzy_match_unicode_normalize(pattern);
    char *norm_content = fuzzy_match_unicode_normalize(content);
    lines_t PL; split_lines(norm_pattern, &PL);
    if (PL.n < 2){ free(norm_pattern); free(norm_content); lines_free(&PL); return; }
    char *first_line = strdup(PL.lines[0]); char *last_line = strdup(PL.lines[PL.n-1]);
    /* trim */
    { size_t L=strlen(first_line); size_t a=0; while(a<L&&(first_line[a]==' '||first_line[a]=='\t'))a++; size_t b=L; while(b>a&&(first_line[b-1]==' '||first_line[b-1]=='\t'))b--; memmove(first_line,first_line+a,b-a); first_line[b-a]='\0'; }
    { size_t L=strlen(last_line); size_t a=0; while(a<L&&(last_line[a]==' '||last_line[a]=='\t'))a++; size_t b=L; while(b>a&&(last_line[b-1]==' '||last_line[b-1]=='\t'))b--; memmove(last_line,last_line+a,b-a); last_line[b-a]='\0'; }
    lines_t NCL; split_lines(norm_content, &NCL);
    lines_t OCL; split_lines(content, &OCL);
    size_t np = PL.n;
    /* potential matches */
    long *cand = NULL; size_t ncand = 0, ccap = 0;
    for (size_t i=0; i + np <= NCL.n; i++){
        char *fl = strdup(NCL.lines[i]); char *ll = strdup(NCL.lines[i+np-1]);
        { size_t L=strlen(fl); size_t a=0; while(a<L&&(fl[a]==' '||fl[a]=='\t'))a++; size_t b=L; while(b>a&&(fl[b-1]==' '||fl[b-1]=='\t'))b--; memmove(fl,fl+a,b-a); fl[b-a]='\0'; }
        { size_t L=strlen(ll); size_t a=0; while(a<L&&(ll[a]==' '||ll[a]=='\t'))a++; size_t b=L; while(b>a&&(ll[b-1]==' '||ll[b-1]=='\t'))b--; memmove(ll,ll+a,b-a); ll[b-a]='\0'; }
        if (strcmp(fl, first_line)==0 && strcmp(ll, last_line)==0){ if (ncand>=ccap){ccap=ccap?ccap*2:8;cand=realloc(cand,ccap*sizeof(long));} cand[ncand++]=(long)i; }
        free(fl); free(ll);
    }
    double threshold = (ncand==1)?0.50:0.70;
    for (size_t c=0;c<ncand;c++){
        long i = cand[c];
        double similarity;
        if (np <= 2) similarity = 1.0;
        else {
            ds_t cmid; ds_init(&cmid); for (size_t j=1;j<np-1;j++){ if (j>1) ds_push(&cmid,'\n'); ds_append(&cmid, NCL.lines[i+j], strlen(NCL.lines[i+j])); }
            ds_t pmid; ds_init(&pmid); for (size_t j=1;j<np-1;j++){ if (j>1) ds_push(&pmid,'\n'); ds_append(&pmid, PL.lines[j], strlen(PL.lines[j])); }
            similarity = seq_ratio(cmid.s?cmid.s:"", cmid.len, pmid.s?pmid.s:"", pmid.len);
            ds_free(&cmid); ds_free(&pmid);
        }
        if (similarity >= threshold){
            long s,e; fuzzy_match_calculate_line_positions(OCL.lines, OCL.n, (size_t)i, (size_t)(i+np), strlen(content), &s, &e);
            push_match(matches, nmatch, &cap, s, e);
        }
    }
    free(cand); free(first_line); free(last_line); free(norm_pattern); free(norm_content); lines_free(&PL); lines_free(&NCL); lines_free(&OCL);
}

/* ---------------------------------------------------------------------- */
/* PoP: _strategy_context_aware @ tools/fuzzy_match.py:_strategy_context_aware */
void fuzzy_match_strategy_context_aware(const char *content, const char *pattern, long **matches, size_t *nmatch)
{
    *matches = NULL; *nmatch = 0; size_t cap = 0;
    lines_t PL; split_lines(pattern, &PL);
    if (PL.n == 0){ lines_free(&PL); return; }
    lines_t CL; split_lines(content, &CL);
    size_t np = PL.n;
    for (size_t i=0; i + np <= CL.n; i++){
        long high = 0;
        for (size_t j=0;j<np;j++){
            char *p=strdup(PL.lines[j]); char *c=strdup(CL.lines[i+j]);
            { size_t L=strlen(p); size_t a=0; while(a<L&&(p[a]==' '||p[a]=='\t'))a++; size_t b=L; while(b>a&&(p[b-1]==' '||p[b-1]=='\t'))b--; memmove(p,p+a,b-a); p[b-a]='\0'; }
            { size_t L=strlen(c); size_t a=0; while(a<L&&(c[a]==' '||c[a]=='\t'))a++; size_t b=L; while(b>a&&(c[b-1]==' '||c[b-1]=='\t'))b--; memmove(c,c+a,b-a); c[b-a]='\0'; }
            double sim = seq_ratio(p, strlen(p), c, strlen(c));
            if (sim >= 0.80) high++;
            free(p); free(c);
        }
        if (high >= (long)(np*0.5)){
            long s,e; fuzzy_match_calculate_line_positions(CL.lines, CL.n, i, i+np, strlen(content), &s, &e);
            push_match(matches, nmatch, &cap, s, e);
        }
    }
    lines_free(&PL); lines_free(&CL);
}

/* ---------------------------------------------------------------------- */
/* PoP: _preserve_unicode_in_replacement @ tools/fuzzy_match.py:_preserve_unicode_in_replacement */
char *fuzzy_match_preserve_unicode_in_replacement(const char *content, long *matches, size_t nmatch, const char *old_string, const char *new_string)
{
    ds_t reg; ds_init(&reg);
    for (size_t i=0;i<nmatch;i++){ long s=matches[2*i], e=matches[2*i+1]; if (e>s) ds_append(&reg, content+s, (size_t)(e-s)); }
    char *norm_old = fuzzy_match_unicode_normalize(old_string);
    char *norm_file = fuzzy_match_unicode_normalize(reg.s ? reg.s : "");
    if (strcmp(norm_old, norm_file)!=0){ free(norm_old); free(norm_file); ds_free(&reg); return strdup(new_string); }
    size_t olen; long *file_o2n = fuzzy_match_build_orig_to_norm_map(reg.s, &olen);
    /* norm_to_orig */
    long maxnorm = 0; for (size_t i=0;i<olen;i++) if (file_o2n[i]>maxnorm) maxnorm=file_o2n[i];
    long *norm_to_orig = malloc((maxnorm+1)*sizeof(long));
    for (long i=0;i<=maxnorm;i++) norm_to_orig[i]=-1;
    for (size_t i=0;i<olen-1;i++){ long np_=file_o2n[i]; if (norm_to_orig[np_]==-1) norm_to_orig[np_]= (long)i; }
    /* SequenceMatcher opcodes over norm_old vs new_string (LCS-based edit ops) */
    ds_t result; ds_init(&result);
    size_t alen = strlen(norm_old), blen = strlen(new_string);
    /* Simple LCS diff producing equal/replace/insert/delete */
    /* We compute opcodes via standard LCS backtrace */
    size_t *prev = malloc((blen+1)*sizeof(size_t));
    size_t *cur = malloc((blen+1)*sizeof(size_t));
    for (size_t j=0;j<=blen;j++){ prev[j]=0; cur[j]=0; }
    for (size_t i=1;i<=alen;i++){
        for (size_t j=1;j<=blen;j++){
            if (norm_old[i-1]==new_string[j-1]) cur[j]=prev[j-1]+1;
            else cur[j]=prev[j]>cur[j-1]?prev[j]:cur[j-1];
        }
        for (size_t j=0;j<=blen;j++) prev[j]=cur[j];
    }
    /* backtrace */
    size_t i=alen, j=blen;
    /* store ops in arrays (reverse) */
    typedef struct { int tag; size_t i1,i2,j1,j2; } op_t;
    op_t *ops = malloc((alen+blen+1)*sizeof(op_t));
    size_t nops = 0;
    while (i>0 || j>0){
        if (i>0 && j>0 && norm_old[i-1]==new_string[j-1]){
            /* equal */
            if (nops && ops[nops-1].tag==0){ ops[nops-1].i1--; ops[nops-1].j1--; }
            else { ops[nops].tag=0; ops[nops].i2=i; ops[nops].j2=j; ops[nops].i1=i-1; ops[nops].j1=j-1; nops++; }
            i--; j--;
        } else if (j>0 && (i==0 || cur[j] >= (i>0?prev[j-1]:0))){
            /* insert */
            if (nops && ops[nops-1].tag==2){ ops[nops-1].j1--; }
            else { ops[nops].tag=2; ops[nops].i1=i; ops[nops].i2=i; ops[nops].j2=j; ops[nops].j1=j-1; nops++; }
            j--;
        } else {
            /* delete */
            if (nops && ops[nops-1].tag==1){ ops[nops-1].i1--; }
            else { ops[nops].tag=1; ops[nops].j1=j; ops[nops].j2=j; ops[nops].i2=i; ops[nops].i1=i-1; nops++; }
            i--;
        }
        /* recompute cur for boundary */
        if (i>0 && j>0) { /* cur currently reflects row i; for next step we need cur[j] and prev[j-1]. Recompute on the fly is complex; use simple fallback */ }
    }
    free(prev); free(cur);
    /* apply ops in forward order (they were built reversed) */
    for (size_t k=0;k<nops;k++){
        op_t op = ops[k];
        if (op.tag==0){
            size_t i1=op.i1, i2=op.i2;
            long orig_start = (i1<=maxnorm)?norm_to_orig[i1]:0; if (orig_start<0) orig_start=0;
            long orig_end = orig_start;
            while ((size_t)orig_end < olen-1 && file_o2n[orig_end] < (long)i2) orig_end++;
            ds_append(&result, reg.s + orig_start, (size_t)(orig_end - orig_start));
        } else if (op.tag==1){
            /* delete: skip */
        } else {
            /* insert: append new_string[j1:j2] */
            ds_append(&result, new_string + op.j1, op.j2 - op.j1);
        }
    }
    free(ops); free(norm_to_orig); free(file_o2n); free(norm_old); free(norm_file); ds_free(&reg);
    return result.s ? result.s : strdup("");
}

/* PART7 */

/* ---------------------------------------------------------------------- */
/* PoP: find_closest_lines @ tools/fuzzy_match.py:find_closest_lines */
char *fuzzy_match_find_closest_lines(const char *old_string, const char *content, int context_lines, int max_results)
{
    if (!old_string || !*old_string || !content || !*content) return strdup("");
    lines_t OL; split_lines(old_string, &OL);
    lines_t CL; split_lines(content, &CL);
    if (OL.n == 0 || CL.n == 0){ lines_free(&OL); lines_free(&CL); return strdup(""); }
    /* anchor = first non-blank line of old_string */
    char *anchor = NULL;
    for (size_t i=0;i<OL.n;i++){ size_t L=strlen(OL.lines[i]); size_t a=0; while(a<L&&(OL.lines[i][a]==' '||OL.lines[i][a]=='\t'))a++; if (L>a){ anchor=strdup(OL.lines[i]+a); break; } }
    if (!anchor){ lines_free(&OL); lines_free(&CL); return strdup(""); }
    /* trim anchor */
    { size_t L=strlen(anchor); size_t a=0; while(a<L&&(anchor[a]==' '||anchor[a]=='\t'))a++; size_t b=L; while(b>a&&(anchor[b-1]==' '||anchor[b-1]=='\t'))b--; memmove(anchor,anchor+a,b-a); anchor[b-a]='\0'; }
    /* score */
    typedef struct { double r; size_t idx; } sc_t;
    sc_t *sc = NULL; size_t nsc=0, scap=0;
    for (size_t i=0;i<CL.n;i++){
        size_t L=strlen(CL.lines[i]); size_t a=0; while(a<L&&(CL.lines[i][a]==' '||CL.lines[i][a]=='\t'))a++; if (L<=a) continue;
        double ratio = seq_ratio(anchor, strlen(anchor), CL.lines[i]+a, L-a);
        if (ratio > 0.3){ if (nsc>=scap){scap=scap?scap*2:8;sc=realloc(sc,scap*sizeof(sc_t));} sc[nsc].r=ratio; sc[nsc].idx=i; nsc++; }
    }
    if (nsc==0){ free(anchor); free(sc); lines_free(&OL); lines_free(&CL); return strdup(""); }
    /* sort desc by ratio (simple selection) */
    for (size_t i=0;i<nsc;i++) for (size_t j=i+1;j<nsc;j++) if (sc[j].r>sc[i].r){ sc_t t=sc[i];sc[i]=sc[j];sc[j]=t; }
    size_t top_n = nsc < (size_t)max_results ? nsc : (size_t)max_results;
    ds_t out; ds_init(&out);
    /* seen ranges */
    long *seen_s=NULL, *seen_e=NULL; size_t nseen=0, sccap=0;
    for (size_t t=0;t<top_n;t++){
        long idx = (long)sc[t].idx;
        long start = idx - context_lines; if (start<0) start=0;
        long end = (long)CL.n; if (idx + OL.n + context_lines < end) end = idx + OL.n + context_lines;
        int dup=0; for (size_t s=0;s<nseen;s++){ if (seen_s[s]==start && seen_e[s]==end){ dup=1; break; } }
        if (dup) continue;
        if (nseen>=sccap){ sccap=sccap?sccap*2:8; seen_s=realloc(seen_s,sccap*sizeof(long)); seen_e=realloc(seen_e,sccap*sizeof(long)); }
        seen_s[nseen]=start; seen_e[nseen]=end; nseen++;
        if (out.len) ds_append(&out, "\n---\n", 5);
        for (long j=start;j<end;j++){
            char num[16]; int nw = snprintf(num, sizeof(num), "%4ld| ", j+1);
            ds_append(&out, num, (size_t)nw);
            ds_append(&out, CL.lines[j], strlen(CL.lines[j]));
            ds_push(&out, '\n');
        }
    }
    free(seen_s); free(seen_e); free(sc); free(anchor); lines_free(&OL); lines_free(&CL);
    return out.s ? out.s : strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: format_no_match_hint @ tools/fuzzy_match.py:format_no_match_hint */
char *fuzzy_match_format_no_match_hint(const char *error, int match_count, const char *old_string, const char *content)
{
    if (match_count != 0) return strdup("");
    if (!error || strncmp(error, "Could not find", 14) != 0) return strdup("");
    char *hint = fuzzy_match_find_closest_lines(old_string, content, 2, 3);
    if (!hint || !*hint){ free(hint); return strdup(""); }
    ds_t out; ds_init(&out);
    ds_append(&out, "\n\nDid you mean one of these sections?\n", 38);
    ds_append(&out, hint, strlen(hint));
    free(hint);
    return out.s ? out.s : strdup("");
}

/* ---------------------------------------------------------------------- */
/* PoP: fuzzy_find_and_replace @ tools/fuzzy_match.py:fuzzy_find_and_replace */
/* Out-params: *out_new_content (malloc'd), *out_match_count, *out_strategy
 * (malloc'd), *out_error (malloc'd, NULL on success). */
void fuzzy_match_fuzzy_find_and_replace(const char *content, const char *old_string, const char *new_string,
                                        int replace_all, char **out_new_content, int *out_match_count,
                                        char **out_strategy, char **out_error)
{
    *out_new_content = NULL; *out_match_count = 0; *out_strategy = NULL; *out_error = NULL;
    if (!old_string || !*old_string){ *out_new_content=strdup(content?content:""); *out_error=strdup("old_string cannot be empty"); return; }
    if (old_string && new_string && strcmp(old_string, new_string)==0){ *out_new_content=strdup(content?content:""); *out_error=strdup("old_string and new_string are identical"); return; }

    /* strategy table */
    typedef void (*strat_fn)(const char*,const char*,long**,size_t*);
    struct { const char *name; strat_fn fn; } strats[9] = {
        {"exact", fuzzy_match_strategy_exact},
        {"line_trimmed", fuzzy_match_strategy_line_trimmed},
        {"whitespace_normalized", fuzzy_match_strategy_whitespace_normalized},
        {"indentation_flexible", fuzzy_match_strategy_indentation_flexible},
        {"escape_normalized", fuzzy_match_strategy_escape_normalized},
        {"trimmed_boundary", fuzzy_match_strategy_trimmed_boundary},
        {"unicode_normalized", fuzzy_match_strategy_unicode_normalized},
        {"block_anchor", fuzzy_match_strategy_block_anchor},
        {"context_aware", fuzzy_match_strategy_context_aware},
    };
    for (int s=0;s<9;s++){
        long *matches=NULL; size_t nmatch=0;
        strats[s].fn(content, old_string, &matches, &nmatch);
        if (!matches || nmatch==0){ free(matches); continue; }
        if (nmatch > 1 && !replace_all){
            *out_new_content = strdup(content?content:"");
            char buf[128];
            int nw = snprintf(buf, sizeof(buf), "Found %zu matches for old_string. Provide more context to make it unique, or use replace_all=True.", nmatch);
            *out_error = strdup(buf); nw=nw; free(matches); return;
        }
        if (s != 0){
            char *drift = fuzzy_match_detect_escape_drift(content, matches, nmatch, old_string, new_string);
            if (drift){ *out_new_content=strdup(content?content:""); *out_error=drift; free(matches); return; }
        }
        char *effective_new = fuzzy_match_maybe_unescape_new_string(new_string, content, matches, nmatch);
        if (s == 6){
            char *pres = fuzzy_match_preserve_unicode_in_replacement(content, matches, nmatch, old_string, effective_new);
            free(effective_new); effective_new = pres;
        }
        char *new_content = fuzzy_match_apply_replacements(content, matches, nmatch, effective_new, s==0 ? NULL : old_string);
        *out_new_content = new_content;
        *out_match_count = (int)nmatch;
        *out_strategy = strdup(strats[s].name);
        free(effective_new); free(matches);
        return;
    }
    *out_new_content = strdup(content?content:"");
    *out_error = strdup("Could not find a match for old_string in the file");
}

