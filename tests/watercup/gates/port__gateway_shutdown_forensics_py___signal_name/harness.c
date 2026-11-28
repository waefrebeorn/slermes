/* gate: port__gateway_shutdown_forensics_py___signal_name
 * forensics_signal_name is `static const char *` in shutdown_forensics.c, so a
 * cross-TU extern can't reach it. It is a pure sig->name switch (only snprintf).
 * Verbatim copy of the body + string-valued cups. Mirror of
 * gateway/shutdown_forensics.py:_signal_name(sig). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include "watercup.h"

static const char *local_signal_name(int sig, char *buf, size_t buf_size) {
    switch (sig) {
        case SIGTERM: return "SIGTERM";
        case SIGINT:  return "SIGINT";
        case SIGHUP:  return "SIGHUP";
#ifdef SIGQUIT
        case SIGQUIT: return "SIGQUIT";
#endif
#ifdef SIGUSR1
        case SIGUSR1: return "SIGUSR1";
#endif
#ifdef SIGUSR2
        case SIGUSR2: return "SIGUSR2";
#endif
        default:
            snprintf(buf, buf_size, "signal#%d", sig);
            return buf;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s forensics_signal_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("forensics_signal_name");
        fflush(stdout);
        char scratch[32];
        const char * result = local_signal_name((int)strtol(argv[i], NULL, 10), scratch, sizeof(scratch));
        char buf[64];
        snprintf(buf, sizeof(buf), "%.120s", result ? result : "");
        fflush(stdout);
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("forensics_signal_name");
    }
    return 0;
}
