#!/usr/bin/env python3
"""Differential oracle for the wave-2 revival of skill_usage.c + model_metadata_gaps.c.

Reads the harness JSONL on stdin ({func, in, out}), recomputes the LIVE quarry
reference from `in` driven by the SAME scripted seams, and diffs ordered trace +
return. Pure leaves diff the value directly. Fail-closed: any reference exception
becomes a {"__raise__": ...} expectation; a harness crash is caught by the runner.

Reference quarry (READ-ONLY): C:/Users/eman5/hermes-agent
  tools/skill_usage.py, agent/model_metadata.py, agent/usage_anchor.py

Projections (fixed C signatures narrower than Python):
  latest_activity_at: C double = epoch of the newest timestamp (0.0 == None); the
    faithful Optional[str] is compared via the raw accessor field.
  is_hub_installed / is_bundled: C takes a record; name = record.get("name") or "".
  restore_skill: live tuple[bool, str] -> (rc 0/-1, msg).
  anchored_context_tokens: _anchor_matches + estimate_messages_tokens_rough seamed.
  verified_codex_ctx_for_slug: Optional[int] -> value or null.
"""
import sys, os, json, ast, types, math, re, shutil
from pathlib import Path
from datetime import datetime, timezone, UTC

_REPO = Path(__file__).resolve().parent.parent
_QUARRY = Path("C:/Users/eman5/hermes-agent")
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))


def norm(x):
    """Integral floats -> int (libjson serializes them int-like); recurse."""
    if isinstance(x, bool):
        return x
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x):
            return int(x)
        return x
    if isinstance(x, list):
        return [norm(i) for i in x]
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    return x


def posix(p):
    return Path(p).as_posix()


def extract(module_path, func_names, ns):
    src = module_path.read_text(encoding="utf-8")
    tree = ast.parse(src)
    fname = str(module_path)

    def safe_target(t):
        if isinstance(t, ast.Name):
            return True
        if isinstance(t, (ast.Tuple, ast.List)):
            return all(isinstance(e, ast.Name) for e in t.elts)
        return False

    def seed():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if all(safe_target(t) for t in tgts):
                    try:
                        exec(compile(ast.get_source_segment(src, node), fname, "exec"), ns)
                    except Exception:
                        pass
    seed()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in func_names:
            try:
                exec(compile(ast.get_source_segment(src, node), fname, "exec"), ns)
            except Exception:
                pass
    seed()  # constants that call extracted functions
    return ns


# ---- third-party stubs (leaf symbols only; runtime calls are seamed) ----
def install_module_stubs():
    import logging, threading
    hc = types.ModuleType("hermes_constants")
    hc.get_hermes_home = lambda: Path(os.environ.get("HERMES_HOME", "."))
    sys.modules["hermes_constants"] = hc
    su = types.ModuleType("agent.skill_utils")
    su.is_excluded_skill_path = lambda p: False
    su.is_external_skill_path = lambda p: False
    su.iter_skill_index_files = lambda base, name: []
    su.get_all_skills_dirs = lambda: []
    sys.modules.setdefault("agent", types.ModuleType("agent"))
    sys.modules["agent.skill_utils"] = su
    ut = types.ModuleType("utils")
    ut.atomic_write_text = lambda *a, **k: None
    sys.modules["utils"] = ut
    # hermes_cli.config.load_config (in-function import in _prune_builtins_enabled)
    sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
    cfg = types.ModuleType("hermes_cli.config")
    cfg.load_config = lambda: {}
    sys.modules["hermes_cli.config"] = cfg
    # tools.skill_ledger (in-function import in _relocate)
    tools = types.ModuleType("tools")
    ledger = types.ModuleType("tools.skill_ledger")
    ledger.capture_before = lambda *a, **k: []
    ledger.record_mutation = lambda *a, **k: None
    tools.skill_ledger = ledger
    sys.modules["tools"] = tools
    sys.modules["tools.skill_ledger"] = ledger
    # agent.model_metadata.estimate_messages_tokens_rough (in-function import)
    mm = types.ModuleType("agent.model_metadata")
    mm.estimate_messages_tokens_rough = lambda messages, **k: 0
    sys.modules["agent.model_metadata"] = mm


install_module_stubs()

SU = extract(_QUARRY / "tools/skill_usage.py", {
    "latest_activity_at", "_parse_iso_timestamp", "_prune_builtins_enabled",
    "is_hub_installed", "is_bundled", "_external_read_only_message",
    "restore_skill", "_relocate", "_archive_dir", "_skills_dir", "read_skill_name",
    "_read_hub_installed_names", "_read_bundled_names", "set_state",
    "_toggle_suppressed_name",
}, {"__name__": "su_live", "logging": __import__("logging"),
    "threading": __import__("threading"), "re": re, "json": json,
    "datetime": datetime, "timezone": timezone, "UTC": UTC,
    "suppress": __import__("contextlib").suppress,
    "contextmanager": __import__("contextlib").contextmanager,
    "Path": Path, "os": os})

