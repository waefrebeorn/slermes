"""twin for anthropic_adapter_image_source — LIVE
agent/anthropic_adapter.py::_image_source_from_openai_url."""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_antr", os.path.join(DEV, "agent", "anthropic_adapter.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_antr"] = mod
        spec.loader.exec_module(mod)
        return mod._image_source_from_openai_url
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(url):
    url = str(url or "").strip()
    if not url:
        return {"type": "url", "url": ""}
    if url.startswith("data:"):
        header, _, data = url.partition(",")
        media_type = "image/jpeg"
        if header.startswith("data:"):
            mime_part = header[len("data:"):].split(";", 1)[0].strip()
            if mime_part.startswith("image/"):
                media_type = mime_part
        return {"type": "base64", "media_type": media_type, "data": data}
    return {"type": "url", "url": url}


def trace(raw):
    url = str(raw or "").strip()
    c = Cup("_image_source_from_openai_url")
    empty = not url
    is_data = url.startswith("data:")
    c.branch(empty)                                     # B1
    c.branch(is_data)                                   # B2
    if is_data:
        header, _, _data = url.partition(",")
        c.branch("," in raw)                            # B3
        if "," in raw:
            mime_part = header[len("data:"):].split(";", 1)[0].strip()
            c.branch(mime_part.startswith("image/") and len(mime_part) > 6)  # B4

    result = _live_fn(raw) if _live_fn else _fallback(raw)
    c.valstr(str(result).replace("'", '"'))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
