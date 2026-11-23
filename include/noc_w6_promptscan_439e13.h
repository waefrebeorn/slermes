#ifndef NOC_W6_PROMPTSCAN_439E13_H
#define NOC_W6_PROMPTSCAN_439E13_H

/* Port of upstream Hermes cron/prompt_scan.py::_strip_invisible_unicode.
 *
 * Returns {"cleaned": <string>, "removed": ["U+XXXX", ...]} where `removed`
 * is the sorted set of invisible-unicode labels stripped. ZWJ inside a
 * legitimate emoji grapheme and the three RGI subdivision-flag tag sequences
 * are preserved, exactly as the Python scanner does. Caller owns the node.
 */
#include "hermes_json.h"

json_t *w6_promptscan_439e13_strip_invisible_unicode(const char *prompt);

#endif /* NOC_W6_PROMPTSCAN_439E13_H */
