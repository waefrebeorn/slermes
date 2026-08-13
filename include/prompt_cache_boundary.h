#ifndef PROMPT_CACHE_BOUNDARY_H
#define PROMPT_CACHE_BOUNDARY_H

/* Port of agent/prompt_cache_boundary.py — process-local LRU registry of
 * builder-declared stable prefixes for Anthropic prompt caching (#81867). */

void pcache_register_stable_prefix(const char *prefix);
char *pcache_find_stable_prefix(const char *content);  /* caller frees   */
void pcache_clear_stable_prefixes(void);

#endif /* PROMPT_CACHE_BOUNDARY_H */
