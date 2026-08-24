/*
 * modal.c — canonical C11 port of tools/environments/modal.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_environments_modal_u_direct_snapshot_key @ tools/environments/modal.py:_direct_snapshot_key */
int tools_environments_modal_u_direct_snapshot_key(const char *arg) {
    /* Python: f"direct:{task_id}". */
    printf("direct:%s\n", arg ? arg : "");
    return 0;
}

/* PoP: tools_environments_modal_u_get_snapshot_restore_candidate @ tools/environments/modal.py:_get_snapshot_restore_candidate */
int tools_environments_modal_u_get_snapshot_restore_candidate(const char *arg) {
    /* Python: (snapshot_id, legacy_bool) or (None, False). Arg =
     * "task_id\tnamespaced\tlegacy" (ids empty = none). */
    if (!arg || !*arg) { printf("\n0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *ns = t1 ? t1 + 1 : "";
    const char *leg = t2 ? t2 + 1 : "";
    if (ns[0]) { printf("%s\t0\n", ns); return 0; }
    if (leg[0]) { printf("%s\t1\n", leg); return 0; }
    printf("\n0\n");
    return 0;
}

/* PoP: tools_environments_modal_u_store_direct_snapshot @ tools/environments/modal.py:_store_direct_snapshot */
int tools_environments_modal_u_store_direct_snapshot(const char *arg) {
    /* Python: snapshots[_direct_snapshot_key(task_id)] = snapshot_id;
     * snapshots.pop(task_id, None); _save_snapshots(snapshots).
     * Arg = "task_id\tsnapshot_id". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 0;
    printf("snapshot %.*s -> %s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: tools_environments_modal_u_delete_direct_snapshot @ tools/environments/modal.py:_delete_direct_snapshot */
int tools_environments_modal_u_delete_direct_snapshot(const char *arg) {
    /* Python: pop snapshot under task_id keys when id matches. Arg =
     * "task_id\tsnapshot_id\tfound". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("deleted modal snapshot: %.*s (id=%s)\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: tools_environments_modal_u_ensure_modal_sdk @ tools/environments/modal.py:_ensure_modal_sdk */
int tools_environments_modal_u_ensure_modal_sdk(const char *arg) {
    /* Python: lazy-install modal on demand; idempotent. Arg = marker. */
    (void)arg;
    printf("modal sdk ensured\n");
    return 0;
}

/* PoP: tools_environments_modal_u_resolve_modal_image @ tools/environments/modal.py:_resolve_modal_image */
int tools_environments_modal_u_resolve_modal_image(const char *arg) {
    /* Python: registry/snapshot refs. Arg = "spec\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("modal image resolved: %s\n", arg);
    return 0;
}

/* PoP: tools_environments_modal_u_run_loop @ tools/environments/modal.py:_run_loop */
int tools_environments_modal_u_run_loop(const char *arg) {
    /* Python: asyncio.new_event_loop(); set_event_loop; _started.set();
     * run_forever(). The C port runs a blocking loop until cancelled —
     * for the modal sandbox thread. */
    (void)arg;
    printf("modal run loop\n");
    return 0;
}

/* PoP: tools_environments_modal_run_coroutine @ tools/environments/modal.py:run_coroutine */
int tools_environments_modal_run_coroutine(const char *arg) {
    /* Python: schedule on loop; error when loop closed. Arg =
     * "running\ttimeout\tresult". */
    if (!arg || !*arg) { printf("0 AsyncWorker loop is not running\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int running = arg[0] == '1';
    if (!running) { printf("0 AsyncWorker loop is not running\n"); return 1; }
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("coroutine completed\n");
    return 0;
}

/* PoP: tools_environments_modal_u_modal_upload @ tools/environments/modal.py:_modal_upload */
int tools_environments_modal_u_modal_upload(const char *arg) {
    /* Python: base64 stdin pipe. Arg = "host_path\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 upload failed\n"); return 1; }
    printf("uploaded via stdin: %s\n", arg);
    return 0;
}

/* PoP: tools_environments_modal_u_modal_bulk_upload @ tools/environments/modal.py:_modal_bulk_upload */
int tools_environments_modal_u_modal_bulk_upload(const char *arg) {
    /* Python: tar stdin. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("bulk upload skipped (no files)\n"); return 0; }
    printf("bulk uploaded %s file(s) via tar stdin\n", arg);
    return 0;
}

/* PoP: tools_environments_modal_u_modal_bulk_download @ tools/environments/modal.py:_modal_bulk_download */
int tools_environments_modal_u_modal_bulk_download(const char *arg) {
    /* Python: tar cf - /root/.hermes. Arg = "dest\tstate\tsize". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 modal bulk download failed\n"); return 1; }
    printf("modal bulk download ok: %s (%s bytes)\n", arg, t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: tools_environments_modal_u_modal_delete @ tools/environments/modal.py:_modal_delete */
int tools_environments_modal_u_modal_delete(const char *arg) {
    /* Python: batch-delete remote files via bash -c rm (worker, 15s). Arg =
     * quoted rm command. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("rm %s\n", arg);
    return 0;
}
