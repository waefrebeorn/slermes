#ifndef HERMES_PROCESS_BOOTSTRAP_PURE_H
#define HERMES_PROCESS_BOOTSTRAP_PURE_H

/*
 * process_bootstrap_pure.h — pure helpers from
 * agent/process_bootstrap.py (Happy Eyeballs prep, RFC 8305).
 */

#include <stddef.h>
#include <stdint.h>

/* One getaddrinfo result row, flattened. canonname is ignored by the
 * interleave algorithm (Python binds it to _canonname and never reads it). */
typedef struct {
    int family;    /* AF_INET / AF_INET6 as passed in */
    int socktype;
    int proto;
    char sockaddr[64]; /* opaque text form; identity only */
} pbs_addrinfo_t;

/*
 * Port of _interleave_addrinfos: dedupe by (family, socktype, proto,
 * sockaddr) preserving first sight, bucket by family preserving first-seen
 * family order, then round-robin one address per family until all buckets
 * drain. Returns a malloc'd array of *out_n indices into the INPUT array
 * (the interleaved emission order). Caller frees the array.
 */
size_t *pbs_interleave_addrinfos(const pbs_addrinfo_t *addrinfos, size_t n,
                                 size_t *out_n);

#endif /* HERMES_PROCESS_BOOTSTRAP_PURE_H */
