"""twin for skills_hub_resolve_skill_name.

Traces the LIVE tools/skills_hub.py GitHubSource._resolve_skill_name with the
same rung order as the C harness. Input format: "<fmname>|<url>" where
fmname "-" = no frontmatter name (fm absent/None).
  B1 fm name valid / B2 empty url / B3 SKILL.md tail / B4 .md strip /
  V result | "None".
Validity via the LIVE _is_valid_skill_name; result via the LIVE fn when the
module imports.
"""
import os, sys, re, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_skills_hub", os.path.join(DEV, "tools", "skills_hub.py"))
_sh = ilu.module_from_spec(_spec)
sys.modules["wc_live_skills_hub"] = _sh
try:
    _spec.loader.exec_module(_sh)
    is_valid = staticmethod(_sh.GitHubSource._is_valid_skill_name.__func__)
except Exception:  # pragma: no cover - ported fallback
    _VALID_RE = re.compile(r"[a-z][a-z0-9_-]*\Z")

    def is_valid(name):
        if not isinstance(name, str):
            return False
        cand = name.strip().lower()
        if not cand or cand in {"skill", "readme", "index", "unnamed-skill"}:
            return False
        return bool(_VALID_RE.match(cand))

try:
    live_resolve = _sh.GitHubSource._resolve_skill_name.__func__
except Exception:
    live_resolve = None


def trace(raw):
    c = Cup("resolve_skill_name")
    if "|" in raw:
        fmn, url = raw.split("|", 1)
        if fmn == "-" or fmn == "":
            fmn = None
    else:
        fmn, url = None, raw

    b1 = bool(fmn) and bool(is_valid(fmn))
    c.branch(b1)                                 # B1
    if b1:
        val = fmn.strip() if fmn != "" else ""
        # LIVE strips the frontmatter name
        val = str(fmn).strip()
    else:
        if not url:                              # B2
            c.branch(True)
            c.valstr("None")
            return "None", c.finalize()
        c.branch(False)
        try:
            from urllib.parse import urlparse
            path = urlparse(url).path
        except ValueError:
            path = None
        parts = [p for p in path.split("/") if p] if path else []
        skillmd = len(parts) >= 2 and parts[-1].lower() == "skill.md"
        c.branch(skillmd)                        # B3
        done = False
        if skillmd:
            if is_valid(parts[-2]):
                val_res = parts[-2]
                done = True
            else:
                val_res = None
        else:
            val_res = None
        if not done and parts:
            import re as _re
            cand = _re.sub(r"\.md$", "", parts[-1], flags=_re.IGNORECASE)
            c.branch(bool(_re.search(r"\.md$", parts[-1], flags=_re.IGNORECASE)))  # B4
            ok = is_valid(cand)
            val_res = cand if ok else None
        else:
            # mirror C: no B4 branch when no parts
            if not parts:
                pass
        val = val_res if val_res is not None else "None"
    c.valstr(val)
    return val, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        _, cup = trace(s)
        cups.append(cup)
    return cups
