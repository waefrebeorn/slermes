/*
 * vault_file.c — the vault document: Fernet over {"version": 1, "items": [...]}.
 *
 * Ported from agent/vault_store.py: _read_all, _write_all, _meta, list_items, get_meta.
 *
 * The interesting decisions are all upstream's and are kept deliberately:
 *
 *   - a missing or empty vault.json.enc is an EMPTY VAULT, not an error. Treating it as an error is
 *     how a first run ends up unable to create the first item.
 *   - decrypt failure and corrupt JSON get different messages, because "your key is wrong" and "this
 *     file is damaged" send the user to completely different recoveries.
 *   - the metadata view drops every secret value and only reports has_otp as a boolean; the listing
 *     is what reaches tool results, so a value here is a leak.
 *   - identifier/identifier_type are metadata (the agent may type the email itself); only the
 *     password half of a login stays secret.
 */
#include "vault_file.h"
#include "fernet.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *err_new(const char *msg)
{
    return msg ? dup_cstr(msg) : NULL;
}

static char *path_join(const char *base, const char *name)
{
    size_t bl = strlen(base), nl = strlen(name);
    int need_sep = bl > 0 && base[bl - 1] != '/' && base[bl - 1] != '\\';
    char *p = malloc(bl + (size_t)need_sep + nl + 1);
    if (!p) return NULL;
    memcpy(p, base, bl);
    if (need_sep) p[bl] = '/';
    memcpy(p + bl + need_sep, name, nl + 1);
    return p;
}

