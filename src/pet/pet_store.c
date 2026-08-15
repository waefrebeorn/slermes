/*
 * pet_store.c — On-disk pet store: install, list, resolve, remove
 *
 * Port of Python: agent/pet/store.py
 * Pets live under ~/.slermes/pets/<slug>/ — profile-scoped.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include "pet.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include "hermes_logger.h"

/* PoP: pet_safe_slug @ agent/pet/store.py:_safe_slug */
const char *pet_safe_slug(const char *slug) {
    static char safe[PET_MAX_SLUG];
    if (!slug || !*slug) return "";

    const char *p = strrchr(slug, '/');
    if (p) p++;
    else p = slug;

    int i = 0;
    for (; *p && i < (int)sizeof(safe) - 1; p++) {
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '-' || *p == '_' || *p == '.') {
            safe[i++] = *p;
        }
    }
    safe[i] = '\0';

    if (strcmp(safe, ".") == 0 || strcmp(safe, "..") == 0)
        return "";
    if (safe[0] == '\0')
        return "";
    return safe;
}

/* PoP: pet_slugify @ agent/pet/store.py:slugify */
const char *pet_slugify(const char *name) {
    static char slug[PET_MAX_SLUG];
    if (!name || !*name) return "pet";

    int si = 0;
    bool was_hyphen = true;
    for (const char *p = name; *p && si < (int)sizeof(slug) - 1; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = c + 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            slug[si++] = c;
            was_hyphen = false;
        } else if (!was_hyphen) {
            slug[si++] = '-';
            was_hyphen = true;
        }
    }
    while (si > 0 && slug[si - 1] == '-') si--;
    slug[si] = '\0';

    if (slug[0] == '\0') return "pet";
    return slug;
}

/* PoP: pet_pets_dir @ agent/pet/store.py:pets_dir */
const char *pet_pets_dir(void) {
    static char path[1024];
    static bool initialized = false;
    if (!initialized) {
        const char *home = getenv("SLERMES_HOME");
        if (!home) {
            home = getenv("HOME");
            if (!home) home = "/tmp";
        }
        snprintf(path, sizeof(path), "%s/pets", home);
        struct stat st = {0};
        if (stat(path, &st) != 0)
            mkdir(path, 0755);
        initialized = true;
    }
    return path;
}

/* Read pet.json from pet directory */
/* PoP: read_pet_json @ agent/pet/store.py:_read_pet_json */
static json_t *read_pet_json(const char *directory) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/pet.json", directory);

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 65536) { fclose(f); return NULL; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)sz, f);
    buf[nread] = '\0';
    fclose(f);

    json_t *meta = json_parse(buf, NULL);
    free(buf);
    return meta;
}

/* Resolve spritesheet path from pet dir and metadata */
/* PoP: resolve_spritesheet @ agent/pet/store.py:_resolve_spritesheet */
static bool resolve_spritesheet(const char *directory, json_t *meta,
                                 char *out, size_t out_sz) {
    if (meta) {
        const char *declared = json_node_get_string(json_obj_get(meta, "spritesheetPath"));
        if (declared && *declared) {
            snprintf(out, out_sz, "%s/%s", directory, declared);
            if (access(out, F_OK) == 0)
                return true;
        }
    }

    const char *names[] = {
        "spritesheet.webp", "spritesheet.png",
        "sprite.webp", "sprite.png",
        NULL
    };
    for (int i = 0; names[i]; i++) {
        snprintf(out, out_sz, "%s/%s", directory, names[i]);
        if (access(out, F_OK) == 0)
            return true;
    }
    snprintf(out, out_sz, "%s/spritesheet.webp", directory);
    return false;
}

/* PoP: pet_load_pet @ agent/pet/store.py:load_pet */
bool pet_load_pet(const char *slug, pet_installed_t *out) {
    if (!slug || !*slug || !out) return false;

    const char *safe = pet_safe_slug(slug);
    if (!*safe) return false;

    const char *base = pet_pets_dir();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base, safe);

    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return false;

    json_t *meta = read_pet_json(dir);

    snprintf(out->slug, sizeof(out->slug), "%s", safe);
    const char *dn = meta ? json_node_get_string(json_obj_get(meta, "displayName")) : NULL;
    snprintf(out->display_name, sizeof(out->display_name), "%s", dn ? dn : safe);
    const char *desc = meta ? json_node_get_string(json_obj_get(meta, "description")) : NULL;
    snprintf(out->description, sizeof(out->description), "%s", desc ? desc : "");
    snprintf(out->directory, sizeof(out->directory), "%s", dir);

    resolve_spritesheet(dir, meta, out->spritesheet_path, sizeof(out->spritesheet_path));
    out->exists = (access(out->spritesheet_path, F_OK) == 0);

    const char *cb = meta ? json_node_get_string(json_obj_get(meta, "createdBy")) : NULL;
    out->generated = (cb && strcmp(cb, "generator") == 0);

    if (meta) json_free(meta);
    return true;
}

