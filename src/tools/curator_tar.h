/*
 * curator_tar.h — tar.gz archive helpers for the curator backup subsystem.
 *
 * Module-internal header (curator_backup.c <-> curator_tar.c).
 */
#ifndef CURATOR_TAR_H
#define CURATOR_TAR_H

int is_excluded_top_level(const char *name);
int create_tar_gz(const char *dest_path, const char *source_dir);
int extract_tar_gz(const char *archive_path, const char *dest_dir);

#endif /* CURATOR_TAR_H */
