/*
 * port_learning_mutations.h — public API for agent/learning_mutations.py.
 *
 * User-initiated edit/delete for journey nodes (learned skills + memories).
 * Maps a node id back to its on-disk home and performs the mutation, shared
 * by the CLI (`hermes journey delete|edit`), the TUI `/journey` overlay, and
 * the desktop GUI (REST). Pure C port; reuses libskillusage (archive/pin),
 * the learning_graph memory_cards reader, and standard POSIX I/O — mirroring
 * the Python module's reliance on skill_usage / _memory_cards / MemoryStore.
 *
 * All "inspect/edit/delete" entry points return malloc'd JSON (caller frees):
 *   {"ok":true,"kind":...,"id":...,"label":...,"content":...}   (node_detail)
 *   {"ok":bool,"message":str}                                    (delete/edit)
 * Memory/§ parsing and skill discovery are internal helpers.
 */

#ifndef PORT_LEARNING_MUTATIONS_H
#define PORT_LEARNING_MUTATIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Node-id helpers */
char                *learning_mutations_parse_node_kind(const char *node_id); /* "memory"|"skill" */
int                  learning_mutations_parse_memory_id(const char *node_id,
                                                       char *out_source,
                                                       int *out_gidx,
                                                       char *err, size_t errsz);
char                *learning_mutations_node_detail(const char *node_id);
char                *learning_mutations_delete_node(const char *node_id);
char                *learning_mutations_edit_node(const char *node_id,
                                                  const char *content);

/* Internal memory-file § parser (exposed for tests): returns malloc'd
 * NULL-terminated array of entry strings (each stripped); *out_n set. Caller
 * frees each + the array. Mirrors MemoryStore._read_file (delim "\n§\n"). */
char               **learning_mutations_read_memory_file(const char *path,
                                                         int *out_n);
/* Atomic rewrite: write non-empty stripped entries joined by "\n§\n". */
int                  learning_mutations_write_memory_file(const char *path,
                                                          char **chunks, int n);

#ifdef __cplusplus
}
#endif

#endif /* PORT_LEARNING_MUTATIONS_H */
