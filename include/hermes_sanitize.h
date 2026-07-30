/*
 * hermes_sanitize.h — Unicode surrogate / invalid-UTF8 sanitization API.
 *
 * Faithful extraction from the monolithic hermes.h god header (the
 * god-header-elimination pass). Defined in src/agent/message_sanitization.c
 * (and src/agent/sanitize.c). Translation units that only need
 * surrogate-cleaning no longer drag in the entire master header.
 */

#ifndef HERMES_SANITIZE_H
#define HERMES_SANITIZE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Replace lone/paired UTF-16 surrogates and invalid UTF-8 with safe
 * placeholders so the text is valid for downstream JSON/LLM calls.
 * Returns a newly allocated, caller-freed string. */
char *sanitize_surrogates(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SANITIZE_H */
