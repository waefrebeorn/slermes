/* gate: normalize_url_ladder — traced copy of
 * src/tools/url_safety.c::cli_tools_url_safety_normalize_url_for_request
 * (LIVE twin: tools/url_safety.py::normalize_url_for_request).
 *
 * Probe encoding: one URL per input.
 *
 * Rung order (must match twin.py EXACTLY):
 *   U1: empty after strip            -> passthrough ""
 *   U2: scheme not http/https        -> passthrough raw
 *   U3: hostname present
 *   U4: hostname changed by IDNA encoding
 *   V: the normalized URL (or passthrough)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

/* Python encodings/punycode.py encoder (port), used by hostname.encode("idna").
 * UTF-8 label -> ASCII xn-- form, byte-compatible with LIVE idna output. */

#define PC_TMIN 1
#define PC_TMAX 26
#define PC_BASE 36
#define PC_SKEW 38
#define PC_DAMP 700
#define PC_INITIAL_BIAS 72
#define PC_INITIAL_N 0x80

static char pc_digit(int d)
{
    return (char)(d < 26 ? d + 'a' : d - 26 + '0');
}

/* decode one UTF-8 code point at s[*i]; returns 0xFFFFFFFF on error */
static uint32_t utf8_next(const unsigned char *s, size_t len, size_t *i)
{
    if (*i >= len) return 0xFFFFFFFF;
    unsigned char c = s[*i];
    uint32_t ch;
    if (c < 0x80) { *i += 1; return c; }
    if ((c & 0xE0) == 0xC0 && *i + 1 < len) {
        ch = (uint32_t)(c & 0x1F);
        ch = (ch << 6) | (s[*i + 1] & 0x3F); *i += 2; return ch;
    }
    if ((c & 0xF0) == 0xE0 && *i + 2 < len) {
        ch = (uint32_t)(c & 0x0F);
        ch = (ch << 6) | (s[*i + 1] & 0x3F);
        ch = (ch << 6) | (s[*i + 2] & 0x3F); *i += 3; return ch;
    }
    if ((c & 0xF8) == 0xF0 && *i + 3 < len) {
        ch = (uint32_t)(c & 0x07);
        ch = (ch << 6) | (s[*i + 1] & 0x3F);
        ch = (ch << 6) | (s[*i + 2] & 0x3F);
        ch = (ch << 6) | (s[*i + 3] & 0x3F); *i += 4; return ch;
    }
    *i += 1;
    return 0xFFFFFFFF;
}

/* Faithful port of CPython Lib/encodings/punycode.py (the encoder used by
 * str.encode("punycode") / encode("idna")). NOTE: CPython implements the
 * ORIGINAL Costello algorithm (segregate/insertion_unsort/generate_integers),
 * which is NOT byte-identical to RFC 3492 incremental encoding. */

static int pc_selective_len(const uint32_t *cps, size_t n, uint32_t b)
{
    size_t l = 0;
    for (size_t k = 0; k < n; k++) if (cps[k] < b) l++;
    return (int)l;
}

/* python selective_find: returns next (index, pos); index counts only
 * characters <= char, pos is the full-string position. -1/-1 at end. */
static void pc_selective_find(const uint32_t *cps, size_t n, uint32_t c,
                              long *index, long *pos)
{
    while (1) {
        *pos += 1;
        if (*pos == (long)n) { *index = -1; *pos = -1; return; }
        uint32_t cc = cps[*pos];
        if (cc == c) { *index += 1; return; }
        else if (cc < c) { *index += 1; }
    }
}

/* CPython adapt(delta, first, numchars) */
static uint32_t pc_adapt(uint64_t delta, int first, uint32_t numchars)
{
    delta = first ? delta / PC_DAMP : delta / 2;
    delta += delta / numchars;
    uint32_t divisions = 0;
    while (delta > 455) {
        delta = delta / (PC_BASE - PC_TMIN);
        divisions += PC_BASE;
    }
    return (uint32_t)(divisions + PC_BASE * delta / (delta + PC_SKEW));
}

