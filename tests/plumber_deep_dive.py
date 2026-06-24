#!/usr/bin/env python3
"""
plumber_deep_dive.py — Full plumbing audit: Python → C function mapping

For every Python function, traces:
1. Does a C function with the exact name exist?
2. Does the C function signature match the Python signature?
3. Does the C function call the right project functions?
4. Are there call graph inconsistencies?
5. Does the C function actually implement the Python logic (not just stub)?

Outputs a detailed report of all plumbing issues.
"""

import ast
import json
import os
import re
import sys
from pathlib import Path
from collections import defaultdict

# Config
HERMES_DEV = Path("/home/wubu/hermes-agent-dev")
SLERMES = HERMES_DEV / "slermes"
PYTHON_DIRS = [
    HERMES_DEV / "hermes_cli",
    HERMES_DEV / "agent",
    HERMES_DEV / "gateway" / "platforms",
]
PORT_DIR = SLERMES / "src"

# Python type → C type mapping
PY_TO_C_TYPE = {
    'Dict': 'json_t*',
    'Dict[str, Any]': 'json_t*',
    'List': 'json_t*',
    'List[str]': 'json_t*',
    'Optional[Dict]': 'json_t*',
    'Optional[str]': 'const char*',
    'Optional[Path]': 'const char*',
    'Optional[int]': 'int',
    'Optional[float]': 'double',
    'Optional[bool]': 'bool',
    'str': 'const char*',
    'int': 'int',
    'float': 'double',
    'bool': 'bool',
    'Path': 'const char*',
    'None': 'void',
    'Any': 'json_t*',
    'bytes': 'const char*',
    'Tuple': 'json_t*',
    'list': 'json_t*',
    'dict': 'json_t*',
    'List[Dict]': 'json_t*',
}


def extract_python_functions(filepath):
    """Extract all function definitions from a Python file."""
    functions = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            source = f.read()
        tree = ast.parse(source)
    except Exception:
        return functions

    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            # Get return type annotation
            return_type = None
            if node.returns:
                try:
                    return_type = ast.unparse(node.returns)
                except Exception:
                    return_type = None

            # Get parameter types
            params = []
            for arg in node.args.args:
                if arg.arg == 'self':
                    continue
                param_type = None
                if arg.annotation:
                    try:
                        param_type = ast.unparse(arg.annotation)
                    except Exception:
                        param_type = None
                params.append({
                    'name': arg.arg,
                    'type': param_type,
                    'has_default': False,  # simplified
                })

            # Check for decorators
            decorators = []
            for d in node.decorator_list:
                try:
                    decorators.append(ast.unparse(d))
                except Exception:
                    pass

            functions.append({
                'name': node.name,
                'return_type': return_type,
                'params': params,
                'is_async': isinstance(node, ast.AsyncFunctionDef),
                'decorators': decorators,
                'line': node.lineno,
                'body_lines': node.end_lineno - node.lineno + 1 if node.end_lineno else 0,
                'is_method': node.args.args and node.args.args[0].arg == 'self',
            })

    return functions


