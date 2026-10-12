<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
