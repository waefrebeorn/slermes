#!/usr/bin/env python3
"""Substitute fixture placeholders for a whole directory in ONE process.

The runner used to run `head | tr | wc | awk` for binary detection (~103 ms) and
`sed` for placeholder substitution (~66 ms) once per case. Measured on a
48-case port that is ~169 ms/case of process startup -- more than the C harness
it was preparing input for.

Binary fixtures (*.in that is really a seeded sqlite DB or a blob) must NOT go
through text substitution: it corrupts them. Detection is NUL-byte density in
the first 8KB, matching the old pipeline.

Usage: prep_fixtures.py <fixture_dir> <work_dir> <sbx_path>
Creates <work_dir>/<case>.subst for every fixture and prints, per case:
    case<TAB>substituted_path<TAB>extra_argv
"""
import shutil
import sys
from pathlib import Path

fixdir, workdir, sbx = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
workdir.mkdir(parents=True, exist_ok=True)

rows = []
for f in sorted(fixdir.glob("*.in")):
    case = f.stem
    extra = ""
    argf = fixdir / f"args.{case}"
    if argf.exists():
        extra = argf.read_text(encoding="utf-8", errors="replace").strip()
    data = f.read_bytes()
    out = workdir / f"{case}.subst"
    if data[:8192].count(0) > 16:
        shutil.copyfile(f, out)
    else:
        text = data.decode("utf-8", errors="replace")
        # Literal replacement: sed's & and \1 metacharacters are not special
        # here, so a sandbox path containing them cannot corrupt the fixture.
        text = (text.replace("@SBX@", sbx)
                    .replace("@NOW@", "1700000000.0")
                    .replace("@NOTREPO@", "/tmp"))
        out.write_text(text, encoding="utf-8", newline="")
    rows.append(f"{case}\t{out}\t{extra}")
print("\n".join(rows))
