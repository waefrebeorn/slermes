/* noc_w19_backend_308e86.h - public C API for persistence/sqlite/_backend.py port. */
#ifndef NOC_W19_BACKEND_308E86_H
#define NOC_W19_BACKEND_308E86_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_backend_308e86_store_error @ persistence/sqlite/_backend.py:_store_error
 * The contract error one SQLite error maps to, in the module's exact precedence:
 * disk full, contention (lock / no-more-rows), disk-I/O marker, FTS corruption, malformed
 * schema, classify=="corrupt", unopenable file, else permanent. */
json_t *w19_backend_308e86_store_error(const char *message, int is_disk_full, int is_lock,
                                      int no_more_rows, int fts_corrupt, int malformed,
                                      const char *classify);

/* PoP: w19_backend_308e86_translate @ persistence/sqlite/_backend.py:translated
 * The error family `translated` raises: exc_kind 0 no error, 1 DeletedWalGenerationError,
 * 2 StateDbReplacedError, 3 StateDbCorruptError, 4 sqlite3.Error (through _store_error),
 * 5 anything else (propagates unchanged). */
json_t *w19_backend_308e86_translate(int exc_kind, const char *exc_name, const char *message,
                                    int is_disk_full, int is_lock, int no_more_rows,
                                    int fts_corrupt, int malformed, const char *classify);

/* PoP: w19_backend_308e86_register_repository @ persistence/sqlite/_backend.py:register_repository
 * Register one repository attribute on every new handle. already_registered holds the attributes
 * a previous registration claimed; capabilities is the flag-override mapping (or NULL).
 * Answers {"ok":bool,"error":str?,"registered":[attrs],"overrides":{...}}. */
json_t *w19_backend_308e86_register_repository(json_t *already_registered, const char *attribute,
                                              json_t *capabilities);

/* PoP: w19_backend_308e86_open_plan @ persistence/sqlite/_backend.py:SQLiteBackend.open
 * A handle bound to scope's store, or the refusal open() raises: an unheld profile first
 * (ValueError), then a closed backend (RuntimeError). store_keys lists the hermes_home_keys held. */
json_t *w19_backend_308e86_open_plan(json_t *store_keys, const char *scope_key,
                                    const char *profile_dir, int closed);

/* PoP: w19_backend_308e86_usable @ persistence/sqlite/_backend.py:SQLiteBackend._usable
 * A holder is usable when it exists and either the backend is read-only or the registry owns it. */
int w19_backend_308e86_usable(int has_holder, int read_only, int registry_owned);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_BACKEND_308E86_H */
