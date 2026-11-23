#ifndef NOC_KEYNOTICE_H
#define NOC_KEYNOTICE_H
/* Refusal text for an unrecognized config key path (malloc'd, caller frees). */
char *keynotice_unknown_subkey_refusal(const char *key, const char *suggestion);
#endif
