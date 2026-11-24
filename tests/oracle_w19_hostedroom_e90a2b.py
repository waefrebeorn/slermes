#!/usr/bin/env python3
"""Differential oracle for tui_gateway/hosted_room_peer_transport.py (gate w19_hostedroom_e90a2b)."""
import ast, dataclasses, hashlib, json, sys, time, types, uuid
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "tui_gateway" / "hosted_room_peer_transport.py"
GATE = "w19_hostedroom_e90a2b"
WANTED_FUNCS = {"route_identity", "build_member_dispatch"}
WANTED_CLASSES = {"RoomLinkCandidate", "FailoverHostedRoomPeerClient", "PeerMemberRoute",
                  "PeerHostedRoomTransport"}


class FakeDispatch:
    def __init__(self, **kw):
        self.__dict__.update(kw)

    @classmethod
    def from_mapping(cls, value):
        return cls(**dict(value))

    def as_mapping(self):
        return dict(self.__dict__)


class FakeBinding:
    def __init__(self, room_id="", gateway_id="", authority_epoch=0):
        self.room_id = room_id
        self.gateway_id = gateway_id
        self.authority_epoch = authority_epoch


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


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "peer_transport_live", "dataclass": dataclasses.dataclass,
         "time": time, "hashlib": hashlib, "uuid": uuid,
         "Callable": object, "Mapping": dict, "Sequence": list, "Any": object, "Optional": object,
         "Protocol": object, "TYPE_CHECKING": False,
         "HostedMemberDispatch": FakeDispatch, "PROTOCOL_VERSION": 2,
         "HostedRoomBinding": FakeBinding, "ROOM_SESSION_SOURCE": "bot_room",
         "InternalSessionRPC": object,
         "room_session_title": lambda room_id: "Group: %s" % room_id,
         "HostedPlacement": object, "HostedTaskIdentity": object, "Persistence": object}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in WANTED_CLASSES:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         dont_inherit=True), g)
    return g


G = extract()


class LinkError(Exception):
    def __init__(self, ambiguous, retryable):
        super().__init__("link failure")
        self.ambiguous = ambiguous
        self.retryable = retryable


class FakeClient:
    def __init__(self, outcomes=None, recorder=None, index=0, dispatch=None, stop=None):
        self._outcomes = outcomes or {}
        self._recorder = recorder
        self._index = index
        self._dispatch = dispatch
        self._stop = stop

    def _maybe_raise(self):
        oc = self._outcomes.get(self._index, {})
        if oc.get("raises"):
            raise LinkError(bool(oc.get("ambiguous")), bool(oc.get("retryable")))
        return {"idx": self._index}

    def prepare(self, **kw):
        if self._recorder is not None:
            self._recorder.append(self._index)
        return self._maybe_raise()

    def dispatch(self, **kw):
        return {"status": "queued"}

    def history(self, **kw):
        return []

    def status(self, **kw):
        return {}

    def stop(self, **kw):
        if self._stop is not None:
            self._stop.append("stop")
        return {}

    def stop_receipt(self, **kw):
        if self._stop is not None:
            self._stop.append("stop_receipt")
        return {}


def run_route_identity(r):
    rows = r.get("stored_rows") or []
    stub_module("gateway.hosted_rooms",
                list_room_link_records=lambda store, placement=None: rows)
    stub_module("gateway")
    result = G["route_identity"](store=object(), placement=object(),
                                 home_install_id=r.get("home_install_id", ""),
                                 room_id=r.get("room_id", ""), member_id=r.get("member_id", ""))
    return [result[0], result[1]]


def run_dispatch(r):
    binding = FakeBinding(**{k: v for k, v in (r.get("binding") or {}).items()
                             if k in ("room_id", "gateway_id", "authority_epoch")})
    route = G["PeerMemberRoute"](**{k: v for k, v in (r.get("route") or {}).items()
                                    if k in ("home_install_id", "member_id", "target_install_id",
                                             "target_profile", "capability_digest",
                                             "cancellation_scope_id", "trace_id", "grant",
                                             "execution_policy_digest")})
    args = r.get("args") or {}
    dispatch = G["build_member_dispatch"](
        binding=binding, route=route, room_id=args.get("room_id", ""),
        task_id=args.get("task_id", ""), target_profile=args.get("target_profile", ""),
        execution_generation=int(args.get("execution_generation", 0)),
        source_event_seq=int(args.get("source_event_seq", 0)),
        prompt=args.get("prompt", ""), trace_id=args.get("trace_id", ""))
    return dispatch.as_mapping()


def make_failover(r):
    outcomes = {i: oc for i, oc in enumerate(r.get("outcomes") or [])}
    clients = [FakeClient(outcomes, r.setdefault("_rec", []), index=i) for i in range(len(outcomes))]
    candidates = [G["RoomLinkCandidate"](name="c%d" % i, mode="direct", target_install_id="inst",
                                         client=clients[i]) for i in range(len(clients))]
    now = r.get("now", 0.0)
    obj = G["FailoverHostedRoomPeerClient"](candidates,
                                            reprobe_interval_seconds=r.get("interval", 60),
                                            clock=lambda: now)
    obj._active = int(r.get("active", 0))
    obj._last_primary_probe = float(r.get("last_probe", 0.0))
    return obj


def run_failover_order(r):
    """Observe the live attempt order by making every candidate fail retryably."""
    n = int(r.get("candidates", 1))
    rec = []
    outcomes = {i: {"raises": True, "ambiguous": False, "retryable": True} for i in range(n)}
    clients = [FakeClient(outcomes, rec, index=i) for i in range(n)]
    candidates = [G["RoomLinkCandidate"](name="c%d" % i, mode="direct", target_install_id="inst",
                                         client=clients[i]) for i in range(n)]
    now = r.get("now", 0.0)
    obj = G["FailoverHostedRoomPeerClient"](candidates,
                                            reprobe_interval_seconds=r.get("interval", 60),
                                            clock=lambda: now)
    obj._active = int(r.get("active", 0))
    obj._last_primary_probe = float(r.get("last_probe", 0.0))
    try:
        obj._call("prepare")
    except BaseException:
        pass
    return rec


