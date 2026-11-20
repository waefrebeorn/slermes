#!/usr/bin/env python3
"""Generate tests/fixtures/platforms_base_flow3.jsonl for the platforms_base_flow3
FLOW oracle. >=5 cases per ported func, exercising decision branches, short-circuit
order, exception fallback/suppress/re-raise, BaseException passthrough, mutation
echo, and FAIL-CLOSED (missing-script) paths.

Seam scripts follow the universal-seam semantics: io[name] is a single value
reused, a bare array consumed per call (last repeats), or {"__script__":true,
"ret":X} for a constant LIST result. Paths are clean absolute POSIX; regex matches
are scripted {start,end,path} records. ASCII only.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "platforms_base_flow3.jsonl")


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── _cache_dir_accessors (factory; no seams) ──
for kind in ("image", "audio", "video", "document", "screenshot"):
    add("_cache_dir_accessors", kind=kind, io={})

# ── get_dir ──
GD = dict(new_subpath="cache/images", old_name="image_cache", constant_name="IMAGE_CACHE_DIR")
add("get_dir", **GD, constant_value=None,
    import_defaults={"IMAGE_CACHE_DIR": "/root/.hermes/cache/images"},
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None})
add("get_dir", **GD, constant_value="/custom/img",
    import_defaults={"IMAGE_CACHE_DIR": "/root/.hermes/cache/images"},
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None})
add("get_dir", **GD, constant_value="/root/.hermes/cache/images",
    import_defaults={"IMAGE_CACHE_DIR": "/root/.hermes/cache/images"},
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None})
add("get_dir", **GD, constant_value="/custom/img", import_defaults={},
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None})
add("get_dir", **GD, constant_value=None, import_defaults={},
    io={"get_hermes_dir": "/root/.hermes/cache/images",
        "secure_media_cache_dir": raise_spec("OSError", "chmod failed")})
add("get_dir", **GD, constant_value=None, import_defaults={},
    io={"get_hermes_dir": raise_spec("ValueError", "bad dir"), "secure_media_cache_dir": None})
add("get_dir", **GD, constant_value=None, import_defaults={}, io={})  # fail-closed

# ── cleanup ──
CL = dict(GD)
add("cleanup", **CL, constant_value=None, import_defaults={}, max_age_hours=24,
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None,
        "unlink_files_older_than": 3})
add("cleanup", **CL, constant_value=None, import_defaults={}, max_age_hours=1,
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None,
        "unlink_files_older_than": 0})
add("cleanup", **CL, constant_value="/custom/img",
    import_defaults={"IMAGE_CACHE_DIR": "/root/.hermes/cache/images"}, max_age_hours=2,
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None,
        "unlink_files_older_than": 7})
add("cleanup", **CL, constant_value=None, import_defaults={}, max_age_hours=24,
    io={"get_hermes_dir": "/root/.hermes/cache/images", "secure_media_cache_dir": None,
        "unlink_files_older_than": raise_spec("OSError", "unlink failed")})
add("cleanup", **CL, constant_value=None, import_defaults={}, max_age_hours=24,
    io={"get_hermes_dir": raise_spec("RuntimeError", "x"), "secure_media_cache_dir": None,
        "unlink_files_older_than": 3})
add("cleanup", **CL, constant_value=None, import_defaults={}, max_age_hours=24, io={})

# ── validate_media_delivery_path ──
VM = dict(path="/a.png", session_key="s1")
add("validate_media_delivery_path", **VM, io={"host_feature_enabled": False})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": None})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png",
        "expanduser": raise_spec("ValueError", "embedded null byte")})
add("validate_media_delivery_path", path="x.png", session_key="s1",
    io={"host_feature_enabled": True, "normalize_media_tag_path": "x.png", "expanduser": "x.png"})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": "/a.png", "is_file": False})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": "/a.png", "is_file": True, "is_project_env_basename": True})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": "/a.png", "is_file": True, "is_project_env_basename": False,
        "allowed_roots": script(["/cache"]), "resolve_path": "/cache", "path_is_within": True})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": None, "resolve_path": "/a.png", "is_file": True,
        "is_project_env_basename": False, "allowed_roots": script([]),
        "media_delivery_strict": False, "path_under_denied_prefix": False})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": "/a.png", "is_file": True, "is_project_env_basename": False,
        "allowed_roots": script([]), "media_delivery_strict": False, "path_under_denied_prefix": True})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": "/a.png", "is_file": True, "is_project_env_basename": False,
        "allowed_roots": script([]), "media_delivery_strict": True, "recency_seconds": 0})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png", "expanduser": "/a.png",
        "translate_docker": "/a.png", "is_file": True, "is_project_env_basename": False,
        "allowed_roots": script([]), "media_delivery_strict": True, "recency_seconds": 100,
        "path_under_denied_prefix": False, "file_is_recently_produced": True})
add("validate_media_delivery_path", **VM, hosted_policy_import_fail=True, io={})
add("validate_media_delivery_path", **VM,
    io={"host_feature_enabled": True, "normalize_media_tag_path": "/a.png",
        "expanduser": raise_spec("SystemExit", "bye")})  # BaseException passthrough
add("validate_media_delivery_path", **VM, io={})  # fail-closed

# ── _validated_delivery_path ──
VD = dict(raw_path="/raw.png", session_key="s1", label="MEDIA directive path")
add("_validated_delivery_path", **VD, dropped=None,
    io={"validate_media_delivery_path": "/safe.png"})
add("_validated_delivery_path", **VD, dropped=None,
    io={"validate_media_delivery_path": None, "fetch_remote_media": "/remote.png"})
add("_validated_delivery_path", **VD, dropped=None,
    io={"validate_media_delivery_path": None, "fetch_remote_media": None,
        "existing_regular_file": False, "log_safe_path": "/raw.png", "log_warning": None})
add("_validated_delivery_path", **VD, dropped=[],
    io={"validate_media_delivery_path": None, "fetch_remote_media": None,
        "existing_regular_file": True, "log_safe_path": "/raw.png", "log_warning": None})
add("_validated_delivery_path", **VD, dropped=[{"path": "/old", "reason": "prior"}],
    io={"validate_media_delivery_path": None, "fetch_remote_media": None,
        "existing_regular_file": False, "log_safe_path": "/raw.png", "log_warning": None})
add("_validated_delivery_path", **VD, dropped=None, media_fetch_import_fail=True,
    io={"validate_media_delivery_path": None})
add("_validated_delivery_path", **VD, dropped=None,
    io={"validate_media_delivery_path": raise_spec("RuntimeError", "boom")})
add("_validated_delivery_path", **VD, dropped=None, io={})  # fail-closed

# ── _deliverable_tag_spans ──
add("_deliverable_tag_spans", text="hello",
    io={"mask_media_scan_text": "masked", "real_media_tag_spans": script([[0, 5]]),
        "terminal_sentinel_start": -1})
add("_deliverable_tag_spans", text="hello   ",
    io={"mask_media_scan_text": "masked", "real_media_tag_spans": script([[0, 5]]),
        "terminal_sentinel_start": 3})
add("_deliverable_tag_spans", text="hello",
    io={"mask_media_scan_text": "masked", "real_media_tag_spans": script([]),
        "terminal_sentinel_start": 3})
add("_deliverable_tag_spans", text="ab",
    io={"mask_media_scan_text": "masked", "real_media_tag_spans": script([[0, 5], [6, 9]]),
        "terminal_sentinel_start": 0})
add("_deliverable_tag_spans", text="hello",
    io={"mask_media_scan_text": raise_spec("RuntimeError", "mask"), "real_media_tag_spans": script([]),
        "terminal_sentinel_start": -1})
add("_deliverable_tag_spans", text="hello", io={})  # fail-closed

# ── _real_media_tag_spans ──
add("_real_media_tag_spans", masked="m",
    io={"cleanup_finditer": script([{"start": 0, "end": 5, "path": "/a.png"}]),
        "extensionless_media_matches": script([])})
add("_real_media_tag_spans", masked="m",
    io={"cleanup_finditer": script([]),
        "extensionless_media_matches": script([[{"start": 2, "end": 6, "path": "/b"}, "/safe", 8]])})
add("_real_media_tag_spans", masked="m",
    io={"cleanup_finditer": script([{"start": 0, "end": 5, "path": "/a.png"}]),
        "extensionless_media_matches": script([[{"start": 2, "end": 6, "path": "/b"}, "/s", 9]])})
add("_real_media_tag_spans", masked="m",
    io={"cleanup_finditer": raise_spec("ValueError", "re"), "extensionless_media_matches": script([])})
add("_real_media_tag_spans", masked="m",
    io={"cleanup_finditer": script([]), "extensionless_media_matches": script([])})
add("_real_media_tag_spans", masked="m", io={})  # fail-closed

# ── _extensionless_media_matches ──
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([]),
        "extensionless_finditer": script([{"start": 0, "end": 6, "path": "/Caddyfile"}]),
        "normalize_media_tag_path": "/Caddyfile", "path_lacks_deliverable_extension": True,
        "match_extensionless_path": script(["/safe/Caddyfile", 6])})
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([{"start": 0, "end": 10, "path": "/a.png"}]),
        "normalize_media_tag_path": ["/a.png", "/b"], "existing_regular_file": True,
        "extensionless_finditer": script([{"start": 2, "end": 6, "path": "/b"}]),
        "path_lacks_deliverable_extension": True})
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([]),
        "extensionless_finditer": script([{"start": 0, "end": 6, "path": "/x"}]),
        "normalize_media_tag_path": None})
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([]),
        "extensionless_finditer": script([{"start": 0, "end": 6, "path": "/a.png"}]),
        "normalize_media_tag_path": "/a.png", "path_lacks_deliverable_extension": False})
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([]),
        "extensionless_finditer": script([{"start": 0, "end": 6, "path": "/Caddyfile"}]),
        "normalize_media_tag_path": "/Caddyfile", "path_lacks_deliverable_extension": True,
        "match_extensionless_path": None})
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([{"start": 0, "end": 10, "path": "MEDIA:/a.png"}]),
        "extensionless_finditer": script([{"start": 0, "end": 6, "path": "/C"}]),
        "normalize_media_tag_path": "/C", "path_lacks_deliverable_extension": True,
        "match_extensionless_path": script(["/s", 6])})
add("_extensionless_media_matches", masked="m",
    io={"cleanup_finditer": script([{"start": 0, "end": 10, "path": "/a.png"}]),
        "normalize_media_tag_path": ["/a.png", "/C"], "existing_regular_file": False,
        "extensionless_finditer": script([{"start": 0, "end": 6, "path": "/C"}]),
        "path_lacks_deliverable_extension": True, "match_extensionless_path": script(["/s", 6])})
add("_extensionless_media_matches", masked="m", io={})  # fail-closed

# ── _synthesize_auto_tts ──
add("_synthesize_auto_tts", name="telegram", text_content="hi", source=None,
    io={"synthesize_auto_tts": script(["/a.ogg", "/req.ogg"])})
add("_synthesize_auto_tts", name="discord", text_content="yo", source={"chat_id": "c1"},
    io={"synthesize_auto_tts": script(["/b.ogg", None])})
add("_synthesize_auto_tts", name="telegram", text_content="hi", source=None,
    auto_tts_import_fail=True, io={})
add("_synthesize_auto_tts", name="telegram", text_content="hi", source=None,
    io={"synthesize_auto_tts": raise_spec("TimeoutError", "tts timed out")})
add("_synthesize_auto_tts", name="telegram", text_content="hi", source=None,
    io={"synthesize_auto_tts": None})
add("_synthesize_auto_tts", name="telegram", text_content="hi", source=None, io={})  # fail-closed

# ── _record_delivery (no seams) ──
add("_record_delivery", state={"delivery_attempted": False, "delivery_succeeded": False}, result=None)
add("_record_delivery", state={"delivery_attempted": False, "delivery_succeeded": False},
    result={"success": True})
add("_record_delivery", state={"delivery_attempted": False, "delivery_succeeded": False},
    result={"success": False})
add("_record_delivery", state={"delivery_attempted": False, "delivery_succeeded": False}, result={})
add("_record_delivery", state={"delivery_attempted": True, "delivery_succeeded": True},
    result={"success": False})
add("_record_delivery", state={}, result={"success": True})

# ── _opt (no seams) ──
for v in ("x", "", None, 0, 5, True, False):
    add("_opt", value=v)

# ── _extract_response_content ──
SRC = {"chat_id": "c1"}
FOOT = script({"text_content": "hi", "final": True})
add("_extract_response_content", response="hi", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram",
    io={"media_delivery_scope_enter": None, "extract_media": script([[], "hi"]),
        "filter_media_delivery_paths": script([]), "extract_images": script([[], "hi"]),
        "strip_media_directives": "hi", "hand_back_unreadable_media": None,
        "media_delivery_scope_exit": None, "footer_onto_photo_caption": FOOT})
add("_extract_response_content", response="MEDIA:/a.png [[as_document]]", source=SRC,
    session_key="s1", is_ephemeral_response=False, name="telegram",
    io={"media_delivery_scope_enter": None,
        "extract_media": script([[["/a.png", False]], "cleaned"]),
        "filter_media_delivery_paths": script([["/a.png", False]]),
        "extract_images": script([["img1"], "text"]),
        "strip_media_directives": "text", "log_info": None,
        "extract_local_files": script([["/loc.pdf"], "text2"]),
        "filter_local_delivery_paths": script(["/loc.pdf"]),
        "hand_back_unreadable_media": None, "media_delivery_scope_exit": None,
        "bounded_history": script(["/loc.pdf"]),
        "footer_onto_photo_caption": script({"ok": True})})
add("_extract_response_content", response="MEDIA:/x.png", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram",
    io={"media_delivery_scope_enter": None, "extract_media": script([[], ""]),
        "filter_media_delivery_paths": script([]), "extract_images": script([[], ""]),
        "strip_media_directives": ["", "recovered"], "hand_back_unreadable_media": None,
        "media_delivery_scope_exit": None, "log_warning": None,
        "footer_onto_photo_caption": script({"recovered": True})})
add("_extract_response_content", response="hi", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram",
    io={"media_delivery_scope_enter": raise_spec("RuntimeError", "scope")})
add("_extract_response_content", response="hi", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram",
    io={"media_delivery_scope_enter": None, "extract_media": script([[], "hi"]),
        "filter_media_delivery_paths": script([]), "extract_images": script([[], "hi"]),
        "strip_media_directives": "hi", "hand_back_unreadable_media": raise_spec("ValueError", "x"),
        "media_delivery_scope_exit": True, "footer_onto_photo_caption": script("F")})
add("_extract_response_content", response="hi", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram",
    io={"media_delivery_scope_enter": None, "extract_media": raise_spec("ValueError", "x"),
        "media_delivery_scope_exit": False})
add("_extract_response_content", response="hi", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram", docker_media_import_fail=True,
    io={"media_delivery_scope_enter": None, "extract_media": script([[], "hi"]),
        "filter_media_delivery_paths": script([]), "extract_images": script([[], "hi"]),
        "strip_media_directives": "hi", "media_delivery_scope_exit": False})
add("_extract_response_content", response="hi", source=SRC, session_key="s1",
    is_ephemeral_response=True, name="telegram", io={})  # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-32s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "func with <5 cases"
    assert len(counts) == 12, "expected 12 funcs"


if __name__ == "__main__":
    main()