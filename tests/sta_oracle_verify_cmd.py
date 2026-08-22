#!/usr/bin/env python3
"""sta_oracle_verify_cmd.py — Python twin for hermes_cli/verify_cmd.py,
mirrored by src/hermes_cli/verify_cmd.c.

The real run_verify_command pulls agent.verify / agent.coding_context /
agent.verification_evidence via in-function imports; this twin mirrors the
CLI orchestration, the project-facts merge, the evidence-tail building and
the human report printer exactly (loops_pure precedent), with scripted
hook state driven by the same fixture ops as the C harness.
"""
import io
import json
import sys


def tool_strip(s):
    return s.strip() if isinstance(s, str) else ""


def merge_project_facts_commands(recipe, commands):
    """Mirror of _merge_project_facts_commands (mutates recipe['test'])."""
    existing = {c.strip() for c in
                (*recipe["bootstrap"], *recipe["build"], *recipe["test"]) if c}
    for command in commands:
        command = command.strip()
        if command and command not in existing:
            recipe["test"].append(command)
            existing.add(command)


def record_evidence_output(root, recipe, result, partial):
    tails = []
    for p in result.get("phases", []):
        if p.get("output_tail"):
            tails.append(f"[{p['phase']}] {p['command']}\n{p['output_tail']}")
    r = result.get("readiness")
    if r is not None:
        line = (f"[start] {recipe['start']} -> "
                + (f"ready (HTTP {r['status_code']})" if r["ready"]
                   else f"not ready ({r.get('error') or 'timeout'})"))
        tails.append(line)
    return "\n".join(tails)


def print_human_report(recipe, source, result, out):
    out.write(f"Recipe: {recipe['name']} ({recipe['kind']}) — source: {source}\n\n")
    phases = result.get("phases", [])
    width = max((len(p["phase"]) for p in phases), default=0)
    if phases:
        for p in phases:
            status = "PASS" if p["ok"] else ("TIMEOUT" if p["timed_out"] else "FAIL")
            out.write(f"  {p['phase'].ljust(width)}  {status:<7}  {p['duration']:6.1f}s  {p['command']}\n")
    else:
        out.write("  (no phases executed)\n")
    r = result.get("readiness")
    if r is not None:
        status = (f"ready (HTTP {r['status_code']})" if r["ready"]
                  else f"not ready ({r.get('error') or 'timeout'})")
        out.write(f"  {'start'.ljust(max(5, max((len(p['phase']) for p in phases), default=5)))}  "
                  f"{'PASS' if r['ready'] else 'FAIL':<7}  {r['duration']:6.1f}s  {recipe['start']}\n")
        out.write(f"\nReadiness: {r['url']} -> {status}\n")
    failed = [p for p in phases if not p["ok"]]
    out.write("\n")
    out.write(f"Result: {'OK' if result['ok'] else 'FAILED'}\n")
    for p in failed:
        out.write(f"\n--- output tail: {p['phase']} ({p['command']}) ---\n")
        out.write(p["output_tail"].rstrip() + "\n")
    if r is not None and not r["ready"] and r.get("output_tail"):
        out.write("\n--- output tail: start ---\n")
        out.write(r["output_tail"].rstrip() + "\n")


class Recipe(dict):
    """dict with attribute access used by the orchestration mirror."""
    def __getattr__(self, k):
        return self[k]

    @property
    def to_dict(self):
        return dict(self)


def run_verify_command(c):
    """Mirror of run_verify_command with scripted hooks; returns a dict."""
    args = c.get("args", {})
    root = args.get("path") or "."
    out_lines, err_lines = [], []

    def emit(out_line=None, err_line=None):
        if out_line is not None:
            out_lines.append(out_line)
        if err_line is not None:
            err_lines.append(err_line)

    if not c.get("path_ok", True):
        # stderr print then exit 2 — the C harness captures the raw line
        err_lines.append(f"error: not a directory: {root}")
        return {"exit_code": 2, "stdout": out_lines, "stderr": err_lines,
                "record_count": 0, "scope": "", "evidence_output": "",
                "saved_to": ""}

    recipe = c.get("recipe")
    if recipe is None:
        if args.get("json"):
            emit(json.dumps({"ok": False, "error": "no-recipe", "root": root}))
        else:
            err_lines.append(
                f"No recognizable project found at {root}.\n"
                f"Create {root}/.hermes/environment.json to define a recipe manually.")
        return {"exit_code": 1, "stdout": out_lines, "stderr": err_lines,
                "record_count": 0, "scope": "", "evidence_output": "",
                "saved_to": ""}
    source = c.get("source", "detected")

    import copy
    recipe = copy.deepcopy(recipe)
    if source == "detected":
        merge_project_facts_commands(recipe, c.get("facts_commands", []))
    if args.get("port"):
        recipe["port"] = args["port"]

    saved_to = ""
    if args.get("save"):
        saved_to = c.get("manifest", "")
        if not args.get("json"):
            emit(f"Saved manifest: {saved_to}")

    if args.get("detect_only"):
        payload = {"source": source, "recipe": {
            "name": recipe["name"], "kind": recipe["kind"],
            "start": recipe["start"], "port": recipe["port"],
            "bootstrap": list(recipe["bootstrap"]),
            "build": list(recipe["build"]), "test": list(recipe["test"])}}
        # both sides capture one stdout LINE; emit compact single-line JSON
        emit(json.dumps(payload))
        return {"exit_code": 0, "stdout": out_lines, "stderr": err_lines,
                "record_count": 0, "scope": "", "evidence_output": "",
                "saved_to": saved_to}

    result = c.get("result")
    if result is None:
        result = {"ok": False, "phases": [], "readiness": None}
    phases = tuple(args.get("phases") or ())
    partial = bool(phases or args.get("skip_start"))

    evidence_output = record_evidence_output(root, recipe, result, partial)
    scope = "targeted" if partial else "full"

    buf = io.StringIO()
    if args.get("json"):
        payload = {"ok": result["ok"], "source": source,
                   "phases": [{"phase": p["phase"], "command": p["command"],
                               "ok": p["ok"], "timed_out": p["timed_out"],
                               "duration": p["duration"]}
                              for p in result.get("phases", [])]}
        if result.get("readiness") is not None:
            payload["readiness"] = {"present": True,
                                    "ready": result["readiness"]["ready"],
                                    "status_code": result["readiness"]["status_code"],
                                    "url": result["readiness"]["url"],
                                    "error": result["readiness"].get("error", ""),
                                    "duration": result["readiness"]["duration"]}
        emit(json.dumps(payload))
    else:
        print_human_report(recipe, source, result, buf)
        for line in buf.getvalue().splitlines():
            emit(line)

    return {"exit_code": 0 if result["ok"] else 1,
            "stdout": out_lines, "stderr": err_lines,
            "record_count": 1, "scope": scope,
            "evidence_output": evidence_output,
            "saved_to": saved_to}


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "merge":
            recipe = json.loads(json.dumps(c["recipe"]))
            merge_project_facts_commands(recipe, c.get("commands", []))
            print(json.dumps({"test": recipe["test"]}, ensure_ascii=False))
        elif kind == "run":
            res = run_verify_command(c)
            # the no-recipe human path prints one two-line message on stderr;
            # split it like the C harness's line capture does
            if len(res["stderr"]) == 1 and "\n" in res["stderr"][0]:
                res["stderr"] = res["stderr"][0].split("\n")
            print(json.dumps(res, ensure_ascii=False))


if __name__ == "__main__":
    main()
