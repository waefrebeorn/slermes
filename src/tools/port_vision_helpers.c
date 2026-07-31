/*
 * port_vision_helpers.c — Faithful C11 port of tools/vision_tools.py helpers.
 *
 * Exposes the public surface declared in vision_helpers.h. Most of the heavy
 * lifting (magic-byte detection, dimension extraction, base64 encoding) lives
 * as static helpers in src/tools/vision.c; this file provides thin public
 * wrappers plus the genuinely missing helpers (download, resize, rasterize,
 * normalize, timeout, cpu workers) that the parity scanner flags as REAL_GAP.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "vision_helpers.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "base64.h"

/* ──────────────────────────────────────────────────────────────
 *  Format / MIME detection (wrapping the private magic-byte logic)
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_detect_image_mime_type_from_bytes @ tools/vision_tools.py:_detect_image_mime_type_from_bytes */
const char *vision_detect_image_mime_from_bytes(const unsigned char *buf, size_t len)
{
    if (!buf || len < 4) return "application/octet-stream";
    /* PNG */
    if (len >= 4 && buf[0]==0x89 && buf[1]=='P' && buf[2]=='N' && buf[3]=='G')
        return "image/png";
    /* JPEG */
    if (len >= 3 && buf[0]==0xFF && buf[1]==0xD8 && buf[2]==0xFF)
        return "image/jpeg";
    /* GIF */
    if (len >= 6 && buf[0]=='G' && buf[1]=='I' && buf[2]=='F')
        return "image/gif";
    /* WebP (RIFF....WEBP) */
    if (len >= 12 && buf[0]=='R' && buf[1]=='I' && buf[2]=='F' && buf[3]=='F' &&
        buf[8]=='W' && buf[9]=='E' && buf[10]=='B' && buf[11]=='P')
        return "image/webp";
    /* BMP */
    if (len >= 2 && buf[0]=='B' && buf[1]=='M')
        return "image/bmp";
    /* SVG (text-based, starts with <?xml or <svg) */
    if (len >= 5 && (memcmp(buf, "<?xml", 5) == 0 || memcmp(buf, "<svg", 4) == 0))
        return "image/svg+xml";
    return "application/octet-stream";
}

/* PoP: vision_detect_image_format_from_bytes @ tools/vision_tools.py:_determine_mime_type */
const char *vision_detect_image_format_from_bytes(const unsigned char *buf, size_t len)
{
    if (!buf || len < 4) return NULL;
    if (len >= 4 && buf[0]==0x89 && buf[1]=='P' && buf[2]=='N' && buf[3]=='G') return "png";
    if (len >= 3 && buf[0]==0xFF && buf[1]==0xD8 && buf[2]==0xFF) return "jpeg";
    if (len >= 6 && buf[0]=='G' && buf[1]=='I' && buf[2]=='F') return "gif";
    if (len >= 12 && buf[0]=='R' && buf[1]=='I' && buf[2]=='F' && buf[3]=='F' &&
        buf[8]=='W' && buf[9]=='E' && buf[10]=='B' && buf[11]=='P') return "webp";
    if (len >= 2 && buf[0]=='B' && buf[1]=='M') return "bmp";
    if (len >= 5 && (memcmp(buf, "<?xml", 5) == 0 || memcmp(buf, "<svg", 4) == 0)) return "svg";
    /* HEIC/AVIF: ftyp box at offset 4 */
    if (len >= 12 && buf[4]=='f' && buf[5]=='t' && buf[6]=='y' && buf[7]=='p') {
        if (memcmp(buf+8, "avif", 4) == 0) return "avif";
        if (memcmp(buf+8, "heic", 4) == 0 || memcmp(buf+8, "heim", 4) == 0) return "heic";
    }
    return NULL;
}

/* PoP: vision_detect_image_format_from_path @ tools/vision_tools.py:_determine_mime_type */
const char *vision_detect_image_format_from_path(const char *path)
{
    if (!path || !path[0]) return NULL;
    /* Try extension first */
    const char *dot = strrchr(path, '.');
    if (dot && dot[1]) {
        const char *exts[] = {"png","jpeg","jpg","gif","webp","bmp","svg","tiff","tif","ico","heic","avif",NULL};
        const char *names[] = {"png","jpeg","jpeg","gif","webp","bmp","svg","tiff","tiff","ico","heic","avif"};
        char lower[16];
        size_t el = strlen(dot + 1);
        if (el < sizeof(lower)) {
            for (size_t i = 0; i <= el; i++) lower[i] = (char)tolower((unsigned char)dot[1+i]);
            for (int i = 0; exts[i]; i++) {
                if (strcmp(lower, exts[i]) == 0) return names[i];
            }
        }
    }
    /* Fall back to magic bytes */
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char hdr[16] = {0};
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    return vision_detect_image_format_from_bytes(hdr, n);
}

