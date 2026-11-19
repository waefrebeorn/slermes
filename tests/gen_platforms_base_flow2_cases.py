#!/usr/bin/env python3
"""Generate tests/fixtures/platforms_base_flow2.jsonl for the
platforms_base_flow2 FLOW oracle (gateway/platforms/base.py unclaimed remainder).

Deterministic cases: >=5 per ported func, covering happy paths, branch
conditions, exception -> fallback / suppress / re-raise paths, BaseException
passthrough, import gates and fail-closed (unscripted seam) behavior. List
seam results are wrapped {"__script__":true,"ret":[...]} (a bare array is a
per-call SEQUENCE). Paths are clean absolute POSIX; data is ASCII.
"""
import json
from pathlib import Path, PurePosixPath

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "platforms_base_flow2.jsonl"

UUID = "abcdef0123456789abcdef0123456789"
H12 = UUID[:12]
CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


def wr(**extra):
    base = {"uuid4_hex": UUID, "os_open": 7, "fdopen_write": None}
    base.update(extra)
    return base


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def script_list(lst):
    return {"__script__": True, "ret": lst}


# ── _secure_media_cache_dir ──
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {"is_managed": False, "make_user_dir": None, "secure_dir": None}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {"is_managed": True, "mkdir": None}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {"is_managed": R("OSError", "boom"), "log_debug": None, "mkdir": None}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {"is_managed": False, "make_user_dir": R("ValueError", "mv"), "log_debug": None, "mkdir": None}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {"is_managed": False, "make_user_dir": None, "secure_dir": R("RuntimeError", "sd"), "log_debug": None}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "config_import_fail": True, "io": {"make_user_dir": None, "log_debug": None}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {"is_managed": R("KeyboardInterrupt", "ki")}})
add("_secure_media_cache_dir", {"cache_dir": "/c", "io": {}})

# ── _write_cache_file ──
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": wr()})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": {"uuid4_hex": UUID, "os_open": R("OSError", "no"), "write_bytes": None}})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "aud", "ext": ".ogg", "data": "y", "io": {"uuid4_hex": UUID, "os_open": R("PermissionError", "p"), "write_bytes": None}})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": {"uuid4_hex": UUID, "os_open": R("ValueError", "v")}})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": {"uuid4_hex": UUID, "os_open": 7, "fdopen_write": R("OSError", "w")}})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": {"uuid4_hex": None}})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": {"uuid4_hex": UUID, "os_open": R("SystemExit", "se")}})
add("_write_cache_file", {"cache_dir": "/c", "prefix": "img", "ext": ".jpg", "data": "xx", "io": {}})
add("_write_cache_file", {"cache_dir": "/", "prefix": "img", "ext": ".jpg", "data": "xx", "io": wr()})
add("_write_cache_file", {"cache_dir": "//", "prefix": "img", "ext": ".jpg", "data": "xx", "io": wr()})

# ── cache_image_from_bytes ──
add("cache_image_from_bytes", {"data": "imgdata", "ext": ".jpg", "io": dict(validate_inbound_media_size=None, looks_like_image=True, get_image_cache_dir="/i", **wr())})
add("cache_image_from_bytes", {"data": "notimg", "ext": ".jpg", "io": {"validate_inbound_media_size": None, "looks_like_image": False}})
add("cache_image_from_bytes", {"data": "x" * 120, "ext": ".png", "io": {"validate_inbound_media_size": None, "looks_like_image": False}})
add("cache_image_from_bytes", {"data": "a'b\"c\td", "ext": ".jpg", "io": {"validate_inbound_media_size": None, "looks_like_image": False}})
add("cache_image_from_bytes", {"data": "img", "ext": ".jpg", "io": {"validate_inbound_media_size": R("ValueError", "big"), "looks_like_image": True}})
add("cache_image_from_bytes", {"data": "img", "ext": ".jpg", "io": {"validate_inbound_media_size": None, "looks_like_image": R("KeyboardInterrupt", "ki")}})
add("cache_image_from_bytes", {"data": "img", "ext": ".jpg", "io": {}})