/* generate_generalized_integer(N, bias) -> digits */
static void pc_generate_integer(uint64_t N, uint32_t bias,
                                char *out, size_t *olen, size_t out_cap)
{
    uint32_t j = 0;
    for (;;) {
        uint32_t t;
        int64_t sres = (int64_t)PC_BASE * ((int64_t)j + 1) - (int64_t)bias;
        if (sres < PC_TMIN) t = PC_TMIN;
        else if (sres > PC_TMAX) t = PC_TMAX;
        else t = (uint32_t)sres;
        if (N < t) {
            /* python digits[N] — negative indices wrap */
            int d = (int)(((int64_t)N % 36 + 36) % 36);
            if (*olen + 2 >= out_cap) { return; }
            out[(*olen)++] = pc_digit(d);
            return;
        }
        if (*olen + 2 >= out_cap) { return; }
        out[(*olen)++] = pc_digit((int)(t + ((N - t) % (PC_BASE - t))));
        N = (N - t) / (PC_BASE - t);
        j++;
    }
}

static int punycode_encode_label(const char *label, size_t label_len,
                                 char *out, size_t out_cap)
{
    /* decode UTF-8 */
    uint32_t cps[256];
    size_t ncp = 0;
    size_t i = 0;
    while (i < label_len && ncp < 256) {
        uint32_t ch = utf8_next((const unsigned char *)label, label_len, &i);
        if (ch == 0xFFFFFFFF) return -1;
        cps[ncp++] = ch;
    }

    /* segregate: base = ASCII chars in order; extended = set of non-ASCII
     * code points sorted ascending */
    char base[256];
    size_t baselen = 0;
    uint32_t extended[256];
    size_t extlen = 0;
    for (size_t k = 0; k < ncp; k++) {
        if (cps[k] < 0x80) base[baselen++] = (char)cps[k];
        else {
            int seen = 0;
            for (size_t m = 0; m < extlen; m++)
                if (extended[m] == cps[k]) { seen = 1; break; }
            if (!seen && extlen < 256) extended[extlen++] = cps[k];
        }
    }
    for (size_t a = 0; a + 1 < extlen; a++)
        for (size_t b = a + 1; b < extlen; b++)
            if (extended[b] < extended[a]) {
                uint32_t tmp = extended[a]; extended[a] = extended[b]; extended[b] = tmp;
            }

    /* insertion_unsort -> deltas (python appends delta-1) */
    uint64_t deltas[512];
    size_t ndeltas = 0;
    {
        uint32_t oldchar = PC_INITIAL_N;
        long oldindex = -1;
        for (size_t e = 0; e < extlen; e++) {
            uint32_t c = extended[e];
            long index = -1, pos = -1;
            uint64_t curlen = pc_selective_len(cps, ncp, c);
            uint64_t delta = (curlen + 1) * ((uint64_t)c - oldchar);
            for (;;) {
                pc_selective_find(cps, ncp, c, &index, &pos);
                if (index == -1) break;
                delta += (uint64_t)(index - oldindex);
                if (ndeltas < 512) deltas[ndeltas++] = delta - 1;
                oldindex = index;
                delta = 0;
            }
            oldchar = c;
        }
    }

    /* generate_integers(baselen, deltas) */
    char ext_out[1024];
    size_t eo = 0;
    uint32_t bias = PC_INITIAL_BIAS;
    for (size_t p = 0; p < ndeltas; p++) {
        pc_generate_integer(deltas[p], bias, ext_out, &eo, sizeof ext_out);
        bias = pc_adapt(deltas[p], p == 0, (uint32_t)(baselen + p + 1));
    }

    /* assemble: base + "-" + extended when base non-empty, else extended */
    size_t o = 0;
    if (baselen) {
        if (baselen + 1 + eo + 1 > out_cap) return -1;
        memcpy(out, base, baselen); o = baselen;
        out[o++] = '-';
    } else if (eo + 1 > out_cap) {
        return -1;
    }
    memcpy(out + o, ext_out, eo); o += eo;
    out[o] = '\0';
    return 0;
}

