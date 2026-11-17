#!/usr/bin/env python3
"""Reference oracle for agent_process_bootstrap.

_install_safe_stdio is a void, idempotent side-effect setup. The observable
contract is that repeated calls complete without raising; emit the same
record the C harness emits instead of validating its output on stdin.
"""
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
from agent.process_bootstrap import _install_safe_stdio

_install_safe_stdio()
_install_safe_stdio()
print('{"func":"install_safe_stdio","ok":true}')