def run_failover_call(r):
    rec = []
    outcomes = r.get("outcomes") or []
    clients = [FakeClient({i: oc for i, oc in enumerate(outcomes)}, rec, index=i)
               for i in range(len(outcomes))]
    candidates = [G["RoomLinkCandidate"](name="c%d" % i, mode="direct", target_install_id="inst",
                                         client=clients[i]) for i in range(len(clients))]
    now = r.get("now", 0.0)
    obj = G["FailoverHostedRoomPeerClient"](candidates,
                                            reprobe_interval_seconds=r.get("interval", 60),
                                            clock=lambda: now)
    obj._active = int(r.get("active", 0))
    obj._last_primary_probe = float(r.get("last_probe", 0.0))
    result_index = None
    raised = None
    try:
        out = obj._call("prepare")
        result_index = out.get("idx")
    except LinkError as exc:
        raised = "ambiguous" if exc.ambiguous else ("non_retryable" if not exc.retryable else "link failure")
    except RuntimeError:
        raised = "no_candidate"
    return {"order": rec, "active": obj._active, "result_index": result_index, "raised": raised}


def make_transport(r, client=None, dispatch=None, stop=None):
    binding = FakeBinding(room_id=r.get("room_id", "room-1"), gateway_id="gw-1", authority_epoch=3)
    route = G["PeerMemberRoute"](
        home_install_id="home-1", member_id=r.get("route_member_id", "m-1"),
        target_install_id="inst-1", target_profile=r.get("route_profile", "work"),
        capability_digest="cap", cancellation_scope_id="cancel-room-1", trace_id="trace-1",
        grant="grant-1")
    return G["PeerHostedRoomTransport"](binding=binding, route=route,
                                        client=client or FakeClient(dispatch=dispatch),
                                        source_event_seq=r.get("source_event_seq", 1))


def run_validate(r):
    try:
        transport = make_transport(r)
        transport._validate_coordinates(profile=r.get("profile", ""),
                                        source=r.get("source", ""),
                                        title=r.get("title"))
        return None
    except ValueError as exc:
        return str(exc)


def run_submit_guard(r):
    transport = make_transport(r)
    transport._session_id = r.get("self_session_id")
    task = types.SimpleNamespace(room_id=r.get("room_id", "room-1"), task_id="task-1")
    try:
        transport.submit(profile=r.get("profile", ""), session_id=r.get("session_id", "s1"),
                         prompt="hello", source=r.get("source", ""), task=task,
                         execution_generation=1, on_terminal=lambda result: None,
                         member_id=r.get("member_id"),
                         attachments=[{"type": "image"}] if r.get("attachments_present") else None)
        return None
    except ValueError as exc:
        return str(exc)


def run_interrupt(r):
    stop = []
    dispatch = None
    if r.get("dispatch_task_id") is not None:
        dispatch = FakeDispatch(task_id=r["dispatch_task_id"])

    class NoReceiptClient:
        def __init__(self, stop):
            self._stop = stop

        def stop(self, **kw):
            if self._stop is not None:
                self._stop.append("stop")
            return {}

    if r.get("has_stop_receipt"):
        client = FakeClient(stop=stop)
    else:
        client = NoReceiptClient(stop)
    transport = make_transport(r, client=client)
    transport._dispatch = dispatch
    transport.task_id = r.get("task_id")
    transport.execution_generation = r.get("execution_generation")
    transport.interrupt(profile=r.get("profile", "work"), session_id="s1",
                        source=r.get("source", "bot_room"),
                        expected_task_id=r.get("expected_task_id", ""))
    return stop[0] if stop else "none"


def run_seq(r):
    try:
        make_transport(r)
        return None
    except ValueError as exc:
        return str(exc)


def run_bind_args(r):
    captured = {}

    class Client:
        def bind_room_scope(self, **kw):
            captured.update(kw)

    b = r.get("binding") or {}
    ro = r.get("route") or {}
    binding = FakeBinding(room_id=b.get("room_id", ""), gateway_id=b.get("gateway_id", ""),
                          authority_epoch=b.get("authority_epoch", 0))
    route = G["PeerMemberRoute"](home_install_id=ro.get("home_install_id", ""),
                                 member_id=ro.get("member_id", ""),
                                 target_install_id=ro.get("target_install_id", ""),
                                 target_profile=ro.get("target_profile", ""),
                                 capability_digest=ro.get("capability_digest", ""),
                                 cancellation_scope_id=ro.get("cancellation_scope_id", ""),
                                 trace_id=ro.get("trace_id", ""), grant=ro.get("grant", ""))
    G["PeerHostedRoomTransport"](binding=binding, route=route, client=Client())
    return captured


FUNCS = {"route_identity", "dispatch", "failover_order", "failover_call", "validate",
         "submit_guard", "interrupt", "seq", "bind_args"}


def run_case(r):
    f = r.get("func")
    if f == "route_identity":
        return run_route_identity(r)
    if f == "dispatch":
        return run_dispatch(r)
    if f == "failover_order":
        return run_failover_order(r)
    if f == "failover_call":
        return run_failover_call(r)
    if f == "validate":
        return run_validate(r)
    if f == "submit_guard":
        return run_submit_guard(r)
    if f == "interrupt":
        return run_interrupt(r)
    if f == "seq":
        return run_seq(r)
    if f == "bind_args":
        return run_bind_args(r)
    return None


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
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
