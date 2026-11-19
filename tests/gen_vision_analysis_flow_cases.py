#!/usr/bin/env python3
"""Generate tests/fixtures/vision_analysis_flow.jsonl for the vision-analysis
FLOW + leaf port. Each case is {"func": <live name>, "in": {..., "io": {...}}}.

Script semantics (shared with the harness dispatcher + oracle Ctx):
- a plain JSON value is returned verbatim on every call;
- a LIST is a queue consumed one element per call;
- {"__script__": true, "ret": X} returns X (wrap list-valued returns so they
  are not consumed as a queue);
- {"__script__": true, "raise": {"type","message"}} raises that exception.
A missing/exhausted script is a fail-closed RuntimeError on both sides.

Fixtures are ASCII; no integral-float literals in str()-computed positions and
no non-finite floats. >=5 cases per function.
"""
import json
import os
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "vision_analysis_flow.jsonl"


def R(type_, msg=""):
    return {"__script__": True, "raise": {"type": type_, "message": msg}}


def S(ret):
    return {"__script__": True, "ret": ret}


CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── leaves ──────────────────────────────────────────────────────────────────
for prompt, part, url in [
    ("describe this", "image_url", "data:image/png;base64,AAA"),
    ("what happens", "video_url", "data:video/mp4;base64,BBB"),
    ("", "image_url", ""),
    ("a" * 5, "image_url", "data:image/jpeg;base64,CCC"),
    ("unicode-free question?", "image_url", "data:image/png;base64,DDD"),
]:
    case("_media_messages", user_prompt=prompt, part_type=part, data_url=url)

case("_debug_call_data", kind="image", source="http://x/a.png", user_prompt="short", model="m1")
case("_debug_call_data", kind="video", source="/v.mp4", user_prompt="p" * 250, model=None)
case("_debug_call_data", kind="image", source="s", user_prompt="x" * 200, model="m2")
case("_debug_call_data", kind="image", source="s", user_prompt="x" * 201, model=None)
case("_debug_call_data", kind="video", source="s", user_prompt="", model="m3")

for n in [0, 100000, 524288, 1048576, 1572864]:
    case("_too_large_message", image_data_url="a" * n)

for suf in [".flv", ".wmv", "", ".MP4", ".xyz"]:
    case("_unsupported_video_format", suffix=suf)

case("_build_native_vision_tool_result", image_url="http://x/a.png", question="what is this?",
     image_data_url="data:image/png;base64,AAA", image_size_bytes=2048, scale_note=None)
case("_build_native_vision_tool_result", image_url="http://x/a.png", question="   ",
     image_data_url="data:img", image_size_bytes=1024, scale_note="downscaled")
case("_build_native_vision_tool_result", image_url="http://x/" + "a" * 300, question="q",
     image_data_url="data:img", image_size_bytes=0, scale_note=None)
case("_build_native_vision_tool_result", image_url="u", question=None,
     image_data_url="d", image_size_bytes=1536, scale_note="note here")
case("_build_native_vision_tool_result", image_url="u", question="  padded  ",
     image_data_url="d", image_size_bytes=10240, scale_note="")
case("_build_native_vision_tool_result", image_url="u", question="q",
     image_data_url="d", image_size_bytes=512, scale_note="crop offset")

# ── _cfg_auxiliary ──────────────────────────────────────────────────────────
case("_cfg_auxiliary", keys=["vision", "timeout"], default=None,
     io={"load_config": {"cfg": 1}, "cfg_get": "val"})
case("_cfg_auxiliary", keys=["vision"], default="dflt",
     io={"load_config": R("RuntimeError", "no config"), "cfg_get": "x"})
case("_cfg_auxiliary", keys=["vision"], default="dflt",
     io={"load_config": {"cfg": 1}, "cfg_get": R("ValueError", "bad")})
case("_cfg_auxiliary", keys=["vision"], default="dflt",
     io={"load_config": R("SystemExit", "bye"), "cfg_get": "x"})
