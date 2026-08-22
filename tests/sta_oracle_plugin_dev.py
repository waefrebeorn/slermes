#!/usr/bin/env python3
"""sta_oracle_plugin_dev.py — Python twin for tests/t_port_plugin_dev.c
(hermes_cli/plugin_dev.py deterministic surface). Imports the REAL module.
"""
import contextlib
import json
import os
import sys
from pathlib import Path, PosixPath
from types import SimpleNamespace

REPO = Path(__file__).resolve().parent.parent
DEV = REPO.parent
sys.path.insert(0, str(DEV))

from hermes_cli import plugin_dev as pdev            # noqa: E402
BUNDLED_OVERRIDE = {"dir": None}

_FALLBACK_HOOKS = {
        "pre_tool_call", "post_tool_call", "transform_terminal_output",
        "transform_tool_result", "transform_llm_output", "pre_llm_call",
        "post_llm_call", "on_stream_start", "on_stream_delta",
        "on_stream_end", "on_interim_message", "pre_verify",
        "pre_api_request", "post_api_request", "api_request_error",
        "transform_api_error_classification", "on_session_start",
        "on_session_end", "on_session_finalize", "on_session_reset",
        "on_skill_lifecycle", "subagent_start", "subagent_stop",
        "pre_gateway_dispatch", "pre_approval_request",
        "post_approval_response", "pre_transcription",
    }

try:
    import hermes_cli.plugins as plugins_mod         # noqa: E402
except Exception:
    # plugins.py is mid-refactor upstream (hermes_constants lacks
    # hermes_home_key). Stub ONLY the constants/helpers plugin_dev imports so
    # the REAL _check_manifest_v2 / resolve_plugin_path logic still runs.
    import types as _types
    plugins_mod = None

    def _fallback_bundled():
        if BUNDLED_OVERRIDE["dir"]:
            return Path(BUNDLED_OVERRIDE["dir"])
        return Path(os.environ.get("HERMES_BUNDLED_PLUGINS") or "plugins")

    _stub = _types.ModuleType("hermes_cli.plugins")
    _stub.SUPPORTED_MANIFEST_VERSION = 2
    _stub._CONFIG_SCHEMA_TYPES = {
        "str": (str,), "string": (str,), "int": (int,), "integer": (int,),
        "float": (int, float), "number": (int, float), "bool": (bool,),
        "boolean": (bool,), "list": (list,), "array": (list,),
        "dict": (dict,), "object": (dict,),
    }
    _stub.get_bundled_plugins_dir = _fallback_bundled
    sys.modules["hermes_cli.plugins"] = _stub


def findings_of(report):
    return [{"level": f.level, "message": f.message} for f in report.findings]


def main():
    ops = json.load(open(sys.argv[1]))["ops"]
    for op in ops:
        kind = op.get("kind", "")
        if kind == "format":
            report = pdev.DoctorReport(Path(op.get("path", ".")))
            man = op.get("manifest")
            if man is not None:
                report.manifest = SimpleNamespace(
                    name=man.get("name", ""),
                    version=man.get("version"),
                    kind=man.get("kind", ""),
                )
            for e in op.get("errors", []):
                report.error(e)
            for w in op.get("warnings", []):
                report.warning(w)
            report.registered_tools = tuple(op.get("tools", []))
            report.registered_hooks = tuple(op.get("hooks", []))
            print(json.dumps({"out": report.format_text()}))
        elif kind == "mkdirs":
            for d in op.get("dirs", []):
                Path(d).mkdir(parents=True, exist_ok=True)
            print(json.dumps({"out": True}))
        elif kind == "resolve":
            real_home = pdev.get_hermes_home
            pdev.get_hermes_home = lambda: Path(op["home"])
            BUNDLED_OVERRIDE["dir"] = op["bundled"]
            real_cwd = os.getcwd()
            try:
                os.chdir(op.get("cwd", "."))
                resolved = pdev.resolve_plugin_path(
                    op.get("target") or None)
                print(json.dumps({"out": str(resolved)}))
            except FileNotFoundError as exc:
                print(json.dumps({"out": None, "error": str(exc)}))
            finally:
                os.chdir(real_cwd)
                pdev.get_hermes_home = real_home
                BUNDLED_OVERRIDE["dir"] = None
        elif kind == "manifest_v2":
            m = op["manifest"]
            manifest = SimpleNamespace(
                manifest_version=m.get("manifest_version", 1),
                api_version=m.get("api_version"),
                requires_plugins=m.get("requires_plugins", []) or [],
                python_dependencies=m.get("python_dependencies", []) or [],
                config_schema=m.get("config_schema", {}) or {},
            )
            installed = op.get("installed", {})

            real_version = __import__("importlib.metadata",
                                      fromlist=["version"]).version

            def fake_version(dist):
                if dist in installed:
                    v = installed[dist]
                    if v is None:
                        raise __import__("importlib.metadata",
                                         fromlist=["PackageNotFoundError"]
                                         ).PackageNotFoundError(dist)
                    return v
                raise RuntimeError(f"no fixture entry for {dist}")

            import importlib.metadata as im
            im.version = fake_version
            report = pdev.DoctorReport(Path("."))
            try:
                pdev._check_manifest_v2(report, manifest)
            finally:
                im.version = real_version
            print(json.dumps({"out": findings_of(report)}))
        elif kind == "registrations":
            report = pdev.DoctorReport(Path("."))
            declared_hooks = op.get("declared_hooks", [])
            declared_tools = op.get("declared_tools", [])
            reg_hooks = op.get("registered_hooks", [])
            reg_tools = op.get("registered_tools", [])
            # replicate the doctor_plugin() ladders over typed lists
            if not isinstance(declared_hooks, list):
                report.error("provides_hooks must be a list")
                declared_hooks = []
            if not isinstance(declared_tools, list):
                report.error("provides_tools must be a list")
                declared_tools = []
            hooks = plugins_mod.VALID_HOOKS if plugins_mod else _FALLBACK_HOOKS
            for name in declared_hooks:
                if not isinstance(name, str):
                    report.error("provides_hooks entries must be strings")
                elif name not in hooks:
                    report.error(f"unknown hook {name!r} in provides_hooks")
            declared_hook_names = {n for n in declared_hooks if isinstance(n, str)}
            registered_hook_names = set(reg_hooks)
            for name in sorted(declared_hook_names - registered_hook_names):
                report.warning(
                    f"manifest declares hook {name!r} but registration did not add it")
            for name in sorted(registered_hook_names - declared_hook_names):
                report.warning(
                    f"registration adds hook {name!r} not listed in provides_hooks")
            declared_tool_names = {n for n in declared_tools if isinstance(n, str)}
            registered_tool_names = set(reg_tools)
            for name in sorted(declared_tool_names - registered_tool_names):
                report.warning(
                    f"manifest declares tool {name!r} but registration did not add it")
            for name in sorted(registered_tool_names - declared_tool_names):
                report.warning(
                    f"registration adds tool {name!r} not listed in provides_tools")
            print(json.dumps({"out": findings_of(report)}))
        else:
            print(json.dumps({"out": None}))


if __name__ == "__main__":
    main()
