#ifndef NOC_DEADLINE_H
#define NOC_DEADLINE_H
#include <stddef.h>

/* A float | None: present==0 means Python None. */
typedef struct {
    int present;
    double value;
} deadline_opt_t;

/* declared_deadline_floor: registry floor + _DECLARED_DEADLINE_GRACE_S (5.0).
 * reg_floor is the (injected) registry.get_deadline_floor result; None stays None. */
deadline_opt_t deadline_declared_floor(deadline_opt_t reg_floor);

/* deadline_with_floor: raise generic to floor; a None on either side leaves
 * generic unchanged (a floor never introduces a deadline). */
deadline_opt_t deadline_with_floor(deadline_opt_t generic, deadline_opt_t floor);

/* batch_deadline: one deadline covering all non-parse-error calls. parse_error[i]
 * != 0 means skip; declared[i] is each call's declared deadline floor. */
deadline_opt_t deadline_batch(deadline_opt_t generic,
                              const int *parse_error,
                              const deadline_opt_t *declared, size_t n);
#endif
