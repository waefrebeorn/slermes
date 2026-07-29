/* hermes_reactions.h */
#ifndef HERMES_REACTIONS_H
#define HERMES_REACTIONS_H
#include <stdbool.h>
#include <stddef.h>

/* Return "vibe" if text contains an affection reaction, else NULL. */
extern const char *reactions_detect(const char *text);

#endif
