"""twin: focus_toolsets, LIVE (portable-plugin union stubbed: it needs the plugin registry)."""
import json as _j
import agent.coding_context as _cc
import hermes_cli.tools_config as _tc
from watercup_twin_runtime import TwinBase
_tc._projected_plugin_toolsets = lambda: None
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "focus_toolsets"
    C_FN = "coding_context_focus_toolsets"
    NORM_NONE = True
    def call(self, inp, inputs, idx=0):
        return _cc.focus_toolsets(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("None" if result is None else ",".join(result))
_t=_Twin(); trace=_t.trace; run=_t.run
