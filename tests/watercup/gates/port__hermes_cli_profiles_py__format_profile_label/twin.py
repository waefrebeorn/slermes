"""twin: format_profile_label(name, display_name) -> str."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"; PY_FN = "format_profile_label"
    C_FN = "profiles_format_profile_label"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d.get("name",""), d.get("display_name"))
    def norm(self, c, result):
        c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
