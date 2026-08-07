"""Oracle for tools/flux3_video_tool.py.
Tests _still_generating, _resolve_destination, _free_path against Python.
"""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from tools.flux3_video_tool import _still_generating, _resolve_destination, _free_path, _shared_submit_properties


def run(c):
    op = c.get("op")
    if op == "_still_generating":
        return _still_generating(c["job_id"])
    if op == "_resolve_destination":
        save_to = c.get("save_to")
        filename = c.get("filename", "flux3-video.mp4")
        from pathlib import Path
        result = _resolve_destination(save_to, filename)
        return str(result)
    if op == "_free_path":
        directory = c.get("directory", "/tmp")
        name = c.get("name", "flux3-video.mp4")
        from pathlib import Path
        result = _free_path(Path(directory) / name)
        return str(result)
    if op == "_shared_submit_properties":
        return _shared_submit_properties()
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
