/* Harness for the local backend over real vault files.
 *
 *   list <base>                        -> {"ok": [meta...]}   (upstream's vl_wrap_ok shape)
 *   meta <base> <handle>               -> {"ok": {...}|null}
 *   password <base> <handle>           -> {"ok": "..."}
 *   secret <base> <handle>             -> {"ok": {...}}
 *   otp <base> <handle> [epoch]        -> {"ok": "123456"|null}
 *   info                               -> {"name","display_name","prefix","needs_unlock"}
 */
#include "vault_local_store.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int emit(json_t *o) {
    char *s = json_serialize(o);
    json_free(o);
    if (!s) { printf("{\"error\":\"serialize failed\"}\n"); return 2; }
    fputs(s, stdout);
    free(s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cmd> ...\n", argv[0]); return 2; }
    const char *cmd = argv[1];

    if (!strcmp(cmd, "info")) return emit(vault_local_backend_info());
    if (argc < 3) { fprintf(stderr, "missing base dir\n"); return 2; }
    const char *base = argv[2];

    if (!strcmp(cmd, "list")) return emit(vault_local_list(base));
    if (argc < 4) { fprintf(stderr, "missing handle\n"); return 2; }
    const char *handle = argv[3];
    if (!strcmp(cmd, "meta")) return emit(vault_local_get_meta(base, handle));
    if (!strcmp(cmd, "password")) return emit(vault_local_resolve_password(base, handle));
    if (!strcmp(cmd, "secret")) return emit(vault_local_resolve_secret(base, handle));
    if (!strcmp(cmd, "otp")) return emit(vault_local_resolve_otp(base, handle, argc >= 5 ? argv[4] : NULL));
    fprintf(stderr, "unknown command %s\n", cmd);
    return 2;
}
