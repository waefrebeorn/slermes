"""twin: reference command line for the repo-root probe."""
import shlex
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_git_root"
    C_FN = "cc_build_git_root_cmd"
    def call(self, inp, inputs, idx=0):
        return "git -C %s rev-parse --show-toplevel" % shlex.quote(inp)
    def norm(self, c, result):
        c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
