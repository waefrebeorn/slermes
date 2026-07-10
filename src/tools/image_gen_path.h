#ifndef SRC_TOOLS_IMAGE_GEN_PATH_H
#define SRC_TOOLS_IMAGE_GEN_PATH_H

/*
 * image_gen_path.h — focused extraction from tools/image_generation_tool.py
 *
 * Owns the single pure, oracle-verifiable path classifier:
 *   image_gen_path_looks_like_absolute_file_path()
 *
 * The config/mount-coupled path-translation helpers
 * (_agent_cache_base_for_env, _agent_visible_cache_path,
 * _postprocess_image_generate_result) are NOT here — they depend on
 * tools.credential_files.map_cache_path_to_container + env class dispatch
 * and stay in port_image_generation_tool.c (documented PoP ports).
 */

#include <stdbool.h>

/* Faithful C port of tools/image_generation_tool.py:_looks_like_absolute_file_path.
 * Returns true for POSIX absolute ("/a/b") or Windows drive ("C:/x", "D:\\x")
 * paths; false for URLs, data: URIs, and relative paths. */
bool image_gen_path_looks_like_absolute_file_path(const char *value);

#endif /* SRC_TOOLS_IMAGE_GEN_PATH_H */
