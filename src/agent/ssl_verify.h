#ifndef AGENT_SSL_VERIFY_H
#define AGENT_SSL_VERIFY_H
#include <stdbool.h>
bool agent_ssl_verify_coerce_insecure(const char *ssl_verify);
const char *agent_ssl_verify_resolve_httpx_verify(const char *ca_bundle, const char *ssl_verify);
#endif