/* PoP: pet_installed_pets @ agent/pet/store.py:installed_pets */
int pet_installed_pets(pet_installed_t *out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int count = 0;

    const char *base = pet_pets_dir();
    DIR *d = opendir(base);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < max_count) {
        if (entry->d_type != DT_DIR) continue;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (strcmp(entry->d_name, ".thumbs") == 0)
            continue;

        pet_installed_t pet;
        if (pet_load_pet(entry->d_name, &pet) && pet.exists)
            out[count++] = pet;
    }
    closedir(d);
    return count;
}

/* PoP: pet_resolve_active_pet @ agent/pet/store.py:resolve_active_pet */
bool pet_resolve_active_pet(const char *configured_slug, pet_installed_t *out) {
    if (!out) return false;

    if (configured_slug && *configured_slug) {
        if (pet_load_pet(configured_slug, out) && out->exists)
            return true;
    }

    pet_installed_t pets[64];
    int count = pet_installed_pets(pets, 64);
    if (count > 0) {
        *out = pets[0];
        return true;
    }
    return false;
}

/* PoP: pet_install_pet @ agent/pet/store.py:install_pet */
bool pet_install_pet(const char *slug, pet_installed_t *out, bool force) {
    if (!slug || !*slug || !out) return false;

    const char *safe = pet_safe_slug(slug);
    if (!*safe) return false;

    if (!force && pet_load_pet(slug, out) && out->exists)
        return true;

    pet_manifest_entry_t entry;
    if (!pet_find_entry(slug, &entry)) {
        hermes_log(LOG_WARNING, "pet", "pet '%s' not in manifest", slug);
        return false;
    }

    const char *base = pet_pets_dir();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base, safe);
    mkdir(dir, 0755);

    /* Download spritesheet */
    char sprite_path[1024];
    const char *ext = strstr(entry.spritesheet_url, ".png") ? "png" : "webp";
    snprintf(sprite_path, sizeof(sprite_path), "%s/spritesheet.%s", dir, ext);

    http_t *client = http_new((int)PET_DOWNLOAD_TIMEOUT);
    if (!client) { rmdir(dir); return false; }

    http_resp_t *resp = http_get(client, entry.spritesheet_url, NULL);
    if (!resp || resp->status != 200 || !resp->body) {
        hermes_log(LOG_WARNING, "pet", "spritesheet download failed for '%s'", slug);
        if (resp) http_resp_free(resp);
        http_free(client);
        rmdir(dir);
        return false;
    }

    FILE *f = fopen(sprite_path, "wb");
    if (!f) {
        http_resp_free(resp);
        http_free(client);
        rmdir(dir);
        return false;
    }
    fwrite(resp->body, 1, strlen(resp->body), f);
    fclose(f);
    http_resp_free(resp);
    http_free(client);

    /* Fetch pet.json if available */
    json_t *meta = NULL;
    if (entry.pet_json_url[0] &&
        strstr(entry.pet_json_url, "petdex.dev") != NULL) {
        http_t *mclient = http_new((int)PET_DOWNLOAD_TIMEOUT);
        if (mclient) {
            http_resp_t *mresp = http_get(mclient, entry.pet_json_url, NULL);
            if (mresp && mresp->status == 200 && mresp->body) {
                meta = json_parse(mresp->body, NULL);
                http_resp_free(mresp);
            }
            http_free(mclient);
        }
    }

    /* Write pet.json */
    char pet_json_path[1024];
    snprintf(pet_json_path, sizeof(pet_json_path), "%s/pet.json", dir);

    json_t *meta_out = meta ? meta : json_object();
    if (!meta) {
        json_set(meta_out, "id", json_string(safe));
        json_set(meta_out, "displayName", json_string(entry.display_name));
        json_set(meta_out, "description", json_string(""));
    }
    json_set(meta_out, "spritesheetPath", json_string(sprite_path));

    char *json_str = json_serialize(meta_out);
    if (json_str) {
        FILE *jf = fopen(pet_json_path, "w");
        if (jf) {
            fputs(json_str, jf);
            fclose(jf);
        }
        free(json_str);
    }
    json_free(meta_out);

    return pet_load_pet(slug, out);
}

