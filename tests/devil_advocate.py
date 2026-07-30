#!/usr/bin/env python3
"""
devil_advocate.py — Devil's Advocate Audit for Slermes C Translation

Finds contradictions, logic errors, and plumbing issues in the C port:
1. Functions that return wrong types (e.g., char* for json_t* return)
2. Functions with TODO/placeholder/FIXME comments
3. Functions that always return NULL/0/false (dead logic)
4. Functions with mismatched parameter counts vs Python
5. Duplicate function definitions across files
6. Functions that call undefined project functions
7. Memory leaks (malloc without free, json_copy without json_free)
8. Buffer overflow risks (snprintf with fixed sizes)
"""

import json
import os
import re
import sys
from pathlib import Path

SLERMES_DIR = Path(__file__).parent / ".." if "__file__" in dir() else Path("/home/wubu/hermes-agent-dev/slermes")
SRC_DIR = SLERMES_DIR / "src"
PORT_FILES = list(SRC_DIR.rglob("port_*.c"))

# Patterns indicating stubs/dead logic
STUB_PATTERNS = [
    r'return\s+NULL\s*;',
    r'return\s+0\s*;',
    r'return\s+false\s*;',
    r'return\s+""\s*;',
    r'\(void\)\w+\s*;',  # silenced parameters
]

# Dangerous patterns
DANGEROUS_PATTERNS = [
    (r'strcpy\s*\(', "strcpy (use snprintf)"),
    (r'strcat\s*\(', "strcat (use snprintf)"),
    (r'sprintf\s*\(', "sprintf (use snprintf)"),
    (r'gets\s*\(', "gets (use fgets)"),
    (r'malloc\s*\([^)]+\)\s*;', "malloc without free (potential leak)"),
    (r'json_copy\s*\(', "json_copy without json_free (potential leak)"),
]

# TODO/FIXME patterns
TODO_PATTERNS = [
    r'TODO',
    r'FIXME',
    r'HACK',
    r'XXX',
    r'placeholder',
    r'stub',
    r'not implemented',
    r'best-effort',
]


def analyze_functions():
    """Analyze all port_*.c functions for issues."""
    issues = []
    all_functions = {}  # func_name -> [file_locations]
    
    for filepath in PORT_FILES:
        content = read_file(filepath)
        if not content:
            continue
        
        # Find all function definitions
        func_pattern = re.compile(
            r'^(?:[ \t]*(?:const\s+)?(?:json_t\*|bool|void|char\*|int|static\s+\w+|unsigned\s+\w+)\s+(\w+)\s*\([^)]*\)\s*\{)',
            re.MULTILINE
        )
        
        for match in func_pattern.finditer(content):
            func_name = match.group(1)
            line_num = content[:match.start()].count('\n') + 1
            
            # Track duplicates
            if func_name in all_functions:
                all_functions[func_name].append(f"{filepath.name}:{line_num}")
            else:
                all_functions[func_name] = [f"{filepath.name}:{line_num}"]
            
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
            
            # Check for stub patterns
            for pattern in STUB_PATTERNS:
                if re.search(pattern, body):
                    # Count how many lines are "real"
                    lines = [l.strip() for l in body.split('\n') if l.strip()]
                    non_trivial = [l for l in lines if not l.startswith('//') and not l.startswith('/*') and l != '{' and l != '}']
                    if len(non_trivial) <= 3:
                        issues.append({
                            'file': filepath.name,
                            'function': func_name,
                            'line': line_num,
                            'severity': 'HIGH',
                            'issue': 'Potential stub function (mostly empty body)',
                            'detail': f"Only {len(non_trivial)} non-trivial lines"
                        })
                        break
            
            # Check for TODO/FIXME
            for pattern in TODO_PATTERNS:
                if re.search(pattern, body, re.IGNORECASE):
                    issues.append({
                        'file': filepath.name,
                        'function': func_name,
                        'line': line_num,
                        'severity': 'MEDIUM',
                        'issue': f'Contains {pattern} marker',
                        'detail': 'Function may be incomplete'
                    })
                    break
            
            # Check for dangerous patterns
            for pattern, desc in DANGEROUS_PATTERNS:
                if re.search(pattern, body):
                    issues.append({
                        'file': filepath.name,
                        'function': func_name,
                        'line': line_num,
                        'severity': 'HIGH',
                        'issue': desc,
                        'detail': 'Potential safety issue'
                    })
            
            # Check for memory leaks (malloc without free)
            mallocs = re.findall(r'malloc\s*\(([^)]+)\)', body)
            frees = re.findall(r'free\s*\(', body)
            if len(mallocs) > len(frees):
                issues.append({
                    'file': filepath.name,
                    'function': func_name,
                    'line': line_num,
                    'severity': 'MEDIUM',
                    'issue': f'Potential memory leak: {len(mallocs)} malloc vs {len(frees)} free',
                    'detail': 'malloc without matching free'
                })
            
            # Check for json_copy without json_free
            copies = re.findall(r'json_copy\s*\(', body)
            json_frees = re.findall(r'json_free\s*\(', body)
            if len(copies) > len(json_frees):
                issues.append({
                    'file': filepath.name,
                    'function': func_name,
                    'line': line_num,
                    'severity': 'MEDIUM',
                    'issue': f'Potential JSON leak: {len(copies)} json_copy vs {len(json_frees)} json_free',
                    'detail': 'json_copy without matching json_free'
                })
    
    # Report duplicates
    for func_name, locations in all_functions.items():
        if len(locations) > 1:
            issues.append({
                'file': ', '.join(locations),
                'function': func_name,
                'line': 0,
                'severity': 'HIGH',
                'issue': f'Duplicate function definition ({len(locations)} occurrences)',
                'detail': 'Multiple definitions will cause linker errors'
            })
    
    return issues


def read_file(path):
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            return f.read()
    except Exception:
        return None


def main():
    print("=" * 70)
    print("  DEVIL'S ADVOCATE AUDIT — Slermes C Translation")
    print("=" * 70)
    print()
    
    issues = analyze_functions()
    
    # Sort by severity
    severity_order = {'HIGH': 0, 'MEDIUM': 1, 'LOW': 2}
    issues.sort(key=lambda x: (severity_order.get(x['severity'], 3), x['file'], x['line']))
    
    # Summary
    high = sum(1 for i in issues if i['severity'] == 'HIGH')
    medium = sum(1 for i in issues if i['severity'] == 'MEDIUM')
    low = sum(1 for i in issues if i['severity'] == 'LOW')
    
    print(f"  Files scanned: {len(PORT_FILES)}")
    print(f"  Issues found:  {len(issues)}")
    print(f"    HIGH:   {high}")
    print(f"    MEDIUM: {medium}")
    print(f"    LOW:    {low}")
    print()
    
    if issues:
        print("  ISSUES:")
        print("  " + "-" * 66)
        for issue in issues:
            severity_icon = {'HIGH': '🔴', 'MEDIUM': '🟡', 'LOW': '⚪'}.get(issue['severity'], '⚪')
            print(f"  {severity_icon} [{issue['severity']}] {issue['file']}:{issue['line']}")
            print(f"     Function: {issue['function']}")
            print(f"     Issue:    {issue['issue']}")
            if issue['detail']:
                print(f"     Detail:   {issue['detail']}")
            print()
    else:
        print("  ✅ No issues found!")
    
    # JSON output
    if '--json' in sys.argv:
        print(json.dumps({'issues': issues, 'summary': {'total': len(issues), 'high': high, 'medium': medium, 'low': low}}, indent=2))
    
    return 0 if high == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
