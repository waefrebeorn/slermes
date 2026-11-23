#!/usr/bin/env python3
"""Differential oracle for w14_methodsima_f49896 (tui_gateway/methods_images.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
S = Q + "/tui_gateway/methods_images.py"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [Q + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg
_mc = types.ModuleType("tui_gateway.method_ctx")
class _HR:
    def __init__(self):
        self._pending = []
    def method(self, name=None):
        def d(fn):
            self._pending.append((name, fn))
            return fn
        return d
_mc.HandlerRegistry = _HR
_mc.bind_module = lambda *a, **kw: None
sys.modules["tui_gateway.method_ctx"] = _mc

spec = importlib.util.spec_from_file_location("tui_gateway.methods_images_w14", S)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)
live.json = json  # methods_images.py references server.py's json global
BY = dict(live._registry._pending)
live._ok = lambda rid, result: result
live._err = lambda rid, code, message, data=None: {"code": code, "error": message}

TRUTHY = frozenset({"1", "true", "yes", "on"})
def is_truthy_value(value, default=False):
    if value is None: return default
    if isinstance(value, str): return value.strip().lower() in TRUTHY
    return bool(value)
live.is_truthy_value = is_truthy_value

INLINE_MODES = ("auto", "kitty", "iterm", "unicode", "off")
def resolve_mode(configured, graphics, detected):
    if configured is False: return "off"
    mode = "auto" if configured in (None, True) else str(configured).strip().lower()
    mode = mode if mode in INLINE_MODES else "auto"
    if mode == "iterm": mode = "auto"
    if mode == "off": return "off"
    if mode == "unicode" or not graphics: return "unicode"
    if mode in ("kitty", "iterm"): return mode
    return "kitty" if detected == "kitty" else "unicode"

def mod(name, **attrs):
    m = types.ModuleType(name); m.__path__ = []
    for k, v in attrs.items(): setattr(m, k, v)
    return m

def compute(r):
    f, env = r.get("func"), (r.get("env") or {})
    params = dict(r.get("params") or {})
    if f == "image_generate":
        gen_err = env.get("gen_error")
        def check(): return bool(env.get("available"))
        def generate(args):
            if gen_err: raise Exception(gen_err)
            return json.dumps(env.get("gen") or {})
        keys = [("tools", mod("tools")),
                ("tools.image_generation_tool", mod("tools.image_generation_tool",
                    check_image_generation_requirements=check, _handle_image_generate=generate))]
        fn = BY["image.generate"]
    elif f == "image_render":
        cfg = {"inline_images": env.get("inline_images", "auto")}
        live._display_cfg = lambda: cfg
        def local_path(candidate):
            return "/fake/input.png" if env.get("path_present") else None
        rdata = env.get("render")
        obj = types.SimpleNamespace(**rdata) if isinstance(rdata, dict) else None
        ti = mod("agent.terminal_image",
                 local_image_path=local_path,
                 resolve_inline_mode=lambda configured, *, graphics, detected, **kw: resolve_mode(configured, graphics, detected),
                 render_inline_image=lambda *a, **kw: obj)
        keys = [("agent", mod("agent")), ("agent.terminal_image", ti)]
        fn = BY["image.render"]
    else:
        return None
    saved = {k: sys.modules.get(k) for k, _ in keys}
    try:
        for k, m in keys: sys.modules[k] = m
        return fn("rid1", params)
    finally:
        for k, v in saved.items():
            if v is None: sys.modules.pop(k, None)
            else: sys.modules[k] = v

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") is None: continue
    cases += 1
    try: py = compute(r)
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')} {r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1; continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_methodsima_f49896: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
