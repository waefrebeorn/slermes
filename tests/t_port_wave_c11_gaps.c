/* t_port_wave_c11_gaps.c -- leaf harness for the C11 wave re-ports.
 *
 * Links the three real, wired objects (no #include of the .c files):
 *   src/hermes_cli/backup_gaps.o
 *   src/hermes_cli/setup_gaps.o
 *   src/hermes_cli/web_server_gaps.o
 *   lib/libjson/json.o   (setup_gaps.o dependency)
 *
 * Emits JSONL. Every case echoes the exact fixture the C side used; the
 * Python oracle re-drives the LIVE function from those same fixtures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(__has_include)
#  if __has_include(<zlib.h>)
#    include <zlib.h>
#  endif
#endif

/* Under test */
int default_new_file_mode(void);
int validate_backup_zip(const char *path);
int extract_member_atomically(const char *zip, const char *member, const char *dst);
void write_machine_sentinel_line(const char *line);
int gateway_intentionally_stopped(void);

static char g_scratch[1200];

static void jstr(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:
                if (*p < 0x20) printf("\\u%04x", *p);
                else putchar(*p);
        }
    }
    putchar('"');
}

/* ── minimal STORED zip writer (zlib only for CRC32) ── */
typedef struct { const char *name; const char *data; size_t dlen; } zmem_t;
static unsigned char zbuf[262144];
static size_t zpos;

static void w16(uint16_t v) { zbuf[zpos++] = (unsigned char)(v & 0xff); zbuf[zpos++] = (unsigned char)((v >> 8) & 0xff); }
static void w32(uint32_t v) { w16((uint16_t)(v & 0xffff)); w16((uint16_t)((v >> 16) & 0xffff)); }
static void wbytes(const void *p, size_t n) { memcpy(zbuf + zpos, p, n); zpos += n; }
static void patch32(size_t at, uint32_t v) { zbuf[at]=(unsigned char)(v&0xff); zbuf[at+1]=(unsigned char)((v>>8)&0xff); zbuf[at+2]=(unsigned char)((v>>16)&0xff); zbuf[at+3]=(unsigned char)((v>>24)&0xff); }

static int make_zip(const char *path, const zmem_t *mem, int n) {
    zpos = 0;
    uint32_t offs[64], crcs[64];
    for (int i = 0; i < n; i++) {
        size_t nl = strlen(mem[i].name);
        offs[i] = (uint32_t)zpos;
        w32(0x04034b50); w16(20); w16(0); w16(0); w16(0); w16(0);
        w32(0); w32((uint32_t)mem[i].dlen); w32((uint32_t)mem[i].dlen);
        w16((uint16_t)nl); w16(0);
        wbytes(mem[i].name, nl); wbytes(mem[i].data, mem[i].dlen);
        crcs[i] = (uint32_t)crc32(0L, (const Bytef *)mem[i].data, (uInt)mem[i].dlen);
        patch32(offs[i] + 14, crcs[i]);
    }
    uint32_t cd_off = (uint32_t)zpos;
    for (int i = 0; i < n; i++) {
        size_t nl = strlen(mem[i].name);
        w32(0x02014b50); w16(0x0314); w16(20); w16(0); w16(0); w16(0); w16(0);
        w32(crcs[i]); w32((uint32_t)mem[i].dlen); w32((uint32_t)mem[i].dlen);
        w16((uint16_t)nl); w16(0); w16(0); w16(0); w16(0); w32(0); w32(offs[i]);
        wbytes(mem[i].name, nl);
    }
    uint32_t cd_size = (uint32_t)zpos - cd_off;
    w32(0x06054b50); w16(0); w16(0); w16((uint16_t)n); w16((uint16_t)n);
    w32(cd_size); w32(cd_off); w16(0);
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    size_t wrote = fwrite(zbuf, 1, zpos, fp);
    fclose(fp);
    return wrote == zpos;
}

static char *slurp(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long sz = ftell(fp);
    if (sz < 0 || sz > 65536) { fclose(fp); return NULL; }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, fp) != (size_t)sz) { free(buf); fclose(fp); return NULL; }
    fclose(fp);
    buf[sz] = 0;
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

static void emit_sentinel(const char *line) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/sentinel_cap.bin", g_scratch);
    int saved = dup(1);
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_BINARY
    flags |= O_BINARY;   /* Python os.write(1, ...) is a RAW fd write: no CRLF. */
