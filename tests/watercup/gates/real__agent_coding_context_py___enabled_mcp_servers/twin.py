"""twin: agent/coding_context.py:_enabled_mcp_servers, LIVE.

The portable-plugin union is stubbed to empty: it needs the plugin registry, which is
process state, not config. Everything else -- which entries count and the sort -- runs
in the live function.
"""
import agent.coding_context as _cc
import hermes_cli.tools_config as _tc
from watercup_twin_runtime import TwinBase
_tc._projected_plugin_toolsets = lambda: None
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_enabled_mcp_servers"
    C_FN = "_enabled_mcp_servers"
    def call(self, inp, inputs, idx=0):
        import json as _j
        return _cc._enabled_mcp_servers(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(",".join(result))
_t=_Twin(); trace=_t.trace; run=_t.run
