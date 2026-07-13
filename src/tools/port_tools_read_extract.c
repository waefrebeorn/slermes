/*
 * port_tools_read_extract.c — Port of Python tools/read_extract.py
 *
 * Stdlib document-to-text extraction for read_file. Supports Jupyter
 * notebooks (.ipynb), DOCX, and XLSX without external dependencies:
 *   - DOCX / XLSX are OOXML zip packages (DEFLATE-compressed XML). We read
 *     the zip central directory, inflate members with zlib (already linked),
 *     and walk the XML with a tiny namespace-agnostic element scanner.
 *   - .ipynb is JSON (parsed with the vendored libjson).
 *
 * Faithful to the Python source; raises (returns an error string via
 * extract_document_text's out_error param) on malformed documents so callers
 * fall back to normal text/binary handling.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <zlib.h>

#define RE_MAX_XLSX_BYTES (50 * 1024 * 1024)
#define RE_MAX_XLSX_ROWS_PER_SHEET 5000
#define RE_MAX_XLSX_COLS 256

/* ------------------------------------------------------------------ */
/* Minimal ZIP reader (central-directory based, DEFLATE via zlib)     */
/* ------------------------------------------------------------------ */
typedef struct {
    char *name;
    unsigned char *data;
    size_t size;
} re_zip_entry_t;

typedef struct {
    re_zip_entry_t *entries;
    size_t count;
    size_t cap;
} re_zip_t;

static void re_zip_free(re_zip_t *z)
{
    if (!z) return;
    for (size_t i = 0; i < z->count; i++) {
        free(z->entries[i].name);
        free(z->entries[i].data);
    }
    free(z->entries);
    free(z);
}

/* Read a little-endian u32 from buf at off (bounds-checked by caller). */
static unsigned re_le32(const unsigned char *b, size_t off)
{
    return (unsigned)b[off] | ((unsigned)b[off+1] << 8) |
           ((unsigned)b[off+2] << 16) | ((unsigned)b[off+3] << 24);
}
static unsigned re_le16(const unsigned char *b, size_t off)
{
    return (unsigned)b[off] | ((unsigned)b[off+1] << 8);
}

/* Inflate DEFLATE data (raw, no zlib header — OOXML uses raw deflate in zip). */
static unsigned char *re_inflate_raw(const unsigned char *src, size_t srclen, size_t *outlen)
{
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    /* 15 window bits, -MAX_WBITS => raw deflate */
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return NULL;
    strm.next_in = (unsigned char *)src;
    strm.avail_in = (uInt)srclen;
    size_t cap = srclen * 4 + 4096;
    unsigned char *out = (unsigned char *)malloc(cap);
    if (!out) { inflateEnd(&strm); return NULL; }
    size_t total = 0;
    int ret;
    do {
        strm.next_out = out + total;
        strm.avail_out = (uInt)(cap - total);
        ret = inflate(&strm, Z_NO_FLUSH);
        total = cap - strm.avail_out;
        if (ret == Z_OK && strm.avail_out == 0) {
            cap *= 2;
            unsigned char *n = (unsigned char *)realloc(out, cap);
            if (!n) { free(out); inflateEnd(&strm); return NULL; }
            out = n;
        }
    } while (ret == Z_OK);
    inflateEnd(&strm);
    if (ret != Z_STREAM_END && ret != Z_BUF_ERROR) { free(out); return NULL; }
    *outlen = total;
    return out;
}

/* Decompress a stored (method 0) or deflated (method 8) zip entry.
 * Always returns a buffer with a trailing NUL so string scanners are safe. */
static unsigned char *re_decompress(const unsigned char *cdata, size_t clen,
                                     unsigned method, size_t *outlen)
{
    if (method == 0) { /* stored */
        unsigned char *o = (unsigned char *)malloc(clen + 1);
        if (!o) return NULL;
        if (clen) memcpy(o, cdata, clen);
        o[clen] = '\0';
        *outlen = clen;
        return o;
    }
    if (method == 8) { /* deflate */
        unsigned char *o = re_inflate_raw(cdata, clen, outlen);
        if (o) { unsigned char *n = (unsigned char *)realloc(o, *outlen + 1); if (n) { o = n; } o[*outlen] = '\0'; }
        return o;
    }
    return NULL;
}

