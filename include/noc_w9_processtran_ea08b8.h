#ifndef NOC_W9_PROCESSTRAN_EA08B8_H
#define NOC_W9_PROCESSTRAN_EA08B8_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* PoP: w9_processtran_ea08b8_is_local_server @ agent/process_transport.py:is_local_server
 * bool(base_url) and is_local_endpoint(str(base_url)) and not external_process. */
bool w9_processtran_ea08b8_is_local_server(const char *base_url,
                                            bool local, bool external);

/* PoP: w9_processtran_ea08b8_waits_like_local @ agent/process_transport.py:waits_like_local
 * external_process or (bool(base_url) and is_local_endpoint(str(base_url))). */
bool w9_processtran_ea08b8_waits_like_local(const char *base_url,
                                             bool local, bool external);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_PROCESSTRAN_EA08B8_H */
