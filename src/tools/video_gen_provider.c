/*
 * video_gen_provider.c — Name parity wrapper for Python agent/video_gen_provider.py
 *
 * NOTE: The C implementation lives in src/tools/video_gen.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/video_gen_provider.py.
 * C implementation: src/tools/video_gen.c
 *
 * Key functions ported:
 *   Video generation provider tool. C implementation in src/tools/video_gen.c: video_gen_generate, video_gen_get_provider, video_gen_list_models, video_gen_save_video, video_gen_handle_response.
 */