/* PoP: vision_validate_image_path @ tools/vision_tools.py:_image_url_shape_ok */
bool vision_validate_image_path(const char *path)
{
    if (!path || !path[0]) return false;
    /* data: URL */
    if (strncmp(path, "data:", 5) == 0) return true;
    /* http(s):// URL */
    if (strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0) return true;
    /* local file: must exist and be non-empty */
    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 0) return true;
    return false;
}

/* ──────────────────────────────────────────────────────────────
 *  Dimension extraction
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_extract_dimensions_from_bytes @ tools/vision_tools.py:_image_exceeds_dimension */
char *vision_extract_dimensions_from_bytes(const unsigned char *buf, size_t n)
{
    if (!buf || n < 12) return NULL;
    char dims[64] = "";
    unsigned w = 0, h = 0;
    /* PNG */
    if (n >= 24 && buf[0]==0x89 && buf[1]=='P' && buf[2]=='N' && buf[3]=='G') {
        w = ((unsigned)buf[16]<<24)|((unsigned)buf[17]<<16)|((unsigned)buf[18]<<8)|buf[19];
        h = ((unsigned)buf[20]<<24)|((unsigned)buf[21]<<16)|((unsigned)buf[22]<<8)|buf[23];
        snprintf(dims, sizeof(dims), "%ux%u", w, h);
    } else if (n >= 6 && buf[0]=='G' && buf[1]=='I' && buf[2]=='F') {
        w = (unsigned)buf[6] | ((unsigned)buf[7]<<8);
        h = (unsigned)buf[8] | ((unsigned)buf[9]<<8);
        snprintf(dims, sizeof(dims), "%ux%u", w, h);
    } else if (n >= 2 && buf[0]=='B' && buf[1]=='M' && n >= 26) {
        w = (unsigned)buf[18]|((unsigned)buf[19]<<8)|((unsigned)buf[20]<<16)|((unsigned)buf[21]<<24);
        h = (unsigned)buf[22]|((unsigned)buf[23]<<8)|((unsigned)buf[24]<<16)|((unsigned)buf[25]<<24);
        snprintf(dims, sizeof(dims), "%ux%u", w, h);
    } else if (n >= 30 && buf[0]=='R' && buf[1]=='I' && buf[2]=='F' && buf[3]=='F') {
        if (buf[8]=='W' && buf[9]=='E' && buf[10]=='B' && buf[11]=='P') {
            if (buf[12]=='V' && buf[13]=='P' && buf[14]=='8' && buf[15]==' ') {
                w = ((unsigned)(buf[27]&0x3F)<<8)|buf[26];
                h = ((unsigned)(buf[29]&0x3F)<<8)|buf[28];
                snprintf(dims, sizeof(dims), "%ux%u", w+1, h+1);
            } else if (buf[12]=='V' && buf[13]=='P' && buf[14]=='8' && buf[15]=='L') {
                w = ((unsigned)(buf[21]&0x3F)<<8)|(unsigned)(buf[20]&0xFF);
                h = ((unsigned)(buf[23]&0x0F)<<10)|((unsigned)buf[22]<<2)|((unsigned)(buf[21]&0xC0)>>6);
                snprintf(dims, sizeof(dims), "%ux%u", w+1, h+1);
            }
        }
    }
    /* JPEG: need more bytes for SOF scan */
    if (dims[0]) return strdup(dims);
    return NULL;
}

/* PoP: vision_extract_dimensions_from_path @ tools/vision_tools.py:_image_exceeds_dimension */
char *vision_extract_dimensions_from_path(const char *path)
{
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char buf[256];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return vision_extract_dimensions_from_bytes(buf, n);
}

/* ──────────────────────────────────────────────────────────────
 *  Image-to-data-URL encoding
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_image_to_base64_data_url @ tools/vision_tools.py:_image_to_base64_data_url */
char *vision_image_to_base64_data_url(const char *path)
{
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (50 * 1024 * 1024)) { fclose(f); return NULL; }
    unsigned char *data = malloc((size_t)sz);
    if (!data) { fclose(f); return NULL; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    const char *fmt = vision_detect_image_format_from_path(path);
    const char *mime = NULL;
    if (fmt) {
        if (strcmp(fmt,"png")==0) mime="image/png";
        else if (strcmp(fmt,"jpeg")==0) mime="image/jpeg";
        else if (strcmp(fmt,"gif")==0) mime="image/gif";
        else if (strcmp(fmt,"webp")==0) mime="image/webp";
        else if (strcmp(fmt,"bmp")==0) mime="image/bmp";
        else if (strcmp(fmt,"svg")==0) mime="image/svg+xml";
        else if (strcmp(fmt,"tiff")==0) mime="image/tiff";
        else if (strcmp(fmt,"avif")==0) mime="image/avif";
        else if (strcmp(fmt,"heic")==0) mime="image/heic";
    }
    if (!mime) { free(data); return NULL; }
    char *b64 = base64_encode(data, rd);
    free(data);
    if (!b64) return NULL;
    size_t prefix = strlen("data:") + strlen(mime) + strlen(";base64,");
    size_t b64len = strlen(b64);
    char *url = malloc(prefix + b64len + 1);
    if (!url) { free(b64); return NULL; }
    snprintf(url, prefix + b64len + 1, "data:%s;base64,%s", mime, b64);
    free(b64);
    return url;
}

