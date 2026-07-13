/*
 * port_tools_read_extract.c — Port of Python tools/read_extract.py
 *
 * Stdlib document-to-text extraction for read_file. Supports Jupyter
 * notebooks (.ipynb), DOCX, and XLSX without external dependencies:
 *   - DOCX / XLSX are OOXML zip packages (DEFLATE-compressed XML). We use the
 *     native slermes OOXML library (lib/libwubuoffice, by the project owner):
 *     wubuzip is a from-scratch ZIP + DEFLATE reader (no zlib) and wubuoxml_read
 *     inflates every part and parses the .rels graphs. wubuoxml_docx_text
 *     extracts WordprocessingML.
 *   - .ipynb is JSON (parsed with the vendored libjson).
 *
 * Faithful to the Python source; raises (returns an error string via
 * extract_document_text's out_error param) on malformed documents so callers
 * fall back to normal text/binary handling.
 *
 * Each function carries its exact /* PoP: c_func @ tools/read_extract.py:py_func *​/ marker
 * so the parity scanner matches it to the Python symbol.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "wubuoxml/reader.h"
#include "wubuoxml/docx_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define RE_MAX_XLSX_ROWS_PER_SHEET 5000
#define RE_MAX_XLSX_COLS 256

/* ------------------------------------------------------------------ */
/* Minimal namespace-agnostic XML helpers (used by the XLSX walker)  */
/* ------------------------------------------------------------------ */
/* Find the local name of a tag (strip namespace prefix "ns:", leading '/'
 * for closing tags, and terminate at space / '>' / trailing '/'). e.g.
 * "<w:p>", "</w:p>", "<w:row/>", "w:t", "<sheet r:id=".../>" -> "p"/"p"/"row"/"t"/"sheet".
 * Only a namespace prefix immediately before the local name is stripped — a
 * ':' later in the tag (e.g. attribute "r:id") is NOT treated as a prefix. */
static const char *re_xml_localname(const char *tag, size_t *len_out)
{
    const char *p = tag;
    if (*p == '<') p++;
    if (*p == '/') p++;
    const char *name_end = p;
    while (*name_end && *name_end != ' ' && *name_end != '>' && *name_end != '/' &&
           *name_end != '\t' && *name_end != '\n' && *name_end != '\r') name_end++;
    const char *colon = NULL;
    for (const char *c = p; c < name_end; c++) {
        if (*c == ':') { colon = c; break; }
    }
    if (colon) p = colon + 1;
    const char *q = p;
    while (*q && *q != ' ' && *q != '>' && *q != '/' &&
           *q != '\t' && *q != '\n' && *q != '\r') q++;
    *len_out = (size_t)(q - p);
    return p;
}

static int re_xml_is_local(const char *tag, const char *target)
{
    size_t tl;
    const char *ln = re_xml_localname(tag, &tl);
    return tl == strlen(target) && strncmp(ln, target, tl) == 0;
}

