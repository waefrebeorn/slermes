#!/usr/bin/env python3
"""Faithfulness oracle for port_tools_read_extract.c.

Reads the *.in fixture (argv[1]); the .in contains the basename of a sample
document (sample.docx / sample.xlsx / sample.ipynb) in the SAME fixture
directory. Recomputes the extraction from the LIVE tools/read_extract.py and
emits a compact JSON line {"fn":<base>,"out":<text>} that the runner diffs
against the C harness.

ensure_ascii=False mirrors the C harness (which emits raw UTF-8 and escapes
only ", \\, and the standard control chars), so the two are byte-identical.
"""
import sys, os, json

sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.read_extract import extract_document_text


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_read_extract.py <sample.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        base = f.read().strip()

    here = os.path.dirname(os.path.abspath(sys.argv[1]))
    sample = os.path.join(here, base)
    text = extract_document_text(sample)
    print(json.dumps({"fn": base, "out": text}, separators=(",", ":"), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