case("_cfg_auxiliary", keys=["vision"], default="dflt", io={})
case("_cfg_auxiliary", keys=["vision"], default=None,
     io={"load_config": {"cfg": 1}, "cfg_get": R("KeyboardInterrupt", "ki")})

# ── _read_vision_setting ────────────────────────────────────────────────────
case("_read_vision_setting", env_var="E", key="k", cast="int", minimum=None,
     io={"os.getenv": "5", "cast": 5})
case("_read_vision_setting", env_var="E", key="k", cast="int", minimum=None,
     io={"os.getenv": "", "cast": [R("TypeError", "None"), 7], "cfg_auxiliary": "7"})
case("_read_vision_setting", env_var="E", key="k", cast="int", minimum=1,
     io={"os.getenv": "0", "cast": [0, 3], "cfg_auxiliary": "3"})
case("_read_vision_setting", env_var="E", key="k", cast="int", minimum=None,
     io={"os.getenv": "abc", "cast": [R("ValueError", "bad"), 2], "cfg_auxiliary": "2"})
case("_read_vision_setting", env_var="E", key="k", cast="float", minimum=None,
     io={"os.getenv": "", "cast": [R("TypeError", "None"), R("TypeError", "None")], "cfg_auxiliary": None})
case("_read_vision_setting", env_var="E", key="k", cast="int", minimum=None, io={})
case("_read_vision_setting", env_var="E", key="k", cast="int", minimum=None,
     io={"os.getenv": "5", "cast": R("SystemExit", "bye")})

# ── _managed_install ────────────────────────────────────────────────────────
case("_managed_install", io={"is_managed": True})
case("_managed_install", io={"is_managed": False})
case("_managed_install", io={"is_managed": 0})
case("_managed_install", io={"is_managed": R("RuntimeError", "x")})
case("_managed_install", io={"is_managed": R("SystemExit", "bye")})
case("_managed_install", io={})

# ── _secure_cache_dir ───────────────────────────────────────────────────────
case("_secure_cache_dir", new_subpath="cache/vision", old_name="temp_vision_images",
     io={"get_hermes_dir": "/h/cache/vision", "managed_install": True, "path.mkdir": None})
case("_secure_cache_dir", new_subpath="cache/vision", old_name="t",
     io={"get_hermes_dir": "/h/cv", "managed_install": False, "make_user_dir": None, "secure_dir": None})
case("_secure_cache_dir", new_subpath="cache/vision", old_name="t",
     io={"get_hermes_dir": "/h/cv", "managed_install": False, "make_user_dir": None,
         "secure_dir": R("OSError", "chmod fail"), "logger.debug": None})
case("_secure_cache_dir", new_subpath="cache/vision", old_name="t",
     io={"get_hermes_dir": "/h/cv", "managed_install": False, "make_user_dir": None,
         "secure_dir": R("SystemExit", "bye")})
case("_secure_cache_dir", new_subpath="cache/vision", old_name="t",
     io={"get_hermes_dir": R("RuntimeError", "no dir")})
case("_secure_cache_dir", new_subpath="cache/vision", old_name="t",
     io={"get_hermes_dir": "/h/cv"})
case("_secure_cache_dir", new_subpath="cache/vision", old_name="t",
     io={"get_hermes_dir": "/h/cv", "managed_install": False, "make_user_dir": R("OSError", "mk fail")})

# ── _write_private_bytes ────────────────────────────────────────────────────
case("_write_private_bytes", path="/h/f.bin", data="abc",
     io={"os.open": 3, "os.fdopen": "handle", "handle.write": None, "os.fdopen.__exit__": None})
case("_write_private_bytes", path="/h/f.bin", data="abc",
     io={"os.open": R("OSError", "open fail"), "path.write_bytes": None})
case("_write_private_bytes", path="/h/f.bin", data="abc",
     io={"os.open": R("ValueError", "bad")})
