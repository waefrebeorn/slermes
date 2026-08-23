"""twin for skills_hub_search_score.

Traces the LIVE tools/skills_hub.py::ClawHubSource._search_score with the
same rung order as the C harness (which is a traced copy of the FIXED
sh_search_score port). All matching helpers (_query_terms, the tier
comparisons) are called on the LIVE module so semantics are LIVE; structure
is the shared ladder.

Input encoding: "query|identifier|name|description".
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_skills_hub", os.path.join(DEV, "tools", "skills_hub.py"))
_sh = ilu.module_from_spec(_spec)
sys.modules["wc_live_skills_hub"] = _sh
_spec.loader.exec_module(_sh)

_ScoreCls = _sh.ClawHubSource
_qterms = _ScoreCls._query_terms


def trace(query, identifier, name, description):
    c = Cup("search_score")
    query_norm = query.strip().lower()
    if not query_norm:                                   # B1
        c.branch(True)
        score = 1
    else:
        c.branch(False)
        identifier = (identifier or "").lower()
        name = (name or "").lower()
        description = (description or "").lower()
        normalized_identifier = " ".join(_qterms(identifier))
        normalized_name = " ".join(_qterms(name))
        query_terms = _qterms(query_norm)
        identifier_terms = _qterms(identifier)
        name_terms = _qterms(name)
        score = 0

        def br(cond, pts):
            if cond:
                c.branch(True)
                return pts
            c.branch(False)
            return 0

        score += br(query_norm == identifier, 140)
        score += br(query_norm == name, 130)
        score += br(normalized_identifier == query_norm, 125)
        score += br(normalized_name == query_norm, 120)
        score += br(normalized_identifier.startswith(query_norm), 95)
        score += br(normalized_name.startswith(query_norm), 90)
        score += br(bool(query_terms) and
                    identifier_terms[:len(query_terms)] == query_terms, 70)
        score += br(bool(query_terms) and
                    name_terms[:len(query_terms)] == query_terms, 65)
        score += br(query_norm in identifier, 40)
        score += br(query_norm in name, 35)
        score += br(query_norm in description, 10)

        for term in query_terms:                         # LOOP
            c.loop()
            score += br(term in identifier_terms, 15)
            score += br(term in name_terms, 12)
            score += br(term in description, 3)
    c.valstr(str(score))
    return score, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        parts = raw.split("|")
        parts += [""] * (4 - len(parts))
        _, cup = trace(parts[0], parts[1], parts[2], parts[3])
        cups.append(cup)
    return cups