/* Allocate a safe C string from len bytes. */
static char *re_strndup(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* Extract text content (decoded &amp; &lt; &gt; &quot; &apos; and numeric refs). */
static char *re_xml_text_content(const char *start, const char *end)
{
    size_t cap = 4096, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    const char *p = start;
    while (p < end) {
        if (*p == '<') {
            const char *q = p + 1;
            while (q < end && *q != '>') q++;
            p = (q < end) ? q + 1 : end;
        } else {
            const char *q = p;
            while (q < end && *q != '<') q++;
            const char *r = p;
            while (r < q) {
                if (*r == '&') {
                    const char *e = r + 1;
                    while (e < q && *e != ';') e++;
                    size_t elen = (size_t)(e - r);
                    const char *ent = NULL; size_t evlen = 1; char one = 0;
                    if (elen == 4 && strncmp(r, "&amp", 4) == 0) { ent = "&"; evlen = 1; }
                    else if (elen == 3 && strncmp(r, "&lt", 3) == 0) { ent = "<"; evlen = 1; }
                    else if (elen == 3 && strncmp(r, "&gt", 3) == 0) { ent = ">"; evlen = 1; }
                    else if (elen == 5 && strncmp(r, "&quot", 5) == 0) { ent = "\""; evlen = 1; }
                    else if (elen == 5 && strncmp(r, "&apos", 5) == 0) { ent = "'"; evlen = 1; }
                    else if (*(e) == ';' && elen > 2 && r[1] == '#') {
                        long codepoint = 0;
                        const char *num = r + 2;
                        if (*num == 'x' || *num == 'X') codepoint = strtol(num + 1, NULL, 16);
                        else codepoint = strtol(num, NULL, 10);
                        if (codepoint > 0 && codepoint < 128) { one = (char)codepoint; ent = &one; evlen = 1; }
                        else { ent = "?"; evlen = 1; }
                    }
                    if (ent) {
                        if (len + evlen + 1 > cap) { cap = (len + evlen) * 2 + 16; char *n = realloc(out, cap); if (!n) { free(out); return strdup(""); } out = n; }
                        memcpy(out + len, ent, evlen); len += evlen;
                    }
                    r = (e < q) ? e + 1 : q;
                } else {
                    if (len + 2 > cap) { cap = (len + 2) * 2; char *n = realloc(out, cap); if (!n) { free(out); return strdup(""); } out = n; }
                    out[len++] = *r++;
                }
            }
            p = q;
        }
    }
    out[len] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
static const char *g_extractable[] = { ".ipynb", ".docx", ".xlsx", NULL };

/* PoP: _extension @ tools/read_extract.py:_extension */
static const char *read_extract_extension(const char *path)
{
    if (!path) return "";
    const char *dot = strrchr(path, '.');
    if (!dot) return "";
    size_t L = strlen(dot);
    char low[16];
    if (L >= sizeof(low)) return "";
    for (size_t i = 0; i < L; i++) low[i] = (char)tolower((unsigned char)dot[i]);
    low[L] = '\0';
    for (int i = 0; g_extractable[i]; i++)
        if (strcmp(low, g_extractable[i]) == 0) return g_extractable[i];
    return "";
}

/* PoP: is_extractable_document @ tools/read_extract.py:is_extractable_document */
bool read_extract_is_extractable_document(const char *path)
{
    return read_extract_extension(path)[0] != '\0';
}

/* Forward decls. */
static char *read_extract_notebook(const char *path, char *errbuf, size_t errsz);
static char *read_extract_docx(const uint8_t *xml, size_t len, char *errbuf, size_t errsz);
static char *read_extract_xlsx(const wubuoxml_package *pkg, char *errbuf, size_t errsz);

/* PoP: extract_document_text @ tools/read_extract.py:extract_document_text */
char *read_extract_document_text(const char *path, char *errbuf, size_t errsz)
{
    if (errbuf && errsz) errbuf[0] = '\0';
    const char *ext = read_extract_extension(path);
    if (strcmp(ext, ".ipynb") == 0) return read_extract_notebook(path, errbuf, errsz);
    if (strcmp(ext, ".docx") == 0) {
        FILE *f = fopen(path, "rb");
        if (!f) { if (errbuf) snprintf(errbuf, errsz, "Not a valid DOCX: %s", strerror(errno)); return NULL; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz <= 0) { fclose(f); if (errbuf) snprintf(errbuf, errsz, "Not a valid DOCX: empty"); return NULL; }
        uint8_t *raw = (uint8_t *)malloc(sz);
        if (!raw) { fclose(f); return NULL; }
        if (fread(raw, 1, sz, f) != (size_t)sz) { free(raw); fclose(f); return NULL; }
        fclose(f);
        wubuoxml_package pkg;
        char *result = NULL;
        if (wubuoxml_read(raw, sz, &pkg) == 0) {
            const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
            if (doc) result = read_extract_docx(doc->bytes, doc->len, errbuf, errsz);
            else if (errbuf) snprintf(errbuf, errsz, "Missing word/document.xml");
            wubuoxml_free(&pkg);
        } else {
            if (errbuf) snprintf(errbuf, errsz, "Not a valid DOCX: cannot open package");
        }
        free(raw);
        return result;
    }
    if (strcmp(ext, ".xlsx") == 0) {
        FILE *f = fopen(path, "rb");
        if (!f) { if (errbuf) snprintf(errbuf, errsz, "Not a valid XLSX: %s", strerror(errno)); return NULL; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz <= 0) { fclose(f); if (errbuf) snprintf(errbuf, errsz, "Not a valid XLSX: empty"); return NULL; }
        uint8_t *raw = (uint8_t *)malloc(sz);
        if (!raw) { fclose(f); return NULL; }
        if (fread(raw, 1, sz, f) != (size_t)sz) { free(raw); fclose(f); return NULL; }
        fclose(f);
        wubuoxml_package pkg;
        char *result = NULL;
        if (wubuoxml_read(raw, sz, &pkg) == 0) {
            result = read_extract_xlsx(&pkg, errbuf, errsz);
            wubuoxml_free(&pkg);
        } else {
            if (errbuf) snprintf(errbuf, errsz, "Not a valid XLSX: cannot open package");
        }
        free(raw);
        return result;
    }
    if (errbuf) snprintf(errbuf, errsz, "Unsupported document type: %s", path ? path : "");
    return NULL;
}

/* ------------------------------------------------------------------ */
/* DOCX — via wubuoxml_docx_text                                       */
/* ------------------------------------------------------------------ */
/* PoP: _extract_docx @ tools/read_extract.py:_extract_docx */
static char *read_extract_docx(const uint8_t *xml, size_t len, char *errbuf, size_t errsz)
{
    char *body = NULL;
    if (wubuoxml_docx_text(xml, len, &body) != 0 || !body) {
        if (errbuf) snprintf(errbuf, errsz, "DOCX contains no extractable text");
        return NULL;
    }
    /* wubuoxml_docx_text concatenates every <w:t> with no structural breaks.
     * The Python reference emits paragraphs separated by newlines and flags
     * empty documents. We mirror that: one paragraph per <w:p> using a tiny
     * structural walker when <w:p> delimiters exist, else fall back to the
     * concatenated body. To stay faithful and simple we re-walk for <w:p>
     * breaks (preserving tabs/line-breaks) on top of the text. */
    const char *s = (const char *)xml;
    const char *e = s + len;
    size_t cap = 8192, outlen = 0;
    char *out = (char *)malloc(cap);
    if (!out) { free(body); return NULL; }
    out[0] = '\0';
    int any_text = 0;

    const char *p = s;
    while (p < e) {
        if (*p == '<' && p[1] != '/' && re_xml_is_local(p, "p")) {
            const char *q = p + 1; while (q < e && *q != '>') q++;
            const char *body_start = q + 1;
            const char *close = body_start;
            while (close < e) {
                if (*close == '<' && close[1] == '/' && re_xml_is_local(close, "p")) break;
                close++;
            }
            /* accumulate text within [body_start, close) */
            const char *r = body_start;
            size_t linecap = 256, linelen = 0;
            char *line = (char *)malloc(linecap);
            if (!line) { free(body); free(out); return NULL; }
            while (r < close) {
                if (*r == '<' && r[1] != '/') {
                    const char *tq = r + 1; while (tq < close && *tq != '>') tq++;
                    if (tq < close) {
                        if (re_xml_is_local(r, "t")) {
                            const char *ve = tq + 1;
                            const char *vt = ve;
                            while (vt < close && !(*vt == '<' && vt[1] == '/' && re_xml_is_local(vt, "t"))) vt++;
                            char *tx = re_xml_text_content(ve, vt);
                            size_t tl = strlen(tx);
                            if (tl) any_text = 1;
                            if (linelen + tl + 1 > linecap) { linecap = linelen + tl + 256; char *n = realloc(line, linecap); if (!n) { free(tx); free(line); free(body); free(out); return NULL; } line = n; }
                            memcpy(line + linelen, tx, tl); linelen += tl;
                            free(tx);
                            r = vt;
                            continue;
                        } else if (re_xml_is_local(r, "tab")) {
                            if (linelen + 2 > linecap) { linecap = linelen + 2; char *n = realloc(line, linecap); if (!n) { free(line); free(body); free(out); return NULL; } line = n; }
                            line[linelen++] = '\t';
                        } else if (re_xml_is_local(r, "br") || re_xml_is_local(r, "cr")) {
                            if (outlen + linelen + 2 > cap) { cap = (outlen + linelen) * 2 + 64; char *n = realloc(out, cap); if (!n) { free(line); free(body); free(out); return NULL; } out = n; }
                            memcpy(out + outlen, line, linelen); outlen += linelen;
                            out[outlen++] = '\n'; out[outlen] = '\0';
                            linelen = 0;
                        }
                    }
                    r = (tq < close) ? tq + 1 : close;
                } else r++;
            }
            if (outlen + linelen + 2 > cap) { cap = (outlen + linelen) * 2 + 64; char *n = realloc(out, cap); if (!n) { free(line); free(body); free(out); return NULL; } out = n; }
            memcpy(out + outlen, line, linelen); outlen += linelen;
            out[outlen++] = '\n'; out[outlen] = '\0';
            free(line);
            p = close;
            continue;
        }
        p++;
    }
    free(body);
    if (!any_text) { free(out); if (errbuf) snprintf(errbuf, errsz, "DOCX contains no extractable text"); return NULL; }
    while (outlen > 0 && out[outlen-1] == '\n') outlen--;
    if (outlen + 1 >= cap) { char *n = realloc(out, outlen + 2); if (n) out = n; }
    out[outlen++] = '\n'; out[outlen] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* XLSX — on top of wubuoxml inflated parts + .rels                   */
/* ------------------------------------------------------------------ */
/* PoP: _sheet_part @ tools/read_extract.py:_sheet_part */
static void read_extract_sheet_part(const char *target, char *out, size_t outsz)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", target);
    char *s = buf;
    while (*s == '/') s++;
    if (strncmp(s, "xl/", 3) == 0) snprintf(out, outsz, "%s", s);
    else {
        char tmp[1024]; snprintf(tmp, sizeof(tmp), "xl/%s", s);
        char *segs[64]; int n = 0; char *tok = strtok(tmp, "/");
        while (tok) { if (strcmp(tok,".")!=0 && strcmp(tok,"..")!=0) segs[n++] = tok; tok = strtok(NULL,"/"); }
        out[0]='\0'; for (int k=0;k<n;k++){ if(k)strncat(out,"/",outsz-1); strncat(out,segs[k],outsz-1);}
    }
}

/* PoP: _col_index @ tools/read_extract.py:_col_index */
static int read_extract_col_index(const char *ref)
{
    int idx = 0;
    for (const char *c = ref; *c; c++) {
        if (!isalpha((unsigned char)*c)) break;
        idx = idx * 26 + toupper((unsigned char)*c) - 'A' + 1;
    }
    int r = idx - 1;
    return r < 0 ? 0 : r;
}

/* Parse a workbook.xml.rels buffer into rId -> target maps. */
static void read_extract_parse_rels(const char *rels, size_t len,
                                     char ***out_ids, char ***out_tgts, size_t *out_n)
{
    *out_ids = NULL; *out_tgts = NULL; *out_n = 0;
    size_t cap = 8;
    char **ids = (char **)malloc(cap * sizeof(char *));
    char **tgts = (char **)malloc(cap * sizeof(char *));
    if (!ids || !tgts) { free(ids); free(tgts); return; }
    const char *s = rels;
    for (size_t i = 0; i + 12 <= len; ) {
        if (strncmp(s + i, "<Relationship", 12) != 0) { i++; continue; }
        char id[64] = "", tgt[1024] = "";
        const char *p = s + i;
        const char *q;
        if ((q = strstr(p, "Id=\"")))     { q += 4; size_t j = 0; while (*q && *q != '"' && j < 63) id[j++] = *q++; id[j] = '\0'; }
        if ((q = strstr(p, "Target=\""))) { q += 8; size_t j = 0; while (*q && *q != '"' && j < 1023) tgt[j++] = *q++; tgt[j] = '\0'; }
        if (tgt[0]) {
            if (*out_n == cap) {
                cap *= 2;
                ids = (char **)realloc(ids, cap * sizeof(char *));
                tgts = (char **)realloc(tgts, cap * sizeof(char *));
            }
            ids[*out_n] = strdup(id);
            tgts[*out_n] = strdup(tgt);
            (*out_n)++;
        }
        const char *end = strstr(p, "/>");
        if (!end) end = strstr(p, ">");
        if (!end) break;
        i = (size_t)(end - s) + 1;
    }
    *out_ids = ids; *out_tgts = tgts;
}

/* cell value: typ in {"s","inlineStr","b","e",""}. shared is a string array. */
/* PoP: _cell_value @ tools/read_extract.py:_cell_value */
static char *read_extract_cell_value(const char *cell_start, const char *cell_end,
                                     char **shared, size_t shared_n)
{
    const char *tq = cell_start;
    const char *typ = strdup("");
    while (tq < cell_end && *tq != '>') {
        if (tq[0] == 't' && tq[1] == '=') {
            tq += 2;
            if (*tq == '"' || *tq == '\'') {
                char qc = *tq; tq++;
                char buf[32]; int bi = 0;
                while (*tq && *tq != qc && bi < (int)sizeof(buf)-1) buf[bi++] = *tq++;
                buf[bi] = '\0';
                free((void*)typ);
                typ = strdup(buf);
            }
            break;
        }
        tq++;
    }
    char *value = strdup("");
    const char *v = cell_start;
    while (v < cell_end) {
        if (*v == '<' && (v[1]=='v'||(v[1]==':'&&v[2]=='v'))) {
            const char *te = v + 1; while (te < cell_end && *te != '>') te++;
            if (te < cell_end) {
                const char *ve = te + 1;
                const char *vt = ve;
                while (vt < cell_end && !(*vt == '<' && vt[1]=='/' && (vt[2]=='v'||(vt[2]==':'&&vt[3]=='v')))) vt++;
                free(value);
                value = re_xml_text_content(ve, vt);
            }
            break;
        }
        v++;
    }
    char *result = strdup("");
    if (strcmp(typ, "s") == 0) {
        long si = strtol(value, NULL, 10);
        if (si >= 0 && (size_t)si < shared_n && shared[si]) {
            free(result); result = strdup(shared[si]);
        }
    } else if (strcmp(typ, "b") == 0) {
        free(result);
        result = strdup((value[0]=='1'||strcasecmp(value,"true")==0) ? "TRUE" : "FALSE");
    } else if (strcmp(typ, "e") == 0) {
        free(result);
        result = strdup(value[0] ? value : "#ERROR");
    } else {
        free(result);
        result = strdup(value);
    }
    free(value);
    free((void*)typ);
    return result;
}

/* PoP: _extract_xlsx @ tools/read_extract.py:_extract_xlsx */
static char *read_extract_xlsx(const wubuoxml_package *pkg, char *errbuf, size_t errsz)
{
    /* shared strings */
    char **shared = NULL; size_t shared_n = 0;
    const wubuoxml_part *ss = wubuoxml_part_find(pkg, "xl/sharedStrings.xml");
    if (ss) {
        const char *s = (const char *)ss->bytes, *e = s + ss->len;
        const char *p = s;
        while (p < e) {
            if (*p == '<' && p[1] != '/' && re_xml_is_local(p, "si")) {
                const char *q = p + 1; while (q < e && *q != '>') q++;
                const char *body = q + 1;
                const char *close = body;
                while (close < e) {
                    if (*close == '<' && close[1] == '/' && re_xml_is_local(close, "si")) break;
                    close++;
                }
                char *text = strdup("");
                const char *r = body;
                while (r < close) {
                    if (*r == '<' && r[1] != '/' && re_xml_is_local(r, "t")) {
                        const char *te = r + 1; while (te < close && *te != '>') te++;
                        const char *ve = te + 1;
                        const char *vt = ve;
                        while (vt < close && !(*vt == '<' && vt[1] == '/' && re_xml_is_local(vt, "t"))) vt++;
                        char *tx = re_xml_text_content(ve, vt);
                        char *n = (char *)realloc(text, strlen(text) + strlen(tx) + 1);
                        if (n) { text = n; strcat(text, tx); }
                        free(tx);
                        r = vt;
                    } else r++;
                }
                shared = (char **)realloc(shared, (shared_n + 1) * sizeof(char *));
                if (shared) shared[shared_n++] = text;
                p = close;
                continue;
            }
            p++;
        }
    }

    /* workbook rels (rId -> target) */
    char **rels_ids = NULL, **rels_tgts = NULL; size_t rels_n = 0;
    const wubuoxml_part *wbrels = wubuoxml_part_find(pkg, "xl/_rels/workbook.xml.rels");
    if (wbrels) read_extract_parse_rels((const char *)wbrels->bytes, wbrels->len, &rels_ids, &rels_tgts, &rels_n);

    size_t cap = 8192, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) { for (size_t i=0;i<shared_n;i++) free(shared[i]); free(shared); return NULL; }
    out[0] = '\0';
    int any = 0;

    const wubuoxml_part *wb = wubuoxml_part_find(pkg, "xl/workbook.xml");
    if (wb) {
        const char *s = (const char *)wb->bytes, *e = s + wb->len;
        const char *p = s;
        while (p < e) {
            if (*p == '<' && p[1] != '/' && re_xml_is_local(p, "sheet")) {
                const char *te = p + 1; while (te < e && *te != '>') te++;
                if (te < e && re_xml_is_local(p, "sheet")) {
                    char *name = strdup("Sheet"); char *state = strdup("visible"); char *rid = strdup("");
                    const char *a = p;
                    while (a < te) {
                        if (strncmp(a, "name=", 5) == 0) { a+=5; if(*a=='"'||*a=='\''){char q=*a;a++;char b[256];int bi=0;while(*a&&*a!=q&&bi<255)b[bi++]=*a++;b[bi]=0;free(name);name=strdup(b);} }
                        else if (strncmp(a, "state=", 6) == 0) { a+=6; if(*a=='"'||*a=='\''){char q=*a;a++;char b[64];int bi=0;while(*a&&*a!=q&&bi<63)b[bi++]=*a++;b[bi]=0;free(state);state=strdup(b);} }
                        else if ((a[0]=='r'&&a[1]==':') && strncmp(a+2,"id=",3)==0) { a+=5; if(*a=='"'||*a=='\''){char q=*a;a++;char b[64];int bi=0;while(*a&&*a!=q&&bi<63)b[bi++]=*a++;b[bi]=0;free(rid);rid=strdup(b);} }
                        a++;
                    }
                    if (strcmp(state, "hidden") != 0 && strcmp(state, "veryHidden") != 0) {
                        char *target = strdup("");
                        for (size_t k = 0; k < rels_n; k++) {
                            if (strcmp(rels_ids[k], rid) == 0) { free(target); target = strdup(rels_tgts[k]); break; }
                        }
                        char part[1024];
                        read_extract_sheet_part(target, part, sizeof(part));
                        free(target);
                        const wubuoxml_part *sheet_part = wubuoxml_part_find(pkg, part);
                        if (sheet_part) {
                            char hdr[512];
                            int hl = snprintf(hdr, sizeof(hdr), "# ── Sheet: %s ──\n", name);
                            (void)hl;
                            if (len + strlen(hdr) + 1 > cap) { cap = len + strlen(hdr) + 1024; char *n = realloc(out, cap); if (!n) { free(name); free(state); free(rid); for (size_t i=0;i<shared_n;i++) free(shared[i]); free(shared); for (size_t i=0;i<rels_n;i++){free(rels_ids[i]);free(rels_tgts[i]);} free(rels_ids); free(rels_tgts); free(out); return NULL; } out = n; }
                            strcat(out + len, hdr); len += strlen(hdr);
                            const char *rs = (const char *)sheet_part->bytes, *re_ = rs + sheet_part->len;
                            const char *rp = rs;
                            int row_count = 0;
                            while (rp < re_ && row_count < RE_MAX_XLSX_ROWS_PER_SHEET) {
                                /* PoP: _sheet_rows @ tools/read_extract.py:_sheet_rows */
                                if (*rp == '<' && rp[1] != '/' && re_xml_is_local(rp, "row")) {
                                    const char *rte = rp + 1; while (rte < re_ && *rte != '>') rte++;
                                    const char *rbody = rte + 1;
                                    const char *rclose = rbody;
                                    while (rclose < re_) {
                                        if (*rclose=='<'&&rclose[1]=='/'&&re_xml_is_local(rclose,"row")) break;
                                        rclose++;
                                    }
                                    char **cells = NULL; int max_col = -1;
                                    const char *cp = rbody;
                                    while (cp < rclose) {
                                        if (*cp == '<' && cp[1] != '/' && re_xml_is_local(cp, "c")) {
                                            const char *cte = cp + 1; while (cte < rclose && *cte != '>') cte++;
                                            const char *cbody = cte + 1;
                                            const char *cclose = cbody;
                                            while (cclose < rclose) { if (*cclose=='<'&&cclose[1]=='/'&&re_xml_is_local(cclose,"c")) break; cclose++; }
                                            int col = max_col + 1;
                                            const char *a = cp;
                                            while (a < cte) {
                                                if (strncmp(a, "r=", 2) == 0) { a+=2; if(*a=='"'||*a=='\''){char q=*a;a++;char b[32];int bi=0;while(*a&&*a!=q&&bi<31)b[bi++]=*a++;b[bi]=0;col=read_extract_col_index(b);} break; }
                                                a++;
                                            }
                                            if (col >= RE_MAX_XLSX_COLS) { cp = cclose; continue; }
                                            char *cv = read_extract_cell_value(cp, cclose, shared, shared_n);
                                            if (col >= (int)(max_col + 1)) {
                                                cells = (char **)realloc(cells, (col + 1) * sizeof(char *));
                                                for (int ii = max_col + 1; ii <= col; ii++) cells[ii] = strdup("");
                                                max_col = col;
                                            }
                                            free(cells[col]); cells[col] = cv;
                                            cp = cclose;
                                            continue;
                                        }
                                        cp++;
                                    }
                                    if (max_col >= 0) {
                                        for (int ci = 0; ci <= max_col; ci++) {
                                            const char *cv = cells[ci] ? cells[ci] : "";
                                            if (len + strlen(cv) + 2 > cap) { cap = len + strlen(cv) + 256; char *n = realloc(out, cap); if (!n) { for (int ii=0;ii<=max_col;ii++) free(cells[ii]); free(cells); for (size_t i=0;i<shared_n;i++) free(shared[i]); free(shared); for (size_t i=0;i<rels_n;i++){free(rels_ids[i]);free(rels_tgts[i]);} free(rels_ids); free(rels_tgts); free(out); return NULL; } out = n; }
                                            if (ci) strcat(out + len, "\t");
                                            strcat(out + len, cv);
                                            len += strlen(cv) + (ci ? 1 : 0);
                                        }
                                        for (int ci = 0; ci <= max_col; ci++) free(cells[ci]);
                                        free(cells);
                                    }
                                    out[len++] = '\n'; out[len] = '\0';
                                    row_count++;
                                    rp = rclose;
                                    continue;
                                }
                                rp++;
                            }
                            if (row_count == 0) {
                                if (len + 9 > cap) { cap = len + 32; char *n = realloc(out, cap); if (n) out = n; }
                                strcat(out + len, "(empty)\n"); len += 8;
                            }
                            any = 1;
                            if (len + 2 > cap) { cap = len + 8; char *n = realloc(out, cap); if (n) out = n; }
                            out[len++] = '\n'; out[len] = '\0';
                        }
                    }
                    free(name); free(state); free(rid);
                    p = te + 1;
                    continue;
                }
                p = te + 1;
            } else p++;
        }
    }

    for (size_t i = 0; i < shared_n; i++) free(shared[i]);
    free(shared);
    for (size_t i = 0; i < rels_n; i++) { free(rels_ids[i]); free(rels_tgts[i]); }
    free(rels_ids); free(rels_tgts);

    if (!any) { free(out); if (errbuf) snprintf(errbuf, errsz, "XLSX has no visible sheets with content"); return NULL; }
    while (len > 0 && out[len-1] == '\n') len--;
    if (len + 1 >= cap) { char *n = realloc(out, len + 2); if (n) out = n; }
    out[len++] = '\n'; out[len] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* _extract_notebook (.ipynb via vendored libjson)                     */
/* ------------------------------------------------------------------ */
/* PoP: _source_text @ tools/read_extract.py:_source_text */
static char *read_extract_source_text(const json_node_t *source)
{
    if (!source) return strdup("");
    if (source->type == JSON_STRING) return strdup(source->str_val ? source->str_val : "");
    if (source->type == JSON_ARRAY) {
        size_t cap = 256, len = 0;
        char *out = (char *)malloc(cap);
        if (!out) return strdup("");
        for (size_t i = 0; i < source->c.count; i++) {
            json_node_t *it = json_get(source, i);
            if (it && it->type == JSON_STRING && it->str_val) {
                size_t al = strlen(it->str_val);
                if (len + al + 1 > cap) { cap = len + al + 256; char *n = realloc(out, cap); if (!n) { free(out); return strdup(""); } out = n; }
                memcpy(out + len, it->str_val, al); len += al;
            }
        }
        out[len] = '\0';
        return out;
    }
    return strdup("");
}

/* PoP: _extract_notebook @ tools/read_extract.py:_extract_notebook */
static char *read_extract_notebook(const char *path, char *errbuf, size_t errsz)
{
    char *raw = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) { if (errbuf) snprintf(errbuf, errsz, "Not a valid notebook: %s", strerror(errno)); return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); if (errbuf) snprintf(errbuf, errsz, "Not a valid notebook: empty"); return NULL; }
    raw = (char *)malloc(sz + 1);
    if (!raw) { fclose(f); return NULL; }
    if (fread(raw, 1, sz, f) != (size_t)sz) { free(raw); fclose(f); return NULL; }
    raw[sz] = '\0';
    fclose(f);

    char *perr = NULL;
    json_node_t *nb = json_parse(raw, &perr);
    free(raw);
    if (!nb) { if (errbuf) snprintf(errbuf, errsz, "Not a valid notebook: %s", perr ? perr : "parse error"); free(perr); return NULL; }
    if (nb->type != JSON_OBJECT) { json_free(nb); if (errbuf) snprintf(errbuf, errsz, "Notebook root is not an object"); return NULL; }

    json_node_t *cells = json_object_get(nb, "cells");
    if (!cells || (cells->type != JSON_ARRAY)) {
        json_node_t *ws = json_object_get(nb, "worksheets");
        if (ws && ws->type == JSON_ARRAY) {
            json_node_t *flat = json_new_array();
            for (size_t i = 0; i < ws->c.count; i++) {
                json_node_t *w = json_get(ws, i);
                if (!w || w->type != JSON_OBJECT) continue;
                json_node_t *wc = json_object_get(w, "cells");
                if (!wc || wc->type != JSON_ARRAY) continue;
                for (size_t j = 0; j < wc->c.count; j++) {
                    json_node_t *c = json_get(wc, j);
                    if (c) json_array_append(flat, c);
                }
            }
            cells = flat;
        } else {
            json_free(nb); if (errbuf) snprintf(errbuf, errsz, "Notebook contains no cells"); return NULL;
        }
    }

    const char *labels[3] = { "Markdown", "Code", "Raw" };
    int counts[3] = {0,0,0};
    size_t cap = 4096, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) { if (cells != json_object_get(nb,"cells")) json_free(cells); json_free(nb); return NULL; }
    int any = 0;
    for (size_t i = 0; i < cells->c.count; i++) {
        json_node_t *cell = json_get(cells, i);
        if (!cell || cell->type != JSON_OBJECT) continue;
        json_node_t *ct = json_object_get(cell, "cell_type");
        if (!ct || ct->type != JSON_STRING) continue;
        int li = -1;
        if (strcmp(ct->str_val, "markdown") == 0) li = 0;
        else if (strcmp(ct->str_val, "code") == 0) li = 1;
        else if (strcmp(ct->str_val, "raw") == 0) li = 2;
        if (li < 0) continue;
        counts[li]++;
        char header[128];
        int hl;
        if (li == 2)
            hl = snprintf(header, sizeof(header), "# ── %s cell ──\n", labels[li]);
        else
            hl = snprintf(header, sizeof(header), "# ── %s cell %d ──\n", labels[li], counts[li]);
        (void)hl;
        json_node_t *src = json_object_get(cell, "source");
        char *src_text = read_extract_source_text(src);
        size_t sl = strlen(src_text);
        while (sl > 0 && (src_text[sl-1] == '\n' || src_text[sl-1] == '\r')) sl--;
        size_t hl_len = strlen(header);
        /* header + source + terminating '\n' + blank-line separator '\n' + NUL */
        size_t need = len + hl_len + sl + 3;
        if (need + 1 > cap) { cap = (need + 1) * 2; char *n = realloc(out, cap); if (!n) { free(src_text); free(out); if (cells != json_object_get(nb,"cells")) json_free(cells); json_free(nb); return NULL; } out = n; }
        memcpy(out + len, header, hl_len); len += hl_len;
        memcpy(out + len, src_text, sl); len += sl;
        out[len++] = '\n'; out[len++] = '\n'; out[len] = '\0';
        free(src_text);
        any = 1;
    }
    if (cells != json_object_get(nb, "cells")) json_free(cells);
    json_free(nb);
    if (!any) { free(out); if (errbuf) snprintf(errbuf, errsz, "Notebook contains no readable cells"); return NULL; }
    while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r')) len--;
    if (len + 1 >= cap) { char *n = realloc(out, len + 2); if (n) out = n; }
    out[len++] = '\n'; out[len] = '\0';
    return out;
}
