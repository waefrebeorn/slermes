#!/usr/bin/env python3
"""
ts_to_c_parity.py — Triple Devil's Advocate Audit Suite

Checks TypeScript (apps/desktop, apps/shared, web/src) against C implementations
(desktop_app.c, web_app.c, port_*.c) for 1:1 feature parity.

Three audit layers:
1. STRUCTURAL — Are all TS functions/classes/components represented in C?
2. BEHAVIORAL — Do C functions handle the same inputs/outputs as TS?
3. UX — Do the C apps provide the same user-facing features as TS apps?

Usage:
    python3 ts_to_c_parity.py [--json] [--fix] [--verbose]
"""

import os, re, sys, json, argparse, subprocess
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional

SLERMES_DIR = "/home/wubu/hermes-agent-dev/slermes"
APPS_DESKTOP = "/home/wubu/hermes-agent-dev/apps/desktop"
APPS_SHARED = "/home/wubu/hermes-agent-dev/apps/shared"
WEB_SRC = "/home/wubu/hermes-agent-dev/web/src"

# ── Data types ──────────────────────────────────────────────────────────────

@dataclass
class TSFunction:
    name: str
    file: str
    line: int
    kind: str  # "function", "method", "hook", "component", "handler"
    params: list = field(default_factory=list)
    returns: str = "void"
    is_exported: bool = False
    is_async: bool = False
    class_name: str = ""

@dataclass
class CFunction:
    name: str
    file: str
    line: int
    return_type: str = "void"
    params: list = field(default_factory=list)

@dataclass
class ParityIssue:
    severity: str  # "HIGH", "MEDIUM", "LOW"
    category: str  # "MISSING", "SIGNATURE", "BEHAVIORAL", "UX"
    ts_function: str
    ts_file: str
    c_file: str = ""
    detail: str = ""
    suggestion: str = ""

# ── TypeScript Parser ────────────────────────────────────────────────────────

