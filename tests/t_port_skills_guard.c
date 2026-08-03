/*
 * t_port_skills_guard.c — oracle harness for the skills_guard helpers in
 * src/cli/port_tools_skills_guard.c (ports of tools/skills_guard.py).
 * Reads the fixture from argv[1] (one op per line), emits one JSON object
 * per line. Ops:
 *   trust <source>                     -> _resolve_trust_level
 *   verdict <severity> <findings>      -> _determine_verdict
 *   content_digest <path>             -> _content_digest (sha256 hex)
 *   full_content_hash <path>          -> full_content_hash ("sha256:...")
 *   finding_dict <pattern_id> <severity> <category> <file> <line> <match> <desc>
 *                                     -> _finding_dict (compact JSON)
 *   scan_skill_cached <path> <source> <source_url> <cache_dir>
 *                                     -> scan_skill_cached (fresh flag + summary)
 */

#include "skills_guard.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_base[4096];  /* fixture directory, for relative paths */

/* Resolve a possibly-relative path against the fixture dir. Returns a
 * malloc'd absolute-ish path (caller frees) or NULL. */
static char *resolve_path(const char *p) {
    if (!p || !*p) return NULL;
    if (p[0] == '/') return strdup(p);
    size_t bl = strlen(g_base);
    size_t pl = strlen(p);
    char *out = malloc(bl + 1 + pl + 1);
    if (!out) return NULL;
    memcpy(out, g_base, bl);
    out[bl] = '/';
    memcpy(out + bl + 1, p, pl + 1);
    return out;
}

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