def extract_c_functions(filepath):
    """Extract all function definitions from a C file."""
    functions = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception:
        return functions

    # Match function definitions: return_type func_name(params) {
    # This is a simplified parser
    pattern = re.compile(
        r'^(?:/\*\s*Port of Python[^*]+\*/\s*\n)?'
        r'[ \t]*(?:const\s+)?'
        r'(json_t\*|bool|void|char\*|int|long|unsigned\s+\w+|static\s+\w+|size_t|double)\s+'
        r'(\w+)\s*'
        r'\(([^)]*)\)\s*\{',
        re.MULTILINE
    )

    for match in pattern.finditer(content):
        return_type = match.group(1)
        func_name = match.group(2)
        params_str = match.group(3).strip()
        line_num = content[:match.start()].count('\n') + 1

        # Parse parameters
        params = []
        if params_str and params_str != 'void':
            for param in params_str.split(','):
                param = param.strip()
                if param:
                    # Simple param parsing
                    parts = param.split()
                    if len(parts) >= 2:
                        param_name = parts[-1].lstrip('*')
                        param_type = ' '.join(parts[:-1])
                        params.append({'name': param_name, 'type': param_type})

        # Check for PoP annotation
        preceding = content[max(0, match.start()-200):match.start()]
        has_pop = '/* Port of Python:' in preceding
        pop_match = re.search(r'/\*\s*Port of Python:\s*(\S+)\s*\*/', preceding)
        pop_python_name = pop_match.group(1) if pop_match else None

        # Extract function body
        body_start = match.end()
        brace_count = 1
        pos = body_start
        while brace_count > 0 and pos < len(content):
            if content[pos] == '{':
                brace_count += 1
            elif content[pos] == '}':
                brace_count -= 1
            pos += 1
        body = content[body_start:pos]

        # Analyze body
        body_lines = [l.strip() for l in body.split('\n') if l.strip() and not l.startswith('//') and not l.startswith('/*')]
        has_return = bool(re.search(r'\breturn\b', body))
        has_malloc = 'malloc' in body
        has_free = 'free(' in body
        has_json_copy = 'json_copy' in body
        has_json_free = 'json_free' in body
        has_hermes_log = 'hermes_log' in body
        has_todo = bool(re.search(r'TODO|FIXME|HACK|XXX', body, re.IGNORECASE))

        # Count project function calls (not stdlib)
        stdlib = {'malloc', 'free', 'memcpy', 'memset', 'strlen', 'strcpy', 'strncpy',
                  'strcmp', 'strncmp', 'strstr', 'strchr', 'strtok', 'strdup',
                  'snprintf', 'printf', 'fprintf', 'sprintf', 'fopen', 'fclose',
                  'fread', 'fwrite', 'fgets', 'fputs', 'fseek', 'ftell',
                  'exit', 'abort', 'atoi', 'atof', 'strtol', 'strtod',
                  'time', 'localtime', 'gmtime', 'mktime',
                  'getenv', 'setenv', 'access', 'stat', 'unlink', 'rename',
                  'system', 'popen', 'pclose', 'fork', 'exec', 'wait', 'waitpid',
                  'getpid', 'getppid', 'kill', 'signal', 'pipe', 'dup2',
                  'malloc', 'calloc', 'realloc', 'free',
                  'json_parse', 'json_serialize', 'json_free', 'json_copy',
                  'json_object', 'json_array', 'json_string', 'json_number',
                  'json_bool', 'json_null', 'json_get', 'json_set',
                  'json_obj_get', 'json_append', 'json_len',
                  'json_node_get_string', 'json_node_get_bool', 'json_node_get_int',
                  'json_node_get_double', 'json_object_set', 'json_object_get',
                  'json_array_append', 'json_array_get', 'json_array_count',
                  'json_new_string', 'json_new_number', 'json_new_bool',
                  'json_new_object', 'json_new_array', 'json_new_null',
                  'hermes_log', 'touch_json'}

        project_calls = set()
        call_pattern = re.compile(r'\b([a-z_][a-z0-9_]*)\s*\(')
        for call_match in call_pattern.finditer(body):
            call_name = call_match.group(1)
            if call_name not in stdlib and call_name != func_name:
                project_calls.add(call_name)

        functions.append({
            'name': func_name,
            'return_type': return_type,
            'params': params,
            'line': line_num,
            'has_pop': has_pop,
            'pop_python_name': pop_python_name,
            'body_lines': len(body_lines),
            'has_return': has_return,
            'has_malloc': has_malloc,
            'has_free': has_free,
            'has_json_copy': has_json_copy,
            'has_json_free': has_json_free,
            'has_hermes_log': has_hermes_log,
            'has_todo': has_todo,
            'project_calls': sorted(project_calls),
            'file': str(filepath.relative_to(SLERMES)),
        })

    return functions


def check_signature_match(py_func, c_func):
    """Check if C function signature matches Python function."""
    issues = []

    # Check return type
    if py_func['return_type']:
        expected_c_type = PY_TO_C_TYPE.get(py_func['return_type'])
        if expected_c_type and c_func['return_type'] not in expected_c_type:
            # Allow some flexibility
            if py_func['return_type'] in ('Dict', 'Dict[str, Any]', 'List', 'List[Dict]'):
                if c_func['return_type'] not in ('json_t*', 'char*'):
                    issues.append(f"Return type mismatch: Python '{py_func['return_type']}' expects json_t* or char*, got '{c_func['return_type']}'")
            elif py_func['return_type'] in ('str', 'Optional[str]'):
                if c_func['return_type'] not in ('const char*', 'json_t*', 'char*'):
                    issues.append(f"Return type mismatch: Python '{py_func['return_type']}' expects const char*, got '{c_func['return_type']}'")
            elif py_func['return_type'] == 'bool':
                if c_func['return_type'] != 'bool':
                    issues.append(f"Return type mismatch: Python 'bool' expects bool, got '{c_func['return_type']}'")
            elif py_func['return_type'] == 'None':
                if c_func['return_type'] != 'void':
                    issues.append(f"Return type mismatch: Python 'None' expects void, got '{c_func['return_type']}'")

    # Check parameter count
    py_params = [p for p in py_func['params'] if p['name'] not in ('self', 'cls')]
    c_params = c_func['params']

    # Allow some flexibility (ctx parameter, etc.)
    if len(c_params) < len(py_params):
        issues.append(f"Parameter count mismatch: Python has {len(py_params)} params, C has {len(c_params)}")
    elif len(c_params) > len(py_params) + 2:
        issues.append(f"Parameter count mismatch: Python has {len(py_params)} params, C has {len(c_params)} (suspicious)")

    return issues