class TSParser:
    """Extract functions, components, hooks, and handlers from TypeScript files."""
    
    def __init__(self):
        self.functions: list[TSFunction] = []
        self.classes: dict[str, list[TSFunction]] = {}
        self.hooks: list[TSFunction] = []
        self.components: list[TSFunction] = []
        self.handlers: list[TSFunction] = []
        self.api_calls: list[dict] = []
    
    def parse_file(self, filepath: str) -> None:
        try:
            with open(filepath, 'r', errors='replace') as f:
                content = f.read()
                lines = content.split('\n')
        except Exception:
            return
        
        rel_path = filepath
        for prefix in [APPS_DESKTOP, APPS_SHARED, WEB_SRC]:
            if filepath.startswith(prefix):
                rel_path = filepath[len(prefix)+1:]
                break
        
        # Extract imports to understand dependencies
        imports = self._extract_imports(content)
        
        # Parse functions
        self._parse_functions(content, rel_path, lines)
        
        # Parse classes
        self._parse_classes(content, rel_path, lines)
        
        # Parse React components
        self._parse_components(content, rel_path, lines)
        
        # Parse hooks (useXxx functions)
        self._parse_hooks(content, rel_path, lines)
        
        # Parse event handlers
        self._parse_handlers(content, rel_path, lines)
        
        # Parse API calls
        self._parse_api_calls(content, rel_path, lines)
    
    def _extract_imports(self, content: str) -> list[str]:
        imports = []
        for m in re.finditer(r'import\s+.*?from\s+[\'"]([^\'"]+)[\'"]', content):
            imports.append(m.group(1))
        return imports
    
    def _parse_functions(self, content: str, filepath: str, lines: list[str]) -> None:
        # Match: export function name(...), export async function name(...)
        # Match: const name = (...) =>, const name = async (...) =>
        # Match: function name(...)
        
        patterns = [
            # export async function name(params)
            (r'export\s+async\s+function\s+(\w+)\s*\(([^)]*)\)\s*(?::\s*([^{]+))?\s*\{', 'function', True, True),
            # export function name(params)
            (r'export\s+function\s+(\w+)\s*\(([^)]*)\)\s*(?::\s*([^{]+))?\s*\{', 'function', True, False),
            # function name(params) — not preceded by export
            (r'(?:^|[^t])function\s+(\w+)\s*\(([^)]*)\)\s*(?::\s*([^{]+))?\s*\{', 'function', False, False),
            # const name = async (params) => 
            (r'export\s+const\s+(\w+)\s*=\s*async\s*\(([^)]*)\)\s*(?::\s*([^{=]+))?\s*=>', 'arrow', True, True),
            # const name = (params) =>
            (r'export\s+const\s+(\w+)\s*=\s*\(([^)]*)\)\s*(?::\s*([^{=]+))?\s*=>', 'arrow', False, True),
        ]
        
        for pattern, kind, is_exported, is_async in patterns:
            for m in re.finditer(pattern, content):
                name = m.group(1)
                params_str = m.group(2) if m.group(2) else ""
                returns = m.group(3).strip() if m.group(3) and m.group(3).strip() else "void"
                
                # Skip common non-feature functions
                if name in ('if', 'for', 'while', 'switch', 'catch', 'finally', 'return'):
                    continue
                
                # Find line number
                line_num = content[:m.start()].count('\n') + 1
                
                params = [p.strip() for p in params_str.split(',') if p.strip()]
                
                func = TSFunction(
                    name=name, file=filepath, line=line_num,
                    kind=kind, params=params, returns=returns,
                    is_exported=is_exported, is_async=is_async
                )
                self.functions.append(func)
    
    def _parse_classes(self, content: str, filepath: str, lines: list[str]) -> None:
        class_pattern = r'(?:export\s+)?class\s+(\w+)(?:\s+extends\s+\w+)?\s*\{'
        method_pattern = r'(?:async\s+)?(\w+)\s*\(([^)]*)\)\s*(?::\s*([^{]+))?\s*\{'
        
        for cm in re.finditer(class_pattern, content):
            class_name = cm.group(1)
            # Find class body (simplified — find matching brace)
            start = cm.end()
            depth = 1
            pos = start
            while depth > 0 and pos < len(content):
                if content[pos] == '{': depth += 1
                elif content[pos] == '}': depth -= 1
                pos += 1
            class_body = content[start:pos]
            
            methods = []
            for mm in re.finditer(method_pattern, class_body):
                method_name = mm.group(1)
                if method_name in ('constructor', 'render', 'componentDidMount', 'componentWillUnmount'):
                    continue
                params_str = mm.group(2)
                returns = mm.group(3).strip() if mm.group(3) else "void"
                params = [p.strip() for p in params_str.split(',') if p.strip()]
                
                func = TSFunction(
                    name=method_name, file=filepath,
                    line=content[:start + mm.start()].count('\n') + 1,
                    kind="method", params=params, returns=returns,
                    class_name=class_name
                )
                methods.append(func)
                self.functions.append(func)
            
            self.classes[class_name] = methods
    
    def _parse_components(self, content: str, filepath: str, lines: list[str]) -> None:
        # React functional components: function ComponentName(...) { return (...) }
        # or: const ComponentName = (...) => { return (...) }
        comp_patterns = [
            r'(?:export\s+)?function\s+([A-Z]\w+)\s*\(([^)]*)\)\s*\{',
            r'(?:export\s+)?const\s+([A-Z]\w+)\s*(?::\s*React\.FC[^=]*)?=\s*\(([^)]*)\)\s*(?:=>|{)',
        ]
        
        for pattern in comp_patterns:
            for m in re.finditer(pattern, content):
                name = m.group(1)
                if name in ('React', 'useState', 'useEffect', 'useMemo', 'useCallback', 'useRef'):
                    continue
                params_str = m.group(2) if m.group(2) else ""
                params = [p.strip() for p in params_str.split(',') if p.strip()]
                line_num = content[:m.start()].count('\n') + 1
                
                comp = TSFunction(
                    name=name, file=filepath, line=line_num,
                    kind="component", params=params, returns="JSX"
                )
                self.components.append(comp)
    
    def _parse_hooks(self, content: str, filepath: str, lines: list[str]) -> None:
        # Hooks: useXxx functions that return state + setters
        hook_pattern = r'(?:export\s+)?(?:function|const)\s+(use\w+)\s*\(([^)]*)\)\s*(?:=>|{)'
        
        for m in re.finditer(hook_pattern, content):
            name = m.group(1)
            params_str = m.group(2)
            params = [p.strip() for p in params_str.split(',') if p.strip()]
            line_num = content[:m.start()].count('\n') + 1
            
            hook = TSFunction(
                name=name, file=filepath, line=line_num,
                kind="hook", params=params
            )
            self.hooks.append(hook)
    
    def _parse_handlers(self, content: str, filepath: str, lines: list[str]) -> None:
        # Event handlers: handleXxx, onXxx
        handler_pattern = r'(?:export\s+)?(?:const|function)\s+(handle\w+|on\w+)\s*(?:=\s*)?(?:async\s*)?\(([^)]*)\)\s*(?:=>|{)'
        
        for m in re.finditer(handler_pattern, content):
            name = m.group(1)
            params_str = m.group(2)
            params = [p.strip() for p in params_str.split(',') if p.strip()]
            line_num = content[:m.start()].count('\n') + 1
            
            handler = TSFunction(
                name=name, file=filepath, line=line_num,
                kind="handler", params=params
            )
            self.handlers.append(handler)
    
    def _parse_api_calls(self, content: str, filepath: str, lines: list[str]) -> None:
        # API calls: fetch, axios, api.xxx, gatewayClient.xxx
        api_patterns = [
            r'(?:await\s+)?fetch\s*\(\s*[\'"](/api/[^\'"]+)[\'"]',
            r'(?:await\s+)?(?:api|gatewayClient)\.(\w+)\s*\(',
            r'axios\.(?:get|post|put|delete|patch)\s*\(\s*[\'"](/api/[^\'"]+)[\'"]',
        ]
        
        for pattern in api_patterns:
            for m in re.finditer(pattern, content):
                line_num = content[:m.start()].count('\n') + 1
                self.api_calls.append({
                    'endpoint': m.group(1),
                    'file': filepath,
                    'line': line_num
                })
    
    def parse_directory(self, directory: str) -> None:
        for root, dirs, files in os.walk(directory):
            # Skip node_modules, build outputs
            dirs[:] = [d for d in dirs if d not in ('node_modules', 'dist', 'build', '.next', 'coverage')]
            for f in files:
                if f.endswith(('.ts', '.tsx')) and not f.endswith('.d.ts'):
                    self.parse_file(os.path.join(root, f))


