#!/usr/bin/env python3
"""
sta_oracle_skills_discover.py — oracle for t_port_skills_discover.c.

Reference implementation of the skill-discovery contract: a directory is a
"skill" iff it contains a SKILL.md; a directory without SKILL.md but with
subdirectories is recursed into. Emits {"discovered":[sorted basenames]}.
The C harness builds the same tree from the fixture and runs the real
_discover_skills_in_dir(); the runner diffs the two.
"""
import sys
import os
import tempfile


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_skills_discover.py <tree.txt>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        text = f.read().decode("utf-8")

    root = tempfile.mkdtemp(prefix="skills_disc_oracle_")
    skills = []

    def wf(p, c):
        with open(p, "w") as fh:
            fh.write(c)

    for line in text.split("\n"):
        line = line.strip()
        if not line:
            continue
        parts = line.split(" ")
        op = parts[0]
        if op == "skill":
            name = parts[1]
            d = os.path.join(root, name)
            os.makedirs(d, exist_ok=True)
            wf(os.path.join(d, "SKILL.md"), "---\nname: %s\n---\n" % name)
            skills.append(name)
        elif op == "nested" and len(parts) >= 3:
            parent, name = parts[1], parts[2]
            pd = os.path.join(root, parent)
            os.makedirs(pd, exist_ok=True)
            d = os.path.join(pd, name)
            os.makedirs(d, exist_ok=True)
            wf(os.path.join(d, "SKILL.md"), "---\nname: %s\n---\n" % name)
            skills.append(name)
        elif op == "noskill":
            name = parts[1]
            os.makedirs(os.path.join(root, name), exist_ok=True)

    skills.sort()
    sys.stdout.write('{"discovered":[%s]}\n' % ",".join('"%s"' % s for s in skills))

    import shutil
    shutil.rmtree(root, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
