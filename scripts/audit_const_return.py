#!/usr/bin/env python3
"""Audit the 48 single-const-return ports functions: do they have a Python twin at all?"""
import ast
import os
import re
import sys

sys.argv = ['x', '.']
import importlib.util
spec = importlib.util.spec_from_file_location('f', 'scripts/find_const_return_gaps.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

# python func name set
pyfuncs = set()
for d in ('agent', 'tools', 'hermes_cli', 'gateway'):
    for dp, _, fs in os.walk(d):
        for fn in fs:
            if fn.endswith('.py'):
                try:
                    src = open(os.path.join(dp, fn)).read()
                except Exception:
                    continue
                try:
                    tree = ast.parse(src)
                except Exception:
                    continue
                for node in ast.walk(tree):
                    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                        pyfuncs.add(node.name)
print('python def names:', len(pyfuncs))

mk = open('build/objects.mk').read()
built = []
for dp, _, fs in os.walk('src'):
    for f in fs:
        if f.endswith('.c'):
            rel = os.path.join(dp, f)
            if (rel[:-2] + '.o') in mk:
                built.append(rel)
ports = [p for p in built if '/ports/' in p]

n = 0
with_twin = 0
for p in ports:
    try:
        src = open(p).read()
    except Exception:
        continue
    rel = p.split('slermes/')[1] if 'slermes/' in p else p
    for name, body, ln in m.find_function_bodies(src):
        if m.is_constant_return(body):
            has = name in pyfuncs
            if has:
                with_twin += 1
            print(f'{rel}:{ln} {name}()  py_twin={has}')
            n += 1
print(f'\nTOTAL const-return: {n}  with_py_twin: {with_twin}  orphaned: {n - with_twin}')
