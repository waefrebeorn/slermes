/*
 * yuanbao_proto.c — Name parity wrapper for Python gateway/platforms/yuanbao_proto.py
 *
 * NOTE: The C implementation for this platform feature is integrated into
 * the main platform adapter file (e.g. feishu.c, yuanbao.c, signal.c),
 * not a standalone C file. This file exists for name parity only.
 *
 * Yuanbao (Tencent Yuanbao) protobuf protocol definitions.
Defines message formats for Yuanbao group chat protocol.

Port of Python gateway/platforms/yuanbao_proto.py (1209 lines).
N/A: Python protobuf class definitions — C uses manual ser/deser.
The actual Yuanbao platform adapter is in src/gateway/platforms/yuanbao.c.
 */

#include "hermes_core_types.h"
#include "hermes_gateway_yuanbao.h"
