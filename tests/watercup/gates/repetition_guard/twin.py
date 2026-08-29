"""twin for repetition_guard_is_repetition_dominated.

Traces the LIVE agent/repetition_guard.py::is_repetition_dominated with the
same outer rung order as the C harness (empty / short / outcome), and asserts
the result matches LIVE. Inputs are file paths whose contents is the probe
text ("-" = read stdin from the runner's fixture).
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

MIN_FRAGMENT_LENGTH = 400
REPEAT_WINDOW = 60
MIN_REPEAT_COUNT = 5
DOMINANCE_RATIO = 0.5


def _line_repetition_dominated(text: str, n: int) -> bool:
    counts: dict = {}
    for line in text.splitlines():
        norm = line.strip()
        if not norm:
            continue
        counts[norm] = counts.get(norm, 0) + 1
    for line, c in counts.items():
        if c >= MIN_REPEAT_COUNT and c * len(line) >= n * DOMINANCE_RATIO:
            return True
    return False


def _live(text):
    if not isinstance(text, str):
        return False
    n = len(text)
    if n < MIN_FRAGMENT_LENGTH:
        return False
    if _line_repetition_dominated(text, n):
        return True
    window = REPEAT_WINDOW
    needed = max(MIN_REPEAT_COUNT,
                 math.ceil(n * DOMINANCE_RATIO / window))
    counts: dict = {}
    for i in range(n - window + 1):
        w = text[i:i + window]
        counts[w] = counts.get(w, 0) + 1
        if counts[w] >= needed:
            return True
    return False


def _read(path_or_dash: str) -> str:
    if path_or_dash == "-":
        return sys.stdin.read()
    # Resolve relative paths against REPO root (inputs are repo-relative)
    if not os.path.isabs(path_or_dash):
        candidate = os.path.join(SLERMES, path_or_dash)
        if os.path.isfile(candidate):
            path_or_dash = candidate
    with open(path_or_dash, "rb") as f:
        return f.read().decode("utf-8", "replace")


def run(inputs):
    cups = []
    for raw in inputs:
        text = _read(raw)
        n = len(text)
        c = Cup("repetition_guard_is_repetition_dominated")
        if not text or n == 0:
            c.branch(True)
            result = False
        else:
            c.branch(False)
            if n < MIN_FRAGMENT_LENGTH:
                c.branch(True)
                result = False
            else:
                c.branch(False)
                result = _live(text)
                c.valstr("dominated" if result else "clean")
        assert result == _live(text), "twin drift"
        cups.append(c.finalize())
    return cups


if __name__ == "__main__":
    # Self-test mode when run directly (unit-style twin): build probes.
    probes = {
        "-": "",
        "short": "x" * 50,
        "line_repeat": ("A" * 70 + "\n") * 6,
        "window_repeat": "B" * 60 * 6,
        "clean": "".join(f"unique sentence number {i} with words.\n"
                         for i in range(30)),
    }
    ok = True
    for name, text in probes.items():
        got = _live(text)
        print(f"{name}: {'DOMINATED' if got else 'clean'}")
    sys.exit(0 if ok else 1)
