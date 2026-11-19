#!/usr/bin/env python3
"""oracle_tail_revive2.py — differential oracle for revival tail group 2.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the
LIVE quarry defs, seeds module constants (Assign + AnnAssign), drives each
live function under the SAME scripted seam results the C harness consumed,
and compares ordered trace + ret + post-state. BaseException is caught per
case (SystemExit included); missing/exhausted scripts fail closed on both
sides.

Covered ports (22): approval_gaps (is_unattended_platform_approval_context,
is_single_query_approval_context, should_fall_through_to_cli_approval,
is_shell_token_spliced_gateway_lifecycle), main_gaps (argv_is_gateway_run,
desktop_linux_userns_sandbox_available,
desktop_linux_needs_disable_setuid_sandbox), plugin_dev_gaps (_has_manifest,
holds_plugin, is_plugin_id, accepts_var_kwargs), _subprocess_compat_gaps
(text_names_hermes, process_command_is_hermes, pid_is_hermes),
message_sanitization_gaps (serialized_messages_bytes,
looks_like_image_content_rejection), catalog_gaps (weights_bytes,
download_bytes, predicted_decode_tok_s), context_policy_gaps
(ub_logits_bytes), runtime_provider_gaps (is_routable_provider,
is_external_process_provider).

Modeling notes (mirrored in the port headers):
- Late-bound imports (gateway.session_finite, cron.lifecycle_guard,
  hermes_cli.providers, providers, psutil) are sys.modules doubles.
  finite_import/guard_import=false model an absent package (the IMPORT
  raises and is caught, with no seam event); the live calls sit outside
  the import try (else: clause / after it), so scripted call raises
  propagate. For is_routable_provider / is_external_process_provider the
  import and the call share one try, so a scripted ImportError on the
  imported callable models import failure identically.
- Tagged values ({"kind","value"}) carry int/float/bool identity where
  isinstance()/int() observe it (pid_is_hermes, ub_logits_bytes).
- is_plugin_id runs the live host PurePath; the C port models the build
  host's PurePath semantics (Windows rules from CPython 3.14 ntpath).
- serialized_messages_bytes: float payloads are outside the fixture model
  (parsed JSON numbers lose int/float identity; see the port header).
- List-valued seam results are {"__script__": true, "ret": [...]} wrapped
  (bare arrays are per-call queues).

Gate:
  bash tests/run_one_oracle.sh tail_revive2 oracle_tail_revive2.py "" "src/tools/approval_gaps.o src/hermes_cli/main_gaps.o src/hermes_cli/plugin_dev_gaps.o src/hermes_cli/_subprocess_compat_gaps.o src/agent/message_sanitization_gaps.o src/hermes_cli/local_runtime/catalog_gaps.o src/hermes_cli/local_runtime/context_policy_gaps.o src/hermes_cli/runtime_provider_gaps.o lib/libjson/json.o"
"""
import ast
import builtins
import copy
import __future__
import inspect as _inspect_real
import itertools
import json
import math
import os
import re
import stat as _stat_real
import subprocess as _subprocess_real
import sys
import types
import asyncio
from pathlib import Path, PurePath

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_tr2"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints (libjson emits %.0f); tuples -> lists; Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, Path):
        return x.as_posix()
    return x


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        if ty == "TimeoutExpired":
            # live catches the REAL subprocess.TimeoutExpired class
            self.exc = _subprocess_real.TimeoutExpired(spec.get("cmd", "x"),
                                                       spec.get("timeout", 5))
            self.spec["message"] = str(self.exc)
        elif ty == "CancelledError":
            self.exc = asyncio.CancelledError(msg)
        else:
            cls = getattr(builtins, ty, None)
            if not (isinstance(cls, type) and issubclass(cls, BaseException)):
                cls = type(ty, (Exception,), {})
            self.exc = cls(msg)
        if ty == "SystemExit":
            self.spec["code"] = self.exc.code


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


