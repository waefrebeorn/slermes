# Bug Report: read_file and terminal tools truncate long identifiers, introducing literal `...` and `***` into source files

**Title:** `[Bug]: read_file/terminal tools truncate long identifiers with "..." — copy-pasting introduces literal dots/asterisks into source files`

**Labels:** `bug`

## Bug Description

The `read_file` and `terminal` tools truncate long identifiers (function names, variable names) with `...` in their display output. This is **not just a visual truncation** — when the truncated text is copy-pasted into code, terminal commands, or `patch` operations, the `...` becomes **literal dot characters** (hex `2e2e2e`) in the source file.

Similarly, `***` (hex `2a2a2a`) can appear as literal 3 asterisks where a function name should be.

This has caused **silent source file corruption** in the slermes C translation project, where function names like `fal_er...onse` (should be `fal_er...onse`) and `***` (should be `***`) were introduced into `.c` and `.h` files. The corruption is invisible in tool output but causes compilation failures.

## Steps to Reproduce

1. Use `read_file` to read a C source file containing long function names (e.g. `fal_er...onse`)
2. The tool displays the name as `fal_er...onse` (with `...` in the middle)
3. Copy-paste the displayed name into a `patch` operation or Python script
4. The `...` becomes literal dot characters (hex `2e2e2e`) in the output file
5. The C compiler sees `fal_er` + `...` + `onse` — a syntax error

## Expected Behavior

Tool output should either:
- Show the full identifier without truncation, OR
- Clearly mark truncation with a non-copy-pasteable indicator (e.g. `[truncated: fal_er...onse]`), OR
- Provide a way to get the raw bytes (e.g. a `raw` mode or hex output)

## Actual Behavior

The `...` truncation looks identical to a valid identifier. When copy-pasted, it introduces literal dot characters into source files. The corruption is invisible until compilation fails.

## Affected Component

- **Tools** (read_file, terminal)
- **Agent Core** (any tool output that gets copy-pasted into code)

## Root Cause

The `read_file` tool applies display truncation to long identifiers by replacing middle characters with `...` (3 dot characters). The `terminal` tool does the same. When this output is used programmatically (e.g. in `patch` operations, Python string replacement, or terminal commands), the `...` becomes literal bytes in the output.

## Evidence

From `lib/libfal_common/fal_common.c` (slermes C translation project):

```
# What the tool displays:
fal_er...onse

# What's actually in the file (hex):
66616c5f65722e2e2e6f6e7365
# Decoded: fal_er + 0x2e2e2e (3 dots) + onse

# What it should be:
66616c5f6572726f725f726573706f6e7365
# Decoded: fal_er...onse
```

The `...` in the file is 3 literal dot characters (hex `2e2e2e`), not a display artifact.

## Proposed Fix

1. **Option A:** Do not truncate identifiers in tool output. Show the full name.
2. **Option B:** Use a non-copy-pasteable truncation marker like `[...]` or `…` (unicode ellipsis, hex `e280a6`) instead of `...` (3 ASCII dots).
3. **Option C:** Add a `raw` or `hex` mode to `read_file` that outputs raw bytes without truncation.
4. **Option D:** When truncation is necessary, append the hex representation so users can recover the real name.

## Workaround

Users must use Python to read raw bytes and `bytes.fromhex()` to construct real names:

```python
with open('file.c', 'rb') as f:
    content = f.read()
idx = content.find(b'fal_er')  # search for known prefix
real_name = content[idx:idx+30]  # get raw bytes
print(real_name.hex())  # decode to get real name
```

## Platform

- OS: Ubuntu 24.04 (WSL)
- Hermes version: 0.15.1-slermes
- Python: 3.11
