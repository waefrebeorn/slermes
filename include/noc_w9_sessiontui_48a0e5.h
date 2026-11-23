#ifndef NOC_W9_SESSIONTUI_48A0E5_H
#define NOC_W9_SESSIONTUI_48A0E5_H
#include "hermes_json.h"

/* PoP port of gateway/session_tui_ops.py — frame-fitting wire-size helpers. */
int   w9_sessiontui_48a0e5_wire_bytes(const char *text);
char *w9_sessiontui_48a0e5_fit_frame(const char *text, long long budget);
#endif /* NOC_W9_SESSIONTUI_48A0E5_H */