#endif
    int fd = open(path, flags, 0644);
    if (fd >= 0) {
        dup2(fd, 1); close(fd);
        write_machine_sentinel_line(line);
        fflush(stdout);
        dup2(saved, 1); close(saved);
    } else {
        write_machine_sentinel_line(line);
    }
    size_t n = 0;
    char *cap = slurp(path, &n);
    printf("{\"fn\":\"sentinel\",\"line\":"); jstr(line);
    printf(",\"out\":"); jstr(cap ? cap : ""); printf("}\n");
    free(cap);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    const char *s = getenv("SLERMES_HOME");
    if (!s || !*s) s = getenv("HOME");
    snprintf(g_scratch, sizeof(g_scratch), "%s", s ? s : ".");

    /* ── default_new_file_mode ── */
    {
        int masks[] = {0000, 0002, 0022, 0027, 0077, 0777};
        for (size_t i = 0; i < sizeof(masks)/sizeof(masks[0]); i++) {
            int before = umask(masks[i]);
            int out = default_new_file_mode();
            umask(before);
            printf("{\"fn\":\"dnfm\",\"mask\":%d,\"out\":%d}\n", masks[i], out);
        }
    }

    /* ── validate_backup_zip: emit the zip PATH; oracle opens the same file ── */
    {
        struct { const char *id; const char *names[4]; int n; } cases[] = {
            {"empty",    {NULL}, 0},
            {"config",   {"config.yaml", NULL}, 1},
            {"env_deep", {"a/b/.env", NULL}, 1},
            {"state",    {"x/state.db", NULL}, 1},
            {"none",     {"readme.txt", "a/b.log", NULL}, 2},
            {"upper",    {"CONFIG.YAML", NULL}, 1},
            {"suffix",   {"config.yaml.bak", NULL}, 1},
        };
        for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            char path[1200];
            snprintf(path, sizeof(path), "%s/vbz_%s.zip", g_scratch, cases[i].id);
            zmem_t mem[4]; int n = 0;
            for (int k = 0; k < cases[i].n; k++) {
                mem[n].name = cases[i].names[k];
                mem[n].data = "x"; mem[n].dlen = 1; n++;
            }
            make_zip(path, mem, n);
            int out = validate_backup_zip(path);
            printf("{\"fn\":\"vbz\",\"id\":"); jstr(cases[i].id);
            printf(",\"path\":"); jstr(path);
            printf(",\"out\":%d}\n", out);
        }
    }

    /* ── extract_member_atomically ──
     * Emit the zip path + member + content; the oracle re-extracts with the
     * LIVE Python function on the same file and compares the resulting bytes.
     */
    {
        struct { const char *id; const char *member; const char *content; int present; } cases[] = {
            {"plain",  "payload.txt", "alpha beta gamma\n", 1},
            {"emptyf", "empty.txt",   "", 1},
            {"nested", "home/state.db", "STATE-DATA-12345", 1},
            {"missing","absent.txt",  "unused", 0},
        };
        for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            char zpath[1200], dst[1200];
            snprintf(zpath, sizeof(zpath), "%s/ema_%s.zip", g_scratch, cases[i].id);
            snprintf(dst, sizeof(dst), "%s/ema_%s.out", g_scratch, cases[i].id);
            remove(dst);
            zmem_t mem[1];
            mem[0].name = cases[i].present ? cases[i].member : "other.txt";
            mem[0].data = cases[i].content;
            mem[0].dlen = strlen(cases[i].content);
            make_zip(zpath, mem, 1);
            int rc = extract_member_atomically(zpath, cases[i].member, dst);
            size_t n = 0;
            char *got = slurp(dst, &n);
            printf("{\"fn\":\"ema\",\"id\":"); jstr(cases[i].id);
            printf(",\"path\":"); jstr(zpath);
            printf(",\"member\":"); jstr(cases[i].member);
            printf(",\"content\":"); jstr(cases[i].content);
            printf(",\"present\":%d,\"rc\":%d,\"bytes\":", cases[i].present, rc);
            jstr(got ? got : "");
            printf(",\"exists\":%d}\n", got ? 1 : 0);
            free(got);
        }
    }

    /* ── write_machine_sentinel_line ── */
    {
        const char *lines[] = {"READY", "PORT=1234", "", "a b c", "sentinel with spaces  "};
        for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++)
            emit_sentinel(lines[i]);
    }

    /* ── gateway_intentionally_stopped ──
     * State file already written at <SLERMES_HOME>/gateway_state.json;
     * emit its content path so the oracle points its stub at the same dir.
     */
    {
        const char *states[] = {
            "{\"desired_state\":\"stopped\"}",
            "{\"desired_state\":\"running\"}",
            "{}",
            "{\"desired_state\": \"stopped\", \"x\": 1}",
            "not json",
            "[]",
            "\xEF\xBB\xBF{\"desired_state\":\"stopped\"}",
        };
        for (size_t i = 0; i < sizeof(states)/sizeof(states[0]); i++) {
            char home[1200], path[1200];
            snprintf(home, sizeof(home), "%s/gstop_%d", g_scratch, (int)i);
            mkdir(home, 0777);
            snprintf(path, sizeof(path), "%s/gateway_state.json", home);
            FILE *fp = fopen(path, "wb");
            if (fp) { fwrite(states[i], 1, strlen(states[i]), fp); fclose(fp); }
            setenv("HERMES_HOME", home, 1);
            int out = gateway_intentionally_stopped();
            printf("{\"fn\":\"gstop\",\"home\":");
            jstr(home);
            printf(",\"content\":"); jstr(states[i]);
            printf(",\"out\":%d}\n", out);
        }
    }

    return 0;
}
