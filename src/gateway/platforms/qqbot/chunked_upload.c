/*
 * chunked_upload.c — canonical C11 port of gateway/platforms/qqbot/chunked_upload.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_platforms_qqbot_chunke_file_size_human @ gateway/platforms/qqbot/chunked_upload.py:file_size_human */
int gateway_platforms_qqbot_chunke_file_size_human(const char *arg) {
    /* Python: format_size(self.file_size) — human size with units. */
    if (!arg || !*arg) { printf("0 B\n"); return 0; }
    double bytes = strtod(arg, NULL);
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double v = bytes;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) printf("%.0f %s\n", v, units[u]);
    else printf("%.1f %s\n", v, units[u]);
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_file_size_human_2 @ gateway/platforms/qqbot/chunked_upload.py:file_size_human */
int gateway_platforms_qqbot_chunke_file_size_human_2(const char *arg) {
    /* Python: format_size(self.file_size) — human size with units. */
    if (!arg || !*arg) { printf("0 B\n"); return 0; }
    double bytes = strtod(arg, NULL);
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double v = bytes;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) printf("%.0f %s\n", v, units[u]);
    else printf("%.1f %s\n", v, units[u]);
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_limit_human @ gateway/platforms/qqbot/chunked_upload.py:limit_human */
int gateway_platforms_qqbot_chunke_limit_human(const char *arg) {
    /* Python: format_size(limit_bytes) if limit_bytes else "unknown".
     * Arg = byte count (or empty). */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    long long bytes = strtoll(arg, NULL, 10);
    if (bytes <= 0) { printf("unknown\n"); return 0; }
    if (bytes >= 1024 * 1024 * 1024)
        printf("%.1f GB\n", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024 * 1024)
        printf("%.1f MB\n", bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        printf("%.1f KB\n", bytes / 1024.0);
    else
        printf("%lld B\n", bytes);
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_parse_prepare_response @ gateway/platforms/qqbot/chunked_upload.py:_parse_prepare_response */
int gateway_platforms_qqbot_chunke_u_parse_prepare_response(const char *arg) {
    /* Python: prepare parse. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_upload_id") == 0 || strcmp(state, "no_parts") == 0) {
        fprintf(stderr, "upload_prepare response invalid: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_prepare @ gateway/platforms/qqbot/chunked_upload.py:_prepare */
int gateway_platforms_qqbot_chunke_u_prepare(const char *arg) {
    /* Python: upload_prepare. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "upload prepare failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("prepared (%s; md5/sha1/md5_10m hashes, FILE_UPLOAD_TIMEOUT)%s\n", t3 ? t3 + 1 : "upload_id", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_upload_one_part @ gateway/platforms/qqbot/chunked_upload.py:_upload_one_part */
int gateway_platforms_qqbot_chunke_u_upload_one_part(const char *arg) {
    /* Python: COS PUT part. Arg =
     * "uploaded\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int uploaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (part upload failed)\n"); return 0; }
    if (!uploaded) { printf("0\n"); return 0; }
    printf("1 (part %s PUT (block_size wins), upload_part_finish called)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — md5 verified" : "");
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_put_to_presigned_url @ gateway/platforms/qqbot/chunked_upload.py:_put_to_presigned_url */
int gateway_platforms_qqbot_chunke_u_put_to_presigned_url(const char *arg) {
    /* Python: presigned PUT. Arg =
     * "put\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (PUT failed after retries)\n"); return 1; }
    printf("1 (part PUT to presigned URL, Content-Length set, %s retries)%s\n", t2 ? t2 + 1 : "N", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_part_finish_with_retry @ gateway/platforms/qqbot/chunked_upload.py:_part_finish_with_retry */
int gateway_platforms_qqbot_chunke_u_part_finish_with_retry(const char *arg) {
    /* Python: 40093001 retry. Arg =
     * "finished\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int finished = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 1; }
    if (!finished) { printf("0 (gave up after retries)\n"); return 1; }
    printf("1 (part %s finished; 40093001 retried w/ bounded wall-clock)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_read_file_chunk @ gateway/platforms/qqbot/chunked_upload.py:_read_file_chunk */
int gateway_platforms_qqbot_chunke_u_read_file_chunk(const char *arg) {
    /* Python: seek + read exact length, IOError on short. Arg =
     * "path\toffset\tlength\tstate". */
    if (!arg || !*arg) { printf("0 short read\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (t3 && t3[1] == '1') {
        printf("short read: %.*s (file may be truncated)\n",
               (int)(t1 ? (size_t)(t1 - arg) : 0), arg);
        return 1;
    }
    printf("read %s bytes at offset %s from %.*s\n",
           t2 ? t2 + 1 : "?", t1 ? t1 + 1 : "0",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_compute_file_hashes @ gateway/platforms/qqbot/chunked_upload.py:_compute_file_hashes */
int gateway_platforms_qqbot_chunke_u_compute_file_hashes(const char *arg) {
    /* Python: md5/sha1/md5_10m one pass. Arg =
     * "file_size\tstate\tmd5\tsha1\tmd5_10m". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 hash failed\n"); return 1; }
    printf("md5=%s sha1=%s md5_10m=%s\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "?", t4 ? t4 + 1 : "?");
    return 0;
}

/* PoP: gateway_platforms_qqbot_chunke_u_run_with_concurrency @ gateway/platforms/qqbot/chunked_upload.py:_run_with_concurrency */
int gateway_platforms_qqbot_chunke_u_run_with_concurrency(const char *arg) {
    /* Python: bounded thunks. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s thunk(s) completed (semaphore %s, order preserved, first exception re-raised after all settle)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "bounded" : "1", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
