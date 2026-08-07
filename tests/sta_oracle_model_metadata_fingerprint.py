"""Oracle for agent/model_metadata.py _msg_fingerprint."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT")
if DEV_ROOT and DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from agent.model_metadata import _msg_fingerprint


# Module-level counter to track which pin index we're at.
_string_idx = [0]


def _djb2(s):
    h = 5381
    for ch in s:
        h = ((h << 5) + h) + ord(ch)
    return h & 0x7FFFFFFF


def _stable(v, pins):
    """Recursively replace id() ints with djb2 hash of the corresponding pin."""
    if isinstance(v, tuple):
        if len(v) == 2 and v[0] == "s" and isinstance(v[1], int) and v[1] > 0:
            # This is ("s", id(value)) — replace with hash of pinned string
            idx = _string_idx[0]
            _string_idx[0] += 1
            if idx < len(pins):
                return ("s", _djb2(pins[idx]))
            return v
        return tuple(_stable(x, pins) for x in v)
    return v


def run(c):
    op = c.get("op", "")
    if op == "mm_msg_fingerprint":
        pins = []
        _string_idx[0] = 0
        result = _msg_fingerprint(c.get("value"), pins)
        result = _stable(result, pins)
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