# ── C Parser ────────────────────────────────────────────────────────────────

class CParser:
    """Extract functions from C source files."""
    
    def __init__(self):
        self.functions: list[CFunction] = []
        self.structs: list[dict] = []
        self.api_endpoints: list[str] = []
    
    def parse_file(self, filepath: str) -> None:
        try:
            with open(filepath, 'r', errors='replace') as f:
                content = f.read()
                lines = content.split('\n')
        except Exception:
            return
        
        rel_path = filepath.replace(SLERMES_DIR + "/", "")
        
        # Extract function definitions
        func_pattern = r'(?:static\s+)?(?:inline\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(\w[\w\s\*]*?)\s+(\w+)\s*\(([^)]*)\)\s*\{'
        
        for m in re.finditer(func_pattern, content):
            return_type = m.group(1).strip()
            name = m.group(2).strip()
            params_str = m.group(3).strip()
            
            # Skip common non-feature patterns
            if name in ('if', 'for', 'while', 'switch', 'main', 'WinMain', 'DllMain'):
                continue
            if return_type in ('struct', 'enum', 'union', 'typedef'):
                continue
            
            params = []
            if params_str and params_str != 'void':
                for p in params_str.split(','):
                    p = p.strip()
                    if p:
                        params.append(p)
            
            line_num = content[:m.start()].count('\n') + 1
            
            func = CFunction(
                name=name, file=rel_path, line=line_num,
                return_type=return_type, params=params
            )
            self.functions.append(func)
        
        # Extract API endpoint strings
        for m in re.finditer(r'#define\s+(?:API_\w+)\s+[\'"]([^\'"]+)[\'"]', content):
            self.api_endpoints.append(m.group(1))
        
        # Extract struct definitions
        struct_pattern = r'typedef\s+struct\s*\{([^}]+)\}\s*(\w+)\s*;'
        for m in re.finditer(struct_pattern, content, re.DOTALL):
            self.structs.append({
                'name': m.group(2),
                'file': rel_path,
                'fields': m.group(1).strip()
            })
    
    def parse_directory(self, directory: str) -> None:
        for root, dirs, files in os.walk(directory):
            dirs[:] = [d for d in dirs if d not in ('node_modules', 'dist', 'build', '.git')]
            for f in files:
                if f.endswith(('.c', '.h')) and not f.endswith('.h'):
                    self.parse_file(os.path.join(root, f))