def untag(t):
    """Tagged value -> Python value with real int/float/bool/str identity."""
    if t is None:
        return None
    k = t.get("kind", "null")
    if k == "null":
        return None
    if k == "int":
        return int(t["value"])
    if k == "float":
        return float(t["value"])
    if k == "bool":
        return bool(t["value"])
    if k == "str":
        return str(t["value"])
    return t.get("value")


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), strict_consts=True):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_tr2_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname  # py3.14 dataclasses resolves cls.__module__
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except BaseException:
                if strict:
                    raise

    seed_consts(False)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            exec(compile_node(n, relpath), ns)
    seed_consts(strict_consts)
    for name in list(want_funcs) + list(want_classes):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    from dataclasses import dataclass, field
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Tuple": object,
        "dataclass": dataclass, "field": field,
        "math": math, "re": re, "json": json, "os": os, "sys": sys,
        "itertools": itertools, "stat": _stat_real,
        "Path": Path, "PurePath": PurePath,
        "inspect": _inspect_real,
        "logging": types.SimpleNamespace(
            getLogger=lambda name: types.SimpleNamespace(
                warning=lambda *a, **k: None, debug=lambda *a, **k: None,
                info=lambda *a, **k: None, error=lambda *a, **k: None)),
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# ── module namespaces (built once) ──────────────────────────────────────────

NS_ENV = load_ns("utils/env.py", base_seeds("quarry_tr2_env"),
                 want_funcs=("is_truthy_value",))

_ac_seeds = base_seeds("quarry_tr2_ac")
_ac_seeds["is_truthy_value"] = NS_ENV["is_truthy_value"]
NS_AC = load_ns("tools/approval_context.py", _ac_seeds,
                want_funcs=("_is_unattended_platform_approval_context",
                            "_is_single_query_approval_context",
                            "_should_fall_through_to_cli_approval",
                            "_get_session_platform"),
                strict_consts=False)
assert "_UNATTENDED_APPROVAL_PLATFORMS" in NS_AC

NS_AD = load_ns("tools/approval_detection.py", base_seeds("quarry_tr2_ad"),
                want_funcs=("_is_shell_token_spliced_gateway_lifecycle",),
                strict_consts=False)

NS_MAIN = load_ns("hermes_cli/main.py", base_seeds("quarry_tr2_main"),
                  want_funcs=("_argv_is_gateway_run",), strict_consts=False)

NS_DESK = load_ns("hermes_cli/main_desktop.py", base_seeds("quarry_tr2_desk"),
                  want_funcs=("_desktop_linux_userns_sandbox_available",
                              "_desktop_linux_needs_disable_setuid_sandbox",
                              "_sandbox_helper_is_setuid_root"),
                  strict_consts=False)

NS_PD = load_ns("hermes_cli/plugin_dev.py", base_seeds("quarry_tr2_pd"),
                want_funcs=("_is_plugin_id", "_accepts_var_kwargs",
                            "_holds_plugin", "_has_manifest"),
                strict_consts=False)
assert "_MANIFEST_NAMES" in NS_PD
REAL_HAS_MANIFEST = NS_PD["_has_manifest"]

NS_SC = load_ns("hermes_cli/_subprocess_compat.py", base_seeds("quarry_tr2_sc"),
                want_funcs=("pid_is_hermes", "_process_command_is_hermes",
                            "_text_names_hermes"),
                strict_consts=False)
REAL_PROC_CMD = NS_SC["_process_command_is_hermes"]

NS_MS = load_ns("agent/message_sanitization.py", base_seeds("quarry_tr2_ms"),
                want_funcs=("serialized_messages_bytes",
                            "_looks_like_image_content_rejection"),
                strict_consts=False)
assert "_IMAGE_REJECTION_PHRASES" in NS_MS

NS_PRODUCTS = load_ns("hermes_platform/host/products.py",
                      base_seeds("quarry_tr2_products"),
                      want_funcs=("is_nvidia_n1x_pci_id",), strict_consts=False)

NS_EST = load_ns("hermes_cli/local_runtime/estimator.py",
                 base_seeds("quarry_tr2_est"),
                 want_classes=("HardwareBudget",), strict_consts=False)

_cat_seeds = base_seeds("quarry_tr2_cat")
_cat_seeds["is_nvidia_n1x_pci_id"] = NS_PRODUCTS["is_nvidia_n1x_pci_id"]
NS_CAT = load_ns("hermes_cli/local_runtime/catalog.py", _cat_seeds,
                 want_funcs=("predicted_decode_tok_s", "_hardware_key"),
                 want_classes=("AssetFile", "QuantVariant", "CatalogEntry"),
                 strict_consts=False)
