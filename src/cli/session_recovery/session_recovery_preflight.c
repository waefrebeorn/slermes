/* session_recovery_preflight.c — disk-space preflight + source-bundle copy.
 * Faithful port of hermes_cli/session_recovery.py (preflight slice).
 */
#define _GNU_SOURCE
#include "session_recovery_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

static long long sr_disk_free(const char *path) {
    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) return -1;
    return (long long)vfs.f_bavail * (long long)vfs.f_frsize;
}

/* PoP: sr_disk_space_preflight @ hermes_cli/session_recovery.py:_disk_space_preflight */
json_t *sr_disk_space_preflight(const char *source, const char *work_root,
                                const char *output_parent,
                                char *err, size_t elen) {
    long long bundle_bytes = 0;
    for (size_t i = 0; i < 4; i++) {
        char *p = sr_sidecar_path(source, SR_SIDECAR_SUFFIXES[i]);
        long long sz = sr_file_size(p);
        if (sz > 0) bundle_bytes += sz;
        free(p);
    }
    long long output_allowance = output_parent ? bundle_bytes : 0;
    long long headroom = (long long)((double)(bundle_bytes + output_allowance) * 0.05);
    if (headroom < SR_MIN_SPACE_HEADROOM) headroom = SR_MIN_SPACE_HEADROOM;

    long long work_free = sr_disk_free(work_root);
    json_t *report = json_object();
    json_set(report, "source_bundle_bytes", json_number((double)bundle_bytes));
    json_set(report, "estimated_output_bytes", json_number((double)output_allowance));
    json_set(report, "headroom_bytes", json_number((double)headroom));
    json_set(report, "work_dir", json_string(work_root));
    json_set(report, "work_dir_free_bytes", json_number((double)work_free));

    if (!output_parent || sr_same_filesystem(work_root, output_parent)) {
        long long required = bundle_bytes + output_allowance + headroom;
        json_set(report, "shared_filesystem", json_bool(true));
        json_set(report, "work_dir_required_bytes", json_number((double)required));
        if (work_free < required) {
            char *avail = session_recovery_format_bytes(work_free);
            char *req = session_recovery_format_bytes(required);
            char *bun = session_recovery_format_bytes(bundle_bytes);
            char *outa = session_recovery_format_bytes(output_allowance);
            char *head = session_recovery_format_bytes(headroom);
            sr_set_err(err, elen,
                "Not enough free disk space for a safe recovery copy: "
                "%s available at %s, %s required (%s source bundle + "
                "%s output allowance + %s headroom). Use --work-dir or "
                "--output on a filesystem with more free space.",
                avail, work_root, req, bun, outa, head);
            free(avail); free(req); free(bun); free(outa); free(head);
            json_free(report);
            return NULL;
        }
        return report;
    }

    long long output_free = sr_disk_free(output_parent);
    long long work_required = bundle_bytes + headroom;
    long long output_required = output_allowance + headroom;
    json_set(report, "shared_filesystem", json_bool(false));
    json_set(report, "work_dir_required_bytes", json_number((double)work_required));
    json_set(report, "output_dir", json_string(output_parent));
    json_set(report, "output_dir_free_bytes", json_number((double)output_free));
    json_set(report, "output_dir_required_bytes", json_number((double)output_required));

    char shortages[1024] = "";
    if (work_free < work_required) {
        char *a = session_recovery_format_bytes(work_free);
        char *r = session_recovery_format_bytes(work_required);
        snprintf(shortages, sizeof(shortages), "%s: %s available, %s required",
                 work_root, a, r);
        free(a); free(r);
    }
    if (output_free < output_required) {
        char *a = session_recovery_format_bytes(output_free);
        char *r = session_recovery_format_bytes(output_required);
        size_t off = strlen(shortages);
        snprintf(shortages + off, sizeof(shortages) - off,
                 "%s%s: %s available, %s required",
                 off ? "; " : "", output_parent, a, r);
        free(a); free(r);
    }
    if (shortages[0]) {
        sr_set_err(err, elen,
            "Not enough free disk space for safe recovery: %s. "
            "Choose work/output filesystems with more free space.", shortages);
        json_free(report);
        return NULL;
    }
    return report;
}

static int sr_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[1 << 16];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    }
    if (ferror(in)) rc = -1;
    fclose(in);
    if (fclose(out) != 0) rc = -1;
    if (rc == 0) {
        /* copy2 preserves mtime */
        struct stat st;
        if (stat(src, &st) == 0) {
            struct timespec times[2] = { st.st_atim, st.st_mtim };
            utimensat(AT_FDCWD, dst, times, 0);
        }
    }
    return rc;
}

/* PoP: sr_copy_source_bundle @ hermes_cli/session_recovery.py:_copy_source_bundle */
int sr_copy_source_bundle(const char *source, const char *snapshot_dir,
                          char **snapshot_source_out, json_t **copied_out,
                          char *err, size_t elen) {
    char *dup = strdup(source);
    char *name = basename(dup);
    size_t n = strlen(snapshot_dir) + 1 + strlen(name);
    char *snapshot_source = malloc(n + 1);
    snprintf(snapshot_source, n + 1, "%s/%s", snapshot_dir, name);
    free(dup);

    json_t *copied = json_array();
    for (size_t i = 0; i < 4; i++) {
        char *source_part = sr_sidecar_path(source, SR_SIDECAR_SUFFIXES[i]);
        if (!sr_path_lexists(source_part)) { free(source_part); continue; }
        char *dest_part = sr_sidecar_path(snapshot_source, SR_SIDECAR_SUFFIXES[i]);
        if (sr_copy_file(source_part, dest_part) != 0) {
            sr_set_err(err, elen, "failed to copy %s", source_part);
            free(source_part);
            free(dest_part);
            free(snapshot_source);
            json_free(copied);
            return SESSION_RECOVERY_SAFETY_ERROR;
        }
        char *dd = strdup(dest_part);
        json_append(copied, json_string(basename(dd)));
        free(dd);
        free(source_part);
        free(dest_part);
    }
    *snapshot_source_out = snapshot_source;
    *copied_out = copied;
    return 0;
}
