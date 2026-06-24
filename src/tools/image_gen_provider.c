/*
 * image_gen_provider.c — Name parity wrapper for Python agent/image_gen_provider.py
 *
 * NOTE: The C implementation lives in src/tools/image_gen.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/image_gen_provider.py.
 * C implementation: src/tools/image_gen.c
 *
 * Key functions ported:
 *   Image generation provider tool. C implementation in src/tools/image_gen.c: image_gen_generate, image_gen_list_models, image_gen_get_provider, image_gen_save_image, image_gen_handle_response.
 */
