#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* shim: has_incomplete_scratchpad — mirrors Python agent/trajectory.py::has_incomplete_scratchpad */
/* Returns True if content has <REASONING_SCRATCHPAD> opening tag without matching </REASONING_SCRATCHPAD> closing tag */

bool has_incomplete_scratchpad(const char *content) {
    if (!content) return false;
    /* Python: if not content: return False, then check tags */
    const char *open_tag = "<REASONING_SCRATCHPAD>";
    const char *close_tag = "</REASONING_SCRATCHPAD>";
    bool has_open = strstr(content, open_tag) != NULL;
    bool has_close = strstr(content, close_tag) != NULL;
    /* True when opening tag present but closing tag absent */
    return has_open && !has_close;
}
