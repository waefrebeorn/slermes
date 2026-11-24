#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/event.py (gate w19_event_eea6c7)."""
import ast, dataclasses, datetime, enum, json, re, sys, types, unicodedata
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "gateway" / "platforms" / "event.py"
GATE = "w19_event_eea6c7"
WANTED_FUNCS = {"clean_attachment_filename"}
WANTED_CLASSES = {"MessageType", "ProcessingOutcome", "MessageEvent"}
WANTED_VALUES = {"_ATTACHMENT_REF_RE", "_MAX_FILENAME_CHARS"}


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "event_live", "dataclass": dataclasses.dataclass,
         "field": dataclasses.field, "datetime": datetime.datetime,
         "Enum": enum.Enum, "re": re, "unicodedata": unicodedata,
         "Any": object, "Dict": dict, "List": list, "Optional": object,
         "SessionSource": object}
    for node in tree.body:
        if isinstance(node, ast.Import):
            for alias in node.names:
                g[alias.asname or alias.name.split(".")[0]] = __import__(alias.name)
        elif isinstance(node, ast.ImportFrom):
            if node.module and node.module.startswith("gateway"):
                for alias in node.names:
                    g[alias.asname or alias.name] = object
                continue
            for alias in node.names:
                mod = __import__(node.module, fromlist=[alias.name])
                g[alias.asname or alias.name] = getattr(mod, alias.name, object)
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
ME = G["MessageEvent"]


def ev(r, **kw):
    return ME(text=kw.pop("text", r.get("text")), **kw)


def run_case(r):
    f = r.get("func")
    text = r.get("text")
    allow = r.get("allow_gateway_control", True)
    if f == "clean_filename":
        return G["clean_attachment_filename"](r.get("name"))
    if f == "command_text":
        return ME(text=text)._command_text()
    if f == "is_command":
        return ME(text=text, allow_gateway_control=allow).is_command()
    if f == "get_command":
        return ME(text=text, allow_gateway_control=allow).get_command()
    if f == "get_command_args":
        return ME(text=text, allow_gateway_control=allow).get_command_args()
    if f == "sender_id":
        src = types.SimpleNamespace(user_id=r.get("source_user_id"))
        return ME(text="", user_id=r.get("user_id"), source=src).sender_id()
    if f == "absorb_reply_expected":
        e = ME(text="", reply_expected=r.get("self_reply_expected"))
        o = ME(text="", reply_expected=r.get("other_reply_expected"))
        e.absorb_reply_expected(o)
        return e.reply_expected
    if f == "absorb_turn_input":
        src_self = types.SimpleNamespace(user_id=r.get("self_source_user_id"))
        src_other = types.SimpleNamespace(user_id=r.get("other_source_user_id"))
        e = ME(text="", user_id=r.get("self_user_id"), user_name=r.get("self_user_name"),
               source=src_self, reply_expected=r.get("self_reply_expected"))
        o = ME(text="", user_id=r.get("other_user_id"), user_name=r.get("other_user_name"),
               source=src_other, reply_expected=r.get("other_reply_expected"))
        e.absorb_turn_input(o)
        return {"user_id": e.user_id, "user_name": e.user_name,
                "reply_expected": e.reply_expected}
    if f == "extend_media":
        a = ME(text="", media_urls=list(r.get("self_urls") or []),
               media_types=list(r.get("self_types") or []),
               media_filenames=list(r.get("self_names") or []))
        b = ME(text="", media_urls=list(r.get("other_urls") or []),
               media_types=list(r.get("other_types") or []),
               media_filenames=list(r.get("other_names") or []))
        a.extend_media(b)
        return {"media_urls": a.media_urls, "media_types": a.media_types,
                "media_filenames": a.media_filenames}
    if f == "accept_interrupt":
        events = [ME(text="") for _ in r.get("events") or []]
        for i, spec in enumerate(r.get("events") or []):
            target = spec.get("rewritten_from")
            if target is not None:
                events[i]._rewritten_from = events[target]
        if events:
            events[0].accept_interrupt()
        return [bool(getattr(e, "_interrupt_accepted", False)) for e in events]
    return None


FUNCS = {"clean_filename", "command_text", "is_command", "get_command", "get_command_args",
         "sender_id", "absorb_reply_expected", "absorb_turn_input", "extend_media",
         "accept_interrupt"}


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
