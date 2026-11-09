/*
 * api_server_sse.c - SSE response helpers.
 *
 * Split from api_server.c (soul.md: self-contained modules). Header write,
 * chunk send and named-event emit. No file-scope state.
 */
#include "api_server_sse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* ── SSE helpers ────────────────────────────────────────────────── */ void send_sse_event(int fd, const char *data) {
    dprintf(fd, "data: %s\n\n", data ? data : "");
}
 
 void sse_send_chunk(int fd, const char *content, int index) {
    char event[4096];
    snprintf(event, sizeof(event),
        "{\"choices\":[{\"delta\":{\"content\":\"%s\"},\"index\":%d}]}",
        content, index);
    send_sse_event(fd, event);
}