/* PoP: pet_remove_pet @ agent/pet/store.py:remove_pet */
bool pet_remove_pet(const char *slug) {
    const char *safe = pet_safe_slug(slug);
    if (!*safe) return false;

    const char *base = pet_pets_dir();

    /* Remove cached thumbnail */
    char thumb_path[1024];
    snprintf(thumb_path, sizeof(thumb_path), "%s/.thumbs/%s.png", base, safe);
    unlink(thumb_path);

    /* Remove pet directory */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base, safe);
    struct stat st;
    if (stat(dir, &st) != 0) return false;

    DIR *d = opendir(dir);
    if (!d) return false;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_REG) {
            char fp[1100];
            snprintf(fp, sizeof(fp), "%s/%s", dir, entry->d_name);
            unlink(fp);
        }
    }
    closedir(d);
    rmdir(dir);
    return (stat(dir, &st) != 0);
}

/* PoP: install_generated @ agent/pet/store.py (generation install path)
 *
 * Install a LOCALLY GENERATED pet: creates <pets>/<slug>/ with pet.json
 * (id, displayName, description, spritesheetPath) and copies the
 * spritesheet file into place. Unlike pet_install_pet (petdex download),
 * this takes a local file produced by the image-gen pipeline. */
bool pet_install(const char *slug, const char *display_name,
                 const char *description, const char *spritesheet_src) {
    if (!slug || !*slug || !spritesheet_src) return false;

    const char *safe = pet_safe_slug(slug);
    if (!*safe) return false;

    const char *base = pet_pets_dir();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base, safe);

    /* Create the pet directory. */
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0) return false;
    } else if (!S_ISDIR(st.st_mode)) {
        return false;
    }

    /* Determine the spritesheet extension from the source file. */
    const char *dot = strrchr(spritesheet_src, '.');
    const char *ext = (dot && dot[1]) ? dot : ".png";
    char sheet_name[64];
    snprintf(sheet_name, sizeof(sheet_name), "spritesheet%s", ext);

    /* Copy the spritesheet into place. */
    char sheet_dst[1100];
    snprintf(sheet_dst, sizeof(sheet_dst), "%s/%s", dir, sheet_name);
    FILE *in = fopen(spritesheet_src, "rb");
    if (!in) return false;
    FILE *outf = fopen(sheet_dst, "wb");
    if (!outf) { fclose(in); return false; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, outf);
    fclose(outf);
    fclose(in);

    /* Write pet.json. */
    char meta_path[1100];
    snprintf(meta_path, sizeof(meta_path), "%s/pet.json", dir);
    json_t *meta = json_object();
    json_set(meta, "id", json_string(safe));
    json_set(meta, "displayName", json_string(display_name ? display_name : safe));
    json_set(meta, "description", json_string(description ? description : ""));
    json_set(meta, "createdBy", json_string("generator"));
    json_set(meta, "spritesheetPath", json_string(sheet_name));
    char *ser = json_serialize_pretty(meta, 2);
    json_free(meta);
    if (!ser) return false;

    FILE *mf = fopen(meta_path, "w");
    if (!mf) { free(ser); return false; }
    fputs(ser, mf);
    fclose(mf);
    free(ser);
    return true;
}

/* PoP: pet_rename_pet @ agent/pet/store.py:rename_pet */
const char *pet_rename_pet(const char *slug, const char *display_name) {
    static char new_slug[PET_MAX_SLUG];
    if (!slug || !*slug || !display_name || !*display_name)
        return slug;

    const char *safe = pet_safe_slug(slug);
    if (!*safe) return slug;

    const char *base = pet_pets_dir();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", base, safe);

    json_t *meta = read_pet_json(dir);
    if (!meta) return slug;

    json_set(meta, "displayName", json_string(display_name));

    const char *desired = pet_slugify(display_name);
    snprintf(new_slug, sizeof(new_slug), "%s", safe);

    if (desired && *desired && strcmp(desired, safe) != 0) {
        char new_dir[1024];
        snprintf(new_dir, sizeof(new_dir), "%s/%s", base, desired);
        struct stat st;
        if (stat(new_dir, &st) != 0) {
            if (rename(dir, new_dir) == 0) {
                snprintf(new_slug, sizeof(new_slug), "%s", desired);
                json_set(meta, "id", json_string(desired));

                char old_thumb[1024], new_thumb[1024];
                snprintf(old_thumb, sizeof(old_thumb), "%s/.thumbs/%s.png", base, safe);
                snprintf(new_thumb, sizeof(new_thumb), "%s/.thumbs/%s.png", base, desired);
                rename(old_thumb, new_thumb);

                snprintf(dir, sizeof(dir), "%s/%s", base, desired);
            }
        }
    }

    char pet_json_path[1024];
    snprintf(pet_json_path, sizeof(pet_json_path), "%s/pet.json", dir);

    char *json_str = json_serialize(meta);
    if (json_str) {
        FILE *jf = fopen(pet_json_path, "w");
        if (jf) { fputs(json_str, jf); fclose(jf); }
        free(json_str);
    }
    json_free(meta);

    return new_slug;
}