/* Load all entries of a zip file into re_zip_t. Returns NULL on failure. */
static re_zip_t *re_zip_open(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 22) { fclose(f); return NULL; }
    unsigned char *buf = (unsigned char *)malloc(fsize);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, fsize, f) != (size_t)fsize) { free(buf); fclose(f); return NULL; }
    fclose(f);

    /* Find End Of Central Directory (signature 0x06054b50). */
    size_t eocd = 0;
    for (size_t i = fsize >= 22 ? fsize - 22 : 0; i + 22 <= (size_t)fsize; i--) {
        if (re_le32(buf, i) == 0x06054b50) { eocd = i; break; }
    }
    if (!eocd) { free(buf); return NULL; }
    unsigned cd_offset = re_le32(buf, eocd + 16);
    unsigned cd_count = re_le16(buf, eocd + 10);

    re_zip_t *z = (re_zip_t *)calloc(1, sizeof(re_zip_t));
    if (!z) { free(buf); return NULL; }
    unsigned p = cd_offset;
    for (unsigned n = 0; n < cd_count; n++) {
        if (p + 46 > (size_t)fsize) break;
        if (re_le32(buf, p) != 0x02014b50) break;
        unsigned method = re_le16(buf, p + 10);
        unsigned clen = re_le32(buf, p + 20);
        unsigned ulen = re_le32(buf, p + 24);
        unsigned nlen = re_le16(buf, p + 28);
        unsigned elen = re_le16(buf, p + 30);
        unsigned cl_off = re_le32(buf, p + 42);
        if (p + 46 + nlen > (size_t)fsize) break;
        char *name = (char *)malloc(nlen + 1);
        if (!name) break;
        memcpy(name, buf + p + 46, nlen);
        name[nlen] = '\0';

        /* Read local header to get the compressed data offset + real clen. */
        unsigned char *cdata = NULL;
        size_t outlen = 0;
        if (cl_off + 30 <= (size_t)fsize) {
            unsigned loc_nlen = re_le16(buf, cl_off + 26);
            unsigned loc_elen = re_le16(buf, cl_off + 28);
            unsigned data_off = cl_off + 30 + loc_nlen + loc_elen;
            if (data_off + clen <= (size_t)fsize) {
                cdata = re_decompress(buf + data_off, clen, method, &outlen);
            }
        }
        if (!cdata) {
            /* Fall back to uncompressed size hint if inflate gave nothing. */
            cdata = (unsigned char *)malloc(ulen ? ulen : 1);
            outlen = 0;
        }
        /* grow */
        if (z->count >= z->cap) {
            z->cap = z->cap ? z->cap * 2 : 16;
            re_zip_entry_t *ne = (re_zip_entry_t *)realloc(z->entries, z->cap * sizeof(re_zip_entry_t));
            if (!ne) { free(name); free(cdata); free(buf); re_zip_free(z); return NULL; }
            z->entries = ne;
        }
        z->entries[z->count].name = name;
        z->entries[z->count].data = cdata;
        z->entries[z->count].size = outlen;
        z->count++;
        p += 46 + nlen + elen;
    }
    free(buf);
    return z;
}