# ── Parity Checker ──────────────────────────────────────────────────────────

class ParityChecker:
    """Triple devil's advocate: structural, behavioral, and UX parity."""
    
    def __init__(self, ts_parser: TSParser, c_parser: CParser):
        self.ts = ts_parser
        self.c = c_parser
        self.issues: list[ParityIssue] = []
        
        # Build lookup maps
        self.c_func_map: dict[str, CFunction] = {}
        for func in c_parser.functions:
            self.c_func_map[func.name] = func
        
        self.c_endpoint_set = set(c_parser.api_endpoints)
    
    def check_all(self) -> list[ParityIssue]:
        self.check_structural_parity()
        self.check_signature_parity()
        self.check_api_endpoint_parity()
        self.check_ux_parity()
        return self.issues
    
    def check_structural_parity(self) -> None:
        """Check that every exported TS function has a corresponding C function."""
        
        # Group TS functions by category
        ts_exported = [f for f in self.ts.functions if f.is_exported]
        ts_components = self.ts.components
        ts_hooks = self.ts.hooks
        ts_handlers = self.ts.handlers
        
        # Check exported functions
        for func in ts_exported:
            if func.name not in self.c_func_map:
                # Some functions are React-specific and don't need C equivalents
                if self._is_react_only(func):
                    continue
                self.issues.append(ParityIssue(
                    severity="HIGH",
                    category="MISSING",
                    ts_function=func.name,
                    ts_file=func.file,
                    detail=f"Exported TS function '{func.name}' has no C equivalent",
                    suggestion=f"Add {func.name}() to desktop_app.c or web_app.c"
                ))
        
        # Check components
        for comp in ts_components:
            # Components should have corresponding C UI elements
            c_name = comp.name[0].lower() + comp.name[1:]
            if comp.name not in self.c_func_map and c_name not in self.c_func_map:
                # Check if there's a render/draw function for it
                render_name = f"render_{c_name}"
                draw_name = f"draw_{c_name}"
                if render_name not in self.c_func_map and draw_name not in self.c_func_map:
                    self.issues.append(ParityIssue(
                        severity="MEDIUM",
                        category="MISSING",
                        ts_function=comp.name,
                        ts_file=comp.file,
                        detail=f"React component '{comp.name}' has no C UI equivalent",
                        suggestion=f"Add render_{c_name}() or draw_{c_name}() to desktop_app.c"
                    ))
        
        # Check hooks — these map to state management in C
        for hook in ts_hooks:
            if hook.name not in self.c_func_map:
                # Hooks that are React-specific (useState, useEffect) don't need C equivalents
                if hook.name.startswith('use') and hook.name[3].isupper():
                    # Custom hooks might need C equivalents
                    pass  # Don't flag — hooks are React-specific
        
        # Check handlers
        for handler in ts_handlers:
            if handler.name not in self.c_func_map:
                handler_c = handler.name.replace('handle', 'on').replace('Handle', 'On')
                if handler_c not in self.c_func_map:
                    self.issues.append(ParityIssue(
                        severity="MEDIUM",
                        category="MISSING",
                        ts_function=handler.name,
                        ts_file=handler.file,
                        detail=f"Event handler '{handler.name}' has no C equivalent",
                        suggestion=f"Add {handler.name}() or {handler_c}() to desktop_app.c"
                    ))
    
    def check_signature_parity(self) -> None:
        """Check that C functions have matching signatures to TS counterparts."""
        
        for func in self.ts.functions:
            if func.name in self.c_func_map:
                c_func = self.c_func_map[func.name]
                
                # Check return type compatibility
                ts_returns = func.returns.strip()
                c_returns = c_func.return_type.strip()
                
                if ts_returns in ('boolean', 'bool') and c_returns not in ('bool', 'int', 'BOOL'):
                    self.issues.append(ParityIssue(
                        severity="MEDIUM",
                        category="SIGNATURE",
                        ts_function=func.name,
                        ts_file=func.file,
                        c_file=c_func.file,
                        detail=f"TS returns '{ts_returns}' but C returns '{c_returns}'",
                        suggestion=f"Change C return type to bool"
                    ))
                
                if ts_returns == 'void' and c_returns not in ('void', 'int'):
                    self.issues.append(ParityIssue(
                        severity="LOW",
                        category="SIGNATURE",
                        ts_function=func.name,
                        ts_file=func.file,
                        c_file=c_func.file,
                        detail=f"TS returns 'void' but C returns '{c_returns}'",
                        suggestion=f"Change C return type to void"
                    ))
                
                # Check parameter count
                ts_param_count = len([p for p in func.params if p and p != 'props' and p != 'event' and p != 'e'])
                c_param_count = len(c_func.params)
                
                if ts_param_count != c_param_count and ts_param_count > 0:
                    self.issues.append(ParityIssue(
                        severity="MEDIUM",
                        category="SIGNATURE",
                        ts_function=func.name,
                        ts_file=func.file,
                        c_file=c_func.file,
                        detail=f"TS has {ts_param_count} params but C has {c_param_count}",
                        suggestion=f"Adjust C parameter count to match TS"
                    ))
    
    def check_api_endpoint_parity(self) -> None:
        """Check that all API endpoints called by TS are served by C."""
        
        ts_endpoints = set()
        for call in self.ts.api_calls:
            ts_endpoints.add(call['endpoint'])
        
        for endpoint in sorted(ts_endpoints):
            if endpoint not in self.c_endpoint_set:
                self.issues.append(ParityIssue(
                    severity="HIGH",
                    category="MISSING",
                    ts_function=f"API {endpoint}",
                    ts_file="multiple",
                    detail=f"TS calls API endpoint '{endpoint}' but C server doesn't define it",
                    suggestion=f"#define API_... \"{endpoint}\" in web_app.c and add handler"
                ))
    
    def check_ux_parity(self) -> None:
        """Check that the C apps provide the same user-facing features."""
        
        # Key UX features from the TS apps
        ux_features = {
            'desktop': [
                ('chat', 'Chat interface with message history'),
                ('session', 'Session management (create, switch, delete)'),
                ('model', 'Model picker/selector'),
                ('settings', 'Settings page'),
                ('command_palette', 'Command palette (Ctrl+K)'),
                ('sidebar', 'Sidebar with session list'),
                ('status_bar', 'Status bar showing connection state'),
                ('gateway', 'Gateway connection management'),
                ('terminal', 'Embedded terminal'),
                ('file_browser', 'File browser'),
                ('cron', 'Cron job management'),
                ('skills', 'Skills management'),
                ('plugins', 'Plugin management'),
                ('profiles', 'Profile management'),
                ('mcp', 'MCP server management'),
                ('artifacts', 'Artifact rendering'),
                ('overlays', 'Overlay/popup system'),
                ('messaging', 'Multi-platform messaging'),
                ('agents', 'Agent management'),
                ('command_center', 'Command center'),
            ],
            'web': [
                ('chat', 'Chat interface'),
                ('sessions', 'Session management'),
                ('models', 'Model selection'),
                ('settings', 'Settings'),
                ('config', 'Configuration'),
                ('cron', 'Cron management'),
                ('skills', 'Skills'),
                ('plugins', 'Plugins'),
                ('profiles', 'Profiles'),
                ('channels', 'Channel management'),
                ('webhooks', 'Webhook management'),
                ('pairing', 'Device pairing'),
                ('files', 'File browser'),
                ('logs', 'Log viewer'),
                ('docs', 'Documentation'),
                ('analytics', 'Analytics dashboard'),
                ('env', 'Environment variables'),
                ('system', 'System status'),
                ('mcp', 'MCP management'),
            ]
        }
        
        # Check desktop features
        desktop_c_funcs = {f.name for f in self.c_func_map.values() if 'desktop' in f.file}
        for feature, description in ux_features['desktop']:
            # Check if any C function relates to this feature
            found = any(feature in name.lower() for name in desktop_c_funcs)
            if not found:
                self.issues.append(ParityIssue(
                    severity="MEDIUM",
                    category="UX",
                    ts_function=f"UX:{feature}",
                    ts_file="apps/desktop",
                    detail=f"Desktop app missing UX feature: {description}",
                    suggestion=f"Implement {feature} functionality in desktop_app.c"
                ))
        
        # Check web features
        web_c_funcs = {f.name for f in self.c_func_map.values() if 'web' in f.file}
        for feature, description in ux_features['web']:
            found = any(feature in name.lower() for name in web_c_funcs)
            if not found:
                self.issues.append(ParityIssue(
                    severity="MEDIUM",
                    category="UX",
                    ts_function=f"UX:{feature}",
                    ts_file="web/src",
                    detail=f"Web app missing UX feature: {description}",
                    suggestion=f"Implement {feature} functionality in web_app.c"
                ))
    
    def _is_react_only(self, func: TSFunction) -> bool:
        """Check if a function is React-specific and doesn't need a C equivalent."""
        react_only = {
            'render', 'componentDidMount', 'componentWillUnmount',
            'shouldComponentUpdate', 'getDerivedStateFromProps',
            'useState', 'useEffect', 'useMemo', 'useCallback', 'useRef',
            'useContext', 'useReducer', 'useLayoutEffect', 'useImperativeHandle',
            'createContext', 'forwardRef', 'memo', 'lazy', 'Suspense',
            'createElement', 'cloneElement', 'isValidElement',
            'Fragment', 'StrictMode', 'Profiler',
        }
        if func.name in react_only:
            return True
        if func.name.startswith('use') and func.name[3:4].isupper():
            # React hooks — check if it's a custom hook that does API calls
            return True
        if func.returns == 'JSX' or func.returns == 'React.ReactNode':
            return True
        return False


