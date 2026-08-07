"""Oracle for tools/flux3_video_tool.py pure helpers.

Tests against LIVE Python source.
"""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from tools.flux3_video_tool import (
    _looks_like_local_path,
    _display_path,
    _filename_from_url,
    _is_transport_error,
    _poll_is_finished,
    _retry_after_seconds,
    _still_generating,
    _without_media,
    _submit_args,
)


def run(c):
    op = c.get("op")
    if op == "_looks_like_local_path":
        return _looks_like_local_path(c["value"])
    if op == "_display_path":
        return _display_path(c["value"])
    if op == "_filename_from_url":
        return _filename_from_url(c["value"])
    if op == "_is_transport_error":
        return _is_transport_error(c["raw"])
    if op == "_poll_is_finished":
        return _poll_is_finished(c["raw"])
    if op == "_retry_after_seconds":
        r = _retry_after_seconds(c["raw"])
        return r if r is not None else 0
    if op == "_still_generating":
        return _still_generating(c["job_id"])
    if op == "_without_media":
        return _without_media(c.get("args"))
    if op == "_submit_args":
        return _submit_args(c.get("mode", "text_to_video"), c.get("args"))
    return None


def main():
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        elif isinstance(r, bool):
            print("true" if r else "false")
        elif isinstance(r, (int, float)):
            if isinstance(r, float) and r == int(r):
                print(int(r))
            else:
                print(r)
        elif isinstance(r, str):
            print(r)
        else:
            print(json.dumps(r, sort_keys=True))


if __name__ == "__main__":
    main()
