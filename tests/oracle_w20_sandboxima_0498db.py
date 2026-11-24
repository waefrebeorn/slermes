#!/usr/bin/env python3
"""Differential oracle for hermes_cli/sandbox_image_switch.py (w20_sandboxima_0498db).

The live module is imported with its collaborators stubbed at the same seams the
C port takes as arguments: the docker probe (``find_docker``/``_docker_query``),
the YAML config reads, the bound terminal scope and the launch environment.
Compared values: ``_image_pinned``, ``pending`` (the PendingSwitch as a dict),
``explain_lines`` and the image ``decide`` pins.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mk("hermes_cli")
CFG = _mk("hermes_cli.config", load_config_readonly=lambda: {}, read_raw_config=lambda: {},
          load_config=lambda: {}, save_config=lambda *a, **k: None,
          apply_terminal_config_to_env=lambda **k: None)
_mk("tools")
_mk("tools.environments")
DOCKER = _mk("tools.environments.docker", find_docker=lambda: "/usr/bin/docker",
             _docker_query=lambda *a, **k: None, _container_identity=lambda k: k)
SCOPE = _mk("tools.terminal_scope", get_terminal_scope=lambda: None)
_mk("tools.terminal_tool_lifecycle", _evict_environment_for_task=lambda t: None)

NAME = "w20_sandboxima_0498db_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "hermes_cli", "sandbox_image_switch.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def obj(v):
    return v if isinstance(v, dict) else {}


def plain(v):
    if isinstance(v, dict):
        return {k: plain(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [plain(x) for x in v]
    return v


def wire(request):
    """Install the request's scripted seams, then return the terminal config."""
    env_image = request.get("env_image")
    live.os = types.SimpleNamespace(
        environ={} if env_image is None else {"TERMINAL_DOCKER_IMAGE": env_image})
    CFG.load_config_readonly = lambda: request.get("config")
    CFG.read_raw_config = lambda: ({"terminal": request["raw_terminal"]}
                                   if "raw_terminal" in request else {})
    SCOPE.get_terminal_scope = lambda: request.get("scope")
    found = request.get("docker_found", True)
    DOCKER.find_docker = lambda: "/usr/bin/docker" if found else None
    DOCKER._docker_query = lambda *a, **k: (
        None if request.get("listing") is None
        else types.SimpleNamespace(stdout=request.get("listing"), returncode=0))
    return obj(request.get("terminal_cfg"))


def switch_of(request):
    sw = obj(request.get("switch"))
    return live.PendingSwitch(
        current_image=sw.get("current_image", ""),
        target_image=sw.get("target_image", ""),
        containers=list(sw.get("containers") or []),
    )


def expected(r):
    f = r.get("func")
    cfg = wire(r)
    if f == "image_pinned":
        return bool(live._image_pinned(cfg))
    if f == "pending":
        sw = live.pending()
        if sw is None:
            return None
        return {"current_image": sw.current_image, "target_image": sw.target_image,
                "containers": list(sw.containers)}
    if f == "explain_lines":
        return live.explain_lines(switch_of(r))
    if f == "decide_image":
        pinned = {}

        def _save(config, preserve_keys=None):
            pinned["config"] = config

        CFG.load_config = lambda: {"terminal": {}}
        CFG.save_config = _save
        return live.decide(switch_of(r), bool(r.get("approve", False)))
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = plain(expected(r))
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_sandboxima_0498db: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