def main():
    print("=" * 70)
    print("  PLUMBER DEEP DIVE — Python → C Plumbing Audit")
    print("=" * 70)
    print()

    # Step 1: Extract all Python functions
    print("[1/5] Scanning Python source files...")
    py_functions = {}  # func_name -> [(file, func_info)]
    py_module_funcs = {}  # module -> [func_info]

    for py_dir in PYTHON_DIRS:
        if not py_dir.exists():
            continue
        for py_file in py_dir.glob("*.py"):
            module = py_file.stem
            funcs = extract_python_functions(py_file)
            py_module_funcs[module] = funcs
            for func in funcs:
                name = func['name']
                if name not in py_functions:
                    py_functions[name] = []
                py_functions[name].append((module, func))

    total_py = sum(len(f) for f in py_module_funcs.values())
    print(f"  Found {total_py} Python functions across {len(py_module_funcs)} modules")

    # Step 2: Extract all C functions
    print("[2/5] Scanning C port files...")
    c_functions = {}  # func_name -> [func_info]
    c_file_funcs = {}  # file -> [func_info]

    for c_file in PORT_DIR.rglob("port_*.c"):
        funcs = extract_c_functions(c_file)
        rel_path = str(c_file.relative_to(SLERMES))
        c_file_funcs[rel_path] = funcs
        for func in funcs:
            name = func['name']
            if name not in c_functions:
                c_functions[name] = []
            c_functions[name].append(func)

    total_c = sum(len(f) for f in c_file_funcs.values())
    print(f"  Found {total_c} C functions across {len(c_file_funcs)} files")

    # Step 3: Cross-reference
    print("[3/5] Cross-referencing Python → C...")
    issues = []
    matched = 0
    unmatched_py = []
    signature_mismatches = 0
    stub_c_functions = 0
    missing_returns = 0
    memory_leaks = 0
    todo_functions = 0

    for module, funcs in py_module_funcs.items():
        for py_func in funcs:
            func_name = py_func['name']
            if func_name in c_functions:
                matched += 1
                for c_func in c_functions[func_name]:
                    # Check signature
                    sig_issues = check_signature_match(py_func, c_func)
                    if sig_issues:
                        signature_mismatches += 1
                        for issue in sig_issues:
                            issues.append({
                                'severity': 'HIGH',
                                'type': 'signature_mismatch',
                                'module': module,
                                'function': func_name,
                                'c_file': c_func['file'],
                                'detail': issue,
                            })

                    # Check for stubs
                    if c_func['body_lines'] <= 3 and not c_func['project_calls']:
                        stub_c_functions += 1
                        issues.append({
                            'severity': 'MEDIUM',
                            'type': 'likely_stub',
                            'module': module,
                            'function': func_name,
                            'c_file': c_func['file'],
                            'detail': f"Only {c_func['body_lines']} body lines, no project calls",
                        })

                    # Check for missing return
                    if not c_func['has_return'] and c_func['return_type'] != 'void':
                        missing_returns += 1
                        issues.append({
                            'severity': 'HIGH',
                            'type': 'missing_return',
                            'module': module,
                            'function': func_name,
                            'c_file': c_func['file'],
                            'detail': f"Function has return type {c_func['return_type']} but no return statement",
                        })

                    # Check for memory leaks
                    if c_func['has_malloc'] and not c_func['has_free']:
                        memory_leaks += 1
                        issues.append({
                            'severity': 'MEDIUM',
                            'type': 'memory_leak',
                            'module': module,
                            'function': func_name,
                            'c_file': c_func['file'],
                            'detail': "malloc without free",
                        })

                    if c_func['has_json_copy'] and not c_func['has_json_free']:
                        memory_leaks += 1
                        issues.append({
                            'severity': 'MEDIUM',
                            'type': 'json_leak',
                            'module': module,
                            'function': func_name,
                            'c_file': c_func['file'],
                            'detail': "json_copy without json_free",
                        })

                    # Check for TODOs
                    if c_func['has_todo']:
                        todo_functions += 1
                        issues.append({
                            'severity': 'LOW',
                            'type': 'has_todo',
                            'module': module,
                            'function': func_name,
                            'c_file': c_func['file'],
                            'detail': "Function contains TODO/FIXME/HACK marker",
                        })
            else:
                # Check if a PoP annotation maps to a different C name
                found_via_pop = False
                for c_name, c_funcs in c_functions.items():
                    for c_func in c_funcs:
                        if c_func.get('pop_python_name') == func_name:
                            found_via_pop = True
                            matched += 1
                            break
                    if found_via_pop:
                        break

                if not found_via_pop:
                    unmatched_py.append((module, func_name))

    print(f"  Matched: {matched}")
    print(f"  Unmatched Python functions: {len(unmatched_py)}")

    # Step 4: Check for duplicate C functions
    print("[4/5] Checking for duplicate C function definitions...")
    duplicates = []
    for func_name, c_funcs in c_functions.items():
        if len(c_funcs) > 1:
            duplicates.append((func_name, c_funcs))
            issues.append({
                'severity': 'HIGH',
                'type': 'duplicate_c_function',
                'module': '-',
                'function': func_name,
                'c_file': ', '.join(c['file'] for c in c_funcs),
                'detail': f"Defined {len(c_funcs)} times",
            })

    print(f"  Duplicate C functions: {len(duplicates)}")

    # Step 5: Summary
    print("[5/5] Generating report...")
    print()

    high = sum(1 for i in issues if i['severity'] == 'HIGH')
    medium = sum(1 for i in issues if i['severity'] == 'MEDIUM')
    low = sum(1 for i in issues if i['severity'] == 'LOW')

    print("=" * 70)
    print("  PLUMBING AUDIT SUMMARY")
    print("=" * 70)
    print(f"  Python functions:     {total_py}")
    print(f"  C functions:          {total_c}")
    print(f"  Matched:              {matched}")
    print(f"  Unmatched (Python):   {len(unmatched_py)}")
    print(f"  Issues:               {len(issues)}")
    print(f"    HIGH:   {high}")
    print(f"    MEDIUM: {medium}")
    print(f"    LOW:    {low}")
    print()
    print(f"  Signature mismatches: {signature_mismatches}")
    print(f"  Likely stubs:         {stub_c_functions}")
    print(f"  Missing returns:      {missing_returns}")
    print(f"  Memory/JSON leaks:    {memory_leaks}")
    print(f"  TODO functions:       {todo_functions}")
    print(f"  Duplicate C funcs:    {len(duplicates)}")
    print()

    # Show top issues
    if issues:
        print("  TOP ISSUES (HIGH severity):")
        print("  " + "-" * 66)
        high_issues = [i for i in issues if i['severity'] == 'HIGH']
        for issue in high_issues[:30]:
            print(f"  🔴 [{issue['type']}] {issue['module']}::{issue['function']}")
            print(f"     File: {issue['c_file']}")
            print(f"     {issue['detail']}")
            print()

    # Show unmatched functions (first 30)
    if unmatched_py:
        print(f"  UNMATCHED PYTHON FUNCTIONS (first 30 of {len(unmatched_py)}):")
        print("  " + "-" * 66)
        for module, func_name in unmatched_py[:30]:
            print(f"  ⚪ {module}::{func_name}")
        print()

    # JSON output
    if '--json' in sys.argv:
        output = {
            'summary': {
                'python_functions': total_py,
                'c_functions': total_c,
                'matched': matched,
                'unmatched': len(unmatched_py),
                'issues': len(issues),
                'high': high,
                'medium': medium,
                'low': low,
            },
            'issues': issues,
            'unmatched': [{'module': m, 'function': f} for m, f in unmatched_py],
            'duplicates': [{'function': name, 'files': [c['file'] for c in funcs]} for name, funcs in duplicates],
        }
        print(json.dumps(output, indent=2))

    return 0 if high == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