# ── cache_image_from_bytes_async ──
add("cache_image_from_bytes_async", {"data": "x", "ext": ".jpg", "io": {"to_thread": "/i/img.jpg"}})
add("cache_image_from_bytes_async", {"data": "x", "ext": ".png", "io": {"to_thread": R("ValueError", "v")}})
add("cache_image_from_bytes_async", {"data": "x", "io": {"to_thread": R("KeyboardInterrupt", "ki")}})
add("cache_image_from_bytes_async", {"data": "x", "io": {"to_thread": None}})
add("cache_image_from_bytes_async", {"data": "x", "io": {}})

# ── cache_audio_from_bytes ──
add("cache_audio_from_bytes", {"data": "aud", "ext": ".ogg", "io": dict(validate_inbound_media_size=None, get_audio_cache_dir="/a", sniff_audio_ext=".opus", **wr())})
add("cache_audio_from_bytes", {"data": "aud", "ext": ".mp3", "io": dict(validate_inbound_media_size=None, get_audio_cache_dir="/a", sniff_audio_ext=".mp3", **wr())})
add("cache_audio_from_bytes", {"data": "aud", "ext": ".ogg", "audio_import_fail": True, "io": {}})
add("cache_audio_from_bytes", {"data": "aud", "ext": ".ogg", "io": {"validate_inbound_media_size": R("ValueError", "big")}})
add("cache_audio_from_bytes", {"data": "aud", "ext": ".ogg", "io": {"validate_inbound_media_size": None, "get_audio_cache_dir": "/a", "sniff_audio_ext": R("ValueError", "sn")}})
add("cache_audio_from_bytes", {"data": "aud", "ext": ".ogg", "io": {}})

# ── cache_audio_from_bytes_async ──
add("cache_audio_from_bytes_async", {"data": "x", "ext": ".ogg", "io": {"to_thread": "/a/audio.ogg"}})
add("cache_audio_from_bytes_async", {"data": "x", "ext": ".ogg", "io": {"to_thread": R("ValueError", "v")}})
add("cache_audio_from_bytes_async", {"data": "x", "io": {"to_thread": R("SystemExit", "se")}})
add("cache_audio_from_bytes_async", {"data": "x", "io": {"to_thread": None}})
add("cache_audio_from_bytes_async", {"data": "x", "io": {}})

# ── cache_video_from_bytes ──
add("cache_video_from_bytes", {"data": "vid", "ext": ".mp4", "io": dict(validate_inbound_media_size=None, get_video_cache_dir="/v", **wr())})
add("cache_video_from_bytes", {"data": "vid", "ext": ".webm", "io": dict(validate_inbound_media_size=None, get_video_cache_dir="/v", **wr())})
add("cache_video_from_bytes", {"data": "vid", "io": {"validate_inbound_media_size": R("ValueError", "big")}})
add("cache_video_from_bytes", {"data": "vid", "io": {"validate_inbound_media_size": None, "get_video_cache_dir": R("OSError", "gd")}})
add("cache_video_from_bytes", {"data": "vid", "io": {"validate_inbound_media_size": R("KeyboardInterrupt", "ki")}})
add("cache_video_from_bytes", {"data": "vid", "io": {}})

# ── cache_video_from_bytes_async ──
add("cache_video_from_bytes_async", {"data": "x", "ext": ".mp4", "io": {"to_thread": "/v/video.mp4"}})
add("cache_video_from_bytes_async", {"data": "x", "io": {"to_thread": R("ValueError", "v")}})
add("cache_video_from_bytes_async", {"data": "x", "io": {"to_thread": R("GeneratorExit", "ge")}})
add("cache_video_from_bytes_async", {"data": "x", "io": {"to_thread": None}})
add("cache_video_from_bytes_async", {"data": "x", "io": {}})


# ── cache_document_from_bytes ──
def doc_fp(cache_dir, filename):
    safe = (PurePosixPath(filename).name if filename else "document").replace("\x00", "").strip()
    if not safe or safe in {".", ".."}:
        safe = "document"
    return str(PurePosixPath(cache_dir) / ("doc_%s_%s" % (H12, safe)))


