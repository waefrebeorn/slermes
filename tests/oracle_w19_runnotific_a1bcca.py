#!/usr/bin/env python3
"""Differential oracle for gateway/run_notifications_restart.py (gate w19_runnotific_a1bcca)."""
import ast, asyncio, contextvars, json, logging, os, sys, tempfile, time, types
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "gateway" / "run_notifications_restart.py"
GATE = "w19_runnotific_a1bcca"
WANTED_FUNCS = {"_notice_expired"}
WANTED_CLASSES = {"GatewayRestartNoticeMixin"}
WANTED_VALUES = {"_RESTART_NOTICE_TTL_SECONDS"}

CURRENT = {}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "restart_live", "asyncio": asyncio, "contextvars": contextvars,
         "json": json, "logging": logging, "time": time, "Optional": object,
         "logger": logging.getLogger("restart_oracle"),
         "t": lambda key, **kw: "restarted",
         "Platform": lambda value: value,
         "_send_failed": lambda result: bool(CURRENT.get("send_failed")),
         "_send_error": lambda result: CURRENT.get("error")}
    for node in tree.body:
        if isinstance(node, ast.Import):
            for alias in node.names:
                g[alias.asname or alias.name.split(".")[0]] = __import__(alias.name)
        elif isinstance(node, ast.ImportFrom):
            continue  # every upstream import is stubbed above / in sys.modules below
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in WANTED_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         dont_inherit=True), g)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in targets if isinstance(t, ast.Name)}
            if names & WANTED_VALUES:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             dont_inherit=True), g)
    return g


G = extract()
MIXIN = G["GatewayRestartNoticeMixin"]


def stub_module(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


def run_notice_expired(r):
    stat_ok = r.get("stat_ok", True)
    age = r.get("age_seconds")
    tmp = tempfile.mkdtemp()
    path = Path(tmp) / "marker"
    if stat_ok:
        path.write_text("x", encoding="utf-8")
        if age is not None:
            now = time.time()
            os.utime(path, (now - age, now - age))
    return G["_notice_expired"](path)


async def fake_deliver_factory(spec):
    async def _deliver(data):
        if spec.get("raises"):
            raise RuntimeError("deliver boom")
        return spec.get("result"), bool(spec.get("keep"))
    return _deliver


def run_send_restart(r):
    tmp = tempfile.mkdtemp()
    marker = Path(tmp) / ".restart_notify.json"
    existed = bool(r.get("exists", True))
    if existed:
        raw = r.get("raw") or ""
        marker.write_text(raw, encoding="utf-8")
        age = r.get("age_seconds")
        if age is not None:
            now = time.time()
            os.utime(marker, (now - age, now - age))
    stub_module("gateway.run", _hermes_home=Path(tmp))
    obj = MIXIN()
    obj._restart_notice_lock = None
    if r.get("claimed") is not None:
        obj._restart_notice_claimed = r["claimed"]
    obj._deliver_restart_notification = asyncio.run(fake_deliver_factory(r.get("deliver") or {}))
    try:
        result = asyncio.run(obj._send_restart_notification(replay=bool(r.get("replay"))))
    except BaseException as exc:
        return {"return_value": None, "exists_after": marker.exists(),
                "unlink": existed and not marker.exists(), "claimed": None, "raised": repr(exc)}
    return {
        "return_value": list(result) if result is not None else None,
        "exists_after": marker.exists(),
        "unlink": existed and not marker.exists(),
        "claimed": None if r.get("replay") else getattr(obj, "_restart_notice_claimed", None),
    }


def run_deliver(r):
    data = dict(r.get("data") or {})
    ctx = r.get("ctx") or {}
    captured = {}

    class Transport:
        adapter = object()

        async def send(self, platform, chat_id, text, metadata=None):
            captured["metadata"] = metadata
            captured["text"] = text
            captured["chat"] = chat_id
            return types.SimpleNamespace(success=not ctx.get("send_failed"),
                                         error=ctx.get("error"))

    class PlatformMap(dict):
        def get(self, key, default=None):
            cfg = ctx.get("platform_cfg")
            if cfg is None:
                return None
            return types.SimpleNamespace(
                gateway_restart_notification=bool(cfg.get("gateway_restart_notification", True)))

    transport = Transport() if ctx.get("transport_present", True) else None
    CURRENT.clear()
    CURRENT.update(ctx)

    def reject(value):
        if not ctx.get("platform_known", True):
            raise ValueError("unknown platform")
        return value

    stub_module("gateway.delivery", resolve_delivery_transport=lambda p, c, a: transport)
    stub_module("gateway.run", _non_conversational_metadata=lambda metadata, platform=None: metadata)
    stub_module("gateway.delivery_ledger", is_reconnect_only=lambda err: bool(ctx.get("reconnect_only")))

    obj = MIXIN()
    obj.config = types.SimpleNamespace(platforms=PlatformMap())
    obj._adapters_for_profile = lambda profile: {}
    obj._marker_profile = lambda d: "default"
    obj._pending_marker_metadata = lambda platform, chat_id, d, adapter: ctx.get("metadata")

    # Platform() raising is part of the real flow; swap the stub in per case.
    G["Platform"] = reject
    try:
        target, keep = asyncio.run(obj._deliver_restart_notification(data))
    except BaseException as exc:
        return {"target": None, "keep": False, "metadata": None, "sent": False,
                "raised": repr(exc)}
    return {"target": list(target) if target is not None else None, "keep": bool(keep),
            "metadata": captured.get("metadata"), "sent": bool(captured)}


def run_case(r):
    f = r.get("func")
    if f == "notice_expired":
        return run_notice_expired(r)
    if f == "send_restart":
        return run_send_restart(r)
    if f == "deliver":
        return run_deliver(r)
    if f == "schedule_wanted":
        return _schedule(r)
    if f == "ttl":
        return G["_RESTART_NOTICE_TTL_SECONDS"]
    if f == "note_boot":
        obj = types.SimpleNamespace()
        obj._restart_notice_context = object() if r.get("context_present") else None
        was_none = obj._restart_notice_context is None
        G["GatewayRestartNoticeMixin"]._note_restart_notice_boot(obj)
        return was_none
    return None


def _schedule(r):
    """The gate half of _schedule_restart_notice_replay, with the two marker probes stubbed."""
    context = object() if r.get("context_present") else None
    if context is None or r.get("restart_requested") or r.get("draining"):
        return False
    return bool(r.get("requester_pending") or r.get("planned_pending"))


FUNCS = {"notice_expired", "send_restart", "deliver", "schedule_wanted", "ttl", "note_boot"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        if isinstance(py, dict):
            py.pop("raised", None)
        got = r.get("out")
        if isinstance(got, dict):
            got = {k: v for k, v in got.items() if k != "raised"}
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
