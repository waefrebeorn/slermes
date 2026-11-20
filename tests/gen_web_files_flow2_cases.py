#!/usr/bin/env python3
"""Generate tests/fixtures/web_files_flow2.jsonl for the files.py UNCLAIMED FLOW
oracle (5 funcs: _media_proxy_host_allowed, _fs_looks_binary,
_sanitize_chat_image_filename, _decode_data_url, _decode_chat_image_upload).

>=5 cases per ported function. Exercises: host exact/wildcard/lower/rstrip-dot
matching, binary heuristics (nul + 12% control threshold incl. the 1/8 boundary),
Windows filename sanitization, data-URL parse + strict base64 + size cap, and the
composed upload flow (seamed CLAIMED _chat_image_extension) with the FULL builtin
exception hierarchy (BaseException trio + *Group family + subclasses) plus the
fail-closed missing-script path and both 413 branches.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING seam script
returns a RuntimeError raise spec; array = per-call queue (last repeats, empty ->
None); single value = constant; scripted null = None; {"__raise__": cls, "msg":...}
= exception. ASCII domain; integral JSON numbers are int literals; bytes are
base64 strings (data URLs) or arrays of int bytes. The two byte caps are
fixture-supplied (live defaults 100 MiB / 25 MiB).
"""
import base64
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_files_flow2.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def b64(data: bytes) -> str:
    return base64.b64encode(data).decode("ascii")


def du(mime, data: bytes, marker=";base64") -> str:
    return "data:%s%s,%s" % (mime, marker, b64(data))


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


PNG = b"\x89PNG\r\n\x1a\n"          # 8 bytes -> iVBORw0KGgo=
JPG = b"\xff\xd8\xff"               # 3 bytes -> /9j/
HI = b"Hi"                          # 2 bytes -> SGk=

# ── _media_proxy_host_allowed (8) ──
add("media_proxy_host_allowed", {"host": "fal.media"})                 # exact
add("media_proxy_host_allowed", {"host": "FAL.MEDIA."})                # lower + rstrip '.'
add("media_proxy_host_allowed", {"host": "v3.fal.media"})              # exact
add("media_proxy_host_allowed", {"host": "storage.googleapis.com"})    # exact
add("media_proxy_host_allowed", {"host": "a.fal.media"})               # wildcard *.fal.media
add("media_proxy_host_allowed", {"host": "b.fal.run"})                 # wildcard *.fal.run
add("media_proxy_host_allowed", {"host": "evil.example.com"})          # reject
add("media_proxy_host_allowed", {"host": "fal.media.evil.com"})         # suffix but not subdomain
add("media_proxy_host_allowed", {"host": ""})                          # empty -> False

# ── _fs_looks_binary (9) ──
add("fs_looks_binary", {"data": []})                                   # empty -> False
add("fs_looks_binary", {"data": [72, 105]})                            # "Hi" -> False
add("fs_looks_binary", {"data": [72, 0, 105]})                         # nul -> True
add("fs_looks_binary", {"data": [1, 2, 3]})                            # all suspicious -> True
add("fs_looks_binary", {"data": [9, 10, 13, 32, 65, 90]})              # allowed controls -> False
add("fs_looks_binary", {"data": [65] * 100 + [1]})                     # 1/101 -> False
add("fs_looks_binary", {"data": [65] * 7 + [1]})                       # 1/8 = 0.125 > 0.12 -> True
add("fs_looks_binary", {"data": [65] * 8 + [1]})                       # 1/9 ~ 0.111 -> False
add("fs_looks_binary", {"data": [10] * 50 + [127]})                     # 127 not <32 -> False

# ── _sanitize_chat_image_filename (10) ──
add("sanitize_chat_image_filename", {"filename": "photo.png"})
add("sanitize_chat_image_filename", {"filename": None})                # -> pasted-image
add("sanitize_chat_image_filename", {"filename": "  spaced name.png  "})
add("sanitize_chat_image_filename", {"filename": "a\x00b\x1fc.png"})   # control runs -> '_'
add("sanitize_chat_image_filename", {"filename": "..hidden.."})        # strip('.')
add("sanitize_chat_image_filename", {"filename": "/tmp/evil.png"})     # POSIX basename
add("sanitize_chat_image_filename", {"filename": "C:\\secret\\x.png"}) # Windows basename
add("sanitize_chat_image_filename", {"filename": "..."})               # -> pasted-image
add("sanitize_chat_image_filename", {"filename": "."})                 # Path('.').name -> ''
add("sanitize_chat_image_filename", {"filename": "\x01\x02name.png"})  # leading controls -> '_name.png'

# ── _decode_data_url (9) ──
add("decode_data_url", {"data_url": du("image/png", PNG)})             # valid, mime image/png
add("decode_data_url", {"data_url": du("text/plain", HI)})             # valid text/plain
add("decode_data_url", {"data_url": du("", HI)})                       # empty mime -> octet-stream
add("decode_data_url", {"data_url": "data:image/png;base64,"})         # empty payload -> b''
add("decode_data_url", {"data_url": ""})                               # 400 must be a data URL
add("decode_data_url", {"data_url": "hello world"})                    # 400 must be a data URL
add("decode_data_url", {"data_url": "data:image/png," + b64(HI)})      # 400 must be base64
add("decode_data_url", {"data_url": "data:image/png;base64,!!!!"})     # 400 invalid base64
add("decode_data_url", {"data_url": du("image/png", PNG), "manage_max_bytes": 4})  # 413 too large

# ── _decode_chat_image_upload (16) ──
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": ".png"})
add("decode_chat_image_upload", {"data_url": du("image/jpeg", JPG)}, {"_chat_image_extension": ".jpg"})
add("decode_chat_image_upload", {"data_url": du("image/webp", PNG)}, {"_chat_image_extension": ".webp"})
add("decode_chat_image_upload", {"data_url": du("text/plain", HI)}, {})  # 400 must be an image (no seam)
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": ".xyz"})  # 400 unsupported
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": None})     # 400 unsupported
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("SystemExit", "bye")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("KeyboardInterrupt", "ki")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("GeneratorExit", "ge")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("BaseExceptionGroup", "beg")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("ExceptionGroup", "eg")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("ValueError", "v")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("ModuleNotFoundError", "m")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {"_chat_image_extension": R("UnicodeDecodeError", "u")})
add("decode_chat_image_upload", {"data_url": du("image/png", PNG)}, {})  # fail-closed missing script
add("decode_chat_image_upload", {"data_url": du("image/png", PNG), "chat_max_bytes": 2}, {"_chat_image_extension": ".png"})  # 413 cap is 0 MB
add("decode_chat_image_upload", {"data_url": du("image/png", PNG), "manage_max_bytes": 4}, {})  # 413 from decode (no seam)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    if set(counts) != {
        "media_proxy_host_allowed", "fs_looks_binary", "sanitize_chat_image_filename",
        "decode_data_url", "decode_chat_image_upload",
    }:
        raise SystemExit("UNEXPECTED FUNC SET: %s" % sorted(counts))
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()
