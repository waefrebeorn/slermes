"""twin: RuntimeMode.toolset_selection, LIVE, on a directly constructed mode."""
from pathlib import Path
import json as _j
import agent.coding_context as _cc
import hermes_cli.tools_config as _tc
from watercup_twin_runtime import TwinBase
_tc._projected_plugin_toolsets = lambda: None
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "toolset_selection"
    C_FN = "coding_context_toolset_selection"
    NORM_NONE = True
    def call(self, inp, inputs, idx=0):
        prof, _, rest = inp.partition("|")
        mode, _, cfg_s = rest.partition("|")
        profile = _cc.CODING_PROFILE if prof == "coding" else _cc.GENERAL_PROFILE
        m = _cc.RuntimeMode(profile=profile, surface="cli", cwd=Path("."),
                            config_mode=mode, model=None, instructions="")
        return m.toolset_selection(_j.loads(cfg_s))
    def norm(self, c, result):
        c.valstr("None" if result is None else ",".join(result))
_t=_Twin(); trace=_t.trace; run=_t.run