case("_write_private_bytes", path="/h/f.bin", data="abc",
     io={"os.open": 3, "os.fdopen": "handle", "handle.write": R("OSError", "write fail")})
case("_write_private_bytes", path="/h/f.bin", data="abc", io={})
case("_write_private_bytes", path="/h/f.bin", data="abc",
     io={"os.open": 3, "os.fdopen": "handle", "handle.write": None, "os.fdopen.__exit__": R("OSError", "close")})

# ── _import_pillow_for_resize ───────────────────────────────────────────────
case("_import_pillow_for_resize", io={"import_pillow": "ImageMarker"})
case("_import_pillow_for_resize",
     io={"import_pillow": [R("ImportError", "no PIL"), "ImageMarker2"], "ensure_import": None})
case("_import_pillow_for_resize",
     io={"import_pillow": R("ImportError", "no PIL"), "ensure_import": R("RuntimeError", "install fail")})
case("_import_pillow_for_resize",
     io={"import_pillow": [R("ImportError", "no PIL"), R("ImportError", "still no")], "ensure_import": None})
case("_import_pillow_for_resize", io={"import_pillow": R("ValueError", "bad")})
case("_import_pillow_for_resize", io={})
case("_import_pillow_for_resize",
     io={"import_pillow": R("ImportError", "no PIL"), "ensure_import": R("SystemExit", "bye")})

# ── _accepts_tool_result_images ─────────────────────────────────────────────
case("_accepts_tool_result_images", provider="anthropic", model="m", cfg=None,
     io={"profile_rejects_tool_media": True})
case("_accepts_tool_result_images", provider="anthropic", model="m", cfg=None,
     io={"profile_rejects_tool_media": False, "supports_media_in_tool_results": True})
case("_accepts_tool_result_images", provider="custom", model="m", cfg={"a": 1},
     io={"profile_rejects_tool_media": False, "supports_media_in_tool_results": False, "lookup_supports_vision": True})
case("_accepts_tool_result_images", provider="custom", model="m", cfg=None,
     io={"profile_rejects_tool_media": False, "supports_media_in_tool_results": False, "lookup_supports_vision": 1})
case("_accepts_tool_result_images", provider="custom", model="m", cfg=None,
     io={"profile_rejects_tool_media": False, "supports_media_in_tool_results": False, "lookup_supports_vision": None})
case("_accepts_tool_result_images", provider="p", model="m", cfg=None,
     io={"profile_rejects_tool_media": R("RuntimeError", "x")})
case("_accepts_tool_result_images", provider="p", model="m", cfg=None, io={})

# ── _native_tool_result_images ──────────────────────────────────────────────
case("_native_tool_result_images", provider="p", model="m", cfg=None,
     io={"decide_image_input_mode": "native", "accepts_tool_result_images": True})
case("_native_tool_result_images", provider="p", model="m", cfg=None,
     io={"decide_image_input_mode": "aux"})
case("_native_tool_result_images", provider="p", model="m", cfg=None,
     io={"decide_image_input_mode": "native", "accepts_tool_result_images": False})
case("_native_tool_result_images", provider="p", model="m", cfg=None,
     io={"decide_image_input_mode": R("RuntimeError", "x")})
case("_native_tool_result_images", provider="p", model="m", cfg=None, io={})
case("_native_tool_result_images", provider="p", model="m", cfg=None,
     io={"decide_image_input_mode": "native", "accepts_tool_result_images": R("ValueError", "x")})

# ── _should_use_native_vision_fast_path ─────────────────────────────────────
case("_should_use_native_vision_fast_path",
     io={"read_main_provider": "anthropic", "read_main_model": "m", "load_config": {"c": 1},
         "native_tool_result_images": True})
case("_should_use_native_vision_fast_path",
     io={"read_main_provider": R("RuntimeError", "x"), "logger.debug": None})
case("_should_use_native_vision_fast_path",
     io={"read_main_provider": "p", "read_main_model": "m", "load_config": R("ValueError", "x"), "logger.debug": None})
