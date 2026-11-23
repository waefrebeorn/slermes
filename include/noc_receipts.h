#ifndef NOC_RECEIPTS_H
#define NOC_RECEIPTS_H
#include <stddef.h>

/* Executable surface of the WorkerReceiptReads Protocol declaration. */
const char *receipts_protocol_name(void);
int receipts_is_protocol(void);
size_t receipts_method_count(void);
/* Sorted order: claim_origin (0), publication_target (1). NULL if out of range. */
const char *receipts_method_at(size_t i);
int receipts_has_method(const char *name);
/* Protocol method bodies are ``...`` -> calling them returns None (NULL). */
const char *receipts_method_stub(const char *name);
#endif
