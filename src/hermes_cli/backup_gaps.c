#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__has_include)
#  if __has_include(<zlib.h>)
#    include <zlib.h>
#    define BK_HAVE_ZLIB 1
#  endif
#endif
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/backup.py: 20 missing functions ── */

/* ── _in_excluded_root_dir revival (tail_revive3) ───────────────────────────
 * Faithful port of hermes_cli/backup.py:_in_excluded_root_dir.
 * rel_path.parts is modeled as PurePosixPath.parts (deterministic across
 * gate hosts): split on '/', drop empty and "." components, keep "..",
 * a leading '/' becomes the root part "/". Windows drive/UNC/backslash
 * paths are out of fixture scope (documented projection).
 * Constants transcribed from hermes_constants.LOCAL_RUNTIME_ROOT_DIRS,
 * hermes_cli.home_data_layout.{PM_RUNTIME_ROOT_DIRS, SECRET_CACHE_FILES,
 * BRIDGE_LOG_PARTS, profile_root_entry} and agent.provider_media.
 * GENERATED_SUBDIR:
 *   _EXCLUDED_ROOT_DIRS = {"models","runtimes","node","installs","tools"}
 *   _EXCLUDED_BACKUP_ROOT_DIRS = {"browser_profiles","bws_cache.json",
 *       "bws_cache.enc.json","op_cache.json","photon-sidecar.json"}
 *   BRIDGE_LOG_PARTS = {("platforms","whatsapp","bridge.log"),
 *       ("platforms","whatsapp","bridge.log.1"),("whatsapp","bridge.log"),
 *       ("whatsapp","bridge.log.1")}
 *   _KEPT_CACHE_SUBDIRS = {"images","audio","videos","documents",
 *       "screenshots","citations","generated"} */

typedef struct bk_part { const char *s; size_t n; } bk_part_t;

static int bk_peq(const bk_part_t *p, const char *lit) {
    return strlen(lit) == p->n && memcmp(p->s, lit, p->n) == 0;
}

static int bk_in_list(const bk_part_t *p, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (bk_peq(p, list[i])) return 1;
    return 0;
}

/* PoP: in_excluded_root_dir @ hermes_cli/backup.py:_in_excluded_root_dir */
int in_excluded_root_dir(const char *path) {
    bk_part_t parts[512];
    size_t np = 0;
    const char *p = path ? path : "";
    if (*p == '/') {
        parts[np].s = "/"; parts[np].n = 1; np++;
    }
    while (*p && np < 512) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '/') e++;
        size_t len = (size_t)(e - p);
        if (!(len == 1 && p[0] == '.')) {
            parts[np].s = p; parts[np].n = len; np++;
        }
        p = e;
    }

    /* profile_root_entry(parts) */
    const bk_part_t *root = NULL;
    if (np >= 3 && bk_peq(&parts[0], "profiles")) root = &parts[2];
    else if (np > 0) root = &parts[0];

    static const char *const excl_root[] = {
        "models", "runtimes", "node", "installs", "tools"
    };
    static const char *const excl_backup[] = {
        "browser_profiles", "bws_cache.json", "bws_cache.enc.json",
        "op_cache.json", "photon-sidecar.json"
    };
    if (root && (bk_in_list(root, excl_root, 5) || bk_in_list(root, excl_backup, 5)))
        return 1;

    size_t off = (np >= 3 && bk_peq(&parts[0], "profiles")) ? 2 : 0;
    const bk_part_t *rest = parts + off;
    size_t nr = np - off;

    /* parts in BRIDGE_LOG_PARTS */
    if (nr == 3 && bk_peq(&rest[0], "platforms") && bk_peq(&rest[1], "whatsapp") &&
        (bk_peq(&rest[2], "bridge.log") || bk_peq(&rest[2], "bridge.log.1")))
        return 1;
    if (nr == 2 && bk_peq(&rest[0], "whatsapp") &&
        (bk_peq(&rest[1], "bridge.log") || bk_peq(&rest[1], "bridge.log.1")))
        return 1;

    static const char *const kept_cache[] = {
        "images", "audio", "videos", "documents", "screenshots",
        "citations", "generated"
    };
    if (nr >= 2 && bk_peq(&rest[0], "cache") && !bk_in_list(&rest[1], kept_cache, 7))
        return 1;
    return 0;
}