/* PoP: pet_unique_slug @ agent/pet/store.py:unique_slug */
const char *pet_unique_slug(const char *name) {
    static char slug[PET_MAX_SLUG];
    const char *base_slug = pet_slugify(name);
    snprintf(slug, sizeof(slug), "%s", base_slug);

    const char *pets_base = pet_pets_dir();
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/%s", pets_base, slug);

    struct stat st;
    int counter = 2;
    while (stat(dir, &st) == 0) {
        snprintf(slug, sizeof(slug), "%s-%d", base_slug, counter);
        snprintf(dir, sizeof(dir), "%s/%s", pets_base, slug);
        counter++;
        if (counter > 1000) return base_slug;
    }
    return slug;
}

/* PoP: pet_thumbnail_png @ agent/pet/store.py:thumbnail_png */
unsigned char *pet_thumbnail_png(const char *slug, int *out_len) {
    if (!slug || !*slug || !out_len) return NULL;
    *out_len = 0;

    const char *safe = pet_safe_slug(slug);
    if (!*safe) return NULL;

    const char *base = pet_pets_dir();
    char thumb_path[1024];
    snprintf(thumb_path, sizeof(thumb_path), "%s/.thumbs/%s.png", base, safe);

    /* Check cache */
    FILE *tf = fopen(thumb_path, "rb");
    if (tf) {
        fseek(tf, 0, SEEK_END);
        long sz = ftell(tf);
        rewind(tf);
        if (sz > 0 && sz < 1048576) {
            unsigned char *data = malloc((size_t)sz);
            if (data) {
                fread(data, 1, (size_t)sz, tf);
                *out_len = (int)sz;
                fclose(tf);
                return data;
            }
        }
        fclose(tf);
    }

    /* Try spritesheet */
    pet_installed_t pet;
    if (!pet_load_pet(slug, &pet) || !pet.exists) return NULL;

    FILE *sf = fopen(pet.spritesheet_path, "rb");
    if (!sf) return NULL;

    fseek(sf, 0, SEEK_END);
    long sz = ftell(sf);
    rewind(sf);
    if (sz <= 0 || sz > 10485760) { fclose(sf); return NULL; }

    unsigned char *data = malloc((size_t)sz);
    if (!data) { fclose(sf); return NULL; }
    fread(data, 1, (size_t)sz, sf);
    fclose(sf);

    /* Cache as thumbnail */
    char thumb_dir[1024];
    snprintf(thumb_dir, sizeof(thumb_dir), "%s/.thumbs/", base);
    mkdir(thumb_dir, 0755);

    FILE *cf = fopen(thumb_path, "wb");
    if (cf) {
        fwrite(data, 1, (size_t)sz, cf);
        fclose(cf);
    }

    *out_len = (int)sz;
    return data;
}

/* PoP: pet_is_generated @ agent/pet/store.py:generated */
bool pet_is_generated(const char *slug) {
    if (!slug || !*slug) return false;
    pet_installed_t pet;
    if (!pet_load_pet(slug, &pet)) return false;
    return pet.generated;
}

/* PoP: pet_export_pet @ agent/pet/store.py:export_pet */
unsigned char *pet_export_pet(const char *slug, int *out_len) {
    if (!slug || !*slug || !out_len) return NULL;
    *out_len = 0;

    pet_installed_t pet;
    if (!pet_load_pet(slug, &pet) || !pet.exists) return NULL;

    FILE *sf = fopen(pet.spritesheet_path, "rb");
    if (!sf) return NULL;

    fseek(sf, 0, SEEK_END);
    long sz = ftell(sf);
    rewind(sf);
    if (sz <= 0 || sz > 10485760) { fclose(sf); return NULL; }

    unsigned char *data = malloc((size_t)sz + 1);
    if (!data) { fclose(sf); return NULL; }

    size_t n = fread(data, 1, (size_t)sz, sf);
    fclose(sf);

    if (n != (size_t)sz) { free(data); return NULL; }
    data[sz] = '\0';
    *out_len = (int)sz;
    return data;
}

