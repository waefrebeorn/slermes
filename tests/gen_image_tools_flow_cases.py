#!/usr/bin/env python3
"""Generate tests/fixtures/image_tools_flow.jsonl for the image_tools_flow port.

>=5 cases per ported function (20 funcs), covering happy paths, decision
branches, real exception kinds, BaseException passthrough, and fail-closed
(missing-script) seams. Scripts live in in.io (seam-name -> value / queue /
{"__script__":true,"ret":...,"raise":{type,message}}). logger.* default to null
so logging never fail-closes accidentally. Pure Python; run with the quarry
venv interpreter (no quarry import needed — catalog meta is embedded).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "image_tools_flow.jsonl")

KLEIN = "fal-ai/flux-2/klein/9b"
KLING = "fal-ai/kling-image/v3/text-to-image"
GPT15 = "fal-ai/gpt-image-1.5"
NBP = "fal-ai/nano-banana-pro"
ZIMAGE = "fal-ai/z-image/turbo"

KLEIN_META = {"edit_endpoint": KLEIN + "/edit", "display": "FLUX 2 Klein 9B",
              "max_reference_images": 9}
ZIMAGE_META = {"display": "Z-Image Turbo"}
KLING_META = {"edit_endpoint": KLING.replace("text-to-image", "image-to-image"),
              "display": "Kling Image v3", "max_reference_images": 1,
              "edit_image_param": "image_url"}

CASES = []


def io(**scripts):
    d = {"logger.debug": None, "logger.info": None,
         "logger.warning": None, "logger.error": None}
    d.update(scripts)
    return d


def raise_spec(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def desc(bands=None, info=None, size=None):
    return {"bands": bands if bands is not None else ["R", "G", "B"],
            "info": info if info is not None else {},
            "size": size if size is not None else [400, 300]}


# ── _read_image_gen_key ─────────────────────────────────────────────────────
case("_read_image_gen_key", key="model",
     io=io(load_config={"image_gen": {"model": "  fal-ai/x  "}}))
case("_read_image_gen_key", key="model", io=io(load_config="not-a-dict"))
case("_read_image_gen_key", key="model", io=io(load_config={"image_gen": 5}))
case("_read_image_gen_key", key="model", io=io(load_config={"image_gen": {"model": "   "}}))
case("_read_image_gen_key", key="provider",
     io=io(load_config={"image_gen": {"provider": "openai"}}))
case("_read_image_gen_key", key="model",
     io=io(load_config=raise_spec("RuntimeError", "boom")))
case("_read_image_gen_key", key="model",
     io=io(load_config=raise_spec("SystemExit", "bye")))
case("_read_image_gen_key", key="model", io={})  # fail-closed load_config (caught -> debug log)

# ── _plugin_provider_name ───────────────────────────────────────────────────
case("_plugin_provider_name", io=io(read_configured_image_provider="openai"))
case("_plugin_provider_name", io=io(read_configured_image_provider="fal"))
case("_plugin_provider_name", io=io(read_configured_image_provider="nous"))
case("_plugin_provider_name", io=io(read_configured_image_provider=None))
case("_plugin_provider_name", io=io(read_configured_image_provider=""))
case("_plugin_provider_name", io=io(read_configured_image_provider=raise_spec("ValueError", "x")))
case("_plugin_provider_name", io={})  # fail-closed

# ── _build_payload (pure; real catalog + overrides) ─────────────────────────
case("_build_payload", model_id=KLEIN, prompt="  a cat  ", aspect_ratio="landscape",
     seed=7, overrides={"num_inference_steps": 8, "output_format": None})
case("_build_payload", model_id=KLEIN, prompt="edit", aspect_ratio="square",
     seed=None, overrides={"seed": 3}, image_urls=["u1", "u2"])
case("_build_payload", model_id=KLING, prompt="p", aspect_ratio="landscape",
     seed=None, overrides=None, image_urls=["a", "b"])
case("_build_payload", model_id=KLEIN, prompt="p", aspect_ratio="bogus",
     seed=None, overrides=None)
case("_build_payload", model_id=GPT15, prompt="p", aspect_ratio="portrait",
     seed=None, overrides=None)
case("_build_payload", model_id=NBP, prompt="p", aspect_ratio="square",
     seed=None, overrides=None)
case("_build_payload", model_id=KLEIN, prompt="p", aspect_ratio="landscape",
     seed=True, overrides=None)
case("_build_payload", model_id="m", prompt="p", aspect_ratio="landscape", seed=None,
     overrides=None,
     catalog={"m": {"size_style": "weird", "sizes": {"landscape": "x"},
                    "defaults": {}, "supports": ["prompt"]}})
case("_build_payload", model_id="m", prompt="p", aspect_ratio="landscape", seed=None,
     overrides=None, image_urls=["u"],
     catalog={"m": {"size_style": "weird", "sizes": {"landscape": "x"},
                    "defaults": {}, "supports": ["prompt"]}})
case("_build_payload", model_id="nope", prompt="p", aspect_ratio="landscape",
     seed=None, overrides=None)
case("_build_payload", model_id=KLING, prompt="p", aspect_ratio="landscape",
     seed=None, overrides=None, image_urls=[])
case("_build_payload", model_id="m", prompt="p", aspect_ratio="landscape", seed=None,
     overrides={"a": None, "b": 0, "c": ""},
     catalog={"m": {"size_style": "image_size_preset", "sizes": {"landscape": "square_hd"},
                    "defaults": {}, "supports": ["prompt", "b", "c"]}})

# ── _build_fal_payload ──────────────────────────────────────────────────────
case("_build_fal_payload", model_id=KLEIN, prompt="hi")
case("_build_fal_payload", model_id=KLEIN, prompt="hi", aspect_ratio="portrait")
case("_build_fal_payload", model_id=KLEIN, prompt="hi", seed=5, overrides={"num_images": 2})
case("_build_fal_payload", model_id="nope", prompt="hi")
case("_build_fal_payload", model_id=KLEIN, prompt="hi", aspect_ratio=None)
case("_build_fal_payload", model_id="m", prompt="p",
     catalog={"m": {"size_style": "weird", "sizes": {"landscape": "x"},
                    "defaults": {}, "supports": ["prompt"]}})

# ── _build_fal_edit_payload ─────────────────────────────────────────────────
case("_build_fal_edit_payload", model_id=KLEIN, prompt="p", image_urls=["u"])
case("_build_fal_edit_payload", model_id=KLING, prompt="p", image_urls=["a", "b"])
case("_build_fal_edit_payload", model_id=KLEIN, prompt="p", image_urls=[])
case("_build_fal_edit_payload", model_id=KLING, prompt="p", image_urls=[])
case("_build_fal_edit_payload", model_id=KLEIN, prompt="p", image_urls=["u"],
     aspect_ratio="square", seed=9)
case("_build_fal_edit_payload", model_id="nope", prompt="p", image_urls=["u"])

# ── _format_images ──────────────────────────────────────────────────────────
case("_format_images", images=[{"url": "u", "width": 10, "height": 20}, {"url": "v"},
                               "skip", {"no": "url"}], should_upscale=False, prompt="p")
case("_format_images", images=[{"url": "u"}], should_upscale=True, prompt="  p  ",
     io=io(upscale_image={"url": "U", "width": 1, "height": 2, "upscaled": True}))
case("_format_images", images=[{"url": "u"}], should_upscale=True, prompt="p",
     io=io(upscale_image=None))
case("_format_images", images=[{"url": "u"}], should_upscale=True, prompt="p",
     io=io(upscale_image=raise_spec("ImageGenerationInterrupted", "stop")))
case("_format_images", images=[{"url": "u"}], should_upscale=True, prompt="p",
     io=io(upscale_image=raise_spec("ValueError", "x")))
case("_format_images", images=[{"url": "u"}], should_upscale=True, prompt=5)
case("_format_images", images=[{"url": "u"}], should_upscale=True, prompt="p", io={})

# ── _inline_local_sources ───────────────────────────────────────────────────
case("_inline_local_sources", sources=["a", "b"],
     io=io(resolve_local_source_to_data_url=["data:a", "data:b"], _run_async=["A", "B"]))
case("_inline_local_sources", sources=["a"],
     io=io(resolve_local_source_to_data_url=raise_spec("ImageResolutionError", "bad")))
case("_inline_local_sources", sources=["a"],
     io=io(resolve_local_source_to_data_url="x",
           _run_async=raise_spec("ImageResolutionError", "bad2")))
case("_inline_local_sources", sources=["a"],
     io=io(resolve_local_source_to_data_url=raise_spec("ValueError", "other")))
case("_inline_local_sources", sources=["a"],
     io=io(resolve_local_source_to_data_url=raise_spec("KeyboardInterrupt", "")))
case("_inline_local_sources", sources=[])
case("_inline_local_sources", sources=["a"], io={})  # fail-closed

# ── _fit_inline_budget ──────────────────────────────────────────────────────
case("_fit_inline_budget", urls=["data:short", "http://x"], budget=100)
case("_fit_inline_budget", urls=["data:" + "a" * 50, "http://x"], budget=20,
     io=io(pil_open=desc(size=[300, 300]), pil_load=None,
           exif_transpose=desc(size=[300, 300]), pil_convert=desc(size=[300, 300]),
           pil_encode=[{"b64": "x", "size": [300, 300]}, {"b64": "x", "size": [225, 225]}]))
case("_fit_inline_budget", urls=["data:x", 5], budget=100)
case("_fit_inline_budget", urls=["http://x"], budget=-5)
case("_fit_inline_budget", urls=["data:" + "a" * 30, "data:" + "b" * 30], budget=-5,
     io=io(pil_open=desc(size=[100, 100]), pil_load=None,
           exif_transpose=desc(size=[100, 100]), pil_convert=desc(size=[100, 100]),
           pil_encode={"b64": "x", "size": [100, 100]}))
case("_fit_inline_budget", urls=["http://x", "http://y"], budget=10)

# ── _inline_fal_sources ─────────────────────────────────────────────────────
case("_inline_fal_sources", sources=["p"],
     io=io(resolve_local_source_to_data_url="data:x", _run_async="data:x"))
case("_inline_fal_sources", sources=[])
case("_inline_fal_sources", sources=["p"],
     io=io(resolve_local_source_to_data_url=raise_spec("ImageResolutionError", "bad")))
case("_inline_fal_sources", sources=["p"],
     io=io(resolve_local_source_to_data_url="x", _run_async=raise_spec("ValueError", "x")))
case("_inline_fal_sources", sources=["p"],
     io=io(resolve_local_source_to_data_url=raise_spec("GeneratorExit", "")))
case("_inline_fal_sources", sources=["p"], io={})  # fail-closed

# ── _shrink_data_url ────────────────────────────────────────────────────────
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=desc(), pil_load=None, exif_transpose=desc(), pil_convert=desc(),
           pil_encode={"b64": "xyz", "size": [400, 300]}))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=desc(bands=["R", "G", "B", "A"]), pil_load=None,
           exif_transpose=desc(bands=["R", "G", "B", "A"]),
           pil_convert=desc(bands=["R", "G", "B", "A"]),
           pil_encode={"b64": "q", "size": [10, 10]}))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=desc(info={"transparency": 1}), pil_load=None,
           exif_transpose=desc(info={"transparency": 1}),
           pil_convert=desc(bands=["R", "G", "B", "A"]),
           pil_encode={"b64": "q", "size": [10, 10]}))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=raise_spec("OSError", "bad")))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=desc(), pil_load=raise_spec("DecompressionBombError", "huge")))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=raise_spec("ValueError", "bad")))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=26,
     io=io(pil_open=desc(size=[1000, 1000]), pil_load=None,
           exif_transpose=desc(size=[1000, 1000]), pil_convert=desc(size=[1000, 1000]),
           pil_encode=[{"b64": "aaaa", "size": [1000, 1000]},
                       {"b64": "bb", "size": [750, 750]}]))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=1,
     io=io(pil_open=desc(size=[200, 200]), pil_load=None,
           exif_transpose=desc(size=[200, 200]), pil_convert=desc(size=[200, 200]),
           pil_encode={"b64": "xyz", "size": [200, 200]}))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100,
     io=io(pil_open=desc(), pil_load=None, exif_transpose=desc(),
           pil_convert=raise_spec("RuntimeError", "x")))
case("_shrink_data_url", url="data:image/png;base64,ABC", budget=100, io={})  # fail-closed
case("_shrink_data_url", url="abc", budget=100,
     io=io(pil_open=desc(), pil_load=None, exif_transpose=desc(), pil_convert=desc(),
           pil_encode={"b64": "z", "size": [400, 300]}))

# ── _prepare_fal_request ────────────────────────────────────────────────────
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="a cat",
     aspect_ratio="landscape", seed=7, overrides={}, source_images=[],
     io=io(fal_key_set="KEY"))
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="edit",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=["u1", "u2"],
     io=io(fal_key_set="KEY", resolve_local_source_to_data_url=["data:u1", "data:u2"],
           _run_async=["data:u1", "data:u2"]))
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="   ",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=[])
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt=5,
     aspect_ratio="landscape", seed=None, overrides={}, source_images=[])
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="p",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=[],
     io=io(fal_key_set=None, resolve_managed_fal_gateway=None,
           build_no_backend_setup_message="SETUP"))
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="p",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=[],
     io=io(fal_key_set=None, resolve_managed_fal_gateway=raise_spec("ValueError", "selection")))
case("_prepare_fal_request", model_id=ZIMAGE, meta=ZIMAGE_META, prompt="p",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=["u"],
     io=io(fal_key_set="KEY"))
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="p",
     aspect_ratio="bogus", seed=None, overrides={}, source_images=[],
     io=io(fal_key_set="KEY"))
case("_prepare_fal_request", model_id=KLEIN, meta=KLING_META, prompt="p",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=["a", "b", "c"],
     io=io(fal_key_set="KEY", resolve_local_source_to_data_url="data:a", _run_async="data:a"))
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="p",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=[],
     io=io(fal_key_set=raise_spec("SystemExit", "bye")))
case("_prepare_fal_request", model_id=KLEIN, meta=KLEIN_META, prompt="p",
     aspect_ratio="landscape", seed=None, overrides={}, source_images=[], io={})

# ── check_fal_api_key ───────────────────────────────────────────────────────
case("check_fal_api_key", io=io(resolve_managed_fal_gateway={"origin": "x"}))
case("check_fal_api_key", io=io(resolve_managed_fal_gateway=None, fal_key_set="KEY"))
case("check_fal_api_key", io=io(resolve_managed_fal_gateway=None, fal_key_set=None))
case("check_fal_api_key", io=io(resolve_managed_fal_gateway=raise_spec("ValueError", "x")))
case("check_fal_api_key", io=io(resolve_managed_fal_gateway=raise_spec("RuntimeError", "x")))
case("check_fal_api_key", io=io(resolve_managed_fal_gateway=raise_spec("SystemExit", "bye")))
case("check_fal_api_key", io=io(resolve_managed_fal_gateway={}, fal_key_set=""))
case("check_fal_api_key", io={})  # fail-closed

# ── _get_plugin_provider ────────────────────────────────────────────────────
case("_get_plugin_provider", name="krea", force=False,
     io=io(ensure_plugins_discovered=None, get_provider={"name": "krea", "display_name": "Krea"}))
case("_get_plugin_provider", name="krea", force=True,
     io=io(ensure_plugins_discovered=None, get_provider={"name": "krea", "display_name": "Krea"}))
case("_get_plugin_provider", name="nope",
     io=io(ensure_plugins_discovered=None, get_provider=None))
case("_get_plugin_provider", name="krea",
     io=io(ensure_plugins_discovered=raise_spec("RuntimeError", "x")))
case("_get_plugin_provider", name="krea",
     io=io(ensure_plugins_discovered=None, get_provider=raise_spec("ValueError", "x")))
case("_get_plugin_provider", name="krea", io={})  # fail-closed

# ── _provider_error (pure) ──────────────────────────────────────────────────
case("_provider_error", error="e", error_type="t")
case("_provider_error", error="", error_type="")
case("_provider_error", error='a"b\\c\nd', error_type="provider_exception")
case("_provider_error", error="tab\there", error_type="provider_contract")
case("_provider_error", error="x", error_type="provider_not_registered")

# ── _provider_result ────────────────────────────────────────────────────────
case("_provider_result", result={"success": True, "image": "u"}, contract_error="ce")
case("_provider_result", result="x", contract_error="ce")
case("_provider_result", result=None, contract_error="ce")
case("_provider_result", result=[], contract_error="ce")
case("_provider_result", result={"a": 1, "b": {"c": True}}, contract_error="ce")
case("_provider_result", result={}, contract_error="ce")

# ── _add_provider_kwargs ────────────────────────────────────────────────────
case("_add_provider_kwargs", kwargs={"prompt": "p"}, image_url="  u  ",
     reference_image_urls=None, upscale=None, model="m", controls={"creativity": "high"})
case("_add_provider_kwargs", kwargs={"prompt": "p"}, image_url=None,
     reference_image_urls=["a", "b"], upscale=None, model=None, controls=None,
     io=io(normalize_reference_images=["a", "b"]))
case("_add_provider_kwargs", kwargs={"prompt": "p"}, image_url=None,
     reference_image_urls=[], upscale=None, model=None, controls=None,
     io=io(normalize_reference_images=None))
case("_add_provider_kwargs", kwargs={}, image_url=None, reference_image_urls=None,
     upscale=True, model=None, controls=None)
case("_add_provider_kwargs", kwargs={}, image_url=None, reference_image_urls=None,
     upscale=0, model=None, controls=None)
case("_add_provider_kwargs", kwargs={"prompt": "p"}, image_url="   ",
     reference_image_urls=None, upscale=None, model="", controls=None)
case("_add_provider_kwargs", kwargs={}, image_url=None, reference_image_urls=["a"],
     upscale=None, model=None, controls=None,
     io=io(normalize_reference_images=raise_spec("ValueError", "x")))
case("_add_provider_kwargs", kwargs={}, image_url=None, reference_image_urls=["a"],
     upscale=None, model=None, controls=None, io={})  # fail-closed

# ── _declared_controls ──────────────────────────────────────────────────────
case("_declared_controls", provider={"name": "krea"},
     controls={"creativity": "high", "thinking": "fast"},
     io=io(**{"provider.capabilities": {"creative_controls": ["creativity"]}}))
case("_declared_controls", provider={"name": "krea"}, controls=None)
case("_declared_controls", provider={"name": "krea"}, controls={})
case("_declared_controls", provider={"name": "krea"}, controls={"a": 1},
     io=io(**{"provider.capabilities": raise_spec("RuntimeError", "x")}))
case("_declared_controls", provider={"name": "krea"}, controls={"a": 1},
     io=io(**{"provider.capabilities": None}))
case("_declared_controls", provider={"name": "krea"}, controls={"a": 1},
     io=io(**{"provider.capabilities": {"creative_controls": []}}))
case("_declared_controls", provider={"name": "krea"}, controls={"x": 1},
     io=io(**{"provider.capabilities": {"creative_controls": ["y"]}}))
case("_declared_controls", provider=None, controls={"a": 1})
case("_declared_controls", provider={"name": "krea"}, controls={"a": 1},
     io=io(**{"provider.capabilities": raise_spec("SystemExit", "bye")}))
case("_declared_controls", provider={"name": "krea"}, controls=[1],
     io=io(**{"provider.capabilities": {"creative_controls": ["x"]}}))
case("_declared_controls", provider={"name": "krea"}, controls={"a": 1}, io={})  # fail-closed

# ── _managed_model_plugin ───────────────────────────────────────────────────
case("_managed_model_plugin",
     io=io(read_configured_image_model="krea-2-medium",
           read_configured_image_provider="nous", managed_route="krea"))
case("_managed_model_plugin",
     io=io(read_configured_image_model="gpt-image-2",
           read_configured_image_provider="nous", managed_route="portal"))
case("_managed_model_plugin",
     io=io(read_configured_image_model="fal-ai/x",
           read_configured_image_provider=None, managed_route="fal"))
case("_managed_model_plugin",
     io=io(read_configured_image_model="x", read_configured_image_provider="nous",
           managed_route=None))
case("_managed_model_plugin",
     io=io(read_configured_image_model=None, read_configured_image_provider=None,
           managed_route="fal"))
case("_managed_model_plugin",
     io=io(read_configured_image_model=raise_spec("ValueError", "x")))
case("_managed_model_plugin",
     io=io(read_configured_image_model="x", read_configured_image_provider="nous",
           managed_route=raise_spec("RuntimeError", "x")))
case("_managed_model_plugin", io={})  # fail-closed

# ── _maybe_route_managed_model ──────────────────────────────────────────────
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="fal-ai/x", read_configured_image_provider=None,
           managed_route="fal"))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway={"origin": "x"},
           ensure_plugins_discovered=None,
           get_provider={"name": "krea", "display_name": "Krea"},
           **{"provider.generate": {"success": True, "image": "u"}}))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway=None))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="gpt-image-2", read_configured_image_provider="nous",
           managed_route="portal", ensure_plugins_discovered=None, get_provider=None))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway={"origin": "x"},
           ensure_plugins_discovered=None,
           get_provider={"name": "krea", "display_name": "Krea"},
           **{"provider.generate": raise_spec("ValueError", "boom")}))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway={"origin": "x"},
           ensure_plugins_discovered=None,
           get_provider={"name": "krea", "display_name": "Krea"},
           **{"provider.generate": "not-a-dict"}))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway={"origin": "x"},
           ensure_plugins_discovered=None,
           get_provider=raise_spec("RuntimeError", "x")))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway={"origin": "x"},
           ensure_plugins_discovered=None,
           get_provider={"name": "krea", "display_name": "Krea"},
           **{"provider.generate": raise_spec("SystemExit", "bye")}))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape",
     controls={"creativity": "high"},
     io=io(read_configured_image_model="krea-2-medium", read_configured_image_provider="nous",
           managed_route="krea", resolve_managed_krea_gateway={"origin": "x"},
           ensure_plugins_discovered=None,
           get_provider={"name": "krea", "display_name": "Krea"},
           **{"provider.capabilities": {"creative_controls": ["creativity"]},
              "provider.generate": {"success": True}}))
case("_maybe_route_managed_model", prompt="p", aspect_ratio="landscape", io={})  # fail-closed

# ── _fall_back_to_fal ───────────────────────────────────────────────────────
case("_fall_back_to_fal", raw="{bad", prompt="p", aspect_ratio="landscape", sources={})
case("_fall_back_to_fal", raw='{"success": true, "fallback_eligible": true}',
     prompt="p", aspect_ratio="landscape", sources={})
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": false}',
     prompt="p", aspect_ratio="landscape", sources={})
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true}',
     prompt="p", aspect_ratio="landscape", sources={"image_url": "u"})
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true}',
     prompt="p", aspect_ratio="landscape", sources={},
     io=io(fallback_to_fal_enabled=False))
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true, "error": "krea down"}',
     prompt="p", aspect_ratio="landscape", sources={},
     io=io(fallback_to_fal_enabled=True,
           image_generate_tool='{"success": true, "image": "u", "upscaled": false}'))
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true, "error": "krea down"}',
     prompt="p", aspect_ratio="landscape", sources={},
     io=io(fallback_to_fal_enabled=True,
           image_generate_tool='{"success": false, "error": "fal down"}'))
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true, "error": "krea down"}',
     prompt="p", aspect_ratio="landscape", sources={},
     io=io(fallback_to_fal_enabled=True,
           image_generate_tool='{"success": true, "upscaled": true}'))
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true, "error": "krea down"}',
     prompt="p", aspect_ratio="landscape", sources={"upscale": True},
     io=io(fallback_to_fal_enabled=True,
           image_generate_tool='{"success": true, "image": "u"}'))
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true}',
     prompt="p", aspect_ratio="landscape", sources={},
     io=io(fallback_to_fal_enabled=True, image_generate_tool=raise_spec("ValueError", "x")))
case("_fall_back_to_fal", raw="[]", prompt="p", aspect_ratio="landscape", sources={})
case("_fall_back_to_fal", raw='{"success": false, "fallback_eligible": true}',
     prompt="p", aspect_ratio="landscape", sources={}, io={})  # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func counts
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print(f"  {fn}: {counts[fn]}{flag}")
    short = [fn for fn, n in counts.items() if n < 5]
    if short:
        raise SystemExit(f"funcs with <5 cases: {short}")


if __name__ == "__main__":
    main()