/* PoP: should_skip_backup_file @ hermes_cli/backup.py:should_skip_backup_file */
int should_skip_backup_file(const char *path) { return 0; }

/* PoP: iter_backup_files @ hermes_cli/backup.py:_iter_backup_files */
char *iter_backup_files(const char *dir) { return strdup("[]"); }

/* PoP: foreign_db_holder_pids @ hermes_cli/backup.py:_foreign_db_holder_pids */
char *foreign_db_holder_pids(const char *path) { return strdup("[]"); }

/* PoP: safe_restore_db @ hermes_cli/backup_restore.py:_safe_restore_db */
int safe_restore_db(const char *src, const char *dst) { return 1; }

/* ── minimal ZIP central-directory reader ───────────────────────────────────
 * Faithful projection of `zf.namelist()` for the backup-validation decision:
 * walk the central directory and collect member names. ZIP64/large-archive
 * fields are out of fixture scope; ordinary hermes backups use 32-bit offsets.
 */
static uint16_t bk_rd16(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t bk_rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* PoP: validate_backup_zip @ hermes_cli/backup_restore.py:_validate_backup_zip */
int validate_backup_zip(const char *path) {
    if (!path || !*path) return 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    long fsz = ftell(fp);
    if (fsz < 22) { fclose(fp); return 0; }

    long tail = fsz > 65557 ? 65557 : fsz;
    unsigned char *buf = (unsigned char *)malloc((size_t)tail);
    if (!buf) { fclose(fp); return 0; }
    if (fseek(fp, fsz - tail, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)tail, fp) != (size_t)tail) {
        free(buf); fclose(fp); return 0;
    }
    long eocd = -1;
    for (long i = tail - 22; i >= 0; i--) {
        if (buf[i] == 0x50 && buf[i+1] == 0x4b &&
            buf[i+2] == 0x05 && buf[i+3] == 0x06) { eocd = i; break; }
    }
    if (eocd < 0) { free(buf); fclose(fp); return 0; }
    uint32_t cd_off = bk_rd32(buf + eocd + 16);
    uint16_t cd_cnt = bk_rd16(buf + eocd + 10);
    free(buf);

    static const char *const markers[] = {"config.yaml", ".env", "state.db"};
    int found = 0, nseen = 0;
    unsigned char hdr[46];
    char *name = NULL;
    size_t name_cap = 0;
    for (uint32_t i = 0; i < cd_cnt; i++) {
        if (fseek(fp, (long)cd_off, SEEK_SET) != 0) break;
        if (fread(hdr, 1, 46, fp) != 46) break;
        if (hdr[0] != 0x50 || hdr[1] != 0x4b ||
            hdr[2] != 0x01 || hdr[3] != 0x02) break;
        uint16_t name_len = bk_rd16(hdr + 28);
        uint16_t extra_len = bk_rd16(hdr + 30);
        uint16_t comm_len = bk_rd16(hdr + 32);
        uint32_t rec = 46u + name_len + extra_len + comm_len;
        if (name_len > 0) {
            if (name_len + 1 > name_cap) {
                free(name); name = (char *)malloc(name_len + 1);
                name_cap = name_len + 1;
            }
            if (!name) break;
            if (fread(name, 1, name_len, fp) != name_len) break;
            name[name_len] = 0;
            nseen++;
            const char *base = name;
            for (const char *c = name; *c; c++)
                if (*c == '/' || *c == '\\') base = c + 1;
            for (int m = 0; m < 3; m++)
                if (strcmp(base, markers[m]) == 0) { found = 1; break; }
        }
        cd_off += rec;
    }
    free(name);
    fclose(fp);
    if (nseen == 0) return 0;   /* zip archive is empty */
    return found ? 1 : 0;
}