/* PoP: pet_thumbs_dir @ agent/pet/store.py:_thumbs_dir */
const char *pet_thumbs_dir(void) {
    static char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.thumbs/", pet_pets_dir());
    return dir;
}

/* PoP: pet_is_petdex_host @ agent/pet/store.py:_is_petdex_host */
bool pet_is_petdex_host(const char *url) {
    if (!url || !*url) return false;
    /* petdex.dev, www.petdex.dev, api.petdex.dev all valid */
    return (strstr(url, "petdex.dev") != NULL);
}

/* PoP: pet_download_json @ agent/pet/store.py:_download_json */
json_t *pet_download_json(const char *url) {
    if (!url || !*url) return NULL;

    http_client_t *client = http_new(NULL);
    if (!client) return NULL;

    http_resp_t *resp = http_get(client, url, NULL);
    http_free(client);

    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        return NULL;
    }

    json_t *json = json_parse(resp->body, NULL);
    http_resp_free(resp);
    return json;
}

/* PoP: pet_write_spritesheet @ agent/pet/store.py:_write_spritesheet */
bool pet_write_spritesheet(const char *source_path, const char *dest_path) {
    if (!source_path || !*source_path || !dest_path || !*dest_path) return false;

    FILE *src = fopen(source_path, "rb");
    if (!src) return false;

    FILE *dst = fopen(dest_path, "wb");
    if (!dst) { fclose(src); return false; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            fclose(src); fclose(dst); unlink(dest_path);
            return false;
        }
    }

    fclose(src);
    fclose(dst);
    return true;
}

/* PoP: pet_register_local_pet @ agent/pet/store.py:register_local_pet */
bool pet_register_local_pet(const char *slug, const char *display_name,
                            const char *spritesheet_path, const char *description) {
    if (!slug || !*slug || !spritesheet_path || !*spritesheet_path) return false;

    const char *safe = pet_safe_slug(slug);
    if (!*safe) return false;

    /* Create pet directory */
    const char *pets_base = pet_pets_dir();
    char pet_dir[1024];
    snprintf(pet_dir, sizeof(pet_dir), "%s/%s", pets_base, safe);
    mkdir(pet_dir, 0755);

    /* Copy spritesheet */
    char dest_spritesheet[1024];
    snprintf(dest_spritesheet, sizeof(dest_spritesheet), "%s/spritesheet.png", pet_dir);
    if (!pet_write_spritesheet(spritesheet_path, dest_spritesheet)) return false;

    /* Write pet.json */
    json_t *meta = json_object();
    if (!meta) return false;

    json_set(meta, "slug", json_new_string(safe));
    json_set(meta, "display_name", json_new_string(display_name ? display_name : safe));
    json_set(meta, "generated", json_new_bool(true));
    if (description && *description)
        json_set(meta, "description", json_new_string(description));

    char pet_json_path[1024];
    snprintf(pet_json_path, sizeof(pet_json_path), "%s/pet.json", pet_dir);
    char *json_str = json_serialize(meta);
    if (json_str) {
        FILE *jf = fopen(pet_json_path, "w");
        if (jf) { fputs(json_str, jf); fclose(jf); }
        free(json_str);
    }
    json_free(meta);
    return true;
}

/* PoP: pet_download_file @ agent/pet/store.py:_download */
bool pet_download_file(const char *url, const char *dest_path, int timeout_sec)
{
    (void)timeout_sec;
    if (!url || !*url || !dest_path || !*dest_path) return false;

    http_client_t *client = http_new(timeout_sec > 0 ? timeout_sec : 30);
    if (!client) return false;

    http_resp_t *resp = http_get(client, url, NULL);
    http_free(client);

    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        return false;
    }

    size_t dest_len = strlen(dest_path);
    char *tmp_path = malloc(dest_len + 6);
    if (!tmp_path) { http_resp_free(resp); return false; }
    snprintf(tmp_path, dest_len + 6, "%s.part", dest_path);

    FILE *fh = fopen(tmp_path, "wb");
    if (!fh) { free(tmp_path); http_resp_free(resp); return false; }

    size_t written = fwrite(resp->body, 1, resp->body_len, fh);
    fclose(fh);

    if (written != resp->body_len) {
        unlink(tmp_path); free(tmp_path);
        http_resp_free(resp); return false;
    }

    if (rename(tmp_path, dest_path) != 0) {
        unlink(tmp_path); free(tmp_path);
        http_resp_free(resp); return false;
    }

    free(tmp_path);
    http_resp_free(resp);
    return true;
}
