/*
 * vault_local_store.c — the local backend over the encrypted vault files.
 *
 * Every method is load-then-delegate: the document comes from vault_file.c, and the shape questions
 * (metadata view, handle lookup, TOTP minting) stay in the backend module that already owned them.
 * Keeping that split means a fix to what a "handle" is lands in one place.
 */
#include "vault_local_store.h"
#include "vault_file.h"
#include "noc_w22_local_259750.h"

#include <stdlib.h>
#include <string.h>

/* One load, shared by every method. *items is never NULL: an empty vault (no key, no file, or a
 * document without an items array) yields an empty array, so the helpers below see exactly what they
 * would see for an in-memory records list and nobody has to special-case a NULL records list.
 *
 * Returns 0 on success. On failure returns -1 with *err set and the caller answers with that error.
 * Caller frees *doc and *synthetic. */
static int open_vault(const char *base, json_t **doc, json_t **synthetic,
                      const json_t **items, char **err)
{
    *doc = NULL;
    *synthetic = NULL;
    *items = NULL;
    *err = NULL;
    if (vault_load(base, doc, err) != 0) return -1;

    const json_t *found = json_obj_get(*doc, "items");
    if (found && found->type == JSON_ARRAY) {
        *items = found;
        return 0;
    }
    *synthetic = json_array();          /* data.get("items", []) */
    *items = *synthetic;
    return 0;
}

static json_t *load_error(const char *err)
{
    json_t *o = json_object();
    json_set(o, "error", json_string(err ? err : "the vault could not be opened"));
    return o;
}

json_t *vault_local_list(const char *base)
{
    json_t *doc = NULL, *empty = NULL;
    const json_t *items = NULL;
    char *err = NULL;
    if (open_vault(base, &doc, &empty, &items, &err) != 0) {
        json_t *o = load_error(err);
        free(err);
        return o;
    }
    json_t *out = w22_local_259750_list_items(items);
    json_free(empty);
    json_free(doc);
    return out;
}

json_t *vault_local_get_meta(const char *base, const char *handle)
{
    json_t *doc = NULL, *empty = NULL;
    const json_t *items = NULL;
    char *err = NULL;
    if (open_vault(base, &doc, &empty, &items, &err) != 0) {
        json_t *o = load_error(err);
        free(err);
        return o;
    }
    json_t *out = w22_local_259750_get_meta(items, handle);
    json_free(empty);
    json_free(doc);
    return out;
}

json_t *vault_local_resolve_password(const char *base, const char *handle)
{
    json_t *doc = NULL, *empty = NULL;
    const json_t *items = NULL;
    char *err = NULL;
    if (open_vault(base, &doc, &empty, &items, &err) != 0) {
        json_t *o = load_error(err);
        free(err);
        return o;
    }
    json_t *out = w22_local_259750_resolve_password(items, handle);
    json_free(empty);
    json_free(doc);
    return out;
}

json_t *vault_local_resolve_secret(const char *base, const char *handle)
{
    json_t *doc = NULL, *empty = NULL;
    const json_t *items = NULL;
    char *err = NULL;
    if (open_vault(base, &doc, &empty, &items, &err) != 0) {
        json_t *o = load_error(err);
        free(err);
        return o;
    }
    json_t *out = w22_local_259750_resolve_secret(items, handle);
    json_free(empty);
    json_free(doc);
    return out;
}

json_t *vault_local_resolve_otp(const char *base, const char *handle, const char *at_epoch)
{
    json_t *doc = NULL, *empty = NULL;
    const json_t *items = NULL;
    char *err = NULL;
    if (open_vault(base, &doc, &empty, &items, &err) != 0) {
        json_t *o = load_error(err);
        free(err);
        return o;
    }
    json_t *out = w22_local_259750_resolve_otp(items, handle, at_epoch);
    json_free(empty);
    json_free(doc);
    return out;
}

json_t *vault_local_backend_info(void)
{
    return w22_local_259750_backend_info();
}
