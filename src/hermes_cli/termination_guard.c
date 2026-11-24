/*
 * termination_guard.c — real C11 port of selected pieces of upstream Hermes
 * hermes_cli/termination_guard.py.
 *
 * PoP: w16_termination_ef34ac_exit_status @ hermes_cli/termination_guard.py:_exit_on_signal
 * PoP: w16_termination_ef34ac_termination_signals @ hermes_cli/termination_guard.py:TERMINATION_SIGNALS
 * PoP: w16_termination_ef34ac_guard_plan @ hermes_cli/termination_guard.py:unwind_on_termination
 */
#include "noc_w16_termination_ef34ac.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int array_has_signal(json_t *arr, int value) {
    size_t i, n = (arr && arr->type == JSON_ARRAY) ? arr->c.count : 0;
    for (i = 0; i < n; i++) {
        json_t *v = arr->c.items[i];
        if (v && (v->type == JSON_NUMBER) && v->num_val == (double)value) return 1;
    }
    return 0;
}

int w16_termination_ef34ac_exit_status(int signum) { return 128 + signum; }

json_t *w16_termination_ef34ac_termination_signals(json_t *available) {
    static const int candidates[2] = {15 /* SIGTERM */, 1 /* SIGHUP */};
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < 2; i++)
        if (array_has_signal(available, candidates[i]))
            json_append(out, json_number((double)candidates[i]));
    return out;
}

json_t *w16_termination_ef34ac_guard_plan(int is_main, json_t *signals,
                                          json_t *prev_kinds) {
    json_t *installed = json_array();
    json_t *out = json_object();
    size_t i, nsig = (signals && signals->type == JSON_ARRAY) ? signals->c.count : 0;
    size_t nk = (prev_kinds && prev_kinds->type == JSON_ARRAY) ? prev_kinds->c.count : 0;

    /* A non-main thread yields without touching process-wide handlers. */
    if (is_main) {
        for (i = 0; i < nsig; i++) {
            json_t *s = signals->c.items[i];
            json_t *prev = (i < nk) ? prev_kinds->c.items[i] : NULL;
            int value, is_default = 0;
            if (!s || s->type != JSON_NUMBER) continue;
            value = (int)s->num_val;
            /* An absent record means FakeSignal.getsignal's default: SIG_DFL.
             * Only SIG_DFL is replaced; custom/ignored handlers are retained. */
            if (i >= nk) is_default = 1;
            else if (prev && prev->type == JSON_STRING && prev->str_val &&
                     strcmp(prev->str_val, "DFL") == 0) is_default = 1;
            if (!is_default) continue;
            json_append(installed, json_number((double)value));
        }
    }
    json_set(out, "installed", installed);
    return out;
}