add("cache_document_from_bytes", {"data": "d", "filename": "a/b.txt", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", "a/b.txt"), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_document_from_bytes", {"data": "d", "filename": "evil.txt", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": ["/elsewhere/doc_%s_evil.txt" % H12, "/d"], "is_relative_to": False}})
add("cache_document_from_bytes", {"data": "d", "filename": "", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", ""), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_document_from_bytes", {"data": "d", "filename": "..", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", ".."), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_document_from_bytes", {"data": "d", "filename": "  x.txt  ", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", "  x.txt  "), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_document_from_bytes", {"data": "d", "filename": "ok.txt", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", "ok.txt"), "/d"], "is_relative_to": True, "write_bytes": R("OSError", "w")}})
add("cache_document_from_bytes", {"data": "d", "filename": "ok.txt", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": R("ValueError", "r")}})
add("cache_document_from_bytes", {"data": "d", "filename": "ok.txt", "io": {}})

# ── cache_document_from_bytes_async ──
add("cache_document_from_bytes_async", {"data": "x", "filename": "f.txt", "io": {"to_thread": "/d/doc_f.txt"}})
add("cache_document_from_bytes_async", {"data": "x", "filename": "f.txt", "io": {"to_thread": R("ValueError", "v")}})
add("cache_document_from_bytes_async", {"data": "x", "filename": "", "io": {"to_thread": R("KeyboardInterrupt", "ki")}})
add("cache_document_from_bytes_async", {"data": "x", "filename": "f", "io": {"to_thread": None}})
add("cache_document_from_bytes_async", {"data": "x", "filename": "f", "io": {}})

# ── _resolve_media_ext (pure) ──
add("_resolve_media_ext", {"filename": "a.PNG", "mime_type": ""})
add("_resolve_media_ext", {"filename": "", "mime_type": "image/jpeg"})
add("_resolve_media_ext", {"filename": "a.tar.gz", "mime_type": ""})
add("_resolve_media_ext", {"filename": ".hidden", "mime_type": ""})
add("_resolve_media_ext", {"filename": "a.", "mime_type": ""})
add("_resolve_media_ext", {"filename": "C:\\x\\y.TXT", "mime_type": ""})
add("_resolve_media_ext", {"filename": "/.hidden", "mime_type": "video/mp4"})
add("_resolve_media_ext", {"filename": "", "mime_type": "application/x-unknown"})
add("_resolve_media_ext", {"filename": "noext", "mime_type": "TEXT/PLAIN"})

# ── cache_media_bytes ──
add("cache_media_bytes", {"data": "x", "filename": "p.png", "mime_type": "image/png", "io": dict(validate_inbound_media_size=None, looks_like_image=True, get_image_cache_dir="/i", **wr())})
add("cache_media_bytes", {"data": "x", "filename": "p.png", "mime_type": "image/png", "io": {"validate_inbound_media_size": None, "looks_like_image": False}})
add("cache_media_bytes", {"data": "x", "filename": "v.mp4", "mime_type": "video/mp4", "io": dict(validate_inbound_media_size=None, get_video_cache_dir="/v", **wr())})
add("cache_media_bytes", {"data": "x", "filename": "", "mime_type": "", "default_kind": "audio", "io": dict(validate_inbound_media_size=None, get_audio_cache_dir="/a", sniff_audio_ext=".ogg", **wr())})
add("cache_media_bytes", {"data": "x", "filename": "r.pdf", "mime_type": "", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", "r.pdf"), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_media_bytes", {"data": "x", "filename": "x.weird", "mime_type": "", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", "x.weird"), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_media_bytes", {"data": "x", "filename": "a/b?c.png", "mime_type": "image/png", "io": dict(validate_inbound_media_size=None, looks_like_image=True, get_image_cache_dir="/i", **wr())})
add("cache_media_bytes", {"data": "x", "filename": "v.mp4", "mime_type": "video/mp4", "io": {"validate_inbound_media_size": R("ValueError", "big")}})
add("cache_media_bytes", {"data": "x", "filename": "", "mime_type": "", "io": {"get_document_cache_dir": "/d", "uuid4_hex": UUID, "resolve": [doc_fp("/d", ""), "/d"], "is_relative_to": True, "write_bytes": None}})
add("cache_media_bytes", {"data": "x", "filename": "p.png", "mime_type": "image/png", "io": {}})

# ── cache_media_bytes_async ──
add("cache_media_bytes_async", {"data": "x", "filename": "f", "mime_type": "image/png", "default_kind": "image", "io": {"to_thread": {"path": "/p", "media_type": "image/png", "kind": "image", "display_name": "f"}}})
add("cache_media_bytes_async", {"data": "x", "filename": "f", "mime_type": "", "io": {"to_thread": None}})
add("cache_media_bytes_async", {"data": "x", "filename": "", "mime_type": "", "io": {"to_thread": R("ValueError", "v")}})
add("cache_media_bytes_async", {"data": "x", "filename": "f", "mime_type": "image/png", "default_kind": "image", "io": {"to_thread": R("KeyboardInterrupt", "ki")}})
add("cache_media_bytes_async", {"data": "x", "io": {}})

# ── _sqlite_files (pure) ──
for nm in ("state.db", "kanban.db", "", "a-b", "x.db"):
    add("_sqlite_files", {"name": nm})

# ── _docker_env_active ──
add("_docker_env_active", {"io": {"tenv": "docker"}})
add("_docker_env_active", {"io": {"tenv": " Docker "}})
add("_docker_env_active", {"io": {"tenv": "local"}})
add("_docker_env_active", {"io": {"tenv": ""}})
add("_docker_env_active", {"io": {"tenv": None}})
add("_docker_env_active", {"io": {}})

# ── _docker_persistent_active ──
add("_docker_persistent_active", {"io": {"tenv": ["docker", "true"]}})
add("_docker_persistent_active", {"io": {"tenv": ["docker", "0"]}})
add("_docker_persistent_active", {"io": {"tenv": ["local"]}})
add("_docker_persistent_active", {"io": {"tenv": ["docker", "YES"]}})
add("_docker_persistent_active", {"io": {"tenv": ["docker", None]}})
add("_docker_persistent_active", {"io": {}})

# ── _cache_dir_container_mounts ──
add("_cache_dir_container_mounts", {"io": {"tenv": "local"}})
add("_cache_dir_container_mounts", {"io": {"tenv": "docker", "get_cache_directory_mounts": script_list([{"host_path": "/h", "container_path": "/c"}])}})
add("_cache_dir_container_mounts", {"io": {"tenv": "docker", "get_cache_directory_mounts": R("KeyError", "host_path")}})
add("_cache_dir_container_mounts", {"cred_import_fail": True, "io": {"tenv": "docker"}})
add("_cache_dir_container_mounts", {"io": {"tenv": "docker", "get_cache_directory_mounts": script_list([{"container_path": "/c"}])}})
add("_cache_dir_container_mounts", {"io": {"tenv": "docker", "get_cache_directory_mounts": R("KeyboardInterrupt", "ki")}})
add("_cache_dir_container_mounts", {"io": {}})

# ── _media_delivery_recency_seconds ──
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": False}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": "30"}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": ""}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": "abc"}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": "-5"}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": "nan"}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": "1_000"}})
add("_media_delivery_recency_seconds", {"io": {"media_delivery_trust_recent": True, "media_delivery_trust_recent_seconds": None}})
add("_media_delivery_recency_seconds", {"policy_import_fail": True, "io": {}})
add("_media_delivery_recency_seconds", {"io": {}})

# ── _resolve_path ──
add("_resolve_path", {"path": "/a/b", "io": {"resolve": "/a/b"}})
add("_resolve_path", {"path": "/a/b", "io": {"resolve": R("OSError", "o")}})
add("_resolve_path", {"path": "/a/b", "io": {"resolve": R("ValueError", "v")}})
add("_resolve_path", {"path": "/a/b", "io": {"resolve": R("RuntimeError", "r")}})
add("_resolve_path", {"path": "/a/b", "io": {"resolve": R("TypeError", "t")}})
add("_resolve_path", {"path": "~/x", "expand": True, "io": {"expanduser": "/root/x", "resolve": "/root/x"}})
add("_resolve_path", {"path": "~/x", "expand": True, "io": {"expanduser": R("ValueError", "nul")}})
add("_resolve_path", {"path": "/a/b", "strict": True, "io": {"resolve": None}})
add("_resolve_path", {"path": "/a/b", "io": {"resolve": R("KeyboardInterrupt", "ki")}})
add("_resolve_path", {"path": "/a/b", "io": {}})

# ── _parse_docker_volume_mounts ──
add("_parse_docker_volume_mounts", {"io": {"tenv": ""}})
add("_parse_docker_volume_mounts", {"io": {"tenv": '["/h:/c"]', "expanduser": "/h", "resolve": "/h"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": '["C:/data:/workspace:ro"]', "expanduser": "C:/data", "resolve": "C:/data"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": '["vol:/c"]', "expanduser": "vol"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": "[1,]"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": "{}"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": "[1,2]"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": '["/h:/c"]', "expanduser": "/h", "resolve": None}})
add("_parse_docker_volume_mounts", {"io": {"tenv": '["/h:/c"]', "expanduser": R("ValueError", "nul")}})
add("_parse_docker_volume_mounts", {"io": {"tenv": "[\"/h:/c\n\"]"}})
add("_parse_docker_volume_mounts", {"io": {"tenv": '["/h://c"]', "expanduser": "/h", "resolve": "/h"}})
add("_parse_docker_volume_mounts", {"io": {}})

# ── _profile_dirs ──
add("_profile_dirs", {"hermes_root": "/r", "io": {"iterdir": script_list(["/r/profiles/a", "/r/profiles/b", "/r/profiles/c"]), "is_dir": [True, False, True]}})
add("_profile_dirs", {"hermes_root": "/r", "io": {"iterdir": R("OSError", "no")}})
add("_profile_dirs", {"hermes_root": "/r", "io": {"iterdir": script_list(["/r/profiles/a", "/r/profiles/b"]), "is_dir": [True, R("OSError", "mid")]}})
add("_profile_dirs", {"hermes_root": "/r", "io": {"iterdir": script_list(["/r/profiles/a"]), "is_dir": [R("ValueError", "v")]}})
add("_profile_dirs", {"hermes_root": "/r", "io": {"iterdir": script_list([])}})
add("_profile_dirs", {"hermes_root": "/r", "io": {"iterdir": R("KeyboardInterrupt", "ki")}})
add("_profile_dirs", {"hermes_root": "/r", "io": {}})

# ── _credential_home_roots ──
add("_credential_home_roots", {"hermes_root": "/r", "io": {"get_hermes_home": "/r", "iterdir": script_list([])}})
add("_credential_home_roots", {"hermes_root": "/r", "io": {"get_hermes_home": "/home", "iterdir": script_list(["/r/profiles/a"]), "is_dir": [True]}})
add("_credential_home_roots", {"hermes_root": "/r", "io": {"get_hermes_home": "/home", "iterdir": R("OSError", "no")}})
add("_credential_home_roots", {"hermes_root": "/r", "io": {"get_hermes_home": R("RuntimeError", "h")}})
add("_credential_home_roots", {"hermes_root": "/r", "io": {"get_hermes_home": "/home", "iterdir": script_list(["/r", "/r/profiles/a"]), "is_dir": [True, True]}})
add("_credential_home_roots", {"hermes_root": "/r", "io": {}})

# ── _existing_regular_file ──
add("_existing_regular_file", {"raw": "/x", "io": {"expanduser": "/x", "is_file": True}})
add("_existing_regular_file", {"raw": "/x", "io": {"expanduser": "/x", "is_file": False}})
add("_existing_regular_file", {"raw": "~/x", "io": {"expanduser": R("ValueError", "nul")}})
add("_existing_regular_file", {"raw": "/x", "io": {"expanduser": "/x", "is_file": R("OSError", "o")}})
add("_existing_regular_file", {"raw": "/x", "io": {"expanduser": "/x", "is_file": R("TypeError", "t")}})
add("_existing_regular_file", {"raw": "/x", "io": {}})

# ── _mask_media_scan_text ──
add("_mask_media_scan_text", {"text": "hello", "io": {"mask_protected_spans": "hello", "mask_json_string_media": "hello", "terminal_sentinel_start": -1}})
add("_mask_media_scan_text", {"text": "hi <|eos|>  ", "io": {"mask_protected_spans": "hi <|eos|>  ", "mask_json_string_media": "hi <|eos|>  ", "terminal_sentinel_start": 3, "blank_spans": "hi         "}})
add("_mask_media_scan_text", {"text": "t", "io": {"mask_protected_spans": R("ValueError", "m")}})
add("_mask_media_scan_text", {"text": "t", "io": {"mask_protected_spans": "t", "mask_json_string_media": "t", "terminal_sentinel_start": R("ValueError", "s")}})
add("_mask_media_scan_text", {"text": "t", "io": {"mask_protected_spans": "t", "mask_json_string_media": "t", "terminal_sentinel_start": 0, "blank_spans": R("OSError", "b")}})
add("_mask_media_scan_text", {"text": "t", "io": {"mask_protected_spans": R("KeyboardInterrupt", "ki")}})
add("_mask_media_scan_text", {"text": "t", "io": {}})

# ── _session_task_is_stale ──
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": {"done": {"fn": 1}}}}, "session_key": "k", "io": {"task_done": True}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": {"done": {"fn": 1}}}}, "session_key": "k", "io": {"task_done": False}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": None}}, "session_key": "k", "io": {}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": {}}}, "session_key": "k", "io": {}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": {"done": None}}}, "session_key": "k", "io": {}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"other": {"done": {"fn": 1}}}}, "session_key": "k", "io": {}})
add("_session_task_is_stale", {"self": {}, "session_key": "k", "io": {}})
add("_session_task_is_stale", {"self": {"_session_tasks": None}, "session_key": "k", "io": {}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": {"done": {"fn": 1}}}}, "session_key": "k", "io": {"task_done": R("RuntimeError", "d")}})
add("_session_task_is_stale", {"self": {"_session_tasks": {"k": {"done": {"fn": 1}}}}, "session_key": "k", "io": {}})

# ── truncate_message ──
add("truncate_message", {"content": "long text", "max_length": 10, "io": {"chunk_markdown_message": script_list(["long", "text"])}})
add("truncate_message", {"content": "abc", "max_length": 4096, "len_fn": {"fn": 1}, "io": {"chunk_markdown_message": script_list(["abc"])}})
add("truncate_message", {"content": "abc", "io": {"chunk_markdown_message": R("ValueError", "c")}})
add("truncate_message", {"content": "abc", "chunking_import_fail": True, "io": {}})
add("truncate_message", {"content": "abc", "io": {"chunk_markdown_message": R("KeyboardInterrupt", "ki")}})
add("truncate_message", {"content": "abc", "io": {}})

# ── supports_exec_approval_buttons (pure identity token) ──
add("supports_exec_approval_buttons", {"override": False})
add("supports_exec_approval_buttons", {"override": True})
add("supports_exec_approval_buttons", {})
add("supports_exec_approval_buttons", {"override": False})
add("supports_exec_approval_buttons", {"override": True})


def main():
    # cross-check: every ported func has >=5 cases; func set matches the port.
    expected = {
        "_secure_media_cache_dir", "_write_cache_file",
        "cache_image_from_bytes", "cache_image_from_bytes_async",
        "cache_audio_from_bytes", "cache_audio_from_bytes_async",
        "cache_video_from_bytes", "cache_video_from_bytes_async",
        "cache_document_from_bytes", "cache_document_from_bytes_async",
        "_resolve_media_ext", "cache_media_bytes", "cache_media_bytes_async",
        "_sqlite_files", "_docker_env_active", "_docker_persistent_active",
        "_cache_dir_container_mounts", "_media_delivery_recency_seconds",
        "_resolve_path", "_parse_docker_volume_mounts", "_profile_dirs",
        "_credential_home_roots", "_existing_regular_file", "_mask_media_scan_text",
        "_session_task_is_stale", "truncate_message", "supports_exec_approval_buttons",
    }
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    assert set(counts) == expected, (set(counts) ^ expected)
    for fn in expected:
        assert counts[fn] >= 5, (fn, counts[fn])
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))


if __name__ == "__main__":
    main()