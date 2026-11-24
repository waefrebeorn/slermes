#!/usr/bin/env python3
"""Differential oracle for agent/conversation_compression_manual.py (gate w19_conversati_84b9cf)."""
import ast, dataclasses, importlib, json, sys, types
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "agent" / "conversation_compression_manual.py"
PARTIAL = REPO.parent / "hermes-agent" / "hermes_cli" / "partial_compress.py"
FEEDBACK = REPO.parent / "hermes-agent" / "agent" / "manual_compression_feedback.py"
GATE = "w19_conversati_84b9cf"
MARKER = "__db_persisted__"
WANTED_FUNCS = {"summary_committed", "parse_compress_args", "estimate_request_tokens",
                "compress_now", "render_compress_result"}
WANTED_CLASSES = {"CompressRequest", "CompressResult"}
WANTED_VALUES = {"AGGRESSIVE_UNSUPPORTED", "MIN_MESSAGES"}
CURRENT = {}


def _exec(path, wanted_funcs, wanted_classes, wanted_values, g, flags=0):
    tree = ast.parse(Path(path).read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted_funcs:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=flags, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in wanted_classes:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=flags, dont_inherit=True), g)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            names = {t.id for t in targets if isinstance(t, ast.Name)}
            if names & wanted_values:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                             flags=flags, dont_inherit=True), g)
    return g


def extract():
    g = {"__name__": "manual_live", "dataclass": dataclasses.dataclass,
         "field": dataclasses.field, "Any": object, "Callable": object, "Dict": dict,
         "List": list, "Optional": object, "Sequence": list, "list": list, "dict": dict,
         "bool": bool, "int": int, "str": str}
    _exec(SRC, WANTED_FUNCS, WANTED_CLASSES, WANTED_VALUES, g)
    return g


def extract_partial():
    g = {"__name__": "partial_compress_live", "List": list, "Any": object, "Optional": object,
         "Dict": dict, "Sequence": object}
    _exec(PARTIAL, {"extract_compress_flags", "parse_partial_compress_args", "_coerce_keep",
                    "split_history_for_partial_compress", "summarize_compress_preview",
                    "rejoin_compressed_head_and_tail"},
          set(), {"DEFAULT_KEEP_LAST", "MAX_KEEP_LAST"}, g)
    return g


def extract_feedback():
    g = {"__name__": "feedback_live", "Any": object, "Sequence": object,
         "redact_sensitive_text": lambda text, force=False: text}
    try:
        import agent.redact as _redact  # live redactor when importable
        g["redact_sensitive_text"] = _redact.redact_sensitive_text
    except Exception:
        pass
    _exec(FEEDBACK, {"describe_compression_lock_skip", "summarize_manual_compression"},
          set(), set(), g)
    return g


G = extract()
P = extract_partial()
F = extract_feedback()


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


stub_module("hermes_cli.partial_compress", **{k: P[k] for k in
            ("extract_compress_flags", "parse_partial_compress_args", "split_history_for_partial_compress",
             "summarize_compress_preview", "rejoin_compressed_head_and_tail")})
stub_module("hermes_cli")
stub_module("agent.context_compressor", _DB_PERSISTED_MARKER=MARKER,
            _fresh_compaction_message_copy=lambda m: {k: v for k, v in m.items() if k != MARKER})
stub_module("agent.conversation_compression",
            finalize_context_engine_compression_notification=lambda agent, committed=False: None)
stub_module("agent.manual_compression_feedback",
            summarize_manual_compression=F["summarize_manual_compression"],
            describe_compression_lock_skip=F["describe_compression_lock_skip"])
stub_module("agent.model_metadata", estimate_request_tokens_rough=None)


def install_estimator(r):
    def rough(messages, system_prompt=None, tools=None):
        key = str(len(messages))
        mp = r.get("rough_map") or {}
        return mp.get(key, r.get("rough_default", 0))
    stub_module("agent.model_metadata", estimate_request_tokens_rough=rough)


class FakeCompressor:
    def __init__(self, has_content, state=None):
        if has_content is not None:
            self.has_content_to_compress = lambda head: bool(has_content)
        for key, value in (state or {}).items():
            setattr(self, key, value)


class FakeAgent:
    def __init__(self, r):
        self._cached_system_prompt = ""
        self.tools = None
        self.context_compressor = FakeCompressor(r.get("has_content"), r.get("compression_state"))
        self.compression_summary = r.get("agent_summary")
        self._compression_skipped_due_to_lock = r.get("lock_signal")
        self._r = r

    def _compress_context(self, head, system_message, **kw):
        if self._r.get("context_error"):
            raise RuntimeError("compress blew up")
        if self._r.get("stamped"):
            for row in kw.get("verbatim_tail") or []:
                row[MARKER] = True
        return list(self._r.get("compressed") or []), None


def run_compress_now(r):
    install_estimator(r)
    req = G["CompressRequest"](**{k: r.get("request", {}).get(k) for k in
                                  ("preview", "aggressive", "partial", "keep_last", "focus_topic")
                                  if k in (r.get("request") or {})})
    if "keep_last" not in (r.get("request") or {}):
        req.keep_last = 2
    agent = FakeAgent(r)
    try:
        res = G["compress_now"](agent, list(r.get("history") or []), req,
                                skip_without_window=bool(r.get("skip_without_window")))
    except BaseException:
        return {"status": "error"}
    return {
        "status": res.status,
        "before_tokens": res.before_tokens,
        "after_tokens": res.after_tokens,
        "removed": res.removed,
        "committed": res.committed,
        "lock_holder": res.lock_holder,
        "summary": res.summary,
        "lines": list(res.lines),
        "after_count": len(res.after_messages),
    }


def run_case(r):
    f = r.get("func")
    if f == "summary_committed":
        return G["summary_committed"](r.get("summary"))
    if f == "parse":
        req = G["parse_compress_args"](r.get("raw") or "")
        return {"preview": req.preview, "aggressive": req.aggressive, "partial": req.partial,
                "keep_last": req.keep_last, "focus_topic": req.focus_topic}
    if f == "estimate":
        install_estimator(r)
        agent = types.SimpleNamespace(_cached_system_prompt="", tools=None)
        return G["estimate_request_tokens"](agent, list(r.get("messages") or []))
    if f == "compress_now":
        return run_compress_now(r)
    if f == "render":
        spec = r.get("result") or {}
        result = types.SimpleNamespace(status=spec.get("status"), lines=list(spec.get("lines") or []),
                                       lock_holder=spec.get("lock_holder"),
                                       summary=spec.get("summary"))
        return G["render_compress_result"](result, prefix=r.get("prefix", ""))
    if f == "props":
        req = G["CompressRequest"]()
        res = G["CompressResult"](status=r.get("status", ""),
                                  before_messages=list(r.get("before_messages") or []),
                                  after_messages=list(r.get("after_messages") or []),
                                  before_tokens=0, after_tokens=0, request=req,
                                  summary=r.get("summary"))
        return {"removed": res.removed, "committed": res.committed}
    if f == "constants":
        return {"MIN_MESSAGES": G["MIN_MESSAGES"],
                "AGGRESSIVE_UNSUPPORTED": G["AGGRESSIVE_UNSUPPORTED"],
                "DEFAULT_KEEP_LAST": P["DEFAULT_KEEP_LAST"],
                "MAX_KEEP_LAST": P["MAX_KEEP_LAST"]}
    return None


FUNCS = {"summary_committed", "parse", "estimate", "compress_now", "render", "props", "constants"}


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
