#ifndef NOC_W9_STATUSLOCK_CE2CE2_H
#define NOC_W9_STATUSLOCK_CE2CE2_H
#include "hermes_json.h"

/* PoP port of gateway/status_lock_dir.py — scoped-lock-dir report state. */
void    w9_statuslock_ce2ce2_reset(void);
int     w9_statuslock_ce2ce2_reported(const char *lock_dir);
void    w9_statuslock_ce2ce2_recovered(const char *lock_dir);
json_t *w9_statuslock_ce2ce2_unusable(const char *lock_dir, int err_num,
                                      const char *strerror_text, const char *filename);
#endif /* NOC_W9_STATUSLOCK_CE2CE2_H */