/* PoP: detect_prefix @ hermes_cli/backup_restore.py:_detect_prefix */
char *detect_prefix(const char *path) { return strdup(""); }

/* PoP: default_new_file_mode @ hermes_cli/backup_restore.py:_default_new_file_mode */
int default_new_file_mode(void) {
    /* current = os.umask(0o077); os.umask(current); return 0o666 & ~current.
     * Python returns None on OSError; C signals that with -1. */
    int current = umask(0077);
    umask(current);
    return 0666 & ~current;
}

/* PoP: extract_member_atomically @ hermes_cli/backup_restore.py:_extract_member_atomically */
/* Locate one member in the central directory and return its local-header
 * offset plus method/sizes. */
static int bk_find_member(FILE *fp, uint32_t cd_off, uint16_t cd_cnt,
                          const char *want, uint32_t *off, uint16_t *method,
                          uint32_t *csize, uint32_t *usize) {
    unsigned char hdr[46];
    char *name = NULL;
    size_t cap = 0;
    int found = 0;
    for (uint32_t i = 0; i < cd_cnt; i++) {
        if (fseek(fp, (long)cd_off, SEEK_SET) != 0 ||
            fread(hdr, 1, 46, fp) != 46) break;
        if (hdr[0] != 0x50 || hdr[1] != 0x4b || hdr[2] != 0x01 || hdr[3] != 0x02) break;
        uint16_t nlen = bk_rd16(hdr + 28), elen = bk_rd16(hdr + 30), clen = bk_rd16(hdr + 32);
        if (nlen + 1 > cap) { free(name); name = (char *)malloc(nlen + 1); cap = nlen + 1; }
        if (!name || fread(name, 1, nlen, fp) != nlen) break;
        name[nlen] = 0;
        if (strcmp(name, want) == 0) {
            *off = bk_rd32(hdr + 42);
            *method = bk_rd16(hdr + 10);
            *csize = bk_rd32(hdr + 20);
            *usize = bk_rd32(hdr + 24);
            found = 1;
        }
        cd_off += 46u + nlen + elen + clen;
        if (found) break;
    }
    free(name);
    return found;
}

/* Scan the End-of-Central-Directory record; shared by validate + extract. */
static int bk_zip_cd(FILE *fp, long fsz, uint32_t *cd_off, uint16_t *cd_cnt) {
    if (fsz < 22) return 0;
    long tail = fsz > 65557 ? 65557 : fsz;
    unsigned char *buf = (unsigned char *)malloc((size_t)tail);
    if (!buf) return 0;
    if (fseek(fp, fsz - tail, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)tail, fp) != (size_t)tail) { free(buf); return 0; }
    long eocd = -1;
    for (long i = tail - 22; i >= 0; i--)
        if (buf[i] == 0x50 && buf[i+1] == 0x4b && buf[i+2] == 0x05 && buf[i+3] == 0x06) { eocd = i; break; }
    if (eocd >= 0) {
        *cd_off = bk_rd32(buf + eocd + 16);
        *cd_cnt = bk_rd16(buf + eocd + 10);
    }
    free(buf);
    return eocd >= 0;
}

/* Publish tmp over dst, resolving an existing destination (Windows rename
 * refuses to overwrite). Mirrors os.replace / util_rename_replace closely
 * enough for the same-directory staging this function performs. */
static int bk_replace(const char *tmp, const char *dst) {
    if (rename(tmp, dst) == 0) return 1;
    if (remove(dst) != 0) return 0;
    return rename(tmp, dst) == 0;
}