static const unsigned char *re_zip_find(re_zip_t *z, const char *name, size_t *size)
{
    for (size_t i = 0; i < z->count; i++) {
        if (strcmp(z->entries[i].name, name) == 0) {
            *size = z->entries[i].size;
            return z->entries[i].data;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Minimal XML scanner (namespace-agnostic: matches by local name)   */
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
    /* Find the end of the tag name (stop at space / '>' / '/'). */
    const char *name_end = p;
    while (*name_end && *name_end != ' ' && *name_end != '>' && *name_end != '/' &&
           *name_end != '\t' && *name_end != '\n' && *name_end != '\r') name_end++;
    /* Look for a namespace prefix ':' only within [p, name_end). */
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

/* Return true if tag's local name equals target (e.g. "t", "p", "sheet"). */
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

/* Extract text content (decoded entities &amp; &lt; &gt; &quot; &apos;) of an XML element
 * region [start,end). Returns malloc'd string (caller frees) or "" on empty. */
static char *re_xml_text_content(const char *start, const char *end)
{
    /* We collect character data outside of tags. */
    size_t cap = 4096, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    const char *p = start;
    while (p < end) {
        if (*p == '<') {
            /* skip tag */
            const char *q = p + 1;
            while (q < end && *q != '>') q++;
            p = (q < end) ? q + 1 : end;
        } else {
            const char *q = p;
            while (q < end && *q != '<') q++;
            /* decode entities in [p,q) */
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
                        /* numeric entity */
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

/* Find the first element with local name `tag` starting at *pp, within `end`.
 * On success sets *pp to just after the opening tag and returns the element
 * text region end (the matching close tag position); caller walks siblings by
 * advancing *pp. Simple, non-nested-aware scan sufficient for OOXML flat use. */
/* For our purposes we implement an iterator-style scan in the callers. */

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
/* PoP: _extension @ tools/read_extract.py:_extension */
static const char *g_extractable[] = { ".ipynb", ".docx", ".xlsx", NULL };

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
    for (int i = 0; g_extractable[i]; i++) {
        if (strcmp(low, g_extractable[i]) == 0) return g_extractable[i];
    }
    return "";
}

/* PoP: is_extractable_document @ tools/read_extract.py:is_extractable_document */
bool read_extract_is_extractable_document(const char *path)
{
    return read_extract_extension(path)[0] != '\0';
}

/* Forward decls. */
static char *read_extract_notebook(const char *path, char *errbuf, size_t errsz);
static char *read_extract_docx(const char *path, char *errbuf, size_t errsz);
static char *read_extract_xlsx(const char *path, char *errbuf, size_t errsz);

/* PoP: extract_document_text @ tools/read_extract.py:extract_document_text */
char *read_extract_document_text(const char *path, char *errbuf, size_t errsz)
{
    if (errbuf && errsz) errbuf[0] = '\0';
    const char *ext = read_extract_extension(path);
    if (strcmp(ext, ".ipynb") == 0) return read_extract_notebook(path, errbuf, errsz);
    if (strcmp(ext, ".docx") == 0)  return read_extract_docx(path, errbuf, errsz);
    if (strcmp(ext, ".xlsx") == 0)  return read_extract_xlsx(path, errbuf, errsz);
    if (errbuf) snprintf(errbuf, errsz, "Unsupported document type: %s", path ? path : "");
    return NULL;
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
        /* fall back to worksheets (legacy) */
        json_node_t *ws = json_object_get(nb, "worksheets");
        if (ws && ws->type == JSON_ARRAY) {
            /* build a synthetic flat list */
            /* simplest: treat worksheets[].cells as the list */
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
        int hl = snprintf(header, sizeof(header), "# ── %s cell%s ──\n",
                          labels[li], li == 2 ? "" : " ");
        (void)hl;
        json_node_t *src = json_object_get(cell, "source");
        char *src_text = read_extract_source_text(src);
        /* rstrip newline(s) */
        size_t sl = strlen(src_text);
        while (sl > 0 && (src_text[sl-1] == '\n' || src_text[sl-1] == '\r')) sl--;
        size_t hl_len = strlen(header);
        size_t need = len + hl_len + sl + 2;
        if (need + 1 > cap) { cap = (need + 1) * 2; char *n = realloc(out, cap); if (!n) { free(src_text); free(out); if (cells != json_object_get(nb,"cells")) json_free(cells); json_free(nb); return NULL; } out = n; }
        memcpy(out + len, header, hl_len); len += hl_len;
        memcpy(out + len, src_text, sl); len += sl;
        out[len++] = '\n'; out[len] = '\0';
        free(src_text);
        any = 1;
    }
    if (cells != json_object_get(nb, "cells")) json_free(cells);
    json_free(nb);
    if (!any) { free(out); if (errbuf) snprintf(errbuf, errsz, "Notebook contains no readable cells"); return NULL; }
    /* rstrip */
    while (len > 0 && (out[len-1] == '\n' || out[len-1] == '\r')) len--;
    if (len + 1 >= cap) { char *n = realloc(out, len + 2); if (n) out = n; }
    out[len++] = '\n'; out[len] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* _extract_docx                                                       */
/* ------------------------------------------------------------------ */
/* PoP: _extract_docx @ tools/read_extract.py:_extract_docx */
static char *read_extract_docx(const char *path, char *errbuf, size_t errsz)
{
    /* PoP: _zip_xml @ tools/read_extract.py:_zip_xml */
    re_zip_t *z = re_zip_open(path);
    if (!z) { if (errbuf) snprintf(errbuf, errsz, "Not a valid DOCX: cannot open"); return NULL; }
    size_t sz = 0;
    const unsigned char *xml = re_zip_find(z, "word/document.xml", &sz);
    if (!xml) { re_zip_free(z); if (errbuf) snprintf(errbuf, errsz, "Missing word/document.xml"); return NULL; }
    const char *start = (const char *)xml;
    const char *end = start + sz;

    size_t cap = 8192, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) { re_zip_free(z); return NULL; }
    out[0] = '\0';
    int any_text = 0;

    /* Walk <w:p> paragraphs; inside, accumulate <w:t> text, <w:tab>=tab, <w:br>/<w:cr>=newline. */
    const char *p = start;
    while (p < end) {
        if (*p == '<' && p[1] != '/' && re_xml_is_local(p, "p")) {
            /* process paragraph body [q+1 .. matching </w:p>] */
            const char *q = p + 1; while (q < end && *q != '>') q++;
            if (q >= end) break;
            const char *body = q + 1;
            /* find matching close tag */
            const char *close = body;
            int depth = 1;
            (void)depth;
            const char *cp = body;
            while (cp < end) {
                if (*cp == '<' && cp[1] == '/' && re_xml_is_local(cp, "p")) { close = cp; break; }
                cp++;
            }
            /* accumulate text within [body, close) */
            const char *r = body;
            size_t linecap = 256, linelen = 0;
            char *line = (char *)malloc(linecap);
            if (!line) { free(out); re_zip_free(z); return NULL; }
            while (r < close) {
                if (*r == '<' && r[1] != '/') {
                    const char *tq = r + 1; while (tq < close && *tq != '>') tq++;
                    if (tq < close) {
                        if (re_xml_is_local(r, "t")) {
                            const char *ve = tq + 1;
                            /* find </w:t> */
                            const char *vt = ve;
                            while (vt < close) {
                                if (*vt == '<' && vt[1] == '/' && re_xml_is_local(vt, "t")) break;
                                vt++;
                            }
                            char *tx = re_xml_text_content(ve, vt);
                            size_t tl = strlen(tx);
                            if (tl) any_text = 1;
                            if (linelen + tl + 1 > linecap) { linecap = linelen + tl + 256; char *n = realloc(line, linecap); if (!n) { free(tx); free(line); free(out); re_zip_free(z); return NULL; } line = n; }
                            memcpy(line + linelen, tx, tl); linelen += tl;
                            free(tx);
                            r = vt;
                            continue;
                        } else if (re_xml_is_local(r, "tab")) {
                            if (linelen + 1 + 1 > linecap) { linecap = linelen + 2; char *n = realloc(line, linecap); if (!n) { free(line); free(out); re_zip_free(z); return NULL; } line = n; }
                            line[linelen++] = '\t';
                        } else if (re_xml_is_local(r, "br") || re_xml_is_local(r, "cr")) {
                            /* newline: flush current line into out */
                            if (len + linelen + 2 > cap) { cap = (len + linelen) * 2 + 64; char *n = realloc(out, cap); if (!n) { free(line); free(out); re_zip_free(z); return NULL; } out = n; }
                            memcpy(out + len, line, linelen); len += linelen;
                            out[len++] = '\n'; out[len] = '\0';
                            linelen = 0;
                        }
                    }
                    r = (tq < close) ? tq + 1 : close;
                } else {
                    r++;
                }
            }
            /* flush remaining line */
            if (len + linelen + 2 > cap) { cap = (len + linelen) * 2 + 64; char *n = realloc(out, cap); if (!n) { free(line); free(out); re_zip_free(z); return NULL; } out = n; }
            memcpy(out + len, line, linelen); len += linelen;
            out[len++] = '\n'; out[len] = '\0';
            free(line);
            p = close;
            continue;
        }
        p++;
    }
    re_zip_free(z);
    if (!any_text) { free(out); if (errbuf) snprintf(errbuf, errsz, "DOCX contains no extractable text"); return NULL; }
    while (len > 0 && out[len-1] == '\n') len--;
    if (len + 1 >= cap) { char *n = realloc(out, len + 2); if (n) out = n; }
    out[len++] = '\n'; out[len] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* _extract_xlsx                                                      */
/* ------------------------------------------------------------------ */
/* PoP: _sheet_part @ tools/read_extract.py:_sheet_part */
static void read_extract_sheet_part(const char *target, char *out, size_t outsz)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", target);
    /* lstrip / */
    char *s = buf;
    while (*s == '/') s++;
    if (strncmp(s, "xl/", 3) == 0) snprintf(out, outsz, "%s", s);
    else {
        /* posixpath.normpath */
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

/* cell value: typ in {"s","inlineStr","b","e",""}. shared is a string array. */
/* PoP: _cell_value @ tools/read_extract.py:_cell_value */
static char *read_extract_cell_value(const char *cell_start, const char *cell_end,
                                     char **shared, size_t shared_n)
{
    /* read t="..." attribute and <v> text */
    const char *tq = cell_start;
    const char *typ = strdup("");
    /* scan opening tag for t= */
    while (tq < cell_end && *tq != '>') {
        if (tq[0] == 't' && tq[1] == '=') {
            tq += 2;
            if (*tq == '"' || *tq == '\'') {
                char qc = *tq; tq++;
                char buf[32]; int bi = 0;
                while (*tq && *tq != qc && bi < (int)sizeof(buf)-1) buf[bi++] = *tq++;
                buf[bi] = '\0';
            typ = strdup(buf);
            }
            break;
        }
        tq++;
    }
    /* find <v> text */
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
    } else if (strcmp(typ, "inlineStr") == 0) {
        /* find <is> ... text */
        const char *is = cell_start;
        while (is < cell_end) {
            if (*is == '<' && (is[1]=='i'&&is[2]==':')==0 && (is[1]=='i')) { /* <is> or <w:is> not here */ }
            is++;
        }
        free(result); result = strdup(""); /* inlineStr rare; best-effort empty */
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
static char *read_extract_xlsx(const char *path, char *errbuf, size_t errsz)
{
    re_zip_t *z = re_zip_open(path);
    if (!z) { if (errbuf) snprintf(errbuf, errsz, "Not a valid XLSX: cannot open"); return NULL; }

    /* shared strings */
    char **shared = NULL; size_t shared_n = 0;
    size_t ssz = 0;
    const unsigned char *sxml = re_zip_find(z, "xl/sharedStrings.xml", &ssz);
    if (sxml) {
        /* PoP: _shared_strings @ tools/read_extract.py:_shared_strings */
        const char *s = (const char *)sxml, *e = s + ssz;
        const char *p = s;
        while (p < e) {
            if (*p == '<' && p[1] != '/' && re_xml_is_local(p, "si")) {
                const char *q = p + 1; while (q < e && *q != '>') q++;
                const char *body = q + 1;
                const char *close = body;
                while (close < e) {
                    if (*close == '<' && close[1]=='/' && re_xml_is_local(close, "si")) break;
                    close++;
                }
                /* gather all <t> text inside */
                char *text = strdup("");
                const char *r = body;
                while (r < close) {
                    if (*r == '<' && r[1] != '/' && re_xml_is_local(r, "t")) {
                        const char *te = r + 1; while (te < close && *te != '>') te++;
                        const char *ve = te + 1;
                        const char *vt = ve;
                        while (vt < close && !(*vt=='<'&&vt[1]=='/'&&re_xml_is_local(vt,"t"))) vt++;
                        char *tx = re_xml_text_content(ve, vt);
                        if (!tx) { r = vt; continue; }
                        if (!text) text = strdup("");
                        if (!text) { free(tx); r = vt; break; }
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

    /* workbook sheets */
    size_t wsz = 0;
    const unsigned char *wxml = re_zip_find(z, "xl/workbook.xml", &wsz);
    /* PoP: _workbook_sheets @ tools/read_extract.py:_workbook_sheets */
    char **rels_ids = NULL; char **rels_tgts = NULL; size_t rels_n = 0;
    {
        char nameset[1][1]; (void)nameset;
        /* PoP: _workbook_rels @ tools/read_extract.py:_workbook_rels */
        for (size_t i = 0; i < z->count; i++) {
            if (strcmp(z->entries[i].name, "xl/_rels/workbook.xml.rels") == 0) {
                const char *s = (const char *)z->entries[i].data, *e = s + z->entries[i].size;
                const char *p = s;
                while (p < e) {
                    if (*p == '<' && (p[1]=='R'&&p[2]==':')==0 && p[1]=='R') {
                        /* Relationship */
                    }
                    if (*p == '<' && strncmp(p+1, "Relationship", 11) == 0) {
                        /* parse Id and Target attributes */
                        const char *te = p; while (te < e && *te != '>') te++;
                        char *id = NULL, *tgt = NULL;
                        /* crude attr scan */
                        const char *a = p;
                        while (a < te) {
                            if (a[0]=='I'&&a[1]=='d'&&a[2]=='=') { a+=3; if(*a=='"'||*a=='\''){char q=*a;a++;char b[256];int bi=0;while(*a&&*a!=q&&bi<255)b[bi++]=*a++;b[bi]=0;id=strdup(b);} }
                            else if (a[0]=='T'&&a[1]=='a'&&a[2]=='r'&&a[3]=='g'&&a[4]=='e'&&a[5]=='t'&&a[6]=='=') { a+=7; if(*a=='"'||*a=='\''){char q=*a;a++;char b[1024];int bi=0;while(*a&&*a!=q&&bi<1023)b[bi++]=*a++;b[bi]=0;tgt=strdup(b);} }
                            a++;
                        }
                        if (id && tgt) {
                            rels_ids = (char **)realloc(rels_ids, (rels_n+1)*sizeof(char*));
                            rels_tgts = (char **)realloc(rels_tgts, (rels_n+1)*sizeof(char*));
                            if (rels_ids && rels_tgts) { rels_ids[rels_n] = id; rels_tgts[rels_n] = tgt; rels_n++; }
                            else { free(id); free(tgt); }
                        } else { free(id); free(tgt); }
                        p = te + 1;
                        continue;
                    }
                    p++;
                }
                break;
            }
        }
    }

    size_t cap = 8192, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) { /* cleanup */ for (size_t i=0;i<shared_n;i++) free(shared[i]); free(shared); for (size_t i=0;i<rels_n;i++){free(rels_ids[i]);free(rels_tgts[i]);} free(rels_ids); free(rels_tgts); re_zip_free(z); return NULL; }
    out[0] = '\0';
    int any = 0;

    if (wxml) {
        const char *s = (const char *)wxml, *e = s + wsz;
        const char *p = s;
        while (p < e) {
            if (*p == '<' && p[1] != '/' && re_xml_is_local(p, "sheet")) {
                const char *te = p + 1; while (te < e && *te != '>') te++;
                if (te < e && re_xml_is_local(p, "sheet")) {
                    /* read name, state, r:id */
                    char *name = strdup("Sheet"); char *state = strdup("visible"); char *rid = strdup("");
                    const char *a = p;
                    while (a < te) {
                        if (strncmp(a, "name=", 5) == 0) { a+=5; if(*a=='"'||*a=='\''){char q=*a;a++;char b[256];int bi=0;while(*a&&*a!=q&&bi<255)b[bi++]=*a++;b[bi]=0;free(name);name=strdup(b);} }
                        else if (strncmp(a, "state=", 6) == 0) { a+=6; if(*a=='"'||*a=='\''){char q=*a;a++;char b[64];int bi=0;while(*a&&*a!=q&&bi<63)b[bi++]=*a++;b[bi]=0;free(state);state=strdup(b);} }
                        else if ((a[0]=='r'&&a[1]==':') && strncmp(a+2, "id=", 3)==0) { a+=5; if(*a=='"'||*a=='\''){char q=*a;a++;char b[64];int bi=0;while(*a&&*a!=q&&bi<63)b[bi++]=*a++;b[bi]=0;free(rid);rid=strdup(b);} }
                        else if (strncmp(a, "r:id=", 5) == 0) { a+=5; if(*a=='"'||*a=='\''){char q=*a;a++;char b[64];int bi=0;while(*a&&*a!=q&&bi<63)b[bi++]=*a++;b[bi]=0;free(rid);rid=strdup(b);} }
                        a++;
                    }
                    if (strcmp(state, "hidden") != 0 && strcmp(state, "veryHidden") != 0) {
                        /* resolve target */
                        char *target = strdup("");
                        for (size_t k = 0; k < rels_n; k++) {
                            if (strcmp(rels_ids[k], rid) == 0) { free(target); target = strdup(rels_tgts[k]); break; }
                        }
                        char part[1024];
                        read_extract_sheet_part(target, part, sizeof(part));
                        free(target);
                        size_t psz = 0;
                        const unsigned char *pxml = re_zip_find(z, part, &psz);
                        if (pxml) {
                            /* header "# ── Sheet: name ──" */
                            char hdr[512];
                            int hl = snprintf(hdr, sizeof(hdr), "# ── Sheet: %s ──\n", name);
                            (void)hl;
                            if (len + strlen(hdr) + 1 > cap) { cap = len + strlen(hdr) + 1024; char *n = realloc(out, cap); if (!n) { free(name); free(state); free(rid); for (size_t i=0;i<shared_n;i++) free(shared[i]); free(shared); for (size_t i=0;i<rels_n;i++){free(rels_ids[i]);free(rels_tgts[i]);} free(rels_ids); free(rels_tgts); re_zip_free(z); return NULL; } out = n; }
                            strcat(out + len, hdr); len += strlen(hdr);
                            /* walk rows */
                            const char *rs = (const char *)pxml, *re_ = rs + psz;
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
                                    /* cells */
                                    char **cells = NULL; int max_col = -1;
                                    const char *cp = rbody;
                                    while (cp < rclose) {
                                        if (*cp == '<' && cp[1] != '/' && re_xml_is_local(cp, "c")) {
                                            const char *cte = cp + 1; while (cte < rclose && *cte != '>') cte++;
                                            const char *cbody = cte + 1;
                                            const char *cclose = cbody;
                                            while (cclose < rclose) { if (*cclose=='<'&&cclose[1]=='/'&&re_xml_is_local(cclose,"c")) break; cclose++; }
                                            /* col index */
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
                                    /* emit row as tab-joined */
                                    if (max_col >= 0) {
                                        for (int ci = 0; ci <= max_col; ci++) {
                                            const char *cv = cells[ci] ? cells[ci] : "";
                                            if (len + strlen(cv) + 2 > cap) { cap = len + strlen(cv) + 256; char *n = realloc(out, cap); if (!n) { for (int ii=0;ii<=max_col;ii++) free(cells[ii]); free(cells); /* cleanup */ for (size_t i=0;i<shared_n;i++) free(shared[i]); free(shared); for (size_t i=0;i<rels_n;i++){free(rels_ids[i]);free(rels_tgts[i]);} free(rels_ids); free(rels_tgts); re_zip_free(z); return NULL; } out = n; }
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
    re_zip_free(z);

    if (!any) { free(out); if (errbuf) snprintf(errbuf, errsz, "XLSX has no visible sheets with content"); return NULL; }
    while (len > 0 && out[len-1] == '\n') len--;
    if (len + 1 >= cap) { char *n = realloc(out, len + 2); if (n) out = n; }
    out[len++] = '\n'; out[len] = '\0';
    return out;
}