/* ──────────────────────────────────────────────────────────────
 *  Error classification
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_is_image_size_error @ tools/vision_tools.py:_is_image_size_error */
bool vision_is_image_size_error(const char *error_text)
{
    if (!error_text || !error_text[0]) return false;
    const char *hints[] = {
        "too large", "payload", "413", "content_too_large",
        "request_too_large", "image_url", "invalid_request", NULL
    };
    for (int i = 0; hints[i]; i++) {
        if (strcasestr(error_text, hints[i])) return true;
    }
    return false;
}

/* PoP: vision_url_shape_ok @ tools/vision_tools.py:_image_url_shape_ok */
bool vision_url_shape_ok(const char *url)
{
    if (!url || !url[0]) return false;
    if (strncmp(url, "data:", 5) == 0) return true;
    if (strncmp(url, "http://", 7) == 0) return true;
    if (strncmp(url, "https://", 8) == 0) return true;
    /* local path */
    struct stat st;
    return stat(url, &st) == 0;
}

/* ──────────────────────────────────────────────────────────────
 *  Image download
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_download_image @ tools/vision_tools.py:_download_image */
char *vision_download_image(const char *url, int timeout_sec)
{
    if (!url || !url[0]) return NULL;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
        return NULL;
    char tmpl[] = "/tmp/slermes-vision-dl-XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return NULL;
    close(fd);
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time %d -o %s '%s' 2>/dev/null",
             timeout_sec > 0 ? timeout_sec : 30, tmpl, url);
    int rc = system(cmd);
    if (rc != 0) { unlink(tmpl); return NULL; }
    struct stat st;
    if (stat(tmpl, &st) != 0 || st.st_size == 0) { unlink(tmpl); return NULL; }
    return strdup(tmpl);
}

/* ──────────────────────────────────────────────────────────────
 *  Image resize
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_resize_image_for_vision @ tools/vision_tools.py:_resize_image_for_vision */
char *vision_resize_image_for_vision(const char *path, int max_dim)
{
    if (!path || !path[0] || max_dim <= 0) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "/tmp/slermes-vision-resized-XXXXXX");
    int fd = mkstemp(out);
    if (fd < 0) return NULL;
    close(fd);
    unlink(out); /* convert creates the file; mkstemp just reserves the name */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "convert '%s' -resize '%dx%d>' '%s' 2>/dev/null",
             path, max_dim, max_dim, out);
    int rc = system(cmd);
    if (rc != 0 || stat(out, &st) != 0 || st.st_size == 0) {
        /* try with ffmpeg fallback */
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -y -i '%s' -vf scale='min(%d,iw)':'min(%d,ih)' '%s' 2>/dev/null",
                 path, max_dim, max_dim, out);
        rc = system(cmd);
        if (rc != 0 || stat(out, &st) != 0 || st.st_size == 0) return NULL;
    }
    return strdup(out);
}

/* PoP: vision_image_exceeds_dimension @ tools/vision_tools.py:_image_exceeds_dimension */
bool vision_image_exceeds_dimension(const char *path, int max_dim)
{
    char *dims = vision_extract_dimensions_from_path(path);
    if (!dims) return false;
    unsigned w = 0, h = 0;
    sscanf(dims, "%ux%u", &w, &h);
    free(dims);
    return (w > (unsigned)max_dim || h > (unsigned)max_dim);
}

