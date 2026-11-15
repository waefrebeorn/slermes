<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Tools Commands

## `/tools`

List all available tools and their registration status.

```
/tools
```

Shows each tool's name, whether it's enabled, and its current status.

## `/tools-verify`

Verify that all expected tools are properly registered in the tool registry. CLI-only.

```
/tools-verify
```

Reports any missing tools or registration issues.

## `/commands` (`/cmds`)

List all available slash commands. CLI-only.

```
/commands
```

## `/toolsets`

List available toolsets.

```
/toolsets
```

## `/image`

Attach a local image file to include with the next prompt.

```
/image <path>
```

**Example:**
```
/image screenshot.png
```

## `/paste`

Attach an image from the system clipboard. CLI-only.

```
/paste
```

## `/browser`

Connect the browser tool to a Chromium instance via Chrome DevTools Protocol. CLI-only.

```
/browser
```

## `/deps`

Install third-party Python bridge dependencies. CLI-only.

```
/deps
```

## `/skills`

Search and manage installed skills. CLI-only.

```
/skills
```
