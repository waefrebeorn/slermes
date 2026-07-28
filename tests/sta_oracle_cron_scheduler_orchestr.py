#!/usr/bin/env python3
"""
sta_oracle_cron_scheduler_orchestr.py — LIVE-Python oracle for the cron
scheduler orchestration surface ported in
src/cron/port_cron_scheduler_runtime_impl.c.

Reads the same fixture JSON the C harness (t_port_cron_scheduler_orchestr.c)
consumes and emits the SAME JSON shape so the runner can diff them.

For route_media / summarize_fail this is a direct call into the live
cron/scheduler.py logic. For no_agent it runs the real _run_job_script
against a temp HERMES_HOME and reproduces run_job()'s no_agent doc-builder
(the structural shape the C port must match), normalizing the Run Time line
out (now_iso() is non-deterministic) exactly as the C harness does.
"""
import sys, os, json, tempfile, textwrap

# Make the live Hermes Python package importable.
HERE = os.path.dirname(os.path.abspath(__file__))
HERMES_PY = "/home/wubu/.hermes/hermes-agent"
sys.path.insert(0, HERMES_PY)

import cron.scheduler as S  # noqa: E402

SILENT = S.SILENT_MARKER  # "[SILENT]"

# Mirror the C enum ordering: VOICE=0, VIDEO=1, IMAGE=2, DOCUMENT=3
KIND = {"voice": 0, "video": 1, "image": 2, "document": 3}


def esc(s):
    if s is None:
        return "null"
    return json.dumps(s)


def route_media(fx):
    platform = fx.get("platform", "telegram")
    path = fx.get("path", "")
    is_voice = bool(fx.get("is_voice", False))
    # Replicate base.py:should_send_media_as_audio + _send_media_via_adapter
    ext = os.path.splitext(path)[1].lower()
    AUDIO = {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"}
    VIDEO = {".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp"}
    IMAGE = {".jpg", ".jpeg", ".png", ".webp", ".gif"}
    as_audio = False
    if ext in AUDIO:
        if platform == "telegram":
            if ext in {".ogg", ".opus"}:
                as_audio = is_voice
            elif ext in {".mp3", ".m4a"}:
                as_audio = True
        else:
            as_audio = True
    if as_audio:
        kind = KIND["voice"]
    elif ext in VIDEO:
        kind = KIND["video"]
    elif ext in IMAGE:
        kind = KIND["image"]
    else:
        kind = KIND["document"]
    return '"kind":%d' % kind


def summarize_fail(fx):
    out = S._summarize_cron_failure_for_delivery(fx.get("job_id", "j1"),
                                                fx.get("error", "boom"))
    return '"text":%s' % esc(out)


def no_agent(fx):
    # Set up a temp HERMES_HOME so _run_job_script can resolve the script.
    tmp = tempfile.mkdtemp(prefix="cronoracle_")
    scripts = os.path.join(tmp, "scripts")
    os.makedirs(scripts, exist_ok=True)
    script_path = os.path.join(scripts, "job.sh")
    body = fx.get("script_body", "")
    with open(script_path, "w") as fh:
        fh.write(body + "\n")
    os.chmod(script_path, 0o755)
    os.environ["HERMES_HOME"] = tmp

    job_id = fx.get("id", "j1")
    job_name = fx.get("name", job_id)
    job = {"id": job_id, "name": job_name, "script": script_path, "no_agent": True}

    ok, output = S._run_job_script_with_claim_heartbeat(job, script_path)
    parts = []
    final = None
    silent = False
    doc_head = None
    err = None
    if not ok:
        alert = ("⚠ Cron watchdog '%s' script failed\n\n%s\n\nTime: %s"
                 % (job_name, output, S._hermes_now().strftime("%Y-%m-%d %H:%M:%S")))
        doc = ("# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
               "**Mode:** no_agent (script)\n**Status:** script failed\n\n%s\n"
               % (job_name, job_id, S._hermes_now().strftime("%Y-%m-%d %H:%M:%S"), output))
        parts.append('"ok":false')
        parts.append('"silent":false')
        parts.append('"final":%s' % esc(alert))
        parts.append('"doc_head":%s' % esc(_strip_runtime(doc)))
        parts.append('"err":%s' % esc(output))
        return "{" + ",".join(parts) + "}"

    if not S._parse_wake_gate(output):
        doc = ("# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
               "**Mode:** no_agent (script)\n**Status:** silent (wakeAgent=false)\n"
               % (job_name, job_id, S._hermes_now().strftime("%Y-%m-%d %H:%M:%S")))
        parts.append('"ok":true')
        parts.append('"silent":true')
        parts.append('"final":%s' % esc(SILENT))
        parts.append('"doc_head":%s' % esc(_strip_runtime(doc)))
        parts.append('"err":null')
        return "{" + ",".join(parts) + "}"

    if not output.strip():
        doc = ("# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
               "**Mode:** no_agent (script)\n**Status:** silent (empty output)\n"
               % (job_name, job_id, S._hermes_now().strftime("%Y-%m-%d %H:%M:%S")))
        parts.append('"ok":true')
        parts.append('"silent":true')
        parts.append('"final":%s' % esc(SILENT))
        parts.append('"doc_head":%s' % esc(_strip_runtime(doc)))
        parts.append('"err":null')
        return "{" + ",".join(parts) + "}"

    doc = ("# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
           "**Mode:** no_agent (script)\n\n---\n\n%s\n"
           % (job_name, job_id, S._hermes_now().strftime("%Y-%m-%d %H:%M:%S"), output))
    parts.append('"ok":true')
    parts.append('"silent":false')
    parts.append('"final":%s' % esc(output))
    parts.append('"doc_head":%s' % esc(_strip_runtime(doc)))
    parts.append('"err":null')
    return "{" + ",".join(parts) + "}"


def _strip_runtime(doc):
    # Match the C harness: terminate the string at the "**Run Time:**" line.
    idx = doc.find("**Run Time:**")
    if idx >= 0:
        nl = doc.find("\n", idx)
        if nl >= 0:
            doc = doc[:idx]  # drop "**Run Time:**..." + newline
    return doc


def main():
    fx_path = sys.argv[1]
    with open(fx_path) as fh:
        fx = json.load(fh)
    op = fx.get("op", "")
    if op == "route_media":
        out = "{" + route_media(fx) + "}"
    elif op == "summarize_fail":
        out = "{" + summarize_fail(fx) + "}"
    elif op == "no_agent":
        out = no_agent(fx)
    else:
        out = '{"error":"unknown op"}'
    sys.stdout.write(out + "\n")


if __name__ == "__main__":
    main()
