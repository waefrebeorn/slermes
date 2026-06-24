# Battleship v299 — Name Parity Complete

## Status
- 0 GAP, 0 PARTIAL across all modules
- **Name parity: 107/107 candidates (77 renamed + 30 retained)**
- Retained: hermes_resolve_path (30 call sites), register_provider (3 modules, generic verb)
- Plugins: 19/19 (11 real + 8 annotation stubs)
- Gateway: 18/18 platforms

## Last Session (v299)
- 15 provider_metadata.c renames (model_id_matches, is_openrouter_base_url, etc.)
- 12 auxiliary_client.c renames (task_timeout, task_extra_body, task_provider_model, etc.)
- 1 markdown_tables.c rename (disp_width)
- Batch process: `sed -i` across .c + .h files, zero build errors