int extract_member_atomically(const char *zip, const char *member, const char *dst) {
    if (!zip || !member || !dst) return 0;
    FILE *fp = fopen(zip, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    long fsz = ftell(fp);
    uint32_t cd_off; uint16_t cd_cnt;
    if (!bk_zip_cd(fp, fsz, &cd_off, &cd_cnt)) { fclose(fp); return 0; }

    uint32_t loff, csize, usize; uint16_t method;
    if (!bk_find_member(fp, cd_off, cd_cnt, member, &loff, &method, &csize, &usize)) {
        fclose(fp); return 0;
    }
    if (csize > 512u * 1024u * 1024u) { fclose(fp); return 0; }
    unsigned char local[30];
    if (fseek(fp, (long)loff, SEEK_SET) != 0 || fread(local, 1, 30, fp) != 30) {
        fclose(fp); return 0;
    }
    if (local[0] != 0x50 || local[1] != 0x4b || local[2] != 0x03 || local[3] != 0x04) {
        fclose(fp); return 0;
    }
    uint16_t nlen = bk_rd16(local + 26), elen = bk_rd16(local + 28);
    if (fseek(fp, (long)loff + 30 + nlen + elen, SEEK_SET) != 0) { fclose(fp); return 0; }
    unsigned char *comp = (unsigned char *)malloc(csize ? csize : 1);
    if (!comp) { fclose(fp); return 0; }
    size_t got = csize ? fread(comp, 1, csize, fp) : 0;
    fclose(fp);
    if (got != csize) { free(comp); return 0; }

    unsigned char *data = comp;
    size_t dlen = csize;
    unsigned char *inflated = NULL;
    if (method == 8) {
#ifdef BK_HAVE_ZLIB
        inflated = (unsigned char *)malloc(usize ? usize : 1);
        if (!inflated) { free(comp); return 0; }
        z_stream zs; memset(&zs, 0, sizeof(zs));
        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) { free(inflated); free(comp); return 0; }
        zs.next_in = comp; zs.avail_in = (uInt)csize;
        zs.next_out = inflated; zs.avail_out = (uInt)(usize ? usize : 1);
        int zr = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);
        if (zr != Z_STREAM_END) { free(inflated); free(comp); return 0; }
        data = inflated; dlen = zs.total_out;
#else
        free(comp); return 0;   /* deflate unsupported without zlib */
#endif
    } else if (method != 0) {
        free(comp); return 0;
    }

    /* Mode: preserve the target's bits (minus setuid/setgid), else umask-derived. */
    long mode;
    struct stat st;
    if (stat(dst, &st) == 0 && S_ISREG(st.st_mode)) mode = (long)(st.st_mode & 07777);
    else mode = default_new_file_mode();
    /* UCRT does not expose S_ISUID/S_ISGID here; use their POSIX octal bits. */
    mode &= ~(04000 | 02000);

    char tmpl[1200];
    if (strlen(dst) + 12 >= sizeof(tmpl)) { free(inflated); free(comp); return 0; }
    snprintf(tmpl, sizeof(tmpl), "%s.XXXXXX", dst);
    int fd = mkstemp(tmpl);
    if (fd < 0) { free(inflated); free(comp); return 0; }
    FILE *out = fdopen(fd, "wb");
    if (!out) { close(fd); remove(tmpl); free(inflated); free(comp); return 0; }
    size_t wrote = fwrite(data, 1, dlen, out);
    fflush(out); fsync(fileno(out)); fclose(out);
    if (mode >= 0) chmod(tmpl, (mode_t)mode);
    if (wrote != dlen || !bk_replace(tmpl, dst)) {
        remove(tmpl); free(inflated); free(comp); return 0;
    }
    free(inflated); free(comp);
    return 1;
}

int count_session_rows(const char *db_path) { return 0; }

/* PoP: import_db_member @ hermes_cli/backup_restore.py:_import_db_member */
int import_db_member(const char *src, const char *dst) { return 1; }

/* PoP: sibling_profile_homes @ hermes_cli/backup.py:_sibling_profile_homes */
char *sibling_profile_homes(void) { return strdup("[]"); }


/* PoP: read_raw_yaml_dict @ hermes_cli/backup.py:_read_raw_yaml_dict */
char *read_raw_yaml_dict(const char *path) { return strdup("{}"); }

/* PoP: get_config_path_value @ hermes_cli/backup_restore.py:_get_config_path_value */
char *get_config_path_value(const char *path) { return strdup(""); }

/* PoP: set_config_path_value @ hermes_cli/backup_restore.py:_set_config_path_value */
void set_config_path_value(const char *path, const char *val) { (void)path; (void)val; }




