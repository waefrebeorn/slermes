"""twin: _normalize_skill_names(values) -> set[str] (rendered sorted)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skills_config.py"; PY_FN = "_normalize_skill_names"
    C_FN = "hermes_cli_skills_config_u_normalize_skill_names"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(sorted(result), separators=(",", ":")))
_t=_Twin(); trace=_t.trace; run=_t.run