static unsigned char *read_file(const char *path, size_t *out_len, int *missing)
{
    *missing = 0;
    FILE *f = fopen(path, "rb");
    if (!f) { *missing = 1; return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    unsigned char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    *out_len = got;
    return buf;
}

/* ── key ────────────────────────────────────────────────────────────────────── */

int vault_key_load(const char *base, char **out_secret, char **err)
{
    if (out_secret) *out_secret = NULL;
    if (err) *err = NULL;
    char *path = path_join(base, "vault.key");
    if (!path) { if (err) *err = err_new("out of memory"); return -1; }

    int missing = 0;
    size_t len = 0;
    unsigned char *raw = read_file(path, &len, &missing);
    free(path);
    if (!raw || len == 0) {
        free(raw);
        if (err) *err = err_new(missing ? "vault.key is missing" : "vault.key is empty");
        return -1;
    }
    /* Upstream does read_bytes().strip(): the trailing newline an editor leaves behind is not part of
     * the key, and a key with one appended decrypts nothing. */
    size_t end = len;
    while (end > 0 && isspace((unsigned char)raw[end - 1])) end--;
    size_t start = 0;
    while (start < end && isspace((unsigned char)raw[start])) start++;
    char *secret = malloc(end - start + 1);
    if (!secret) { free(raw); if (err) *err = err_new("out of memory"); return -1; }
    memcpy(secret, raw + start, end - start);
    secret[end - start] = '\0';
    free(raw);
    *out_secret = secret;
    return 0;
}

/* ── document ───────────────────────────────────────────────────────────────── */

int vault_doc_parse(const char *key_secret, const unsigned char *blob, size_t len,
                    json_t **doc, char **err)
{
    if (doc) *doc = NULL;
    if (err) *err = NULL;

    size_t plain_len = 0;
    char *dec_err = NULL;
    unsigned char *plain = fernet_decrypt(key_secret, (const char *)blob, 0, -1, &plain_len, &dec_err);
    free(dec_err);
    if (!plain) {
        if (err) *err = err_new("vault file could not be decrypted (key mismatch or corruption)");
        return -1;
    }

    /* json_parse takes a C string, and the decrypted buffer is raw bytes with no terminator. Handing
     * it over as-is reads whatever follows in the heap: that is how a perfectly good document gets
     * reported as "invalid JSON" depending on what was allocated next. */
    char *text = malloc(plain_len + 1);
    if (!text) {
        free(plain);
        if (err) *err = err_new("out of memory");
        return -1;
    }
    memcpy(text, plain, plain_len);
    text[plain_len] = '\0';
    int embedded_nul = memchr(plain, '\0', plain_len) != NULL;
    free(plain);
    if (embedded_nul) {
        free(text);
        if (err) *err = err_new("vault file is corrupted (invalid JSON)");
        return -1;
    }

    json_t *parsed = json_parse(text, NULL);
    free(text);
    if (!parsed) {
        if (err) *err = err_new("vault file is corrupted (invalid JSON)");
        return -1;
    }
    if (parsed->type != JSON_OBJECT) {
        json_free(parsed);
        if (err) *err = err_new("vault file is corrupted (unexpected shape)");
        return -1;
    }
    *doc = parsed;
    return 0;
}

char *vault_doc_serialize(const char *key_secret, const json_t *doc, char **err)
{
    if (err) *err = NULL;
    char *payload = json_serialize((json_t *)doc);
    if (!payload) { if (err) *err = err_new("out of memory"); return NULL; }
    char *token = fernet_encrypt_random(key_secret, (const unsigned char *)payload, strlen(payload), err);
    free(payload);
    return token;
}

int vault_load(const char *base, json_t **doc, char **err)
{
    if (doc) *doc = NULL;
    if (err) *err = NULL;

    char *key = NULL;
    char *key_err = NULL;
    if (vault_key_load(base, &key, &key_err) != 0) {
        /* No key and no ciphertext: an empty vault, and the caller may create a key. No key WITH
         * ciphertext is the dangerous case, and it is not silently "empty". */
        char *path = path_join(base, "vault.json.enc");
        int missing = 0;
        size_t len = 0;
        unsigned char *blob = path ? read_file(path, &len, &missing) : NULL;
        free(path);
        int has_ct = blob && len > 0;
        free(blob);
        free(key_err);
        if (!has_ct) {
            json_t *empty = json_object();
            json_set(empty, "version", json_number(1));
            json_set(empty, "items", json_array());
            *doc = empty;
            return 0;
        }
        if (err) *err = err_new("the vault key could not be found; vault.json.enc was left untouched");
        return -1;
    }

    char *path = path_join(base, "vault.json.enc");
    int missing = 0;
    size_t len = 0;
    unsigned char *blob = path ? read_file(path, &len, &missing) : NULL;
    free(path);
    if (!blob || len == 0) {
        free(blob);
        free(key);
        json_t *empty = json_object();
        json_set(empty, "version", json_number(1));
        json_set(empty, "items", json_array());
        *doc = empty;
        return 0;
    }

    int rc = vault_doc_parse(key, blob, len, doc, err);
    free(blob);
    free(key);
    return rc;
}

/* ── metadata view ──────────────────────────────────────────────────────────── */

static int rec_has_otp(const json_t *rec)
{
    const json_t *secret = json_obj_get(rec, "secret");
    if (!secret || secret->type != JSON_OBJECT) return 0;
    const json_t *otp = json_obj_get(secret, "otp_secret");
    if (!otp || otp->type == JSON_NULL) return 0;
    /* bool(...) in Python: an empty string is falsy, anything else present is truthy. */
    if (otp->type == JSON_STRING) return json_string_value(otp) && json_string_value(otp)[0] != '\0';
    return 1;
}

json_t *vault_meta_of(const json_t *rec)
{
    json_t *o = json_object();
    if (!rec || rec->type != JSON_OBJECT) return o;

    const json_t *id = json_obj_get(rec, "id");
    const json_t *kind = json_obj_get(rec, "kind");
    const json_t *label = json_obj_get(rec, "label");
    const json_t *created = json_obj_get(rec, "created_at");
    json_set(o, "id", json_string(json_string_value_safe(id) ? json_string_value_safe(id) : ""));
    json_set(o, "kind", json_string(json_string_value_safe(kind) ? json_string_value_safe(kind) : ""));
    json_set(o, "label", json_string(json_string_value_safe(label) ? json_string_value_safe(label) : ""));
    json_set(o, "origin", json_copy(json_obj_get(rec, "origin")));   /* may be null, and stays null */
    json_set(o, "created_at",
             json_string(json_string_value_safe(created) ? json_string_value_safe(created) : ""));

    /* Upstream keys identifier/identifier_type on the identifier being present at all. */
    const json_t *ident = json_obj_get(rec, "identifier");
    /* Present and non-empty: Python's `identifier or None` collapses "" to absent, and an absent
     * identifier means identifier_type is not reported either. */
    int has_ident = json_string_value_safe(ident) && json_string_value_safe(ident)[0] != '\0';
    if (has_ident) {
        json_set(o, "identifier", json_copy(ident));
        const json_t *itype = json_obj_get(rec, "identifier_type");
        json_set(o, "identifier_type", json_copy(itype));            /* may be null, and stays null */
    }
    if (rec_has_otp(rec)) json_set(o, "has_otp", json_bool(1));

    /* allowed_origins is deliberately absent: upstream's _meta does not populate it, and inventing it
     * here would report a fill scope the Python side never claimed. */
    return o;
}

static const json_t *doc_items(const json_t *doc)
{
    const json_t *items = json_obj_get(doc, "items");
    return (items && items->type == JSON_ARRAY) ? items : NULL;
}

json_t *vault_meta_list(const json_t *doc)
{
    json_t *out = json_array();
    const json_t *items = doc_items(doc);
    if (!items) return out;
    for (size_t i = 0; i < json_len(items); i++) {
        const json_t *rec = json_get(items, i);
        if (!rec || rec->type != JSON_OBJECT) continue;
        json_append(out, vault_meta_of(rec));
    }
    return out;
}

json_t *vault_find(const json_t *doc, const char *item_id)
{
    const json_t *items = doc_items(doc);
    if (!items || !item_id) return NULL;
    for (size_t i = 0; i < json_len(items); i++) {
        const json_t *rec = json_get(items, i);
        if (!rec || rec->type != JSON_OBJECT) continue;
        const char *id = json_get_str(rec, "id", "");
        if (id && strcmp(id, item_id) == 0) return (json_t *)rec;
    }
    return NULL;
}
