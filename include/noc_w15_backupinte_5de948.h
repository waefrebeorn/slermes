#ifndef NOC_W15_BACKUPINTE_5DE948_H
#define NOC_W15_BACKUPINTE_5DE948_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_backupinte_5de948_verify_sqlite_integrity @ hermes_cli/backup_integrity.py:verify_sqlite_integrity
 * Returns {"valid":bool,"message":str,"size":null|int}. The PRAGMA / schema
 * probe branches are represented for header-valid files; live SQLite walks stay
 * in the Python owner. */
struct json_t *w15_backupinte_5de948_verify_sqlite_integrity(
    const char *path, int check_header, int run_pragma, long long max_bytes);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_BACKUPINTE_5DE948_H */
