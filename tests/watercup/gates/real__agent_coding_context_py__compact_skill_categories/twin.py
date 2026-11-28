"""twin: RuntimeMode.compact_skill_categories, LIVE.

A RuntimeMode is constructed directly from the two inputs under test (profile + config_mode),
so the property runs for real without a filesystem probe.
"""
from pathlib import Path
import agent.coding_context as _cc
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "compact_skill_categories"
    C_FN = "coding_context_compact_categories"
    def call(self, inp, inputs, idx=0):
        prof, _, mode = inp.partition("|")
        profile = _cc.CODING_PROFILE if prof == "coding" else _cc.GENERAL_PROFILE
        m = _cc.RuntimeMode(profile=profile, surface="cli", cwd=Path("."),
                            config_mode=mode, model=None, instructions="")
        return sorted(m.compact_skill_categories())   # it is a METHOD here, not a property
    def norm(self, c, result):
        c.valstr(",".join(result))
_t=_Twin(); trace=_t.trace; run=_t.run
