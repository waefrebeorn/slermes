#ifndef SLERMES_PORT_RFB_FILTER_H
#define SLERMES_PORT_RFB_FILTER_H

#include <stddef.h>

/* RFB client-message framer. Returns the total message length (incl. type
 * byte), or: -1 buffer too short to frame (Python None), -2 unknown message
 * type (ValueError), -3 oversized clipboard (ValueError). */
int rfb_msg_length(const unsigned char *buf, size_t len);

#endif /* SLERMES_PORT_RFB_FILTER_H */
