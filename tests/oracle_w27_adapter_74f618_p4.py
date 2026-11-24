#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p4 (file classification + attachment diagnostics)."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p4"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

WANTED = {"resolve_audio_ext", "is_voice_clip", "attachment_label", "file_kind",
          "describe_api_error", "describe_download_failure", "retryable_upload_error"}


class _FakeApiError(Exception):
    def __init__(self, response, message="api error"):
        super().__init__(message)
        self.response = response
        self.message = message


def bare():
    return live.SlackAdapter.__new__(live.SlackAdapter)


def expected(r):
    f = r.get("func")
    if f == "resolve_audio_ext":
        return live._resolve_slack_audio_ext(r.get("file_obj") or {}, r.get("mimetype", ""))
    if f == "is_voice_clip":
        return live._is_slack_voice_clip(r.get("file_obj") or {})
    if f == "attachment_label":
        return live._attachment_label(r.get("file_obj"))
    if f == "file_kind":
        return bare()._slack_file_kind(r.get("file_obj") or {}, r.get("mimetype", ""))
    if f == "describe_api_error":
        return bare()._describe_slack_api_error(r.get("response"), file_obj=r.get("file_obj"))
    if f == "describe_download_failure":
        exc = _FakeApiError(r.get("response"), r.get("message", "boom"))
        return bare()._describe_slack_download_failure(exc, file_obj=r.get("file_obj"))
    if f == "retryable_upload_error":
        status = r.get("status_code")
        resp = SimpleNamespace(status_code=status, get=lambda key, default=None: r.get("response_error"))
        exc = _FakeApiError(resp, r.get("message", "boom"))
        return bare()._is_retryable_upload_error(exc)
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
