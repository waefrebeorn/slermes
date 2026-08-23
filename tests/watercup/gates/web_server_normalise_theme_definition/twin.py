"""twin for web_server_normalise_theme_definition.

Traces LIVE hermes_cli/web_server.py::_normalise_theme_definition with the
same rung ladder as the C harness (B1..B11) and a canonical serializer that
byte-matches libjson's json_serialize (compact, int-valued floats via %.0f,
others %.17g, JSON escapes, non-ASCII raw).
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

# hermes_cli/web_server.py imports plugins.memory.config_schema at module
# scope; that file is loaded by path in production (it does not exist as a
# package module), so stub it before exec with the four imported names.
import types


def _stub_provider_config_schema(*a, **k):
    return {}


class _ProviderConfigSchema:
    def __init__(self, *a, **k):
        self.provider_id = a[0] if a else None


_stub_cs = types.ModuleType("plugins.memory.config_schema")
_stub_cs.ProviderConfigSchema = _ProviderConfigSchema
_stub_cs.ProviderField = lambda *a, **k: None
_stub_cs.STORAGE_HONCHO_HOST_BLOCK = ""
_stub_cs.get_provider_config_schema = _stub_provider_config_schema
for _name in ("plugins", "plugins.memory"):
    if _name not in sys.modules:
        sys.modules[_name] = types.ModuleType(_name)
sys.modules["plugins.memory.config_schema"] = _stub_cs

spec = ilu.spec_from_file_location(
    "wc_live_ws_theme", os.path.join(DEV, "hermes_cli", "web_server.py"))
_ws = ilu.module_from_spec(spec)
sys.modules["wc_live_ws_theme"] = _ws
spec.loader.exec_module(_ws)

_fn = _ws._normalise_theme_definition

_DENSITY = {"compact", "comfortable", "spacious"}
_VARIANT = {"standard", "cockpit", "tiled"}
_OVERRIDE_KEYS = set(_ws._THEME_OVERRIDE_KEYS)
_NAMED_ASSETS = {"bg", "hero", "logo", "crest", "sidebar", "header"}
_BUCKETS = set(_ws._THEME_COMPONENT_BUCKETS)
_TYPO_KEYS = ("fontSans", "fontMono", "fontDisplay", "fontUrl",
              "baseSize", "lineHeight", "letterSpacing")

_ESC = {'"': '\\"', "\\": "\\\\", "\b": "\\b", "\f": "\\f",
        "\n": "\\n", "\r": "\\r", "\t": "\\t"}


def _ser(v):
    if v is None:
        return "null"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        if float(v) == float(int(v)):
            return "%.0f" % float(v)
        return "%.17g" % float(v)
    if isinstance(v, str):
        out = ['"']
        for ch in v:
            o = ord(ch)
            if ch in _ESC:
                out.append(_ESC[ch])
            elif o < 32:
                out.append("\\u%04x" % o)
            else:
                out.append(ch)
        out.append('"')
        return "".join(out)
    if isinstance(v, dict):
        return "{" + ",".join(_ser(str(k)) + ":" + _ser(x) for k, x in v.items()) + "}"
    if isinstance(v, list):
        return "[" + ",".join(_ser(x) for x in v) + "]"
    raise TypeError(type(v))


def _nonblank_str(v):
    return isinstance(v, str) and bool(v.strip())


def trace(theme_json):
    c = Cup("normalise_theme_definition")
    try:
        data = json.loads(theme_json)
    except ValueError:
        data = None
    c.branch(isinstance(data, dict))                                    # B1
    name = data.get("name") if isinstance(data, dict) else None
    c.branch(isinstance(name, str) and bool(name.strip()))              # B2
    palette_src = data.get("palette") if isinstance(data, dict) else None
    c.branch(isinstance(palette_src, dict))                             # B3
    raw_noise = (palette_src.get("noiseOpacity")
                 if isinstance(palette_src, dict) and "noiseOpacity" in palette_src
                 else data.get("noiseOpacity") if isinstance(data, dict) else None)
    c.branch(raw_noise is not None)                                     # B4
    typo_src = data.get("typography") if isinstance(data, dict) else None
    c.branch(isinstance(typo_src, dict) and
             any(isinstance(v, str) and v.strip() for v in typo_src.values()))  # B5
    layout_src = data.get("layout") if isinstance(data, dict) else None
    density = layout_src.get("density") if isinstance(layout_src, dict) else None
    c.branch(isinstance(density, str) and density in _DENSITY)          # B6
    ovr = data.get("colorOverrides") if isinstance(data, dict) else None
    c.branch(isinstance(ovr, dict) and
             any(k in _OVERRIDE_KEYS and _nonblank_str(v)
                 for k, v in ovr.items()))                              # B7
    assets = data.get("assets") if isinstance(data, dict) else None
    assets_kept = 0
    if isinstance(assets, dict):
        for k, v in assets.items():
            if k in _NAMED_ASSETS and _nonblank_str(v):
                assets_kept += 1
        custom = assets.get("custom")
        if isinstance(custom, dict):
            assets_kept += sum(
                1 for k, v in custom.items()
                if isinstance(k, str)
                and k.replace("-", "").replace("_", "").isalnum()
                and _nonblank_str(v))
    c.branch(assets_kept > 0)                                           # B8
    css = data.get("customCSS") if isinstance(data, dict) else None
    c.branch(_nonblank_str(css))                                        # B9
    comp = data.get("componentStyles") if isinstance(data, dict) else None
    comp_kept = 0
    if isinstance(comp, dict):
        for bucket, props in comp.items():
            if bucket not in _BUCKETS or not isinstance(props, dict):
                continue
            for prop, val in props.items():
                ok = (
                    isinstance(prop, str)
                    and prop.replace("-", "").replace("_", "").isalnum()
                    and isinstance(val, (str, int, float))
                    and not isinstance(val, bool)
                    and str(val).strip()
                ) or (isinstance(val, bool))
                if ok:
                    comp_kept += 1
                    break
    c.branch(comp_kept > 0)                                             # B10
    lv = data.get("layoutVariant") if isinstance(data, dict) else None
    c.branch(isinstance(lv, str) and lv in _VARIANT)                    # B11

    res = _fn(data if isinstance(data, dict) else {})
    # _fn(None-typed input): LIVE requires dict; non-dict returns None.
    if not isinstance(data, dict):
        res = None
    elif res is None:
        pass
    c.valstr("" if res is None else _ser(res))                          # V
    return c.finalize()


def run(inputs):
    return [trace(m) for m in inputs]