static int read_line(FILE *fp, char *buf, size_t sz) {
    if (!fgets(buf, (int)sz, fp)) return 0;
    size_t L = strlen(buf);
    while (L > 0 && (buf[L-1] == '\n' || buf[L-1] == '\r')) buf[--L] = '\0';
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.in>\n", argv[0]); return 2; }
    /* Capture fixture directory for relative-path resolution. The runner
     * passes a TEMP-substituted copy (FSUB) as argv[1], so derive the real
     * fixture dir from ORACLE_FIXDIR when set, else fall back to argv[1]'s dir. */
    {
        const char *fixdir = getenv("ORACLE_FIXDIR");
        if (fixdir && *fixdir) {
            snprintf(g_base, sizeof(g_base), "%s", fixdir);
        } else {
            const char *slash = strrchr(argv[1], '/');
            if (slash) { size_t bl = (size_t)(slash - argv[1]); memcpy(g_base, argv[1], bl); g_base[bl] = '\0'; }
            else snprintf(g_base, sizeof(g_base), ".");
        }
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char line[8192];
    while (read_line(fp, line, sizeof(line))) {
        if (!*line || line[0] == '#') continue;

        /* split op / rest */
        char op[64];
        const char *rest = "";
        size_t i = 0;
        while (line[i] && line[i] == ' ') i++;
        size_t s = i;
        while (line[i] && line[i] != ' ' && (i - s) + 1 < sizeof(op)) op[i - s] = line[i], i++;
        op[i - s] = '\0';
        if (line[i] == ' ') rest = line + i + 1;

        if (strcmp(op, "trust") == 0) {
            const char *v = *rest ? rest : "";
            const char *r = cli_tools_skills_guard__resolve_trust_level(v, NULL);
            printf("{\"op\":\"trust\",\"in\":");
            emit_json_string(v);
            printf(",\"out\":");
            emit_json_string(r ? r : "");
            printf("}\n");
        } else if (strcmp(op, "verdict") == 0) {
            int sev = 0, fnd = 0;
            sscanf(rest, "%d %d", &sev, &fnd);
            const char *r = cli_tools_skills_guard__determine_verdict(sev, fnd);
            printf("{\"op\":\"verdict\",\"severity\":%d,\"findings\":%d,\"out\":", sev, fnd);
            emit_json_string(r ? r : "");
            printf("}\n");
        } else if (strcmp(op, "content_digest") == 0) {
            char *rp = resolve_path(rest);
            char *r = skills_guard_content_digest(rp ? rp : rest);
            printf("{\"op\":\"content_digest\",\"path\":");
            emit_json_string(rest);
            printf(",\"out\":");
            emit_json_string(r ? r : "");
            printf("}\n");
            free(rp); free(r);
        } else if (strcmp(op, "full_content_hash") == 0) {
            char *rp = resolve_path(rest);
            char *r = skills_guard_full_content_hash(rp ? rp : rest);
            printf("{\"op\":\"full_content_hash\",\"path\":");
            emit_json_string(rest);
            printf(",\"out\":");
            emit_json_string(r ? r : "");
            printf("}\n");
            free(rp); free(r);
        } else if (strcmp(op, "finding_dict") == 0) {
            /* Mirror sta_oracle_skills_guard.py exactly:
             *   parts = rest.split(" ", 6)   -> 7 parts
             *   Finding(pattern_id=parts[0], severity=parts[1], category=parts[2],
             *           file=parts[3], line=int(parts[4] or 0),
             *           match="",                 <-- oracle HARDCODES match empty
             *           description=parts[6])     <-- remainder after the 6th space
             * The fixture supplies only 5 fixed fields (pid sev cat file line)
             * then a quoted description; the 6th split token is absorbed into the
             * description remainder, and match is forced to "". So we split the
             * first 6 fields (to locate the 6th space), take the rest as
             * description, and clear match. */
            char parts[7][1024];
            for (int k = 0; k < 7; k++) parts[k][0] = '\0';
            const char *p = rest;
            for (int k = 0; k < 6 && *p; k++) {
                while (*p == ' ') p++;                 /* skip run of spaces */
                const char *start = p;
                while (*p && *p != ' ') p++;
                size_t len = (size_t)(p - start);
                if (len >= sizeof(parts[k])) len = sizeof(parts[k]) - 1;
                memcpy(parts[k], start, len);
                parts[k][len] = '\0';
                if (!*p) break;
                p++;                                   /* consume one space */
            }
            /* p now points at the remainder after the 6th space == description. */
            snprintf(parts[6], sizeof(parts[6]), "%s", p ? p : "");
            skills_guard_finding_t f;
            memset(&f, 0, sizeof(f));
            snprintf(f.pattern_id, sizeof(f.pattern_id), "%s", parts[0]);
            snprintf(f.severity, sizeof(f.severity), "%s", parts[1]);
            snprintf(f.category, sizeof(f.category), "%s", parts[2]);
            snprintf(f.file, sizeof(f.file), "%s", parts[3]);
            f.line = (int)strtol(parts[4], NULL, 10);
            snprintf(f.match, sizeof(f.match), "%s", "");  /* oracle hardcodes "" */
            snprintf(f.description, sizeof(f.description), "%s", parts[6]);
            char *r = skills_guard_finding_dict(&f);
            printf("{\"op\":\"finding_dict\",\"out\":%s}\n", r ? r : "{}");
            free(r);
        } else if (strcmp(op, "scan_skill_cached") == 0) {
            /* scan_skill_cached <path> <source> <source_url> <cache_dir>
             * cache_dir is resolved against fixture base too. */
            char path[4096], src[64], surl[256], cdir[4096];
            src[0] = surl[0] = cdir[0] = '\0';
            sscanf(rest, "%4095s %63s %255s %4095s", path, src, surl, cdir);
            char *rpath = resolve_path(path);
            char *rcdir = *cdir ? resolve_path(cdir) : NULL;
            skills_guard_scan_result_t out;
            int rc = skills_guard_scan_skill_cached(rpath ? rpath : path,
                                                    *src ? src : NULL,
                                                    *surl ? surl : NULL,
                                                    rcdir ? rcdir : NULL, &out);
            printf("{\"op\":\"scan_skill_cached\",\"path\":");
            emit_json_string(path);
            printf(",\"rc\":%d,\"fresh\":%d,\"verdict\":", rc, out.fresh);
            emit_json_string(out.verdict);
            printf(",\"trust_level\":");
            emit_json_string(out.trust_level);
            printf(",\"summary\":");
            emit_json_string(out.summary);
            printf("}\n");
            free(rpath); free(rcdir);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    fclose(fp);
    return 0;
}