for _c in ("_MEASURED_DECODE_TOK_S", "_HOST_BANDWIDTH_GB_S",
           "_UMA_BANDWIDTH_GB_S", "_DISCRETE_BANDWIDTH_GB_S"):
    assert _c in NS_CAT, f"catalog constant missing: {_c}"

NS_CP = load_ns("hermes_cli/local_runtime/context_policy.py",
                base_seeds("quarry_tr2_cp"),
                want_funcs=("ub_logits_bytes",), strict_consts=False)

NS_RPC = load_ns("hermes_cli/runtime_provider_custom.py",
                 base_seeds("quarry_tr2_rpc"),
                 want_funcs=("is_routable_provider",), strict_consts=False)

NS_RPB = load_ns("hermes_cli/runtime_provider_backends.py",
                 base_seeds("quarry_tr2_rpb"),
                 want_funcs=("_is_external_process_provider",),
                 strict_consts=False)


# ── recording seam factory ──────────────────────────────────────────────────

def make_rec(script, trace, armed):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                if armed[0]:
                    trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            if armed[0]:
                trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def minus_io(work):
    work.pop("io", None)
    return work


KINDMAP = {name: getattr(_inspect_real.Parameter, name) for name in
           ("POSITIONAL_ONLY", "POSITIONAL_OR_KEYWORD", "VAR_POSITIONAL",
            "KEYWORD_ONLY", "VAR_KEYWORD")}


class PathDouble:
    """Path double for the fixture path model (see port header)."""
    def __init__(self, posix, rec):
        self.posix = posix
        self._rec = rec

    def __truediv__(self, name):
        joined = name if self.posix == "" else self.posix + "/" + name
        return PathDouble(joined, self._rec)

    def exists(self):
        return self._rec("path.exists", lambda a, k: {"path": self.posix})()

    def is_symlink(self):
        return self._rec("path.is_symlink", lambda a, k: {"path": self.posix})()

    def is_dir(self):
        return self._rec("path.is_dir", lambda a, k: {"path": self.posix})()


# ── per-function drivers ────────────────────────────────────────────────────

