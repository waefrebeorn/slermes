/*
 * vault_file.h — the vault on disk: one Fernet-encrypted JSON document.
 *
 * Layout, exactly as agent.vault_store.VaultStore writes it:
 *
 *   <base>/vault.json.enc   Fernet(key, mode 0600) over {"version": 1, "items": [...]}
 *   <base>/vault.key        the base64url Fernet secret, mode 0600
 *
 * A record is {id, kind, label, origin, created_at, identifier_type, identifier, secret{...}}.
 * `vault_meta_of` is the metadata-only view: it must never carry a secret value, because that view is
 * what reaches tool results.
 */
#ifndef HERMES_VAULT_FILE_H
#define HERMES_VAULT_FILE_H

#include "hermes_json.h"

/* Read <base>/vault.key and trim it. Returns 0 on success, -1 with *err set. */
int vault_key_load(const char *base, char **out_secret, char **err);

/* Decrypt a document blob. *doc receives {"version":1,"items":[...]}. Returns 0, or -1 with *err set
 * to upstream's wording: "vault file could not be decrypted (key mismatch or corruption)",
 * "vault file is corrupted (invalid JSON)", "vault file is corrupted (unexpected shape)". */
int vault_doc_parse(const char *key_secret, const unsigned char *blob, size_t len,
                    json_t **doc, char **err);

/* Encrypt a document. Returns a malloc'd token, or NULL with *err set. */
char *vault_doc_serialize(const char *key_secret, const json_t *doc, char **err);

/* Load <base> for reading. A missing or empty vault.json.enc is an empty vault, not an error -- that
 * is what upstream does, and it is what makes a first run work. */
int vault_load(const char *base, json_t **doc, char **err);

/* Metadata-only view of one record. Never includes secret values. */
json_t *vault_meta_of(const json_t *rec);

/* Metadata-only listing, in document order. */
json_t *vault_meta_list(const json_t *doc);

/* The record with this id, or NULL. Caller does not own the result. */
json_t *vault_find(const json_t *doc, const char *item_id);

#endif /* HERMES_VAULT_FILE_H */
