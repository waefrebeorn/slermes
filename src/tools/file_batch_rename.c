/*
 * file_batch_rename.c - batch rename wildcard helpers.
 *
 * Split from file_batch.c (soul.md: self-contained modules). Extracts and
 * substitutes the `*` wildcard in a rename pattern. No file-scope state.
 */
#include "file_batch_rename.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include "hash.h"


/* Glob wildcard helpers for batch rename */
/* Extract the portion of filename that matched * in glob pattern.
   E.g. pattern "prefix_*.txt", filename "prefix_foo.txt" -> "foo".
   Returns strdup'd string or NULL on mismatch. */
static char *extract_wildcard(const char *pattern, const char *filename) {
    const char *star = strchr(pattern, '*');
    if (!star) return strdup(filename);  /* no wildcard -> whole name is match */

    /* Split pattern around * */
    size_t prefix_len = (size_t)(star - pattern);
    const char *suffix = star + 1;
    size_t suffix_len = strlen(suffix);
    size_t name_len = strlen(filename);

    /* Check prefix match */
    if (name_len < prefix_len) return NULL;
    if (strncmp(filename, pattern, prefix_len) != 0) return NULL;

    /* Check suffix match */
    if (name_len < suffix_len) return NULL;
    if (strcmp(filename + name_len - suffix_len, suffix) != 0) return NULL;

    /* Extract the middle portion */
    size_t mid_len = name_len - prefix_len - suffix_len;
    char *portion = (char *)malloc(mid_len + 1);
    if (!portion) return NULL;
    memcpy(portion, filename + prefix_len, mid_len);
    portion[mid_len] = '\0';
    return portion;
}


/* Substitute * in dest_pattern with wildcard_value.
   E.g. dest_pattern "renamed_*.md", value "foo" -> "renamed_foo.md".
   Returns strdup'd string. */
static char *substitute_wildcard(const char *dest_pattern, const char *wildcard_value) {
    const char *star = strchr(dest_pattern, '*');
    if (!star) return strdup(dest_pattern);

    size_t prefix_len = (size_t)(star - dest_pattern);
    const char *suffix = star + 1;

    size_t total_len = prefix_len + strlen(wildcard_value) + strlen(suffix) + 1;
    char *result = (char *)malloc(total_len);
    if (!result) return NULL;

    memcpy(result, dest_pattern, prefix_len);
    result[prefix_len] = '\0';
    strcat(result, wildcard_value);
    strcat(result, suffix);
    return result;
}


/* Rename a single file using pattern-based renaming.
   pattern: glob pattern (e.g. "*.txt")
   dest_pattern: rename template (e.g. "backup_*.md")
   filename: actual matched filename
   Returns new path or NULL on failure/extraction mismatch. */ char *apply_rename_pattern(const char *pattern, const char *dest_pattern,
                                   const char *filename) {
    char *wildcard = extract_wildcard(pattern, filename);
    if (!wildcard) return NULL;
    char *new_path = substitute_wildcard(dest_pattern, wildcard);
    free(wildcard);
    return new_path;
}