MM = extract(_QUARRY / "agent/model_metadata.py", {
    "_stale_pre_catalog_cache_entry", "_verified_codex_ctx_for_slug",
    "_catalog_key_matches", "_normalize_model_version", "_codex_variant_base",
    "_bare_codex_slug", "is_codex_900k_base",
}, {"__name__": "mm_live", "re": re, "math": math})

UA = extract(_QUARRY / "agent/usage_anchor.py", {
    "anchored_context_tokens", "_anchor_matches",
}, {"__name__": "ua_live", "json": json, "hashlib": __import__("hashlib"),
    "logging": __import__("logging")})


class RaiseSpec(Exception):
    pass


def make_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    builtins = {"OSError": OSError, "ValueError": ValueError, "TypeError": TypeError,
                "ImportError": ImportError, "KeyError": KeyError, "Exception": Exception,
                "RuntimeError": RuntimeError, "AttributeError": AttributeError}
    if cls == "KeyError":
        return KeyError(msg)
    klass = builtins.get(cls)
    if klass is None:
        klass = type(cls, (Exception,), {})
    return klass(msg)


def is_raise(v):
    return isinstance(v, dict) and "__raise__" in v


PATH_KEYED = {"exists", "is_dir", "is_file", "read_skill_name"}


def materialize(name, args, io):
    """Mirror the harness rev_materialize(): return the scripted value (or raise)."""
    script = io.get(name)
    if is_raise(script):
        return script  # caller raises after recording
    if name in PATH_KEYED:
        key = args.get("path")
        if isinstance(script, dict) and key in script:
            return script[key]
        if name == "read_skill_name":
            return args.get("fallback", "")
        return False
    if script is None:
        return None
    return script


class Trace:
    def __init__(self, io):
        self.io = io or {}
        self.events = []

    def rec(self, ev, args, ret):
        self.events.append({"ev": ev, "args": norm(args), "ret": norm(ret)})

    def call(self, name, args):
        """Record a seam call and return its scripted result (raising if scripted)."""
        ret = materialize(name, args, self.io)
        self.rec(name, args, ret)
        if is_raise(ret):
            raise make_exc(ret)
        return ret


def ref_latest_activity_at(inp):
    record = inp.get("record") or {}
    raw = SU["latest_activity_at"](record)
    if raw is None:
        epoch = 0.0
    else:
        dt = SU["_parse_iso_timestamp"](raw)
        epoch = dt.timestamp() if dt is not None else 0.0
    return {"raw": raw, "epoch": epoch}


def ref_message(inp):
    return {"msg": SU["_external_read_only_message"](inp.get("skill_name"))}


def ref_stale(inp):
    return {"value": bool(MM["_stale_pre_catalog_cache_entry"](inp.get("model"), inp.get("cached")))}


def ref_verified(inp):
    return {"value": MM["_verified_codex_ctx_for_slug"](inp.get("model_bare"))}


def ref_prune(inp):
    tr = Trace(inp.get("io"))
    cfgmod = sys.modules["hermes_cli.config"]

    def load_config():
        return tr.call("load_config", {})
    cfgmod.load_config = load_config
    try:
        val = SU["_prune_builtins_enabled"]()
    finally:
        cfgmod.load_config = lambda: {}
    return {"value": bool(val), "trace": tr.events}


def ref_hub_bundled(inp, func):
    tr = Trace(inp.get("io"))
    record = inp.get("record") or {}
    name = record.get("name") or ""
    if func == "is_hub_installed":
        live_fn, seam, reader = SU["is_hub_installed"], "hub_names", "_read_hub_installed_names"
    else:
        live_fn, seam, reader = SU["is_bundled"], "bundled_names", "_read_bundled_names"

    def read_names():
        ret = tr.call(seam, {})
        return set(ret or [])
    SU[reader] = read_names
    try:
        val = live_fn(name)
    finally:
        SU[reader] = lambda: set()
    return {"value": bool(val), "trace": tr.events}


def ref_anchored(inp):
    tr = Trace(inp.get("io"))
    messages = inp.get("messages")
    anchor = inp.get("anchor")
    charge = inp.get("charge_stale_thinking", True)

    def anchor_matches(msgs, anc):
        return tr.call("anchor_matches", {"messages": msgs, "anchor": anc})
    UA["_anchor_matches"] = anchor_matches

    mmmod = sys.modules["agent.model_metadata"]

    def estimate(msgs, *, charge_stale_thinking=True):
        return tr.call("estimate_messages_tokens_rough",
                       {"messages": msgs, "charge_stale_thinking": charge_stale_thinking})
    mmmod.estimate_messages_tokens_rough = estimate
    try:
        val = UA["anchored_context_tokens"](messages, anchor, charge_stale_thinking=charge)
    finally:
        UA["_anchor_matches"] = lambda msgs, anc: False
        mmmod.estimate_messages_tokens_rough = lambda msgs, **k: 0
    return {"value": val, "trace": tr.events}


