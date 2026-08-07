"""Oracle for gateway/channel_directory.py _normalize_adapter_channels."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from gateway.channel_directory import _normalize_adapter_channels


def run(c):
    op = c.get("op", "")
    if op == "normalize_adapter_channels":
        result = _normalize_adapter_channels(c.get("value", []))
        return json.dumps(result, sort_keys=True)
    return None


def main():
    fixture = sys.argv[1]
    cases = json.load(open(fixture))
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        else:
            print(r)


if __name__ == "__main__":
    main()
