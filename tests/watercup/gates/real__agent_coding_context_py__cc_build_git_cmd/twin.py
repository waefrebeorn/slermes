"""twin: agent/coding_context.py drives git through an argv list; the shell
equivalent is shlex.quote(cwd), which is what the command builder must emit."""
import shlex
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_git"
    C_FN = "cc_build_git_cmd"
    def call(self, inp, inputs, idx=0):
        cwd, _, args = inp.partition("|")
        return "git -C %s %s" % (shlex.quote(cwd), args)
    def norm(self, c, result):
        c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
