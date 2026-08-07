/*
 * flux3_video_tool.c — Additional helper ports from tools/flux3_video_tool.py.
 *
 * The 8 helpers already ported by port_tools_flux3_video.c
 * (_looks_like_local_path, _display_path, _filename_from_url,
 * _is_transport_error, _poll_is_finished, _retry_after_seconds,
 * _without_media, _submit_args) are NOT duplicated here.
 *
 * This file adds: _resolve_destination, _free_path, _still_generating.
 */
#define _GNU_SOURCE
#include "flux3_video_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

/* PoP: _still_generating @ tools/flux3_video_tool.py:_still_generating */
char *flux3_still_generating(const char *job_id)
{
    char *out = NULL;
    if (!job_id) return NULL;
    /* Python uses em dash (U+2014, UTF-8 E2 80 94) not hyphen */
    asprintf(&out,
        "{\"result\": \"Still generating. This call reached its own time limit, "
        "which the job is unaffected by \xe2\x80\x94 call bfl_flux3_get_result again with "
        "id=%s to keep waiting.\", \"details\": {\"id\": \"%s\", \"status\": \"Generating\"}}",
        job_id, job_id);
    return out;
}

/* PoP: _resolve_destination @ tools/flux3_video_tool.py:_resolve_destination */
char *flux3_resolve_destination(const char *save_to, const char *filename)
{
    if (!filename) filename = "flux3-video.mp4";
    char dir[4096];
    if (save_to && *save_to) {
        strncpy(dir, save_to, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        size_t len = strlen(dir);
        while (len > 0 && (dir[len-1] == '/' || dir[len-1] == '\\'))
            dir[--len] = '\0';
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return strdup(save_to);
        }
    } else {
        const char *home = getenv("HOME");
        if (home)
            snprintf(dir, sizeof(dir), "%s/Downloads", home);
        else
            strcpy(dir, ".");
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
            strcpy(dir, ".");
    }
    return flux3_free_path(dir, filename);
}

/* PoP: _free_path @ tools/flux3_video_tool.py:_free_path */
char *flux3_free_path(const char *directory, const char *name)
{
    char candidate[4096];
    snprintf(candidate, sizeof(candidate), "%s/%s", directory ? directory : ".",
             name ? name : "flux3-video.mp4");
    struct stat st;
    if (stat(candidate, &st) != 0)
        return strdup(candidate);

    const char *dot = strrchr(name, '.');
    const char *base = strrchr(name, '/');
    if (base) base++;
    else base = name;
    char stem[256], suffix[32];
    if (dot && dot > base) {
        size_t sl = (size_t)(dot - base);
        if (sl >= sizeof(stem)) sl = sizeof(stem) - 1;
        memcpy(stem, base, sl); stem[sl] = '\0';
        size_t suf_l = strlen(dot);
        if (suf_l >= sizeof(suffix)) suf_l = sizeof(suffix) - 1;
        memcpy(suffix, dot, suf_l); suffix[suf_l] = '\0';
    } else {
        strncpy(stem, base, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = '\0';
        strcpy(suffix, "");
    }
    for (int s = 2; s < FLUX3_MAX_FILENAME_ATTEMPTS + 2; s++) {
        snprintf(candidate, sizeof(candidate), "%s/%s-%d%s", directory, stem, s, suffix);
        if (stat(candidate, &st) != 0) return strdup(candidate);
    }
    return NULL;
}
