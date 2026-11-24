#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/pipeline.py, ingress half
(gate w33_pipeline_540ecf_p1).

The live module is imported BY FILE PATH: the plugin package import drags the
gateway deps and this host's application-control policy refuses pydantic's
compiled extension, so the shared seam stubs cover the import chain.  Nothing
under test needs them -- every compared value is a pure function of the JSON
inputs the harness feeds the C port.

Ported symbols and their live entry points:

  config                TeamsPipelineConfig.from_dict(payload)
  organizer_user_id     _organizer_user_id_from_payload(payload)
  parse_resource        meetings.parse_graph_meeting_resource(resource)
  transcript_like       meetings.looks_like_transcript_id(value, odata_type=...)
  is_artifact           _resource_data_id_is_artifact(notification, resource_data)
  meeting_ids           _meeting_ids_from_notification(notification)
  receipt_key           TeamsPipelineStore.build_notification_receipt_key(...)
  run_gate              TeamsMeetingPipeline.run_notification status gate
  find_dedupe           TeamsMeetingPipeline._find_job_by_dedupe_key(key)
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_pipeline_540ecf_p1"

_TMPHOME = tempfile.mkdtemp(prefix="w33-teams-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

import importlib.util  # noqa: E402

_SPEC = importlib.util.spec_from_file_location(
    "w33_teams_pipeline_live", QUARRY / "plugins" / "teams_pipeline" / "pipeline.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules["w33_teams_pipeline_live"] = live
_SPEC.loader.exec_module(live)

WANTED = {
    "config", "organizer_user_id", "parse_resource", "transcript_like",
    "is_artifact", "meeting_ids", "receipt_key", "run_gate", "find_dedupe",
}


def as_dict(value):
    return value if isinstance(value, dict) else {}


def config(r):
    cfg = live.TeamsPipelineConfig.from_dict(r.get("payload"))
    return {
        "transcript_preferred": cfg.transcript_preferred,
        "transcript_required": cfg.transcript_required,
        "transcription_fallback": cfg.transcription_fallback,
        "stt_model": cfg.stt_model,
        "ffmpeg_extract_audio": cfg.ffmpeg_extract_audio,
        "transcript_min_chars": cfg.transcript_min_chars,
        "tmp_dir": str(cfg.tmp_dir) if cfg.tmp_dir else None,
        "notion": cfg.notion,
        "linear": cfg.linear,
        "teams_delivery": cfg.teams_delivery,
    }


def organizer_user_id(r):
    return live._organizer_user_id_from_payload(as_dict(r.get("payload")))


def parse_resource(r):
    return live.parse_graph_meeting_resource(r.get("resource") or "")


def transcript_like(r):
    return live.looks_like_transcript_id(str(r.get("value") or ""),
                                         odata_type=r.get("odata_type"))


def is_artifact(r):
    return live._resource_data_id_is_artifact(as_dict(r.get("notification")),
                                              as_dict(r.get("resource_data")))


def meeting_ids(r):
    meeting_id, organizer, extra = live._meeting_ids_from_notification(as_dict(r.get("notification")))
    return {"meeting_id": meeting_id, "organizer_user_id": organizer, "extra_metadata": extra}


def receipt_key(r):
    return live.TeamsPipelineStore.build_notification_receipt_key(as_dict(r.get("notification")))


def run_gate(r):
    status = as_dict(r.get("job")).get("status")
    if status in live.TERMINAL_PIPELINE_STATES:
        return "skip"
    if status in live.ACTIVE_PIPELINE_STATES - {"received"}:
        return "skip"
    return "run"


class _Store:
    def __init__(self, jobs):
        self._jobs = {str(i): job for i, job in enumerate(jobs)}

    def list_jobs(self):
        return dict(self._jobs)


def find_dedupe(r):
    jobs = r.get("jobs")
    pipeline = live.TeamsMeetingPipeline.__new__(live.TeamsMeetingPipeline)
    pipeline.store = _Store(jobs if isinstance(jobs, list) else [])
    job = pipeline._find_job_by_dedupe_key(str(r.get("dedupe_key") or ""))
    return job.job_id if job is not None else None


def expected(r):
    f = r.get("func")
    if f == "config":
        return config(r)
    if f == "organizer_user_id":
        return organizer_user_id(r)
    if f == "parse_resource":
        return parse_resource(r)
    if f == "transcript_like":
        return transcript_like(r)
    if f == "is_artifact":
        return is_artifact(r)
    if f == "meeting_ids":
        return meeting_ids(r)
    if f == "receipt_key":
        return receipt_key(r)
    if f == "run_gate":
        return run_gate(r)
    if f == "find_dedupe":
        return find_dedupe(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
