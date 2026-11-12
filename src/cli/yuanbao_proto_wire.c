/*
 * yuanbao_proto_wire.c - yuanbao protobuf wire helpers.
 *
 * Split from port_yuanbao_proto_helpers.c (soul.md: self-contained modules).
 * Varint / length-delimited field reading and field-list teardown.
 * No file-scope state.
 */
#include "yuanbao_proto_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>



/* ---- growable byte buffer ---- */

/* ---- varint decode ---- */
/* PoP: _decode_varint @ plugins/platforms/yuanbao/yuanbao_proto.py:_decode_varint */
static int yb_decode_varint(const unsigned char *d, size_t n, size_t *pos, uint64_t *out)
{
    uint64_t result = 0; int shift = 0;
    while (*pos < n) {
        unsigned char byte = d[*pos]; (*pos)++;
        result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) { *out = result; return 1; }
        if (shift >= 64) return 0;
    }
    return 0;
}
 void yb_fields_free(yb_fields *fs)
{ for (size_t i=0;i<fs->n;i++) free(fs->f[i].vbytes); free(fs->f); fs->f=NULL; fs->n=fs->cap=0; }

/* PoP: _parse_fields @ plugins/platforms/yuanbao/yuanbao_proto.py:_parse_fields */

/* PoP: _fields_to_dict @ plugins/platforms/yuanbao/yuanbao_proto.py:_fields_to_dict */ int yb_parse_fields(const unsigned char *d, size_t n, yb_fields *out)
{
    out->f = NULL; out->n = out->cap = 0;
    size_t pos = 0;
    while (pos < n) {
        uint64_t tag; if (!yb_decode_varint(d, n, &pos, &tag)) break;
        int fn = (int)(tag >> 3), wt = (int)(tag & 0x07);
        yb_field f; memset(&f, 0, sizeof(f)); f.fn = fn; f.wt = wt; f.vint = 0; f.vbytes = NULL; f.vlen = 0;
        if (wt == WT_VARINT) {
            uint64_t v; if (!yb_decode_varint(d, n, &pos, &v)) break;
            f.vint = v;
        } else if (wt == WT_LEN) {
            uint64_t len; if (!yb_decode_varint(d, n, &pos, &len)) break;
            if (pos + len > n) { f.vlen = n - pos; } else f.vlen = (size_t)len;
            f.vbytes = malloc(f.vlen ? f.vlen : 1);
            memcpy(f.vbytes, d + pos, f.vlen);
            pos += f.vlen;
        } else if (wt == WT_64BIT) {
            if (pos + 8 > n) break;
            f.vbytes = malloc(8); memcpy(f.vbytes, d + pos, 8); f.vlen = 8; pos += 8;
        } else if (wt == WT_32BIT) {
            if (pos + 4 > n) break;
            f.vbytes = malloc(4); memcpy(f.vbytes, d + pos, 4); f.vlen = 4; pos += 4;
        } else { return 0; }
        if (out->n == out->cap) { out->cap = out->cap ? out->cap*2 : 8; out->f = realloc(out->f, out->cap*sizeof(yb_field)); }
        out->f[out->n++] = f;
    }
    return 1;
}

/* accessors: first match by field number */
/* PoP: _get_varint @ plugins/platforms/yuanbao/yuanbao_proto.py:_get_varint */ uint64_t yb_get_varint(const yb_fields *fs, int fn, uint64_t def)
{ for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_VARINT) return fs->f[i].vint; return def; }

/* PoP: _get_string @ plugins/platforms/yuanbao/yuanbao_proto.py:_get_string */ char *yb_get_string(const yb_fields *fs, int fn, char *buf, size_t bufsz)
{ buf[0]=0; for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) {
    size_t c = fs->f[i].vlen < bufsz-1 ? fs->f[i].vlen : bufsz-1;
    memcpy(buf, fs->f[i].vbytes, c); buf[c]=0; return buf; }
  return buf; }

/* PoP: _get_bytes @ plugins/platforms/yuanbao/yuanbao_proto.py:_get_bytes */ unsigned char *yb_get_bytes(const yb_fields *fs, int fn, size_t *outlen)
{ *outlen=0; for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) { *outlen=fs->f[i].vlen; return fs->f[i].vbytes; } return NULL; }

/* collect repeated bytes into a malloc'd array of (ptr,len); returns count */
/* PoP: _get_repeated_bytes @ plugins/platforms/yuanbao/yuanbao_proto.py:_get_repeated_bytes */ size_t yb_get_repeated_bytes(const yb_fields *fs, int fn, unsigned char ***out, size_t **lens)
{
    size_t cnt = 0; for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) cnt++;
    *out = cnt ? malloc(cnt*sizeof(unsigned char*)) : NULL;
    *lens = cnt ? malloc(cnt*sizeof(size_t)) : NULL;
    size_t j = 0;
    for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) { (*out)[j]=fs->f[i].vbytes; (*lens)[j]=fs->f[i].vlen; j++; }
    return cnt;
}
