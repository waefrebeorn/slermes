"""twin for skill_linter_check_metadata_block.

Imports LIVE tools/skill_linter.py::_check_metadata_block and traces the SAME
branch ladder as the C harness. Input encoding (one token per input):
  "<version|->|<author|->|<license|->|<0|1 metadata>|<0|1 metadata.hermes>|<tags|->"
Finding encoding: ";".join of
  missing-metadata:<key> | missing-metadata:hermes | missing-metadata:tags
  | author-caps
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_mod_cache = {}


def _live_fn():
    if "fn" not in _mod_cache:
        import importlib.util as ilu
        path = os.path.join(DEV, "tools", "skill_linter.py")
        spec = ilu.spec_from_file_location("wc_skill_linter_live", path)
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_skill_linter_live"] = mod
        spec.loader.exec_module(mod)
        _mod_cache["fn"] = mod._check_metadata_block
    return _mod_cache["fn"]


_SUSPECT = ("hermes", "agent", "hermes agent")


def run(inputs):
    fn = _live_fn()
    cups = []
    for raw in inputs:
        parts = raw.split("|")
        parts += [""] * (6 - len(parts))
        ver, author, lic, has_meta, has_hermes, tags = parts[:6]
        c = Cup("check_metadata_block")
        findings = []
        frontmatter = {}
        if ver != "-":
            frontmatter["version"] = ver
        if author != "-":
            frontmatter["author"] = author
        if lic != "-":
            frontmatter["license"] = lic
        if has_meta == "1":
            hermes = {}
            if has_hermes == "1" and tags != "-":
                hermes["tags"] = tags
            elif has_hermes == "1":
                hermes = {}  # metadata.hermes present, tags absent
            frontmatter["metadata"] = {"hermes": hermes} if has_hermes == "1" else {"other": 1}
        for key in ("version", "author", "license"):
            c.loop()
            missing = key not in frontmatter
            c.branch(missing)
            if missing:
                findings.append("missing-metadata:" + key)
        meta = frontmatter.get("metadata")
        hermes_meta = meta.get("hermes") if isinstance(meta, dict) else None
        c.branch(not isinstance(hermes_meta, dict))
        if not isinstance(hermes_meta, dict):
            findings.append("missing-metadata:hermes")
        else:
            no_tags = "tags" not in hermes_meta
            c.branch(no_tags)
            if no_tags:
                findings.append("missing-metadata:tags")
        a = str(frontmatter.get("author", ""))
        suspicious = bool(a) and a.strip().lower() in _SUSPECT
        c.branch(suspicious)
        if suspicious:
            proper = a == "Hermes Agent"
            c.branch(not proper)
            if not proper:
                findings.append("author-caps")
        # cross-check against the LIVE fn (rule+severity identity, in order)
        live = fn(frontmatter)
        live_enc = []
        for f in live:
            if f.rule == "author-caps":
                live_enc.append("author-caps")
            elif "metadata.hermes.tags" in f.message:
                live_enc.append("missing-metadata:tags")
            elif "metadata.hermes" in f.message:
                live_enc.append("missing-metadata:hermes")
            else:
                # "...missing '<key>'; every peer skill has it."
                k = f.message.split("missing '")[1].split("'")[0]
                live_enc.append("missing-metadata:" + k)
        assert live_enc == findings, "LIVE diverged on %r: %r vs %r" % (raw, live_enc, findings)
        out = ";".join(findings)
        c.valstr(out)
        cups.append(c.finalize())
    return cups
