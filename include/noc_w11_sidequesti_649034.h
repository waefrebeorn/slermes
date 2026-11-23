#ifndef NOC_W11_SIDEQUESTI_649034_H
#define NOC_W11_SIDEQUESTI_649034_H

#include <hermes_json.h>

/* C11 ports of agent/side_question_lease.py (pure decision helpers).  Every
 * symbol is prefixed w11_sidequesti_649034_.  Each takes one borrowed
 * json_t *args object and returns a NEW json_t * (caller frees).
 * Lease state is injected through the args object so the port is stateless. */
json_t *w11_sidequesti_649034_unattended(json_t *a);
json_t *w11_sidequesti_649034_redeemable(json_t *a);

#endif /* NOC_W11_SIDEQUESTI_649034_H */
