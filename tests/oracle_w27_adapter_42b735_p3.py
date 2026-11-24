#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/whatsapp/adapter.py slice 3
(gate w27_adapter_42b735_p3): inbound message shaping.

``WhatsAppAdapter`` instances come from ``__new__`` with only the attributes
each member reads.  The seams are the media cache (never touched) and the
filesystem (real temp files, written from the fixture's own ``content``):

  * ``_classify_bridge_message``    bridge mediaType -> MessageType value;
  * ``_may_transcribe_at_gate``     the group-ptt + voice_mention_patterns gate;
  * ``_inject_document_text``       text-readable docs prepended to the body;
  * ``_log_delivery_status``        the exact logger.info text of one line.
"""
import contextlib
import importlib.util
import io
import json
import logging
import os
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_42b735_p3"

# The msys bash that runs this oracle has no USERPROFILE, and pathlib.Path.home()
# (touched at import time by hermes_constants) raises without it.  Give it the
# real Windows home the repo lives under.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(REPO).parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = os.environ["USERPROFILE"]

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "wa_adapter_live_" + GATE, QUARRY / "plugins" / "platforms" / "whatsapp" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
with contextlib.redirect_stdout(io.StringIO()):
    _spec.loader.exec_module(live)

ADAPTER = live.WhatsAppAdapter
WANTED = {"classify_bridge_message", "may_transcribe_at_gate",
          "inject_document_text", "delivery_status_line"}


def bare():
    inst = ADAPTER.__new__(ADAPTER)
    inst.platform = live.Platform.WHATSAPP
    return inst


def as_dict(value):
    return value if isinstance(value, dict) else {}


def classify_bridge_message(req):
    return ADAPTER._classify_bridge_message(as_dict(req.get("data"))).value


def may_transcribe_at_gate(req):
    inst = bare()
    inst.config = SimpleNamespace(extra=as_dict(req.get("extra")))
    return bool(inst._may_transcribe_at_gate(as_dict(req.get("data"))))


def inject_document_text(req):
    raw_docs = req.get("docs")
    docs = [d for d in raw_docs if isinstance(d, dict)] if isinstance(raw_docs, list) else []
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for doc in docs:
            path = Path(tmp) / str(doc.get("name") or "doc")
            path.parent.mkdir(parents=True, exist_ok=True)
            content = str(doc.get("content") or "").encode("utf-8")
            path.write_bytes(content)
            # "size" only models the >_MAX_TEXT_INJECT_BYTES skip branch, where the
            # body is never read: pad the file so stat() agrees with the fixture.
            if "size" in doc and int(doc["size"]) > live._MAX_TEXT_INJECT_BYTES:
                with path.open("r+b") as handle:
                    handle.truncate(int(doc["size"]))
            paths.append(str(path))
        body, inlined = bare()._inject_document_text(paths, str(req.get("body") or ""))
    return [body, inlined]


def delivery_status_line(req):
    records = []

    class Capture(logging.Handler):
        def emit(self, record):
            records.append(record.getMessage())

    class NamedAdapter(ADAPTER):
        """The log line only reads ``self.name``; drive it from the fixture."""

        name = property(lambda self: getattr(self, "_fixture_name", "Whatsapp"))

    handler = Capture()
    inst = NamedAdapter.__new__(NamedAdapter)
    inst._fixture_name = str(req.get("name") or "Whatsapp")
    live.logger.addHandler(handler)
    try:
        live.logger.setLevel(logging.INFO)
        inst._log_delivery_status(as_dict(req.get("data")))
    finally:
        live.logger.removeHandler(handler)
    return records[-1] if records else ""


DISPATCH = {
    "classify_bridge_message": classify_bridge_message,
    "may_transcribe_at_gate": may_transcribe_at_gate,
    "inject_document_text": inject_document_text,
    "delivery_status_line": delivery_status_line,
}


def expected(req):
    return DISPATCH[req["func"]](req)


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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