case("_should_use_native_vision_fast_path",
     io={"read_main_provider": "p", "read_main_model": "m", "load_config": {},
         "native_tool_result_images": R("RuntimeError", "x"), "logger.debug": None})
case("_should_use_native_vision_fast_path",
     io={"read_main_provider": R("SystemExit", "bye")})
case("_should_use_native_vision_fast_path", io={"logger.debug": None})

# ── _unlink_quietly ─────────────────────────────────────────────────────────
case("_unlink_quietly", path=None)
case("_unlink_quietly", path="/h/f.img", io={"path.unlink": None})
case("_unlink_quietly", path="/h/f.img", io={"path.unlink": R("OSError", "x")})
case("_unlink_quietly", path="/h/f.img", io={"path.unlink": R("SystemExit", "bye")})
case("_unlink_quietly", path="/h/f.img", io={})
case("_unlink_quietly", path="", io={"path.unlink": None})

# ── _aux_call_kwargs ────────────────────────────────────────────────────────
case("_aux_call_kwargs", messages=[{"role": "user"}], model="m", default_timeout=120.0, min_timeout=None,
     io={"cfg_auxiliary": {}})
case("_aux_call_kwargs", messages=[], model="m", default_timeout=120.0, min_timeout=None,
     io={"cfg_auxiliary": {"timeout": 60}})
case("_aux_call_kwargs", messages=[], model="m", default_timeout=120.0, min_timeout=180.0,
     io={"cfg_auxiliary": {"timeout": 10}})
case("_aux_call_kwargs", messages=[], model="m", default_timeout=120.0, min_timeout=None,
     io={"cfg_auxiliary": {"temperature": 0.5}})
case("_aux_call_kwargs", messages=[], model="m", default_timeout=120.0, min_timeout=None,
     io={"cfg_auxiliary": {"timeout": "abc"}})
case("_aux_call_kwargs", messages=[], model=None, default_timeout=180.0, min_timeout=180.0,
     io={"cfg_auxiliary": R("RuntimeError", "x")})
case("_aux_call_kwargs", messages=[], model="m", default_timeout=120.0, min_timeout=None,
     io={"cfg_auxiliary": None})
case("_aux_call_kwargs", messages=[], model="m", default_timeout=120.0, min_timeout=None,
     io={"cfg_auxiliary": {"timeout": 30, "temperature": 0.9}})

# ── _configured_aux_model ───────────────────────────────────────────────────
case("_configured_aux_model", sections=["vision"], env_vars=["AUXILIARY_VISION_MODEL"],
     io={"cfg_auxiliary": "m1"})
case("_configured_aux_model", sections=["vision"], env_vars=["AUXILIARY_VISION_MODEL"],
     io={"cfg_auxiliary": "   ", "os.getenv": "m2"})
case("_configured_aux_model", sections=["video", "vision"], env_vars=["A", "B"],
     io={"cfg_auxiliary": [None, "m3"]})
case("_configured_aux_model", sections=["vision"], env_vars=["A", "B"],
     io={"cfg_auxiliary": None, "os.getenv": ["", "m4"]})
case("_configured_aux_model", sections=["vision"], env_vars=["A"],
     io={"cfg_auxiliary": None, "os.getenv": ""})
case("_configured_aux_model", sections=["vision"], env_vars=["A"],
     io={"cfg_auxiliary": "  m5  "})
case("_configured_aux_model", sections=["vision"], env_vars=["A"], io={})

# ── _load_auxiliary_client ──────────────────────────────────────────────────
case("_load_auxiliary_client", state={"async_call_llm": None, "extract_content_or_reasoning": None},
     io={"aux.async_call_llm": "ACL", "aux.extract_content_or_reasoning": "ECR"})
case("_load_auxiliary_client", state={"async_call_llm": "ACL", "extract_content_or_reasoning": None},
     io={"aux.extract_content_or_reasoning": "ECR"})
