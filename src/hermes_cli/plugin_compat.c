/*
 * plugin_compat.c — C port of upstream Hermes hermes_cli/plugin_compat.py.
 * PoP: plugcompat_compat_report @ hermes_cli/plugin_compat.py:compat_report
 * PoP: plugcompat_removal_in_effect @ hermes_cli/plugin_compat.py:removal_in_effect
 * PoP: plugcompat_summary_lines @ hermes_cli/plugin_compat.py:summary_lines
 *
 * The compat layer was removed; these are inert stubs with nothing left to
 * report.
 */
#include "noc_plugcompat.h"

json_t *plugcompat_compat_report(void) {
    return json_object(); /* {} */
}

int plugcompat_removal_in_effect(void) {
    return 1; /* True */
}

json_t *plugcompat_summary_lines(void) {
    return json_array(); /* [] */
}
