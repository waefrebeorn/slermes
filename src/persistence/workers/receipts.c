/*
 * receipts.c — C port of upstream Hermes persistence/workers/receipts.py.
 * The module is the WorkerReceiptReads Protocol: its whole executable surface
 * is the structural contract (class name, method names, presence, protocol
 * flag) and the None-returning ``...`` method bodies.
 * PoP: receipts_protocol_name @ persistence/workers/receipts.py:WorkerReceiptReads
 * PoP: receipts_is_protocol @ persistence/workers/receipts.py:WorkerReceiptReads
 * PoP: receipts_method_count @ persistence/workers/receipts.py:WorkerReceiptReads
 * PoP: receipts_method_at @ persistence/workers/receipts.py:WorkerReceiptReads
 * PoP: receipts_has_method @ persistence/workers/receipts.py:WorkerReceiptReads
 * PoP: receipts_method_stub @ persistence/workers/receipts.py:WorkerReceiptReads.claim_origin
 */
#include "noc_receipts.h"
#include <string.h>

/* sorted order: Python callers/tests compare the method set, not insertion order */
static const char *const kMethods[] = {"claim_origin", "publication_target"};
static const size_t kMethodCount = sizeof(kMethods) / sizeof(kMethods[0]);

const char *receipts_protocol_name(void) {
    return "WorkerReceiptReads";
}

int receipts_is_protocol(void) {
    return 1; /* class WorkerReceiptReads(Protocol): ... */
}

size_t receipts_method_count(void) {
    return kMethodCount;
}

const char *receipts_method_at(size_t i) {
    return i < kMethodCount ? kMethods[i] : NULL;
}

int receipts_has_method(const char *name) {
    if (!name) return 0;
    for (size_t i = 0; i < kMethodCount; ++i)
        if (!strcmp(name, kMethods[i])) return 1;
    return 0;
}

const char *receipts_method_stub(const char *name) {
    /* Protocol bodies are literally ``...`` -> calling them yields None. */
    (void)name;
    return NULL;
}
