#!/usr/bin/env python3
"""Differential oracle for agent/provider_route_hooks.py (w13_providerro_daa14d).

Provider registry seams are fixture-driven: `providers`/`hosts` map a name or
base_url to a profile object in `profiles`. A profile is {name, overrides,
answers, raises, api_mode, model_mode_answer, model_mode_raises}. Sticky
failures are scoped per (name, hook); the oracle projects the live
(id, type, name, hook) set down to sorted [name, hook] pairs to match the port.
"""
import copy
import importlib.util
import json
import logging
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


HOOKS = ["client_headers", "request_headers", "max_tokens_param", "reasoning_payload",
         "replays_function_thought_signature", "refresh_runtime_credentials",
         "api_mode_for_model"]


class ProviderProfile:
    name = "?"
    api_mode = "chat_completions"

    def client_headers(self, *a, **k): return None
    def request_headers(self, *a, **k): return None
    def max_tokens_param(self, *a, **k): return None
    def reasoning_payload(self, *a, **k): return None
    def replays_function_thought_signature(self, *a, **k): return None
    def refresh_runtime_credentials(self, *a, **k): return None
    def api_mode_for_model(self, model, api_key=None): return None


providers_pkg = _mod("providers")
providers_pkg.__path__ = []
providers_pkg.base = _mod("providers.base", ProviderProfile=ProviderProfile)
REC = [{}]
PROFILES = {}


def get_provider_profile(name):
    nm = (REC[0].get("providers") or {}).get(name)
    return PROFILES.get(nm) if isinstance(nm, str) else None


def profile_for_host(base_url):
    nm = (REC[0].get("hosts") or {}).get(base_url)
    return PROFILES.get(nm) if isinstance(nm, str) else None


providers_pkg.get_provider_profile = get_provider_profile
providers_pkg.profile_for_host = profile_for_host
providers_pkg.registry_generation = lambda: 0

SRC = Q + "/agent/provider_route_hooks.py"
spec = importlib.util.spec_from_file_location("provider_route_hooks_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["provider_route_hooks_live"] = live
spec.loader.exec_module(live)


def norm(v):
    if isinstance(v, tuple):
        return [norm(x) for x in v]
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


def _hook_fn(h):
    def f(self, *a, **k):
        if h in self.raises:
            raise RuntimeError("hook failed")
        return copy.deepcopy(self.answers.get(h))
    return f


def _mode_fn(self, model, api_key=None):
    if self.model_mode_raises:
        raise RuntimeError("mode failed")
    return copy.deepcopy(self.model_mode_answer)


def make_cls(overrides):
    attrs = {}
    for h in HOOKS:
        if h not in overrides:
            attrs[h] = getattr(ProviderProfile, h)
    for h in overrides:
        attrs[h] = _hook_fn(h)
    attrs["api_mode_for_model"] = _mode_fn
    return type("P", (ProviderProfile,), attrs)


def build_profiles(rec):
    PROFILES.clear()
    pds = list(rec.get("profiles") or [])
    if isinstance(rec.get("profile"), dict) and rec["profile"].get("name") not in [p.get("name") for p in pds]:
        pds.append(rec["profile"])
    for pd in pds:
        name = pd.get("name", "")
        ov = set(pd.get("overrides") or [])
        if pd is rec.get("profile"):
            ov |= set((rec.get("answers") or {}).keys())
            ov |= set(rec.get("raises") or [])
        obj = make_cls(ov)()
        obj.name = name
        obj.answers = pd.get("answers") or {}
        obj.raises = set(pd.get("raises") or [])
        obj.api_mode = pd.get("api_mode")
        obj.model_mode_answer = pd.get("model_mode_answer")
        obj.model_mode_raises = bool(pd.get("model_mode_raises", False))
        PROFILES[name] = obj
    return PROFILES


def build_scope(rec):
    sc_spec = rec.get("scope")
    if sc_spec is None:
        return None
    from types import SimpleNamespace
    sc = SimpleNamespace()
    failed = set()
    for pair in sc_spec.get("_provider_hook_failures") or []:
        nm, hk = pair[0], pair[1]
        p = PROFILES.get(nm)
        if p is None:
            p = SimpleNamespace(name=nm)
            PROFILES[nm] = p
        failed.add((id(p), type(p), nm, hk))
    if failed or "_provider_hook_failures" in sc_spec:
        sc._provider_hook_failures = failed
    return sc


def proj_failures(scope):
    if scope is None:
        return []
    return sorted([[k[2], k[3]] for k in scope._provider_hook_failures])


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    REC[0] = r
    build_profiles(r)
    try:
        if func == "freeze":
            py = norm(live._freeze(r.get("value")))
        elif func == "forces_max":
            py = live.model_forces_max_completion_tokens(r.get("model"))
        elif func == "route":
            py = [getattr(p, "name", "?") for p in live.route_profiles(
                      r.get("hook", ""), r.get("provider", ""), r.get("base_url", ""))]
        elif func == "ask":
            sc = build_scope(r)
            p = PROFILES[r["profile"]["name"]]
            p.answers = r.get("answers") or {}
            p.raises = set(r.get("raises") or [])
            ret = live.ask_profile(p, r.get("hook", ""), r.get("fallback"), scope=sc)
            py = {"ret": norm(ret), "failures": proj_failures(sc)}
        elif func == "hook_failed":
            sc = build_scope(r)
            p = PROFILES[r["profile"]["name"]]
            py = live.hook_failed(p, r.get("hook", ""), sc)
        elif func == "model_api_mode":
            py = live.model_api_mode(PROFILES[r["profile"]["name"]], r.get("model", ""))
        elif func == "route_answer":
            sc = build_scope(r)
            ret = live.route_answer(r.get("hook", ""), provider=r.get("provider", ""),
                                    scope=sc, base_url=r.get("base_url", ""))
            py = {"ret": norm(ret), "failures": proj_failures(sc)}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_providerro_daa14d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