case("_load_auxiliary_client", state={"async_call_llm": "ACL", "extract_content_or_reasoning": "ECR"}, io={})
case("_load_auxiliary_client", state={"async_call_llm": None, "extract_content_or_reasoning": None},
     io={"aux.async_call_llm": R("RuntimeError", "x")})
case("_load_auxiliary_client", state={"async_call_llm": None, "extract_content_or_reasoning": None}, io={})

# ── check_video_requirements ────────────────────────────────────────────────
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": S(["client", "x"]), "aux_probe_mode.__exit__": False})
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": [S(["c", None]), S(["c", "y"])], "aux_probe_mode.__exit__": False})
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": S(["c", None]), "aux_probe_mode.__exit__": False})
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": R("RuntimeError", "x"), "aux_probe_mode.__exit__": False})
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": R("SystemExit", "bye"), "aux_probe_mode.__exit__": False})
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": R("RuntimeError", "x"), "aux_probe_mode.__exit__": True})
case("check_video_requirements", io={})
case("check_video_requirements",
     io={"aux_probe_mode": "cm", "aux_probe_mode.__enter__": None,
         "resolve_vision_provider_client": S(["client", "x"]), "aux_probe_mode.__exit__": R("OSError", "exit fail")})

# ── _validate_image_url_async ───────────────────────────────────────────────
case("_validate_image_url_async", url="http://x.com/a.png",
     io={"urlparse_netloc": "x.com", "async_is_safe_url": True})
case("_validate_image_url_async", url="ftp://x.com", io={})
case("_validate_image_url_async", url="http://", io={"urlparse_netloc": ""})
case("_validate_image_url_async", url=None, io={})
case("_validate_image_url_async", url="https://x.com",
     io={"urlparse_netloc": "x.com", "async_is_safe_url": False})
case("_validate_image_url_async", url="https://x.com",
     io={"urlparse_netloc": "x.com", "async_is_safe_url": R("ValueError", "x")})
case("_validate_image_url_async", url="http://x.com", io={})

# ── _ssrf_redirect_guard ────────────────────────────────────────────────────
case("_ssrf_redirect_guard", response={"url": "http://x"}, io={"redirect_target_from_response": None})
case("_ssrf_redirect_guard", response={"url": "http://x"},
     io={"redirect_target_from_response": "http://169.254.169.254", "async_is_safe_url": False})
case("_ssrf_redirect_guard", response={"url": "http://x"},
     io={"redirect_target_from_response": "http://ok.com", "async_is_safe_url": True})
case("_ssrf_redirect_guard", response={"url": "http://x"}, io={"redirect_target_from_response": ""})
case("_ssrf_redirect_guard", response={"url": "http://x"}, io={"redirect_target_from_response": R("RuntimeError", "x")})
case("_ssrf_redirect_guard", response={"url": "http://x"},
     io={"redirect_target_from_response": "http://e", "async_is_safe_url": R("ValueError", "x")})
case("_ssrf_redirect_guard", response={"url": "http://x"}, io={})

# ── _call_vision_llm ────────────────────────────────────────────────────────
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response=None,
     io={"load_auxiliary_client": None, "async_call_llm": "resp", "extract_content_or_reasoning": "analysis"})
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response="given",
     io={"load_auxiliary_client": None, "extract_content_or_reasoning": "analysis"})
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response=None,
     io={"load_auxiliary_client": None, "async_call_llm": ["r1", "r2"],
         "extract_content_or_reasoning": ["", "analysis2"]})
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response=None,
     io={"load_auxiliary_client": None, "async_call_llm": ["r1", "r2"],
         "extract_content_or_reasoning": ["", ""]})
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response=None,
     io={"load_auxiliary_client": None, "async_call_llm": R("RuntimeError", "x")})
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response=None,
     io={"load_auxiliary_client": R("ValueError", "x")})
case("_call_vision_llm", call_kwargs={"task": "vision"}, empty_log="empty", response=None, io={})

