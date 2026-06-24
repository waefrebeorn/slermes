/*
 * binary_extensions.c — Name parity wrapper for Python tools/binary_extensions.py
 *
 * NOTE: The C implementation lives in lib/libbinary/binary.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/binary_extensions.py.
 * C implementation: lib/libbinary/binary.c
 *
 * Binary file extension detection
 *
 * Key functions ported:
 *   is_binary_extension(), is_text_extension()
 */
#include "hermes.h"