def d_unattended(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_AC["_session_env"] = rec("session_env", lambda a, k: {"name": a[0]})
    return NS_AC["_is_unattended_platform_approval_context"]()


def d_single_query(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_AC["_session_env"] = rec("session_env", lambda a, k: {"name": a[0]})
    if work.get("finite_import", True):
        mod = types.ModuleType("gateway.session_finite")
        mod.finite_turn_required = rec("finite_turn_required", lambda a, k: {})
        sys.modules["gateway.session_finite"] = mod
        pkg("gateway").session_finite = mod
    else:
        # absent package: the IMPORT raises ImportError (caught, no event);
        # the live call sits in the else: clause, outside the try.
        sys.modules.pop("gateway.session_finite", None)
        g = pkg("gateway")
        if hasattr(g, "session_finite"):
            del g.session_finite
    return NS_AC["_is_single_query_approval_context"]()


def d_fall_through(work, trace, script):
    cb = None if work.get("approval_callback") is None else object()
    nb = None if work.get("notify_cb") is None else object()
    return NS_AC["_should_fall_through_to_cli_approval"](
        is_cli=work.get("is_cli"), approval_callback=cb, notify_cb=nb)


def d_spliced(work, trace, script):
    rec = make_rec(script, trace, [True])
    if work.get("guard_import", True):
        mod = types.ModuleType("cron.lifecycle_guard")
        mod.contains_gateway_lifecycle_command = rec(
            "lifecycle_guard", lambda a, k: {"command": a[0]})
        sys.modules["cron.lifecycle_guard"] = mod
        pkg("cron").lifecycle_guard = mod
    else:
        # import failure: except Exception -> False, no seam event.
        # The live delegate call sits OUTSIDE that try block.
        sys.modules.pop("cron.lifecycle_guard", None)
        c = pkg("cron")
        if hasattr(c, "lifecycle_guard"):
            del c.lifecycle_guard
    return NS_AD["_is_shell_token_spliced_gateway_lifecycle"](work["command"])


def d_argv(work, trace, script):
    return NS_MAIN["_argv_is_gateway_run"](work["argv"])


def d_userns(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_DESK["sys"] = types.SimpleNamespace(platform=work["platform"])
    NS_DESK["shutil"] = types.SimpleNamespace(
        which=rec("shutil.which", lambda a, k: {"name": a[0]}))
    NS_DESK["subprocess"] = types.SimpleNamespace(
        run=rec("subprocess.run", lambda a, k: {"argv": a[0]},
                make=lambda r: types.SimpleNamespace(returncode=r["returncode"])
                if isinstance(r, dict) else r),
        DEVNULL=_subprocess_real.DEVNULL,
        TimeoutExpired=_subprocess_real.TimeoutExpired)
    return NS_DESK["_desktop_linux_userns_sandbox_available"]()


def d_setuid(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_DESK["sys"] = types.SimpleNamespace(platform=work["platform"])

    def mk(r):
        st = r.get("st")
        return (Path(r["sandbox"]),
                types.SimpleNamespace(**st) if st is not None else None)

    NS_DESK["_sandbox_helper_lstat"] = rec(
        "sandbox_helper_lstat", lambda a, k: {"path": a[0].as_posix()}, make=mk)
    return NS_DESK["_desktop_linux_needs_disable_setuid_sandbox"](
        Path(work["packaged_executable"]))


def d_has_manifest(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_PD["_has_manifest"] = REAL_HAS_MANIFEST
    return NS_PD["_has_manifest"](PathDouble(work["path"], rec))


def d_holds_plugin(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_PD["_has_manifest"] = rec("has_manifest", lambda a, k: {"path": a[0].posix})
    NS_PD["_category_plugins"] = rec("category_plugins",
                                     lambda a, k: {"path": a[0].posix})
    return NS_PD["_holds_plugin"](PathDouble(work["path"], rec))


def d_is_plugin_id(work, trace, script):
    return NS_PD["_is_plugin_id"](work["raw"])


def d_var_kwargs(work, trace, script):
    rec = make_rec(script, trace, [True])

    def mk(r):
        params = [types.SimpleNamespace(kind=KINDMAP[x]) for x in r["parameters"]]
        return types.SimpleNamespace(
            parameters=types.SimpleNamespace(values=lambda: params))

    NS_PD["inspect"] = types.SimpleNamespace(
        signature=rec("inspect.signature", lambda a, k: {"callback": "<cb>"},
                      make=mk),
        Parameter=_inspect_real.Parameter)
    return NS_PD["_accepts_var_kwargs"](object())


def d_text_names(work, trace, script):
    return NS_SC["_text_names_hermes"](work["text"])


def d_proc_cmd(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_SC["_process_command_is_hermes"] = REAL_PROC_CMD

    class ProcDouble:
        def cmdline(self):
            return rec("psutil.cmdline", lambda a, k: {"handle": "<proc>"})()

        def exe(self):
            return rec("psutil.exe", lambda a, k: {"handle": "<proc>"})()

    sys.modules["psutil"] = types.SimpleNamespace(
        Process=rec("psutil.Process", lambda a, k: {"pid": a[0]},
                    make=lambda r: ProcDouble()))
    return NS_SC["_process_command_is_hermes"](work["pid"])


def d_pid_is_hermes(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_SC["IS_WINDOWS"] = work["is_windows"]
    NS_SC["_process_start_time"] = rec("process_start_time",
                                       lambda a, k: {"pid": a[0]})
    NS_SC["_process_command_is_hermes"] = rec("process_command_is_hermes",
                                              lambda a, k: {"pid": a[0]})
    return NS_SC["pid_is_hermes"](untag(work.get("pid")),
                                  expected_start_time=work.get("expected_start_time"))


def d_ser_bytes(work, trace, script):
    return NS_MS["serialized_messages_bytes"](work.get("messages"))


def d_img_reject(work, trace, script):
    return NS_MS["_looks_like_image_content_rejection"](work.get("error_body"))


def _mk_asset(a):
    if a is None:
        return None
    return NS_CAT["AssetFile"](path=a["path"], size_bytes=a["size_bytes"])


def _mk_variant(v):
    files = tuple(NS_CAT["AssetFile"](path=f["path"], size_bytes=f["size_bytes"])
                  for f in v["files"])
    return NS_CAT["QuantVariant"](quant=v["quant"], files=files,
                                  lazy_bytes=v.get("lazy_bytes", 0))


def _mk_entry(e):
    return NS_CAT["CatalogEntry"](
        id=e["id"], display_name="", description="", repo="", variants=(),
        n_ctx_train=0, full_layers=0, recurrent_layers=0, per_layer_f16=0,
        mtp=e.get("mtp", False), mtp_draft_depth=e.get("mtp_draft_depth", 3),
        decode_fraction=e.get("decode_fraction", 1.0),
        mmproj=_mk_asset(e.get("mmproj")), draft=_mk_asset(e.get("draft")),
        mtp_head=_mk_asset(e.get("mtp_head")))


def _mk_budget(b):
    return NS_EST["HardwareBudget"](
        usable_vram_bytes=0, total_device_bytes=0, ram_available_bytes=0,
        uma=b.get("uma", False), gpu_name=b.get("gpu_name", ""),
        platform=b.get("platform", ""), gpu_pci_id=b.get("gpu_pci_id"))


def d_weights(work, trace, script):
    return _mk_variant(work["variant"]).weights_bytes


def d_download(work, trace, script):
    return _mk_entry(work["entry"]).download_bytes(_mk_variant(work["variant"]))


def d_decode(work, trace, script):
    return NS_CAT["predicted_decode_tok_s"](
        _mk_entry(work["entry"]), _mk_variant(work["variant"]),
        _mk_budget(work["budget"]),
        spilled=work.get("spilled", False), backend=work.get("backend", "auto"))


def d_ub_logits(work, trace, script):
    return NS_CP["ub_logits_bytes"](untag(work["n_vocab"]),
                                    mtp_capable=work["mtp_capable"],
                                    mtp_prefill=work.get("mtp_prefill", False))


def d_routable(work, trace, script):
    rec = make_rec(script, trace, [True])
    rp = types.SimpleNamespace(
        load_config=rec("load_config", lambda a, k: {}),
        get_compatible_custom_providers=rec("get_compatible_custom_providers",
                                            lambda a, k: {"config": a[0]}))
    NS_RPC["_rp"] = lambda: rp
    mod = types.ModuleType("hermes_cli.providers")
    mod.resolve_provider_full = rec(
        "resolve_provider_full",
        lambda a, k: {"name": a[0], "providers": a[1], "custom": a[2]})
    sys.modules["hermes_cli.providers"] = mod
    pkg("hermes_cli").providers = mod
    return NS_RPC["is_routable_provider"](work.get("provider"))


def d_ext_proc(work, trace, script):
    rec = make_rec(script, trace, [True])
    # live reads pconfig.auth_type / getattr(profile, "auth_type", "") —
    # attribute-bearing doubles, not raw dicts
    rp = types.SimpleNamespace(
        PROVIDER_REGISTRY=types.SimpleNamespace(
            get=rec("registry_get", lambda a, k: {"name": a[0]},
                    make=lambda r: types.SimpleNamespace(**r)
                    if isinstance(r, dict) else r)))
    NS_RPB["_rp"] = lambda: rp
    mod = types.ModuleType("providers")
    mod.get_provider_profile = rec("get_provider_profile",
                                   lambda a, k: {"name": a[0]},
                                   make=lambda r: types.SimpleNamespace(**r)
                                   if isinstance(r, dict) else r)
    sys.modules["providers"] = mod
    return NS_RPB["_is_external_process_provider"](work.get("provider"))


DRIVERS = {
    "is_unattended_platform_approval_context": d_unattended,
    "is_single_query_approval_context": d_single_query,
    "should_fall_through_to_cli_approval": d_fall_through,
    "is_shell_token_spliced_gateway_lifecycle": d_spliced,
    "argv_is_gateway_run": d_argv,
    "desktop_linux_userns_sandbox_available": d_userns,
    "desktop_linux_needs_disable_setuid_sandbox": d_setuid,
    "_has_manifest": d_has_manifest,
    "holds_plugin": d_holds_plugin,
    "is_plugin_id": d_is_plugin_id,
    "accepts_var_kwargs": d_var_kwargs,
    "text_names_hermes": d_text_names,
    "process_command_is_hermes": d_proc_cmd,
    "pid_is_hermes": d_pid_is_hermes,
    "serialized_messages_bytes": d_ser_bytes,
    "looks_like_image_content_rejection": d_img_reject,
    "weights_bytes": d_weights,
    "download_bytes": d_download,
    "predicted_decode_tok_s": d_decode,
    "ub_logits_bytes": d_ub_logits,
    "is_routable_provider": d_routable,
    "is_external_process_provider": d_ext_proc,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        if isinstance(e, SystemExit):
            spec["code"] = e.code
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE tail_revive2: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
