/*
 * lock.c — C home for upstream Hermes pm/lock.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: pm_lock_pending @ pm/lock.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const pm_lock_CONST_SCHEMA = 1;
static const char *const pm_lock_CONST_STORE_TOKEN = "{{store}}";

void pm_lock_port_pending(void) { (void)0; }
