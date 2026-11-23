#ifndef NOC_W10_BACKUPATOM_F4248F_H
#define NOC_W10_BACKUPATOM_F4248F_H

/* PoP: w10_backupatom_f4248f_normalize_private_modes @ hermes_cli/backup_atomic.py:_normalize_private_modes
 * PoP: w10_backupatom_f4248f_fsync_staged_dir @ hermes_cli/backup_atomic.py:_fsync_staged_dir */
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Replay of _normalize_private_modes' chmod decisions. os_name is "nt" or
 * "posix"; root/staging are path strings; entries is an array of
 * {"path":<str>,"kind":"dir"|"file"|"symlink"|"other"} from staging.rglob("*").
 * Returns an array of {"path":<str>,"mode":<int>} in call order. */
struct json_t *w10_backupatom_f4248f_normalize_modes(const struct json_t *os_name,
                                                      const struct json_t *root,
                                                      const struct json_t *staging,
                                                      const struct json_t *entries);

/* Replay of _fsync_staged_dir's fsync disposition. os_name is "nt"/"posix";
 * err is a JSON number (the OSError errno) or JSON null (no error);
 * unsupported is the live module's _DIR_FSYNC_UNSUPPORTED errno set.
 * Returns 0=no-op (nt), 1=fsync ok, 2=error suppressed, 3=error re-raised. */
struct json_t *w10_backupatom_f4248f_fsync_dir(const struct json_t *os_name,
                                                const struct json_t *err,
                                                const struct json_t *unsupported);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_BACKUPATOM_F4248F_H */