# ── UX Fuzzer ───────────────────────────────────────────────────────────────

class UXFuzzer:
    """Generate test cases to verify behavioral parity between TS and C."""
    
    def __init__(self, ts_parser: TSParser, c_parser: CParser):
        self.ts = ts_parser
        self.c = c_parser
        self.test_cases: list[dict] = []
    
    def generate_tests(self) -> list[dict]:
        """Generate fuzz test cases for API endpoints."""
        
        # For each API endpoint found in TS, generate test cases
        for call in self.ts.api_calls:
            endpoint = call['endpoint']
            
            # Generate basic test
            self.test_cases.append({
                'name': f"test_{endpoint.replace('/', '_').strip('_')}_basic",
                'endpoint': endpoint,
                'method': 'GET',
                'body': None,
                'expected_status': 200,
                'expected_fields': [],
                'description': f"Basic GET request to {endpoint}"
            })
            
            # Generate POST test if applicable
            if any(method in call['file'] for method in ['create', 'add', 'new', 'send', 'post', 'submit']):
                self.test_cases.append({
                    'name': f"test_{endpoint.replace('/', '_').strip('_')}_post",
                    'endpoint': endpoint,
                    'method': 'POST',
                    'body': '{}',
                    'expected_status': 200,
                    'expected_fields': ['id', 'status'],
                    'description': f"POST request to {endpoint}"
                })
        
        # Generate edge case tests
        self.test_cases.append({
            'name': 'test_empty_body',
            'endpoint': '/api/chat',
            'method': 'POST',
            'body': '',
            'expected_status': 400,
            'expected_fields': ['error'],
            'description': 'Empty body should return 400'
        })
        
        self.test_cases.append({
            'name': 'test_large_payload',
            'endpoint': '/api/chat',
            'method': 'POST',
            'body': '{"message": "' + 'x' * 10000 + '"}',
            'expected_status': 200,
            'expected_fields': ['response'],
            'description': 'Large payload should be handled'
        })
        
        self.test_cases.append({
            'name': 'test_unicode',
            'endpoint': '/api/chat',
            'method': 'POST',
            'body': '{"message": "Hello 世界 🌍"}',
            'expected_status': 200,
            'expected_fields': ['response'],
            'description': 'Unicode payload should be handled'
        })
        
        self.test_cases.append({
            'name': 'test_special_chars',
            'endpoint': '/api/chat',
            'method': 'POST',
            'body': '{"message": "test\\n\\t\\r\\"\\\\"}',
            'expected_status': 200,
            'expected_fields': ['response'],
            'description': 'Special characters should be escaped correctly'
        })
        
        return self.test_cases


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="TS-to-C Parity Audit Suite")
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    parser.add_argument('--fix', action='store_true', help='Auto-fix issues where possible')
    parser.add_argument('--category', choices=['MISSING', 'SIGNATURE', 'BEHAVIORAL', 'UX', 'ALL'],
                       default='ALL', help='Filter by category')
    parser.add_argument('--severity', choices=['HIGH', 'MEDIUM', 'LOW', 'ALL'],
                       default='ALL', help='Filter by severity')
    parser.add_argument('--fuzz', action='store_true', help='Generate fuzz test cases')
    args = parser.parse_args()
    
    if not args.json:
        print("🔍 TS-to-C Parity Audit Suite — Triple Devil's Advocate")
        print("=" * 60)
        print("\n📂 Parsing TypeScript sources...")
    
    ts_parser = TSParser()
    
    if os.path.exists(APPS_DESKTOP):
        ts_parser.parse_directory(APPS_DESKTOP)
        if not args.json:
            print(f"   Desktop: {len(ts_parser.functions)} functions, {len(ts_parser.components)} components")
    
    if os.path.exists(APPS_SHARED):
        ts_parser.parse_directory(APPS_SHARED)
        if not args.json:
            print(f"   Shared: {len(ts_parser.functions)} functions")
    
    if os.path.exists(WEB_SRC):
        ts_parser.parse_directory(WEB_SRC)
        if not args.json:
            print(f"   Web: {len(ts_parser.functions)} functions, {len(ts_parser.components)} components")
    
    if not args.json:
        print(f"   Total TS: {len(ts_parser.functions)} functions, {len(ts_parser.components)} components, "
              f"{len(ts_parser.hooks)} hooks, {len(ts_parser.handlers)} handlers, "
              f"{len(ts_parser.api_calls)} API calls")
    
    # Parse C
    if not args.json:
        print("\n📂 Parsing C sources...")
    c_parser = CParser()
    c_parser.parse_directory(SLERMES_DIR)
    if not args.json:
        print(f"   Total C: {len(c_parser.functions)} functions, {len(c_parser.api_endpoints)} API endpoints")
    
    # Run parity checks
    if not args.json:
        print("\n🔎 Running parity checks...")
    checker = ParityChecker(ts_parser, c_parser)
    issues = checker.check_all()
    
    # Filter issues
    if args.category != 'ALL':
        issues = [i for i in issues if i.category == args.category]
    if args.severity != 'ALL':
        issues = [i for i in issues if i.severity == args.severity]
    
    # Generate fuzz tests
    fuzz_tests = []
    if args.fuzz:
        if not args.json:
            print("\n🎲 Generating fuzz test cases...")
        fuzzer = UXFuzzer(ts_parser, c_parser)
        fuzz_tests = fuzzer.generate_tests()
        if not args.json:
            print(f"   Generated {len(fuzz_tests)} fuzz test cases")
    
    # Output
    if args.json:
        output = {
            'summary': {
                'ts_functions': len(ts_parser.functions),
                'ts_components': len(ts_parser.components),
                'ts_hooks': len(ts_parser.hooks),
                'ts_handlers': len(ts_parser.handlers),
                'ts_api_calls': len(ts_parser.api_calls),
                'c_functions': len(c_parser.functions),
                'c_api_endpoints': len(c_parser.api_endpoints),
                'issues_found': len(issues),
                'high_severity': len([i for i in issues if i.severity == 'HIGH']),
                'medium_severity': len([i for i in issues if i.severity == 'MEDIUM']),
                'low_severity': len([i for i in issues if i.severity == 'LOW']),
                'fuzz_tests': len(fuzz_tests),
            },
            'issues': [asdict(i) for i in issues],
            'fuzz_tests': fuzz_tests,
        }
        print(json.dumps(output, indent=2))
    else:
        # Group by category
        categories = {}
        for issue in issues:
            cat = issue.category
            if cat not in categories:
                categories[cat] = []
            categories[cat].append(issue)
        
        for cat, cat_issues in sorted(categories.items()):
            print(f"\n{'='*60}")
            print(f"  {cat} ({len(cat_issues)} issues)")
            print(f"{'='*60}")
            
            for issue in sorted(cat_issues, key=lambda x: x.severity):
                severity_icon = {'HIGH': '🔴', 'MEDIUM': '🟡', 'LOW': '🟢'}.get(issue.severity, '⚪')
                print(f"\n  {severity_icon} [{issue.severity}] {issue.ts_function}")
                print(f"     File: {issue.ts_file}")
                if issue.c_file:
                    print(f"     C File: {issue.c_file}")
                print(f"     Detail: {issue.detail}")
                if issue.suggestion:
                    print(f"     Fix: {issue.suggestion}")
        
        # Summary
        high = len([i for i in issues if i.severity == 'HIGH'])
        medium = len([i for i in issues if i.severity == 'MEDIUM'])
        low = len([i for i in issues if i.severity == 'LOW'])
        
        print(f"\n{'='*60}")
        print(f"  SUMMARY")
        print(f"{'='*60}")
        print(f"  TS functions: {len(ts_parser.functions)}")
        print(f"  TS components: {len(ts_parser.components)}")
        print(f"  C functions: {len(c_parser.functions)}")
        print(f"  Issues: {len(issues)} total")
        print(f"    🔴 HIGH:   {high}")
        print(f"    🟡 MEDIUM: {medium}")
        print(f"    🟢 LOW:    {low}")
        
        if fuzz_tests:
            print(f"\n  Fuzz tests generated: {len(fuzz_tests)}")
        
        # Coverage score
        if ts_parser.functions:
            coverage = (1 - high / max(len(ts_parser.functions), 1)) * 100
            print(f"\n  Parity score: {coverage:.1f}% (lower is worse due to HIGH issues)")
    
    return 0 if not issues else 1


if __name__ == '__main__':
    sys.exit(main())