# ── _prepare_image ──────────────────────────────────────────────────────────
PREP = {"path": "/h/cv/temp_image_u1.img", "mime": "image/png", "size_bytes": 3, "crop_offset": {}}
case("_prepare_image", image_url="http://x/a.png", task_id="t1", region=None, validate_decode=False,
     io={"host_feature_enabled": True, "resolve_image_source": {"data": "abc", "mime": "image/png"},
         "secure_cache_dir": "/h/cv", "uuid.uuid4": "u1", "write_private_bytes": None,
         "normalize_to_supported_image": {"normalized_path": "/h/cv/temp_image_u1.img", "mime": "image/png", "norm_err": None}})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=None, validate_decode=False,
     io={"host_feature_enabled": False})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=None, validate_decode=False,
     io={"host_feature_enabled": True, "resolve_image_source": R("ImageResolutionError", "cannot resolve")})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=None, validate_decode=False,
     io={"host_feature_enabled": True, "resolve_image_source": {"data": "abc", "mime": "image/png"},
         "secure_cache_dir": "/h/cv", "uuid.uuid4": "u1", "write_private_bytes": None,
         "normalize_to_supported_image": {"normalized_path": None, "mime": None, "norm_err": "bad format"},
         "unlink_quietly": None})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=None, validate_decode=True,
     io={"host_feature_enabled": True, "resolve_image_source": {"data": "abc", "mime": "image/png"},
         "secure_cache_dir": "/h/cv", "uuid.uuid4": "u1", "write_private_bytes": None,
         "normalize_to_supported_image": {"normalized_path": "/h/cv/temp_image_u1.img", "mime": "image/png", "norm_err": None},
         "run_encode_on_cpu_executor": "decode error", "unlink_quietly": None})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=[0, 0, 10, 10], validate_decode=False,
     io={"host_feature_enabled": True, "resolve_image_source": {"data": "abc", "mime": "image/png"},
         "secure_cache_dir": "/h/cv", "uuid.uuid4": "u1", "write_private_bytes": None,
         "normalize_to_supported_image": {"normalized_path": "/h/cv/temp_image_u1.img", "mime": "image/png", "norm_err": None},
         "crop_image_region": {"cropped_path": "/h/cv/crop.png", "mime": "image/png", "crop_err": None, "offset_out": {"x": 0, "y": 0}},
         "unlink_quietly": None, "path.stat": {"st_size": 8}})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=None, validate_decode=False,
     io={"host_feature_enabled": True, "resolve_image_source": {"data": "abc", "mime": "image/png"},
         "secure_cache_dir": "/h/cv", "uuid.uuid4": "u1", "write_private_bytes": None,
         "normalize_to_supported_image": R("SystemExit", "bye"), "unlink_quietly": None})
case("_prepare_image", image_url="http://x/a.png", task_id=None, region=None, validate_decode=False, io={})

# ── _vision_analyze_native ──────────────────────────────────────────────────
NPREP = {"path": "/h/cv/temp_image_u1.img", "mime": "image/png", "size_bytes": 3, "crop_offset": {}}
ENV = {"_multimodal": True, "content": [], "text_summary": "s", "meta": {}}
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": None, "resolve_repeat_cap": 0,
         "is_interrupted": False, "prepare_image": NPREP,
         "run_encode_on_cpu_executor": ["data:small", False], "resolve_embed_target_bytes": 256000,
         "record_embed": None, "build_scale_note": None, "build_native_vision_tool_result": ENV,
         "unlink_quietly": None})
