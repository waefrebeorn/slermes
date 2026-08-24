/*
 * imagegen.c — canonical C11 port of agent/pet/generate/imagegen.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_pet_generate_imagegen_u_forced_provider_from_env @ agent/pet/generate/imagegen.py:_forced_provider_from_env */
int agent_pet_generate_imagegen_u_forced_provider_from_env(const char *arg) {
    /* Python: env name if in ref-capable set else None. Arg = "env\trefset". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *env = arg;
    const char *refs = tab ? tab + 1 : "";
    size_t elen = tab ? (size_t)(tab - arg) : strlen(env);
    const char *p = refs;
    while (*p) {
        const char *t2 = strchr(p, '\t');
        size_t len = t2 ? (size_t)(t2 - p) : strlen(p);
        if (len == elen && strncasecmp(p, env, elen) == 0) { printf("%s\n", env); return 0; }
        p = t2 ? t2 + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: agent_pet_generate_imagegen_resolve_provider @ agent/pet/generate/imagegen.py:resolve_provider */
int agent_pet_generate_imagegen_resolve_provider(const char *arg) {
    /* Python: reference-capable pick. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "none") == 0) {
        fprintf(stderr, "Pet generation needs an image backend that supports reference images. Configure Nous Portal, OpenRouter, or OpenAI (gpt-image-2).\n");
        return 1;
    }
    printf("sprite provider: %s (references=%s)\n", t3 ? t3 + 1 : "?", t2 && t2[1] == '1' ? "yes" : "no");
    return 0;
}

/* PoP: agent_pet_generate_imagegen_list_sprite_providers @ agent/pet/generate/imagegen.py:list_sprite_providers */
int agent_pet_generate_imagegen_list_sprite_providers(const char *arg) {
    /* Python: ref-capable providers. Arg = "providers_json\tcount\tstate". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: agent_pet_generate_imagegen_u_save_local @ agent/pet/generate/imagegen.py:_save_local */
int agent_pet_generate_imagegen_u_save_local(const char *arg) {
    /* Python: download http(s) refs via save_url_image; else local path.
     * Arg = "image_ref\tprefix" (prefix may be empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t rlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (rlen > 7 && (strncmp(arg, "http://", 7) == 0 || strncmp(arg, "https://", 8) == 0)) {
        char url[2048];
        if (rlen >= sizeof(url)) rlen = sizeof(url) - 1;
        memcpy(url, arg, rlen); url[rlen] = '\0';
        char cmd[2400];
        snprintf(cmd, sizeof(cmd),
                 "curl -sS --max-time 60 -o /tmp/hermes_img_%d %s 2>/dev/null && echo /tmp/hermes_img_%d || echo /tmp/hermes_img_%d",
                 (int)getpid(), url, (int)getpid(), (int)getpid());
        FILE *fp = popen(cmd, "r");
        if (!fp) { printf("\n"); return 1; }
        char buf[256];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        pclose(fp);
        buf[n] = '\0';
        printf("%s", buf);
        return 0;
    }
    printf("%.*s\n", (int)rlen, arg);
    return 0;
}

/* PoP: agent_pet_generate_imagegen_u_rejected_background @ agent/pet/generate/imagegen.py:_rejected_background */
int agent_pet_generate_imagegen_u_rejected_background(const char *arg) {
    /* Python: "background" + "not supported"/"transparent". Arg = error. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char low[1200];
    snprintf(low, sizeof(low), "%s", arg);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strstr(low, "background") && (strstr(low, "not supported") || strstr(low, "transparent"))) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}