static void percent_encode(const char *in, char *out, size_t out_cap,
                           const char *safe)
{
    (void)out_cap;
    size_t j = 0;
    for (size_t i = 0; in[i]; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '~' || strchr(safe, c) != NULL) {
            out[j++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            out[j++] = '%';
            out[j++] = hex[(c >> 4) & 0xF];
            out[j++] = hex[c & 0xF];
        }
    }
    out[j] = '\0';
}

static int normalize_url(const char *url, char *normalized_out, size_t norm_size)
{
    WCUP_BEGIN("normalize_url_ladder");
    int rc = 0;
    normalized_out[0] = '\0';

    /* strip surrounding whitespace (python .strip()) — traced as U1 below */
    const char *s = url;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    char raw[4096];
    if (len >= sizeof raw) len = sizeof raw - 1;
    memcpy(raw, s, len); raw[len] = '\0';

    if (!raw[0]) {                                        /* U1 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);

    {
        const char *auth_end = strstr(raw, "://");
        char scheme[16];
        size_t sl;
        if (!auth_end) {
            /* U2 */
            WCUP_BRANCH(1);
            snprintf(normalized_out, norm_size, "%s", raw);
            goto emit;
        }
        sl = (size_t)(auth_end - raw);
        if (sl >= sizeof(scheme)) { rc = -1; goto emit; }
        memcpy(scheme, raw, sl); scheme[sl] = '\0';
        /* urlsplit lowercases the scheme; python compares .lower() to http(s) */
        for (char *p = scheme; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0) {
            /* U2 */
            WCUP_BRANCH(1);
            snprintf(normalized_out, norm_size, "%s", url);
            goto emit;
        }
        WCUP_BRANCH(0);
    }

    {
        char scheme2[16];
        const char *auth_end2 = strstr(raw, "://");
        const char *rest = auth_end2 + 3;

        /* re-use the lowercased scheme */
        size_t sl2 = (size_t)(auth_end2 - raw);
        if (sl2 >= sizeof(scheme2)) { rc = -1; goto emit; }
        memcpy(scheme2, raw, sl2); scheme2[sl2] = '\0';
        for (char *p = scheme2; *p; p++) *p = (char)tolower((unsigned char)*p);
#define scheme scheme2

        size_t netloc_len = 0;
        while (rest[netloc_len] && rest[netloc_len] != '/' &&
               rest[netloc_len] != '?' && rest[netloc_len] != '#')
            netloc_len++;
        char netloc[1024];
        if (netloc_len >= sizeof(netloc)) { rc = -1; goto emit; }
        memcpy(netloc, rest, netloc_len);
        netloc[netloc_len] = '\0';

        const char *after = rest + netloc_len;
        const char *path = (*after == '/') ? after : "/";
        const char *frag = strchr(after, '#');

        char hostname[256];
        const char *at = strrchr(netloc, '@');
        const char *host_start = at ? at + 1 : netloc;
        size_t hlen = strlen(host_start);
        const char *colon = strrchr(host_start, ':');
        const char *port_str = NULL;
        if (colon && strchr(colon, ']') == NULL) {
            port_str = colon + 1;
            hlen = (size_t)(colon - host_start);
        }
        if (hlen >= sizeof(hostname)) { rc = -1; goto emit; }
        memcpy(hostname, host_start, hlen); hostname[hlen] = '\0';
        /* urlsplit lowercases parsed.hostname (ASCII only) */
        for (char *p = hostname; *p; p++)
            if ((unsigned char)*p < 0x80) *p = (char)tolower((unsigned char)*p);
        /* twin does netloc.replace(hostname_lower, ascii_host, 1) only when
         * IDNA changed the host; otherwise netloc keeps its original case.
         * We defer that replacement until after encoding (see below), so
         * remember where the original-case host sits in netloc. */

        /* U3: hostname present */
        if (hostname[0]) {
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }

        /* IDNA-encode labels containing non-ASCII */
        char enc_host[1024];
        enc_host[0] = '\0';
        char *tokctx = NULL;
        char *hcopy = strdup(hostname);
        char *label = strtok_r(hcopy, ".", &tokctx);
        int first = 1;
        int host_changed = 0;
        while (label) {
            const char *out_label = label;
            int has_nonascii = 0;
            for (size_t k = 0; label[k]; k++)
                if ((unsigned char)label[k] >= 0x80) has_nonascii = 1;
            char enc[260];
            if (has_nonascii) {
                if (punycode_encode_label(label, strlen(label), enc + 4,
                                          sizeof(enc) - 4) == 0) {
                    enc[0]='x'; enc[1]='n'; enc[2]='-'; enc[3]='-';
                    out_label = enc;
                    host_changed = 1;
                }
            }
            if (!first) strncat(enc_host, ".", sizeof(enc_host) - strlen(enc_host) - 1);
            strncat(enc_host, out_label, sizeof(enc_host) - strlen(enc_host) - 1);
            first = 0;
            label = strtok_r(NULL, ".", &tokctx);
        }
        free(hcopy);

        /* U4: IDNA encoding changed the host */
        if (host_changed) {
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }

        char final_netloc[1024];
        final_netloc[0] = '\0';
        if (host_changed) {
            /* python: netloc = netloc.replace(lowered_hostname, ascii, 1)
             * — replace the FIRST occurrence of the lowered host inside the
             * original-case netloc with the encoded host. */
            const char *hitp = NULL;
            for (const char *q = netloc; *q; q++)
                if (strncasecmp(q, hostname, hlen) == 0) { hitp = q; break; }
            if (hitp) {
                size_t pre = (size_t)(hitp - netloc);
                strncat(final_netloc, netloc, pre);
                strncat(final_netloc, enc_host,
                        sizeof(final_netloc) - strlen(final_netloc) - 1);
                /* hitp+hlen already carries the ":port" tail — skip the
                 * separate port append by clearing port_str */
                strncat(final_netloc, hitp + hlen,
                        sizeof(final_netloc) - strlen(final_netloc) - 1);
                port_str = NULL;
            } else {
                strncat(final_netloc, netloc,
                        sizeof(final_netloc) - strlen(final_netloc) - 1);
            }
        } else if (at) {
            strncat(final_netloc, netloc, (size_t)(host_start - netloc));
            strncat(final_netloc, hostname,
                    sizeof(final_netloc) - strlen(final_netloc) - 1);
        } else {
            /* unchanged host: keep original netloc as-is */
            strncat(final_netloc, netloc,
                    sizeof(final_netloc) - strlen(final_netloc) - 1);
        }
        if (port_str) {
            strncat(final_netloc, ":", sizeof(final_netloc) - strlen(final_netloc) - 1);
            strncat(final_netloc, port_str, sizeof(final_netloc) - strlen(final_netloc) - 1);
        }

        char enc_path[2048], enc_query[2048], enc_frag[2048];
        const char *path_part = path;
        const char *qstart = strchr(path_part, '?');
        const char *fstart = strchr(path_part, '#');
        size_t path_only_len = strlen(path_part);
        if (qstart) path_only_len = (size_t)(qstart - path_part);
        else if (fstart) path_only_len = (size_t)(fstart - path_part);

        char path_only[2048];
        memcpy(path_only, path_part, path_only_len); path_only[path_only_len] = '\0';
        percent_encode(path_only, enc_path, sizeof(enc_path), "/%:@!$&'()*+,;=");
        const char *qpart = (qstart || frag) ? strchr(after, '?') : NULL;
        if (qpart) {
            percent_encode(qpart + 1, enc_query, sizeof(enc_query), "/%:@!$&'()*+,;=?");
        } else enc_query[0] = '\0';
        const char *fpart = strchr(after, '#');
        if (fpart) {
            percent_encode(fpart + 1, enc_frag, sizeof(enc_frag), "/%:@!$'()*+,;=?");
        } else enc_frag[0] = '\0';

        snprintf(normalized_out, norm_size, "%s://%s%s%s%s%s%s",
                 scheme, final_netloc,
                 enc_path,
                 qpart ? "?" : "", qpart ? enc_query : "",
                 fpart ? "#" : "", fpart ? enc_frag : "");
#undef scheme
    }

emit:
    fprintf(stderr, "DBG out=[%s]\n", normalized_out);
    WCUP_VALSTR(normalized_out);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_url_ladder");
    }
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char out[4096];
        normalize_url(argv[i], out, sizeof out);
    }
    return 0;
}