case("_vision_analyze_native", image_url="  ", question="q", task_id=None, region=None,
     io={"tool_error": "{\"error\":\"image_url is required\"}"})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": "DUP"})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": "REFUSE"})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": None, "resolve_repeat_cap": 0,
         "is_interrupted": True, "tool_error": "{\"error\":\"Interrupted\"}"})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": None, "resolve_repeat_cap": 0,
         "is_interrupted": False, "prepare_image": R("_ImagePrepError", "prep failed"),
         "tool_error": "{\"error\":\"prep failed\"}"})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": None, "resolve_repeat_cap": 0,
         "is_interrupted": False, "prepare_image": NPREP,
         "run_encode_on_cpu_executor": ["data:" + "a" * 300000, False], "resolve_embed_target_bytes": 256000,
         "resize_prepared": {"data_url": "data:resized", "scale_out": {"orig_width": 100, "new_width": 50}},
         "record_embed": None, "build_scale_note": "downscaled", "build_native_vision_tool_result": ENV,
         "unlink_quietly": None})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": None, "resolve_repeat_cap": 2,
         "is_interrupted": False, "prepare_image": NPREP,
         "run_encode_on_cpu_executor": R("RuntimeError", "encode fail"),
         "tool_error": "{\"error\":\"Native vision failed: encode fail\"}",
         "release_embed": None, "unlink_quietly": None})
case("_vision_analyze_native", image_url="http://x/a.png", question="q", task_id=None, region=None,
     io={"native_turn_duplicate": None, "repeat_refusal": None, "resolve_repeat_cap": 0,
         "is_interrupted": False, "prepare_image": R("SystemExit", "bye"), "unlink_quietly": None})

# ── _run_analysis ───────────────────────────────────────────────────────────
DCD = {"parameters": {}, "error": None, "success": False, "analysis_length": 0, "model_used": "m", "image_size_bytes": 0}
case("_run_analysis", kind="image", source="http://x/a.png", user_prompt="q", model="m", stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": S(["analysis text", None]),
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON"})
case("_run_analysis", kind="image", source="s", user_prompt="q", model="m", stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": S(["analysis", "note"]),
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON2"})
case("_run_analysis", kind="image", source="s", user_prompt="q", model="m", stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": S(["", None]),
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON3"})
case("_run_analysis", kind="image", source="s", user_prompt="q", model="m", stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": True, "tool_error": "{\"error\":\"Interrupted\"}"})
case("_run_analysis", kind="image", source="s", user_prompt="q", model="m", stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": R("RuntimeError", "402 payment required"),
         "debug.log_call": None, "debug.save": None, "json.dumps": "ERRJSON"})
case("_run_analysis", kind="image", source="s", user_prompt="q", model="m", stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": R("RuntimeError", "something odd"),
         "debug.log_call": None, "debug.save": None, "json.dumps": "ERRJSON2"})
case("_run_analysis", kind="video", source="s", user_prompt="q", model="m", stage_paths=["/tmp/x"],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": S(["vid", None]),
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON4",
         "path.exists": True, "path.unlink": None})
case("_run_analysis", kind="image", source="s", user_prompt="q", model="m", stage_paths=["/tmp/x"],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": R("SystemExit", "bye"),
         "path.exists": True, "path.unlink": None})
