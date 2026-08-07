#ifndef AGENT_MARKDOWN_TABLES_H
#define AGENT_MARKDOWN_TABLES_H

#include <stddef.h>

/* PoP: realign_markdown_tables @ agent/markdown_tables.py:realign_markdown_tables */
/* Returns a malloc'd realigned string. available_width < 0 means "no limit" */
/* (Python's None). Caller frees the result. */
char *md_realign_markdown_tables(const char *text, int available_width);

#endif