def ref_restore(inp):
    tr = Trace(inp.get("io"))
    home = inp.get("home", "")
    name = inp.get("name", "")
    SU["get_hermes_home"] = lambda: Path(home)

    # module-function recorders
    SU["is_hub_installed"] = lambda nm: tr.call("is_hub_installed", {"name": nm})
    SU["is_bundled"] = lambda nm: tr.call("is_bundled", {"name": nm})
    SU["_prune_builtins_enabled"] = lambda: tr.call("prune_builtins_enabled", {})
    SU["read_skill_name"] = lambda path, fallback: tr.call(
        "read_skill_name", {"path": posix(path), "fallback": fallback})
    SU["_toggle_suppressed_name"] = lambda nm, *, add: tr.call(
        "toggle_suppressed_name", {"name": nm, "add": add})
    SU["set_state"] = lambda nm, state: tr.call("set_state", {"name": nm, "state": state})

    # pathlib filesystem double
    orig = {m: getattr(Path, m) for m in ("exists", "rglob", "is_dir", "is_file", "rename")}

    def p_exists(self):
        return tr.call("exists", {"path": posix(self)})

    def p_rglob(self, pattern):
        ret = tr.call("rglob", {"root": posix(self), "pattern": pattern})
        return iter(Path(p) for p in (ret or []))

    def p_is_dir(self):
        return tr.call("is_dir", {"path": posix(self)})

    def p_is_file(self):
        return tr.call("is_file", {"path": posix(self)})

    def p_rename(self, dest):
        return tr.call("rename", {"src": posix(self), "dest": posix(dest)})

    Path.exists, Path.rglob, Path.is_dir, Path.is_file, Path.rename = \
        p_exists, p_rglob, p_is_dir, p_is_file, p_rename

    orig_move = shutil.move

    def s_move(src, dest):
        return tr.call("move", {"src": posix(src), "dest": posix(dest)})
    shutil.move = s_move

    orig_utime = os.utime

    def o_utime(path, *a, **k):
        return tr.call("utime", {"path": posix(path)})
    os.utime = o_utime

    ledger = sys.modules["tools.skill_ledger"]
    ledger.capture_before = lambda src, **k: tr.call("ledger_capture_before", {"src": posix(src)})
    ledger.record_mutation = lambda action, nm, *, before, after_root: tr.call(
        "ledger_record_mutation",
        {"action": action, "name": nm, "before": before, "after_root": posix(after_root)})

    try:
        ok, msg = SU["restore_skill"](name)
    finally:
        for m, fn in orig.items():
            setattr(Path, m, fn)
        shutil.move = orig_move
        os.utime = orig_utime
        ledger.capture_before = lambda *a, **k: []
        ledger.record_mutation = lambda *a, **k: None
        SU["get_hermes_home"] = lambda: Path(os.environ.get("HERMES_HOME", "."))
    # str(Path) flips slashes on Windows; the C port builds paths with '/'.
    # Normalize the embedded dest path so the message compares equal.
    msg = msg.replace("\\", "/")
    return {"rc": 0 if ok else -1, "msg": msg, "trace": tr.events}


DISPATCH = {
    "latest_activity_at": ref_latest_activity_at,
    "_external_read_only_message": ref_message,
    "_stale_pre_catalog_cache_entry": ref_stale,
    "_verified_codex_ctx_for_slug": ref_verified,
    "_prune_builtins_enabled": ref_prune,
    "is_hub_installed": lambda inp: ref_hub_bundled(inp, "is_hub_installed"),
    "is_bundled": lambda inp: ref_hub_bundled(inp, "is_bundled"),
    "anchored_context_tokens": ref_anchored,
    "restore_skill": ref_restore,
}


def reference(func, inp):
    fn = DISPATCH.get(func)
    if fn is None:
        return {"__raise__": "ValueError", "msg": "unknown func " + func}
    try:
        return norm(fn(inp))
    except BaseException as e:  # fail-closed: SystemExit included
        return {"__raise__": type(e).__name__, "msg": str(e)}


def main():
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.rstrip("\n")
        if not line.strip():
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            print("BAD JSONL: %s" % e, file=sys.stderr)
            mism += 1
            continue
        n += 1
        func = rec.get("func")
        inp = rec.get("in") or {}
        got = norm(rec.get("out"))
        want = reference(func, inp)
        if got != want:
            mism += 1
            print("MISMATCH %s" % func)
            print("  in:   %s" % json.dumps(inp, ensure_ascii=False))
            print("  got:  %s" % json.dumps(got, ensure_ascii=False))
            print("  want: %s" % json.dumps(want, ensure_ascii=False))
    print("ORACLE skill_model_revive: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()