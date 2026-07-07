/*
 * port_tools_voice_mode.c — C port of tools/voice_mode.py
 *
 * Voice mode audio capture and processing.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* PoP: voice_mode__audio_available @ tools/voice_mode.py:_audio_available */

/* Port of Python tools/voice_mode.py:_audio_available */
/* Return 1 if audio libraries can be imported/available, 0 otherwise. */
int voice_mode__audio_available(void)
{
    /* In C, we don't have Python audio libraries */
    /* Check for ALSA/PulseAudio on Linux */
#ifndef _WIN32
    /* Check if /dev/snd exists (ALSA) */
    if (access("/dev/snd", F_OK) == 0) {
        hermes_log(LOG_DEBUG, "voice_mode", "ALSA audio devices found");
        return 1;
    }
    /* Check for PulseAudio socket */
    if (access("/run/pulse/native", F_OK) == 0) {
        hermes_log(LOG_DEBUG, "voice_mode", "PulseAudio socket found");
        return 1;
    }
#endif
    hermes_log(LOG_DEBUG, "voice_mode", "No audio libraries available");
    return 0;
}

/* PoP: voice_mode__termux_microphone_command @ tools/voice_mode.py:_termux_microphone_command */

/* Port of Python tools/voice_mode.py:_termux_microphone_command */
/* Return the termux-microphone-record command path, or NULL if not on Termux. */
char *voice_mode__termux_microphone_command(void)
{
    /* Check if we're in a Termux environment */
    const char *termux = getenv("TERMUX_VERSION");
    if (!termux || !termux[0]) {
        hermes_log(LOG_DEBUG, "voice_mode", "Not in Termux environment");
        return NULL;
    }

    /* Check if termux-microphone-record is available */
    const char *cmd = "/data/data/com.termux/files/usr/bin/termux-microphone-record";
    if (access(cmd, X_OK) == 0) {
        hermes_log(LOG_DEBUG, "voice_mode", "Found termux-microphone-record");
        return strdup(cmd);
    }

    hermes_log(LOG_DEBUG, "voice_mode", "termux-microphone-record not found");
    return NULL;
}

/* PoP: voice_mode__termux_api_app_installed @ tools/voice_mode.py:_termux_api_app_installed */

/* Port of Python tools/voice_mode.py:_termux_api_app_installed */
/* Return 1 if Termux API app is installed, 0 otherwise. */
int voice_mode__termux_api_app_installed(void)
{
    const char *termux = getenv("TERMUX_VERSION");
    if (!termux || !termux[0]) {
        /* Not running under Termux; the Termux API app cannot be present. */
        hermes_log(LOG_DEBUG, "voice_mode", "Not in a Termux environment");
        return 0;
    }

    /* Real check: ask the Android package manager whether the Termux:API app
     * is installed. `pm list packages com.termux.api` prints a line containing
     * the package name when installed. */
    FILE *fp = popen("pm list packages com.termux.api 2>/dev/null", "r");
    if (fp) {
        char line[512];
        int found = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "com.termux.api")) { found = 1; break; }
        }
        pclose(fp);
        if (found) {
            hermes_log(LOG_DEBUG, "voice_mode", "Termux API app is installed");
            return 1;
        }
    }

    /* Fallback: presence of the termux-api helper binary on non-pm systems. */
    if (access("/data/data/com.termux/files/usr/bin/termux-api", X_OK) == 0) {
        hermes_log(LOG_DEBUG, "voice_mode", "Termux API binary present");
        return 1;
    }

    hermes_log(LOG_DEBUG, "voice_mode", "Termux API app not found");
    return 0;
}

/* PoP: voice_mode__termux_voice_capture_available @ tools/voice_mode.py:_termux_voice_capture_available */

/* Port of Python tools/voice_mode.py:_termux_voice_capture_available */
/* Return 1 if Termux voice capture is available, 0 otherwise. */
int voice_mode__termux_voice_capture_available(void)
{
    char *cmd = voice_mode__termux_microphone_command();
    if (!cmd) return 0;
    free(cmd);

    int api = voice_mode__termux_api_app_installed();
    hermes_log(LOG_DEBUG, "voice_mode", "Termux voice capture available: %d", api);
    return api;
}

/* PoP: voice_mode__pulse_socket_reachable @ tools/voice_mode.py:_pulse_socket_reachable */

/* Port of Python tools/voice_mode.py:_pulse_socket_reachable */
/* Return 1 if PulseAudio socket is reachable, 0 otherwise. */
int voice_mode__pulse_socket_reachable(void)
{
#ifndef _WIN32
    /* Check for PulseAudio socket */
    const char *pulse_socket = getenv("PULSE_SERVER");
    if (pulse_socket && pulse_socket[0]) {
        if (access(pulse_socket, F_OK) == 0) {
            hermes_log(LOG_DEBUG, "voice_mode", "PulseAudio socket reachable at %s", pulse_socket);
            return 1;
        }
    }

    /* Check default PulseAudio socket paths */
    const char *default_sockets[] = {
        "/run/pulse/native",
        "/run/user/1000/pulse/native",
        NULL
    };
    for (int i = 0; default_sockets[i]; i++) {
        if (access(default_sockets[i], F_OK) == 0) {
            hermes_log(LOG_DEBUG, "voice_mode", "PulseAudio socket reachable at %s", default_sockets[i]);
            return 1;
        }
    }
#endif
    hermes_log(LOG_DEBUG, "voice_mode", "PulseAudio socket not reachable");
    return 0;
}
