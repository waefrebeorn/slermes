/*
 * skills_fs.h - skill filesystem paths and small file helpers.
 *
 * Module-internal header (skills.c <-> skills_fs.c).
 */
#ifndef SKILLS_FS_H
#define SKILLS_FS_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef SKILL_BUNDLES_SUBDIR
#define SKILL_BUNDLES_SUBDIR     "bundles"
#endif
#ifndef SKILL_PROVENANCE_FILE
#define SKILL_PROVENANCE_FILE    ".provenance"
#endif


void bundle_path(const char *bundle, char *buf, size_t sz);
bool skills_dir_exists(const char *path);
void skills_ensure_dir(const char *path);
bool skills_file_exists(const char *path);
time_t file_mtime(const char *path);
char * read_file(const char *path);
void skill_path(const char *name, char *buf, size_t sz);
void skill_prov_path(const char *name, char *buf, size_t sz);
void skill_sk_path(const char *name, char *buf, size_t sz);
void skills_bundles_dir(char *buf, size_t sz);
void skills_dir(char *buf, size_t sz);
bool write_file(const char *path, const char *content);

#endif /* SKILLS_FS_H */
