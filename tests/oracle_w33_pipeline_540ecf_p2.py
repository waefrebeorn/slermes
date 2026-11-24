#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/pipeline.py, shaping/stage
half (gate w33_pipeline_540ecf_p2).

The live module is imported BY FILE PATH (the package __init__ drags the gateway
stack and this host refuses pydantic's compiled extension); the shared seam
stubs cover the import chain.  The seams the pipeline itself has -- the Graph
client, the LLM call, the STT thread, ffmpeg discovery, tempfile dirs and the
store -- are replaced here with recording fakes, so the *live decision code*
runs and only its I/O is stubbed.

Ported symbols and their live entry points:

  rich_text            _rich_text(content)
  bullets              _bullets(items)
  sections             _sections(payload)
  notion_properties    NotionWriter._build_properties(payload, config)
  notion_blocks        NotionWriter._build_blocks(payload)
  linear_body          LinearWriter.write_summary body branch (transport faked)
  render_markdown      _render_summary_markdown(payload)
  summary_prompt       _build_summary_prompt(ref, transcript, artifacts)
  parse_summary        _parse_summary_json(content)
  heuristic_summary    _heuristic_summary(transcript_text)
  participants         _collect_participants(ref)
  call_metrics         _collect_call_metrics(artifacts)
  payload_targets      _generate_summary_payload field shaping (LLM faked to fail)
  transcript_decision  _obtain_transcript (Graph + STT faked)
  recording_name       _transcribe_recording / _prepare_audio_path (I/O faked)
  stt_outcome          _transcribe_recording result classification
  sink_plan            _write_sinks (sinks + store faked)
"""
import asyncio
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_pipeline_540ecf_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w33-teams-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME
_TMPROOT = Path(tempfile.mkdtemp(prefix="w33-teams-tmp-"))

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

models = sys.modules["plugins.teams_pipeline.models"]

WANTED = {
    "rich_text", "bullets", "sections", "notion_properties", "notion_blocks",
    "linear_body", "render_markdown", "summary_prompt", "parse_summary",
    "heuristic_summary", "participants", "call_metrics", "payload_targets",
    "transcript_decision", "recording_name", "stt_outcome", "sink_plan",
}


def payload_model(r):
    return live.TeamsMeetingSummaryPayload.from_dict(r.get("payload") or {"meeting_ref": {"meeting_id": "m-0"}})


def ref_model(value):
    value = value if isinstance(value, dict) else {}
    return live.TeamsMeetingRef.from_dict(value or {"meeting_id": "m-0"})


class FakeStore:
    """Store seam: keeps whatever the pipeline persists, in memory."""

    def __init__(self):
        self.jobs = {}
        self.sink_records = {}
        self.statuses = []
        self.sink_keys = []

    def upsert_job(self, job_id, payload):
        self.jobs[job_id] = payload
        self.statuses.append(payload.get("status"))
        return payload

    def get_sink_record(self, key):
        return self.sink_records.get(key)

    def upsert_sink_record(self, key, record):
        self.sink_records[key] = record
        self.sink_keys.append(key)
        return record


def bare_pipeline(config_payload=None):
    pipeline = live.TeamsMeetingPipeline.__new__(live.TeamsMeetingPipeline)
    pipeline.config = live.TeamsPipelineConfig.from_dict(config_payload or {})
    pipeline.store = FakeStore()
    pipeline.graph_client = None  # the Graph client is the seam these gates stub
    return pipeline


# ---------------------------------------------------------------- shaping --

def rich_text(r):
    return live._rich_text(r.get("content") or "")


def bullets(r):
    return live._bullets(r.get("items") or [])


def sections(r):
    return [list(pair) for pair in live._sections(payload_model(r))]


def notion_properties(r):
    writer = live.NotionWriter.__new__(live.NotionWriter)
    return writer._build_properties(payload_model(r), r.get("config") or {})


def notion_blocks(r):
    writer = live.NotionWriter.__new__(live.NotionWriter)
    return writer._build_blocks(payload_model(r))


def render_markdown(r):
    return live._render_summary_markdown(payload_model(r))


def summary_prompt(r):
    artifacts = [live.MeetingArtifact.from_dict(item) for item in (r.get("artifacts") or [])]
    return live._build_summary_prompt(ref_model(r.get("meeting_ref")), r.get("transcript_text") or "", artifacts)


def parse_summary(r):
    return live._parse_summary_json(r.get("content") or "")


def heuristic_summary(r):
    return live._heuristic_summary(r.get("transcript_text") or "")


def participants(r):
    return live._collect_participants(ref_model(r.get("meeting_ref")))


def call_metrics(r):
    artifacts = [live.MeetingArtifact.from_dict(item) for item in (r.get("artifacts") or [])]
    return live._collect_call_metrics(artifacts)


def linear_body(r):
    """The real write_summary branch, with only the HTTP transport faked."""
    writer = live.LinearWriter.__new__(live.LinearWriter)
    writer.api_key = "test-key"
    writer._transport = None
    captured = {}

    async def fake_request(method, url, *, headers, body):
        captured["body"] = body
        return {"data": {"issueCreate": {"issue": {"id": "lin-1", "identifier": "ENG-1", "url": "u"}},
                         "issueUpdate": {"issue": {"id": "lin-1", "identifier": "ENG-1", "url": "u"}}}}

    writer._request = fake_request
    existing = r.get("existing_record") if isinstance(r.get("existing_record"), dict) else None
    mode = "update" if (existing or {}).get("issue_id") else "create"
    try:
        asyncio.run(writer.write_summary(payload_model(r), r.get("config") or {}, existing))
    except live.TeamsPipelineSinkError as exc:
        return {"mode": "error", "body": None, "error": str(exc)}
    return {"mode": mode, "body": captured.get("body"), "error": None}


def payload_targets(r):
    pipeline = bare_pipeline({
        "notion": r.get("notion"),
        "linear": r.get("linear"),
        "teams_delivery": r.get("teams_delivery"),
    })

    async def no_llm(**kwargs):
        raise RuntimeError("llm unavailable")

    live.async_call_llm = no_llm
    payload = asyncio.run(pipeline._generate_summary_payload(
        resolved_meeting=ref_model(r.get("meeting_ref")), transcript_text="", artifacts=[]))
    return {
        "title": payload.title,
        "start_time": models._serialize_datetime(payload.start_time),
        "end_time": models._serialize_datetime(payload.end_time),
        "notion_target": payload.notion_target,
        "linear_target": payload.linear_target,
        "teams_target": payload.teams_target,
    }


# ----------------------------------------------------------------- stage --

def _job(r, status="received"):
    return live.TeamsMeetingPipelineJob(
        job_id="teams-job-1", event_id="evt-1", source_event_type="created",
        dedupe_key="evt-1", status=status)


def transcript_decision(r):
    pipeline = bare_pipeline(r.get("config") or {})
    has_artifact = bool(r.get("transcript_artifact"))
    recordings = int(r.get("recordings") or 0)
    artifact = None
    if has_artifact:
        artifact = live.MeetingArtifact.from_dict(
            {"artifact_type": "transcript", "artifact_id": "t-1", "display_name": "t.vtt"})

    async def fake_fetch(client, meeting_ref):
        return artifact, r.get("transcript_text")

    async def fake_list(client, meeting_ref):
        return [live.MeetingArtifact.from_dict(
            {"artifact_type": "recording", "artifact_id": "rec-%d" % i}) for i in range(recordings)]

    async def fake_transcribe(job, meeting_ref, recording):
        return "stub transcript"

    live.fetch_preferred_transcript_text = fake_fetch
    live.list_recording_artifacts = fake_list
    pipeline._transcribe_recording = fake_transcribe
    job = _job(r, r.get("status") or "received")
    try:
        job, _text = asyncio.run(pipeline._obtain_transcript(
            job, ref_model({"meeting_id": r.get("meeting_id") or "m-0"}), []))
    except live.TeamsPipelineError as exc:
        # run_job() persists this as retry_scheduled for the whole retryable family.
        return {"status": "retry_scheduled", "strategy": None,
                "error_type": type(exc).__name__, "error_message": str(exc)}
    return {"status": job.status, "strategy": job.selected_artifact_strategy,
            "error_type": None, "error_message": None}


class _FakeProc:
    returncode = 0

    async def communicate(self):
        return b"", b""


async def _fake_exec(*args, **kwargs):
    return _FakeProc()


def _run_transcribe(r, transcribe_fn):
    """_transcribe_recording with the Graph download, ffmpeg and STT faked."""
    config_payload = dict(r.get("config") or {})
    config_payload["tmp_dir"] = str(_TMPROOT)
    if "ffmpeg_extract_audio" in r:
        config_payload["ffmpeg_extract_audio"] = r["ffmpeg_extract_audio"]
    pipeline = bare_pipeline(config_payload)
    pipeline.transcribe_fn = transcribe_fn
    captured = {}

    async def fake_download(client, meeting_ref, artifact, destination, **kwargs):
        captured["path"] = Path(destination)
        Path(destination).parent.mkdir(parents=True, exist_ok=True)
        Path(destination).write_bytes(b"recording-bytes")
        return {}

    live.download_recording_artifact = fake_download
    live.shutil.which = lambda name: "/usr/bin/" + name
    asyncio.create_subprocess_exec = _fake_exec
    recording = live.MeetingArtifact.from_dict({
        "artifact_type": "recording",
        "artifact_id": r.get("artifact_id"),
        "display_name": r.get("display_name"),
    })
    job = _job(r)
    try:
        transcript = asyncio.run(pipeline._transcribe_recording(job, ref_model({"meeting_id": "m-1"}), recording))
    except live.TeamsPipelineError as exc:
        return {"error": exc, "captured": captured, "transcript": None}
    return {"error": None, "captured": captured, "transcript": transcript}


def recording_name(r):
    state = {"audio": None}

    def transcribe_fn(path, model):
        state["audio"] = Path(path)
        return {"success": True, "transcript": "hello"}

    result = _run_transcribe(r, transcribe_fn)
    path = result["captured"].get("path")
    audio = state["audio"]
    return {
        "path": path.name if path else None,
        "audio_path": audio.name if audio else None,
        "suffix": Path(path.name).suffix if path else None,
        "needs_ffmpeg": bool(path and audio and path.name != audio.name),
    }


def stt_outcome(r):
    result_payload = r.get("result") if isinstance(r.get("result"), dict) else {}

    def transcribe_fn(path, model):
        return dict(result_payload)

    result = _run_transcribe(r, transcribe_fn)
    if result["error"] is not None:
        return {"outcome": "retry_scheduled", "transcript": None,
                "error_type": type(result["error"]).__name__, "error_message": str(result["error"])}
    return {"outcome": "transcript", "transcript": result["transcript"],
            "error_type": None, "error_message": None}


class _Writer:
    async def write_summary(self, payload, config, existing_record):
        return {"ok": True}


def sink_plan(r):
    pipeline = bare_pipeline({
        "notion": r.get("notion"),
        "linear": r.get("linear"),
        "teams_delivery": r.get("teams_delivery"),
    })
    pipeline.notion_writer = _Writer() if r.get("have_notion_writer") else None
    pipeline.linear_writer = _Writer() if r.get("have_linear_writer") else None
    pipeline.teams_sender = _Writer() if r.get("have_teams_sender") else None
    payload = live.TeamsMeetingSummaryPayload.from_dict(
        {"meeting_ref": {"meeting_id": r.get("meeting_id") or "m-0"}})
    asyncio.run(pipeline._write_sinks(_job(r), payload))
    names = {"writing_notion": "notion", "writing_linear": "linear", "sending_teams": "teams"}
    return [{"name": names.get(status), "status": status, "sink_key": key}
            for status, key in zip(pipeline.store.statuses, pipeline.store.sink_keys)]


def expected(r):
    f = r.get("func")
    handlers = {
        "rich_text": rich_text, "bullets": bullets, "sections": sections,
        "notion_properties": notion_properties, "notion_blocks": notion_blocks,
        "linear_body": linear_body, "render_markdown": render_markdown,
        "summary_prompt": summary_prompt, "parse_summary": parse_summary,
        "heuristic_summary": heuristic_summary, "participants": participants,
        "call_metrics": call_metrics, "payload_targets": payload_targets,
        "transcript_decision": transcript_decision, "recording_name": recording_name,
        "stt_outcome": stt_outcome, "sink_plan": sink_plan,
    }
    if f not in handlers:
        raise AssertionError("unknown func %r" % (f,))
    return handlers[f](r)


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
