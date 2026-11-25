/* Differential harness for the ported vault document layer.
 *
 * Commands (one per invocation, so a failure names one thing):
 *   list <base>                 -> {"items": [meta, ...]}
 *   find <base> <id>            -> {"found": <meta|null>}
 *   secret <base> <id>          -> {"secret": {...}} or {"error": ...}
 *   reserialize <base> <out>    -> decrypt, re-encrypt to <out>, print {"ok":true}
 *   errors                      -> the four corrupt-file messages, from crafted blobs
 */
#include "vault_file.h"
#include "fernet.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    return b;
}

static int emit(json_t *o) {
    char *s = json_serialize(o);
    json_free(o);
    if (!s) { printf("{\"error\":\"serialize failed\"}\n"); return 2; }
    fputs(s, stdout);
    free(s);
    return 0;
}

static int cmd_list(const char *base) {
    json_t *doc = NULL; char *err = NULL;
    if (vault_load(base, &doc, &err) != 0) {
        json_t *o = json_object();
        json_set(o, "error", json_string(err ? err : "load failed"));
        free(err);
        return emit(o);
    }
    json_t *o = json_object();
    json_set(o, "items", vault_meta_list(doc));
    json_free(doc);
    return emit(o);
}

static int cmd_find(const char *base, const char *id) {
    json_t *doc = NULL; char *err = NULL;
    if (vault_load(base, &doc, &err) != 0) {
        json_t *o = json_object();
        json_set(o, "error", json_string(err ? err : "load failed"));
        free(err);
        return emit(o);
    }
    json_t *o = json_object();
    json_t *rec = vault_find(doc, id);
    json_set(o, "found", rec ? vault_meta_of(rec) : json_null());
    json_free(doc);
    return emit(o);
}

static int cmd_secret(const char *base, const char *id) {
    json_t *doc = NULL; char *err = NULL;
    if (vault_load(base, &doc, &err) != 0) {
        json_t *o = json_object();
        json_set(o, "error", json_string(err ? err : "load failed"));
        free(err);
        return emit(o);
    }
    json_t *o = json_object();
    json_t *rec = vault_find(doc, id);
    if (!rec) {
        char msg[256];
        snprintf(msg, sizeof msg, "no vault item with id '%s'", id);
        json_set(o, "error", json_string(msg));
    } else {
        json_set(o, "secret", json_copy(json_obj_get(rec, "secret")));
    }
    json_free(doc);
    return emit(o);
}

/* Rewrites the vault in place under the same key. The point is the direction: after this, Python must
 * still be able to read it. */
static int cmd_reserialize(const char *base, const char *out_path) {
    json_t *doc = NULL; char *err = NULL;
    if (vault_load(base, &doc, &err) != 0) {
        json_t *o = json_object();
        json_set(o, "error", json_string(err ? err : "load failed"));
        free(err);
        return emit(o);
    }
    char *key = NULL;
    if (vault_key_load(base, &key, &err) != 0) {
        json_t *o = json_object();
        json_set(o, "error", json_string(err ? err : "no key"));
        free(err);
        json_free(doc);
        return emit(o);
    }
    char *token = vault_doc_serialize(key, doc, &err);
    json_free(doc);
    free(key);
    if (!token) {
        json_t *o = json_object();
        json_set(o, "error", json_string(err ? err : "serialize failed"));
        free(err);
        return emit(o);
    }
    FILE *f = fopen(out_path, "wb");
    if (!f) { free(token); json_t *o = json_object(); json_set(o, "ok", json_bool(0)); return emit(o); }
    fputs(token, f);
    fclose(f);
    free(token);
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    return emit(o);
}

/* The three corruption paths, driven from blobs the oracle writes, so the messages are compared with
 * upstream's rather than assumed. */
static int cmd_errors(const char *dir) {
    char keypath[512];
    snprintf(keypath, sizeof keypath, "%s/key.txt", dir);
    char *key = read_all(keypath);
    if (!key) { printf("{\"error\":\"no key fixture\"}\n"); return 2; }

    json_t *out = json_object();

    /* 1. a blob that is not a Fernet token at all */
    char *e1 = NULL;
    json_t *d1 = NULL;
    int r1 = vault_doc_parse(key, (const unsigned char *)"not-a-token", 11, &d1, &e1);
    json_set(out, "bad_token", json_string(r1 == 0 ? "" : (e1 ? e1 : "")));
    free(e1); json_free(d1);

    /* 2. a valid token over bytes that are not JSON at all. Encrypting the bytes directly matters: a
     * JSON *string* is valid JSON and would land in the shape check, not this one. */
    char *e2 = NULL;
    char *tok2 = NULL;
    {
        const char *junk = "this is not a document";
        char *err = NULL;
        tok2 = fernet_encrypt_random(key, (const unsigned char *)junk, strlen(junk), &err);
        free(err);
    }
    json_t *d2 = NULL;
    int r2 = tok2 ? vault_doc_parse(key, (const unsigned char *)tok2, strlen(tok2), &d2, &e2) : -1;
    json_set(out, "bad_json", json_string(r2 == 0 ? "" : (e2 ? e2 : "")));
    free(e2); json_free(d2); free(tok2);

    /* 3. a valid token over a JSON value that is not an object */
    char *e3 = NULL;
    char *tok3 = NULL;
    {
        char *err = NULL;
        tok3 = vault_doc_serialize(key, json_array(), &err);
        free(err);
    }
    json_t *d3 = NULL;
    int r3 = tok3 ? vault_doc_parse(key, (const unsigned char *)tok3, strlen(tok3), &d3, &e3) : -1;
    json_set(out, "bad_shape", json_string(r3 == 0 ? "" : (e3 ? e3 : "")));
    /* Lengths, so a mismatch says whether the token was even produced. */
    json_set(out, "len_junk_tok", json_number((double)(tok2 ? strlen(tok2) : 0)));
    json_set(out, "len_array_tok", json_number((double)(tok3 ? strlen(tok3) : 0)));
    free(e3); json_free(d3); free(tok3);

    free(key);
    return emit(out);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s list|find|secret|reserialize|errors ...\n", argv[0]);
        return 2;
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "list")) return cmd_list(argv[2]);
    if (!strcmp(cmd, "find") && argc >= 4) return cmd_find(argv[2], argv[3]);
    if (!strcmp(cmd, "secret") && argc >= 4) return cmd_secret(argv[2], argv[3]);
    if (!strcmp(cmd, "reserialize") && argc >= 4) return cmd_reserialize(argv[2], argv[3]);
    if (!strcmp(cmd, "errors")) return cmd_errors(argv[2]);
    fprintf(stderr, "bad usage\n");
    return 2;
}
