#!/usr/bin/env python3
"""Oracle for the tools/image_generation_tool.py image-tools FLOW + leaf port
(src/tools/image_tools_flow.c).

MODULE RESOLUTION: the task named tools/image_tools.py, which does not exist in
the quarry; the real defining module of the image tools surface is
tools/image_generation_tool.py (its DebugSession is named "image_tools"). PoP
credits THAT module. The catalog constants come from the LIVE
tools/image_generation_catalog.py (exec'd real — pure data).

Drives the LIVE 20 UNCLAIMED defs with SCRIPTED I/O: every CLAIMED sibling
(_read_configured_image_model, _read_configured_image_provider, _upscale_image,
fal_key_is_configured, _resolve_managed_fal_gateway, _build_no_backend_setup_message,
image_generate_tool) and every foreign sub-call (config, logger, image_source,
model_tools._run_async, plugin registry, normalize_reference_images,
managed_route, krea gateway, fallback gate, PIL/base64/io doubles) is a recording
stub fed the SAME scripts the C harness feeds its seams. Unclaimed siblings
ported here run REAL when composed; each is also driven directly. Compares the
DECISION SEQUENCE (ordered seam calls {"name","args"} in "trace"), the return
("ret") and the echoed globals ("state").

Live defs are AST-extracted; the heavy quarry import chain is never imported.
In-function imports are satisfied with sys.modules stubs delegating to the
current case's recorder. Fail-closed: missing defs or 0 cases => exit 2; a
missing/exhausted seam script raises RuntimeError on both sides (recorded on the
trace before propagation). Catch BaseException (scripted SystemExit /
KeyboardInterrupt / GeneratorExit propagate through `except Exception` /
`except ValueError` / `except ImageResolutionError` sites).

Representation contract (shared with the C port header):
- provider -> ProviderDouble(desc); capabilities/generate fire seams; display_name
  reads desc (missing -> AttributeError 'ProviderDouble').
- PIL fully seamed via descriptor dicts {bands,info,size}; base64 decode/encode
  are identity doubles; io.BytesIO is a BufDouble.
- json.dumps / json.loads run REAL on the PY side (C computes them); fixture
  domain: no float values in dumped dicts, strict JSON grammar, ASCII.
- Integral floats normalize to ints; None-valued seam arg keys dropped both sides.
Summary: 'ORACLE image_tools_flow: N cases, M mismatches'.
"""
import ast
import copy
import json
import math
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "tools" / "image_generation_tool.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "tools" / "image_generation_tool.py")
CAT_SRC = str(QUARRY / "tools" / "image_generation_catalog.py")

# live name -> C seam-driven target (the "func" key in fixtures is the live name)
PY = {
    "_read_image_gen_key": "_read_image_gen_key",
    "_plugin_provider_name": "_plugin_provider_name",
    "_build_payload": "_build_payload",
    "_build_fal_payload": "_build_fal_payload",
    "_build_fal_edit_payload": "_build_fal_edit_payload",
    "_format_images": "_format_images",
    "_inline_local_sources": "_inline_local_sources",
    "_fit_inline_budget": "_fit_inline_budget",
    "_inline_fal_sources": "_inline_fal_sources",
    "_shrink_data_url": "_shrink_data_url",
    "_prepare_fal_request": "_prepare_fal_request",
    "check_fal_api_key": "check_fal_api_key",
    "_get_plugin_provider": "_get_plugin_provider",
    "_provider_error": "_provider_error",
    "_provider_result": "_provider_result",
    "_add_provider_kwargs": "_add_provider_kwargs",
    "_declared_controls": "_declared_controls",
    "_managed_model_plugin": "_managed_model_plugin",
    "_maybe_route_managed_model": "_maybe_route_managed_model",
    "_fall_back_to_fal": "_fall_back_to_fal",
}
REQUIRED_CONSTS = [
    "FAL_MODELS", "DEFAULT_MODEL", "DEFAULT_ASPECT_RATIO", "VALID_ASPECT_RATIOS",
    "_SIZE_KEY_BY_STYLE", "_FAL_INLINE_BUDGET", "NOUS_MANAGED_PROVIDER",
    "UPSCALER_MODEL",
]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(seg(src, node), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


class ImageResolutionError(Exception):
    pass


class DecompressionBombError(Exception):
    pass


def _exec_catalog():
    """Exec the LIVE catalog module (pure data + _model) for real constants."""
    src = open(CAT_SRC, encoding="utf-8").read()
    ns = {"__name__": "tools.image_generation_catalog",
          "Any": object, "Dict": dict, "Optional": __import__("typing").Optional}
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.Assign, ast.AnnAssign)):
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
            try:
                exec_node(src, node, ns, CAT_SRC)
            except Exception:
                pass
    return ns


