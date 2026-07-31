# WS-16 proposal — weather.type enum (snow, future)

**Status:** not implemented (2026-07-30). **From:** WS-16.

## Context

`build_environment` today supports **rain only** via `include.rain` and
`weather.rain_system_path` / streak fallback. There is no `weather.type` selector.

## Proposed future shape

```json
"weather": {
  "type": "rain | snow | clear",
  "rain_system_path": "...",
  "snow_system_path": "..."
}
```

Snow would require Niagara assets, material response, and validation gates distinct
from rain. Do not infer snow from `include.rain` or silent defaults.

## Current honest behavior

- `include.rain: true` → camera-follow rain (Niagara when path supplied;
  `fallback_policy=allow_approximate` for streak fallback).
- No snow path exists; agents must not assume `weather.type` is parsed.