case("_run_analysis", kind="bogus", source="s", user_prompt="q", model="m", stage_paths=[], io={})
case("_run_analysis", kind="image", source="s", user_prompt=None, model=None, stage_paths=[],
     io={"debug_call_data": DCD, "is_interrupted": False, "stage": S(["a", None]),
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON5"})

# ── vision_analyze_tool ─────────────────────────────────────────────────────
VPREP = {"path": "/h/cv/temp_image_u1.img", "mime": "image/png", "size_bytes": 3, "crop_offset": {}}
case("vision_analyze_tool", image_url="http://x/a.png", user_prompt="q", model="m", task_id=None, region=None,
     io={"debug_call_data": DCD, "is_interrupted": False, "prepare_image": VPREP,
         "run_encode_on_cpu_executor": "data:small", "media_messages": S([{"role": "user", "content": []}]),
         "aux_call_kwargs": {"task": "vision"}, "load_auxiliary_client": None, "async_call_llm": "resp",
         "call_vision_llm": "analysis", "build_scale_note": None,
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON",
         "path.exists": True, "path.unlink": None})
case("vision_analyze_tool", image_url="http://x/a.png", user_prompt="q", model="m", task_id=None, region=None,
     io={"debug_call_data": DCD, "is_interrupted": False, "prepare_image": VPREP,
         "run_encode_on_cpu_executor": "data:" + "a" * 5242881,
         "resize_prepared": {"data_url": "data:resized", "scale_out": {"orig_width": 100}},
         "media_messages": S([{"role": "user", "content": [{}, {"image_url": {"url": "old"}}]}]),
         "aux_call_kwargs": {"task": "vision"}, "load_auxiliary_client": None,
         "async_call_llm": [R("RuntimeError", "image too large"), "resp2"],
         "is_image_size_error": True, "call_vision_llm": "analysis", "build_scale_note": "note",
         "debug.log_call": None, "debug.save": None, "json.dumps": "JSON2",
         "path.exists": True, "path.unlink": None})
case("vision_analyze_tool", image_url="http://x/a.png", user_prompt="q", model="m", task_id=None, region=None,
     io={"debug_call_data": DCD, "is_interrupted": False, "prepare_image": VPREP,
         "run_encode_on_cpu_executor": "data:small", "media_messages": S([{"role": "user", "content": []}]),
         "aux_call_kwargs": {"task": "vision"}, "load_auxiliary_client": None,
         "async_call_llm": R("RuntimeError", "image too large"), "is_image_size_error": True,
         "debug.log_call": None, "debug.save": None, "json.dumps": "ERRJSON",
         "path.exists": True, "path.unlink": None, "logger.debug": None})
case("vision_analyze_tool", image_url="http://x/a.png", user_prompt="q", model="m", task_id=None, region=None,
     io={"debug_call_data": DCD, "is_interrupted": True, "tool_error": "{\"error\":\"Interrupted\"}"})
case("vision_analyze_tool", image_url="http://x/a.png", user_prompt="q", model="m", task_id=None, region=None,
     io={"debug_call_data": DCD, "is_interrupted": False, "prepare_image": R("_ImagePrepError", "prep failed"),
         "debug.log_call": None, "debug.save": None, "json.dumps": "ERRJSON2",
         "path.exists": False})
case("vision_analyze_tool", image_url="http://x/a.png", user_prompt="q", model="m", task_id=None, region=None,
     io={"debug_call_data": DCD, "is_interrupted": False, "prepare_image": VPREP,
         "run_encode_on_cpu_executor": "data:small", "media_messages": S([{"role": "user", "content": []}]),
         "aux_call_kwargs": {"task": "vision"}, "load_auxiliary_client": None,
         "async_call_llm": R("SystemExit", "bye"),
         "path.exists": True, "path.unlink": None})

# ── _handle_vision_analyze ──────────────────────────────────────────────────
case("_handle_vision_analyze", args={"image_url": "http://x/a.png", "question": "q", "region": None}, kw={"task_id": "t"},
     io={"should_use_native_vision_fast_path": True, "vision_analyze_native": "native_result"})
case("_handle_vision_analyze", args={"image_url": "http://x/a.png", "question": "q"}, kw={},
     io={"should_use_native_vision_fast_path": False, "configured_aux_model": "m", "vision_analyze_tool": "aux_result"})
case("_handle_vision_analyze", args={"image_url": "u", "question": "q"}, kw={"task_id": "t"},
     io={"should_use_native_vision_fast_path": False, "configured_aux_model": None, "vision_analyze_tool": "aux2"})
case("_handle_vision_analyze", args={"image_url": "u", "question": "q"}, kw={},
     io={"should_use_native_vision_fast_path": R("RuntimeError", "x")})
case("_handle_vision_analyze", args={"image_url": "u", "question": "q"}, kw={}, io={})
case("_handle_vision_analyze", args={}, kw={},
     io={"should_use_native_vision_fast_path": False, "configured_aux_model": "m", "vision_analyze_tool": "aux3"})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-function count check (>=5)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    if short:
        print("WARNING funcs with <5 cases:", short)
    return 1 if short else 0


if __name__ == "__main__":
    raise SystemExit(main())