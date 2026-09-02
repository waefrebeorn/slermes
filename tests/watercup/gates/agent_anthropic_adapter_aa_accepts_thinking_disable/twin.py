"""Twin for agent_anthropic_adapter_aa_accepts_thinking_disable: _accepts_thinking_disable.

Mirrors Python agent/anthropic_adapter.py::_accepts_thinking_disable.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup
from agent.anthropic_adapter import _accepts_thinking_disable

def trace(input):
    c = Cup("aa_accepts_thinking_disable")
    result = _accepts_thinking_disable(input)
    c.valstr("true" if result else "false")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