def extract():
    cat = _exec_catalog()
    src = open(SRC, encoding="utf-8").read()
    import typing
    ns = {
        "__name__": "tools.image_generation_tool",
        "json": json, "os": os, "math": math,
        "datetime": __import__("datetime"),
        "threading": __import__("threading"),
        "uuid": __import__("uuid"),
        "logging": __import__("logging"),
        "Any": typing.Any, "Dict": dict, "Optional": typing.Optional,
        "logger": _SilentLogger(),
        # catalog constants (real, from the live catalog module)
        "FAL_MODELS": cat["FAL_MODELS"],
        "DEFAULT_MODEL": cat["DEFAULT_MODEL"],
        "DEFAULT_ASPECT_RATIO": cat["DEFAULT_ASPECT_RATIO"],
        "VALID_ASPECT_RATIOS": cat["VALID_ASPECT_RATIOS"],
        "UPSCALER_MODEL": cat["UPSCALER_MODEL"],
        "UPSCALER_FACTOR": cat["UPSCALER_FACTOR"],
        "UPSCALER_CREATIVITY": cat["UPSCALER_CREATIVITY"],
        "UPSCALER_RESEMBLANCE": cat["UPSCALER_RESEMBLANCE"],
        "UPSCALER_GUIDANCE_SCALE": cat["UPSCALER_GUIDANCE_SCALE"],
        "UPSCALER_NUM_INFERENCE_STEPS": cat["UPSCALER_NUM_INFERENCE_STEPS"],
        "UPSCALER_DEFAULT_PROMPT": cat["UPSCALER_DEFAULT_PROMPT"],
        "UPSCALER_NEGATIVE_PROMPT": cat["UPSCALER_NEGATIVE_PROMPT"],
        "UPSCALER_SAFETY_CHECKER": cat["UPSCALER_SAFETY_CHECKER"],
        "NOUS_MANAGED_PROVIDER": "nous",
        # placeholders for names imported at module top (seamed per case)
        "DebugSession": lambda *a, **k: types.SimpleNamespace(
            log_call=lambda *a, **k: None, save=lambda *a, **k: None),
        "fal_client": None,
        "registry": types.SimpleNamespace(register=lambda *a, **k: None),
        "tool_error": lambda *a, **k: "",
    }
    tree = ast.parse(src)
    # the module-local exception class (used by claimed siblings; exec for fidelity)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "ImageGenerationInterrupted":
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass

    def consts():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass

    consts()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    consts()  # re-seed constants that reference functions
    # catalog constants must survive the module's own import-shadowing assigns
    ns["FAL_MODELS"] = cat["FAL_MODELS"]
    ns["DEFAULT_MODEL"] = cat["DEFAULT_MODEL"]
    ns["DEFAULT_ASPECT_RATIO"] = cat["DEFAULT_ASPECT_RATIO"]
    ns["VALID_ASPECT_RATIOS"] = cat["VALID_ASPECT_RATIOS"]
    missing = [t for t in PY.values() if t not in ns]
    for c in REQUIRED_CONSTS:
        if c not in ns:
            missing.append(c)
    if missing:
        print(f"FATAL oracle_image_tools_flow: missing defs/consts: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) if nm in NS}
REAL_CATALOG = NS["FAL_MODELS"]
CURRENT = {"ctx": None}


# ── exception materialization (REAL builtin trio; module-local classes) ─────
EXC_CLASSES = {
    "OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "KeyError": KeyError, "AttributeError": AttributeError,
    "Exception": Exception, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "StopIteration": StopIteration,
    "ZeroDivisionError": ZeroDivisionError, "IndexError": IndexError,
    "UnicodeDecodeError": UnicodeDecodeError, "TimeoutError": TimeoutError,
    "NotImplementedError": NotImplementedError, "JSONDecodeError": json.JSONDecodeError,
    "ImageResolutionError": ImageResolutionError,
    "DecompressionBombError": DecompressionBombError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "ImageGenerationInterrupted" and "ImageGenerationInterrupted" in NS:
        return NS["ImageGenerationInterrupted"](msg)
    if kind == "JSONDecodeError":
        return json.JSONDecodeError(msg, "", 0)
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── provider / PIL doubles ──────────────────────────────────────────────────
class ProviderDouble:
    def __init__(self, desc):
        self.desc = desc

    def capabilities(self):
        return CURRENT["ctx"].call("provider.capabilities", {})

    def generate(self, **kw):
        return CURRENT["ctx"].call("provider.generate", proj(kw))

    @property
    def display_name(self):
        if "display_name" in self.desc:
            return self.desc["display_name"]
        raise AttributeError("'ProviderDouble' object has no attribute 'display_name'")


def mat_provider(r, a, k):
    return ProviderDouble(r) if isinstance(r, dict) else r


class FakeImage:
    def __init__(self, desc):
        self.desc = desc if isinstance(desc, dict) else {}

    def load(self):
        return CURRENT["ctx"].call("pil_load", {})

    def getbands(self):
        return tuple(self.desc.get("bands") or [])

    @property
    def info(self):
        return self.desc.get("info") or {}

    @property
    def size(self):
        return tuple(self.desc.get("size") or [])

    def convert(self, mode):
        r = CURRENT["ctx"].call("pil_convert", {"mode": mode})
        return FakeImage(r) if isinstance(r, dict) else r

    def copy(self):
        return FrameDouble(self.desc)


class FrameDouble:
    def __init__(self, desc):
        self.desc = desc
        self._side = None
        self.size = tuple(desc.get("size") or [])

    def thumbnail(self, box):
        self._side = box[0]

    def save(self, buf, fmt, quality=90):
        r = CURRENT["ctx"].call("pil_encode",
                                {"side": self._side, "fmt": fmt, "img": self.desc})
        if isinstance(r, dict):
            buf._out = (r.get("b64") or "").encode("ascii")
            if isinstance(r.get("size"), list):
                self.size = tuple(r["size"])
        return r


class BufDouble:
    def __init__(self, x=None):
        self.value = x
        self._out = b""

    def getvalue(self):
        return self._out


# ── per-case script backend (fail-closed; record then raise) ────────────────
class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


# ── projection (live object -> JSON-comparable) ─────────────────────────────
def proj(v):
    if isinstance(v, ProviderDouble):
        return proj(v.desc)
    if isinstance(v, BaseException):
        return proj_exc(v)
    if isinstance(v, dict):
        return {k: proj(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [proj(x) for x in v]
    if isinstance(v, set):
        return sorted(proj(x) for x in v)
    return v


def canon(v):
    if isinstance(v, float):
        if math.isnan(v) or math.isinf(v):
            return v
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def deep_eq(a, b):
    return canon(proj(a)) == canon(proj(b))


# ── seam recorders ──────────────────────────────────────────────────────────
def rec(name, argbuilder=None, mat=None):
    def f(*a, **k):
        args = argbuilder(*a, **k) if argbuilder else {}
        r = CURRENT["ctx"].call(name, args)
        return mat(r, a, k) if mat else r
    return f


def _log_recorder(ctx, level):
    def log(fmt, *a, **k):
        msg = fmt % a if a else fmt
        return ctx.call("logger." + level, {"msg": msg})
    return log


def install_seams(ctx, skip):
    CURRENT["ctx"] = ctx
    ns = NS
    for nm, obj in REAL.items():
        ns[nm] = obj
    ns["logger"] = types.SimpleNamespace(
        debug=_log_recorder(ctx, "debug"), info=_log_recorder(ctx, "info"),
        warning=_log_recorder(ctx, "warning"), error=_log_recorder(ctx, "error"))
    # json stays REAL (C computes dumps/loads); os stays REAL.
    ns["json"] = json
    # CLAIMED siblings -> recorders
    ns["_read_configured_image_provider"] = rec("read_configured_image_provider", lambda: {})
    ns["_read_configured_image_model"] = rec("read_configured_image_model", lambda: {})
    ns["_upscale_image"] = rec("upscale_image",
                               lambda image_url, original_prompt:
                               {"image_url": image_url, "original_prompt": original_prompt})
    ns["fal_key_is_configured"] = rec("fal_key_set", lambda: {})
    ns["_resolve_managed_fal_gateway"] = rec("resolve_managed_fal_gateway", lambda: {})
    ns["_build_no_backend_setup_message"] = rec("build_no_backend_setup_message", lambda: {})

    def _igt_args(prompt, aspect_ratio, upscale=None, fal_model=None, **kw):
        d = {"prompt": prompt, "aspect_ratio": aspect_ratio}
        if upscale is not None:
            d["upscale"] = upscale
        if fal_model is not None:
            d["fal_model"] = fal_model
        return d
    ns["image_generate_tool"] = rec("image_generate_tool", _igt_args)
    _install_module_stubs(ctx, skip)


def _install_module_stubs(ctx, skip):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    def pkg(name):
        m = sys.modules.get(name)
        if not isinstance(m, types.ModuleType):
            m = types.ModuleType(name)
            m.__path__ = []
            sys.modules[name] = m
        return m

    for p in ("agent", "hermes_cli", "tools", "plugins", "plugins.image_gen", "model_tools"):
        if p == "model_tools":
            mod("model_tools", _run_async=rec("_run_async", lambda arg: {"arg": proj(arg)}))
        else:
            pkg(p)
    mod("hermes_cli.config", load_config=rec("load_config", lambda: {}))
    mod("hermes_cli.plugins",
        _ensure_plugins_discovered=rec("ensure_plugins_discovered",
                                       lambda force=False: {"force": True} if force else {}))
    mod("tools.image_source",
        ImageResolutionError=ImageResolutionError,
        resolve_local_source_to_data_url=rec("resolve_local_source_to_data_url",
                                             lambda s, task_id=None, **k: {"source": s}))
    mod("agent.image_gen_registry",
        get_provider=rec("get_provider", lambda name: {"name": name}, mat_provider))
    mod("agent.image_gen_provider",
        normalize_reference_images=rec("normalize_reference_images",
                                       lambda value: {"refs": proj(value)}))

    def _mr_args(provider, model_id):
        d = {}
        if provider is not None:
            d["provider"] = provider
        if model_id is not None:
            d["model_id"] = model_id
        return d
    mod("tools.image_generation_managed",
        KREA="krea", PORTAL="portal",
        managed_route=rec("managed_route", _mr_args))
    mod("plugins.image_gen.krea",
        _resolve_managed_krea_gateway=rec("resolve_managed_krea_gateway", lambda: {}),
        fallback_to_fal_enabled=rec("fallback_to_fal_enabled", lambda: {}))

    # PIL / base64 / io doubles for _shrink_data_url
    fake_image = types.SimpleNamespace(
        open=lambda bio: mat_provider_img(ctx, bio),
        DecompressionBombError=DecompressionBombError)
    fake_imageops = types.SimpleNamespace(
        exif_transpose=lambda img: _transpose(ctx, img))
    mod("PIL", Image=fake_image, ImageOps=fake_imageops)
    mod("base64",
        b64decode=lambda s: s,
        b64encode=lambda b: b if isinstance(b, bytes) else b.encode("ascii"))
    mod("io", BytesIO=BufDouble)


def mat_provider_img(ctx, bio):
    r = ctx.call("pil_open", {"b64": getattr(bio, "value", None)})
    return FakeImage(r) if isinstance(r, dict) else r


def _transpose(ctx, img):
    r = ctx.call("exif_transpose", {"img": img.desc})
    return FakeImage(r) if isinstance(r, dict) else r


# ── per-func live drive ─────────────────────────────────────────────────────
def run_case(func, inp):
    ctx = Ctx(inp)
    install_seams(ctx, PY[func])
    ns = NS
    # per-case catalog override drives both sides equally
    ov = inp.get("catalog")
    ns["FAL_MODELS"] = ov if isinstance(ov, dict) else REAL_CATALOG
    g = inp.get
    saved_io = sys.modules.get("io")
    saved_b64 = sys.modules.get("base64")
    saved_pil = sys.modules.get("PIL")
    out = {}
    ret = None
    try:
        if func == "_read_image_gen_key":
            ret = ns[func](g("key", ""))
        elif func == "_plugin_provider_name":
            ret = ns[func]()
        elif func == "_build_payload":
            ret = ns[func](g("model_id", ""), g("prompt"), g("aspect_ratio"),
                           g("seed"), g("overrides"), image_urls=g("image_urls"))
        elif func == "_build_fal_payload":
            ret = ns[func](g("model_id", ""), g("prompt"),
                           aspect_ratio=g("aspect_ratio", ns["DEFAULT_ASPECT_RATIO"]),
                           seed=g("seed"), overrides=g("overrides"))
        elif func == "_build_fal_edit_payload":
            ret = ns[func](g("model_id", ""), g("prompt"), g("image_urls"),
                           aspect_ratio=g("aspect_ratio", ns["DEFAULT_ASPECT_RATIO"]),
                           seed=g("seed"), overrides=g("overrides"))
        elif func == "_format_images":
            ret = ns[func](g("images") or [], g("should_upscale", False), g("prompt", ""))
        elif func == "_inline_local_sources":
            ret = ns[func](g("sources") or [])
        elif func == "_fit_inline_budget":
            ret = ns[func](g("urls") or [], g("budget", 0))
        elif func == "_inline_fal_sources":
            ret = ns[func](g("sources") or [])
        elif func == "_shrink_data_url":
            ret = ns[func](g("url", ""), g("budget", 0))
        elif func == "_prepare_fal_request":
            ret = ns[func](g("model_id", ""), g("meta") or {}, g("prompt"),
                           g("aspect_ratio"), g("seed"), g("overrides"),
                           g("source_images") or [])
        elif func == "check_fal_api_key":
            ret = ns[func]()
        elif func == "_get_plugin_provider":
            ret = ns[func](g("name", ""), force=g("force", False))
        elif func == "_provider_error":
            ret = ns[func](g("error", ""), g("error_type", ""))
        elif func == "_provider_result":
            ret = ns[func](g("result"), g("contract_error", ""))
        elif func == "_add_provider_kwargs":
            kw = copy.deepcopy(g("kwargs") or {})
            ret = ns[func](kw, g("image_url"), g("reference_image_urls"), g("upscale"),
                           model=g("model"), controls=g("controls"))
        elif func == "_declared_controls":
            prov = g("provider")
            prov = ProviderDouble(prov) if isinstance(prov, dict) else prov
            ret = ns[func](prov, g("controls"))
        elif func == "_managed_model_plugin":
            ret = ns[func]()
        elif func == "_maybe_route_managed_model":
            ret = ns[func](g("prompt", ""), g("aspect_ratio", "landscape"),
                           image_url=g("image_url"),
                           reference_image_urls=g("reference_image_urls"),
                           upscale=g("upscale"), controls=g("controls"))
        elif func == "_fall_back_to_fal":
            ret = ns[func](g("raw"), g("prompt", ""), g("aspect_ratio", "landscape"),
                           g("sources") or {})
        else:
            raise ValueError("unknown func " + func)
        ret = proj(ret)
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity
        ret = proj_exc(exc)
    finally:
        # restore real io/base64/PIL so the oracle process is unaffected
        if saved_io is not None:
            sys.modules["io"] = saved_io
        if saved_b64 is not None:
            sys.modules["base64"] = saved_b64
        if saved_pil is not None:
            sys.modules["PIL"] = saved_pil
        ns["FAL_MODELS"] = REAL_CATALOG
    out["ret"] = ret
    out["trace"] = ctx.trace
    out["state"] = copy.deepcopy(inp.get("state") or {})
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            recd = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = recd.get("func")
        inp = recd.get("in", {}) or {}
        c_out = recd.get("out")
        cases += 1
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            py_out = run_case(func, inp)
        except BaseException as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        bad = []
        for key, want in py_out.items():
            got = c_out.get(key)
            if not deep_eq(got, want):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(proj(c_out.get(key))), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(proj(py_out.get(key))), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_image_tools_flow: 0 cases")
        print("ORACLE image_tools_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE image_tools_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


def seam_names():
    return ["load_config", "logger.debug", "logger.info", "logger.warning", "logger.error",
            "read_configured_image_provider", "read_configured_image_model", "upscale_image",
            "fal_key_set", "resolve_managed_fal_gateway", "build_no_backend_setup_message",
            "image_generate_tool", "resolve_local_source_to_data_url", "_run_async",
            "ensure_plugins_discovered", "get_provider", "normalize_reference_images",
            "managed_route", "resolve_managed_krea_gateway", "fallback_to_fal_enabled",
            "provider.capabilities", "provider.generate", "pil_open", "pil_load",
            "exif_transpose", "pil_convert", "pil_encode"]


def self_test(fixpath):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    names = set(PY.values()) | set(seam_names())
    seen = set()
    problems = []
    n = 0
    for line in open(fixpath, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("{"):
            continue
        recd = json.loads(line)
        func = recd["func"]
        seen.add(func)
        n += 1
        try:
            out = run_case(func, recd.get("in") or {})
        except BaseException as exc:  # noqa: BLE001
            problems.append(f"DRIVE-FAIL {func}: {type(exc).__name__}: {exc}")
            continue
        ret = out.get("ret")
        if isinstance(ret, dict) and ret.get("__raise__") == "AttributeError":
            msg = ret.get("message", "")
            for nm in names:
                if f"'{nm}'" in msg:
                    problems.append(f"SUSPECT-ATTR {func}: {msg}")
                    break
    missing = set(PY) - seen
    if missing:
        problems.append(f"FIXTURE missing funcs: {sorted(missing)}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELF-TEST image_tools_flow: {n} cases, {len(problems)} problems")
    for p in problems[:40]:
        print("  " + p)
    return 0 if not problems else 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        fix = sys.argv[2] if len(sys.argv) > 2 else str(_HERE.parent / "fixtures" / "image_tools_flow.jsonl")
        sys.exit(self_test(fix))
    sys.exit(main())
