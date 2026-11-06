#ifndef SLERMES_PORT_FILE_OPERATIONS_NATIVE_H
#define SLERMES_PORT_FILE_OPERATIONS_NATIVE_H

/* sha256_of(path): hex SHA-256 of a regular file's contents, or NULL when it
 * cannot be read natively (missing / non-regular / OS error). Caller frees. */
char *fop_sha256_of(const char *path);

#endif /* SLERMES_PORT_FILE_OPERATIONS_NATIVE_H */