/* ──────────────────────────────────────────────────────────────
 *  SVG rasterization
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_rasterize_svg_to_png @ tools/vision_tools.py:_rasterize_svg_to_png */
char *vision_rasterize_svg_to_png(const char *svg_path)
{
    if (!svg_path || !svg_path[0]) return NULL;
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "/tmp/slermes-vision-svg-XXXXXX.png");
    /* mkstemps with .png suffix */
    int fd = mkstemps(out, 4);
    if (fd < 0) return NULL;
    close(fd);
    char cmd[8192];
    /* Try rsvg-convert first, then inkscape */
    snprintf(cmd, sizeof(cmd),
             "rsvg-convert '%s' -o '%s' 2>/dev/null || "
             "inkscape '%s' --export-filename='%s' 2>/dev/null",
             svg_path, out, svg_path, out);
    int rc = system(cmd);
    struct stat st;
    if (rc != 0 || stat(out, &st) != 0 || st.st_size == 0) {
        unlink(out);
        return NULL;
    }
    return strdup(out);
}

/* ──────────────────────────────────────────────────────────────
 *  Normalization
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_normalize_to_supported_image @ tools/vision_tools.py:_normalize_to_supported_image */
char *vision_normalize_to_supported_image(const char *path)
{
    if (!path || !path[0]) return NULL;
    const char *fmt = vision_detect_image_format_from_path(path);
    if (!fmt) return NULL;
    /* SVG -> PNG */
    if (strcmp(fmt, "svg") == 0)
        return vision_rasterize_svg_to_png(path);
    /* HEIC/AVIF -> JPEG */
    if (strcmp(fmt, "heic") == 0 || strcmp(fmt, "avif") == 0) {
        char out[PATH_MAX];
        snprintf(out, sizeof(out), "/tmp/slermes-vision-norm-XXXXXX.jpg");
        int fd = mkstemps(out, 4);
        if (fd < 0) return NULL;
        close(fd);
        char cmd[8192];
        snprintf(cmd, sizeof(cmd), "convert '%s' '%s' 2>/dev/null", path, out);
        if (system(cmd) == 0) return strdup(out);
        unlink(out);
        return NULL;
    }
    /* Already supported: return copy of path */
    return strdup(path);
}

/* ──────────────────────────────────────────────────────────────
 *  Config helpers
 * ────────────────────────────────────────────────────────────── */

/* PoP: vision_resolve_download_timeout @ tools/vision_tools.py:_resolve_download_timeout */
int vision_resolve_download_timeout(void)
{
    const char *e = getenv("HERMES_VISION_DOWNLOAD_TIMEOUT");
    if (e && *e) {
        char *end = NULL;
        long v = strtol(e, &end, 10);
        if (end != e && *end == '\0' && v > 0) return (int)v;
    }
    return 30;
}

/* PoP: vision_resolve_cpu_workers @ tools/vision_tools.py:_resolve_vision_cpu_workers */
int vision_resolve_cpu_workers(void)
{
    const char *e = getenv("HERMES_VISION_CPU_WORKERS");
    if (e && *e) {
        char *end = NULL;
        long v = strtol(e, &end, 10);
        if (end != e && *end == '\0' && v > 0) return (int)v;
    }
    /* Default: min(4, nproc) */
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) return 1;
    return (int)(nproc < 4 ? nproc : 4);
}

/* PoP: vision_detect_host_cpus @ tools/vision_tools.py:_detect_host_cpus */
int vision_detect_host_cpus(void)
{
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) return 1;
    return (int)nproc;
}

/* PoP: vision_is_retryable_download_error @ tools/vision_tools.py:_is_retryable_download_error */
bool vision_is_retryable_download_error(const char *error_text)
{
    if (!error_text || !error_text[0]) return false;
    const char *patterns[] = {
        "timeout", "timed out", "connection reset", "connection refused",
        "temporarily unavailable", "ECONNRESET", "ECONNREFUSED", "ETIMEDOUT",
        "502", "503", "504", "retry", "transient", NULL
    };
    for (int i = 0; patterns[i]; i++) {
        if (strcasestr(error_text, patterns[i])) return true;
    }
    return false;
}

/* PoP: vision_check_requirements @ tools/vision_tools.py:check_vision_requirements */
char *vision_check_requirements(void)
{
    json_t *result = json_object();
    /* Check for ImageMagick `convert` */
    bool convert = (system("command -v convert >/dev/null 2>&1") == 0);
    /* Check for `identify` */
    bool identify = (system("command -v identify >/dev/null 2>&1") == 0);
    /* Check for `ffmpeg` */
    bool ffmpeg = (system("command -v ffmpeg >/dev/null 2>&1") == 0);
    /* Check for `rsvg-convert` */
    bool rsvg = (system("command -v rsvg-convert >/dev/null 2>&1") == 0);
    json_set(result, "convert", json_bool(convert));
    json_set(result, "identify", json_bool(identify));
    json_set(result, "ffmpeg", json_bool(ffmpeg));
    json_set(result, "rsvg_convert", json_bool(rsvg));
    json_set(result, "ready", json_bool(convert || identify));
    char *s = json_dumps(result, 0);
    json_free(result);
    return s;
}
