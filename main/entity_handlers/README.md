# Entity handlers

Entity-specific tile rendering uses a Strategy pattern:

- `entity_handler.h` defines the common rendering contract and shared helpers.
- `entity_handler_registry.c` maps a Home Assistant domain to its handler.
- Each domain lives in its own subdirectory and owns its tile presentation rules.
- `screen/entity_display.c` supplies the fallback when no domain handler is registered.

To add a domain, create `<domain>/<domain>_entity_handler.c/.h`, export one
`entity_handler_t`, and add its address to `s_entity_handlers` in the registry.
The handler can independently choose the title, state text, icons, colors,
required Store fields, refresh change mask, configured actions, and More Info
content.

`ha_datetime_format.h` provides the shared Home Assistant ISO 8601 relative
time formatter for handlers and screen components. It produces English text
such as `now`, `5 minutes ago`, `yesterday`, or `last month` while respecting
the configured local timezone.

`actionConfig` owns tile interactivity. A non-`nothing` `icon_tap_action`
enables the icon, while a non-`nothing` `tap_action` or `hold_action` enables
the card body. Each domain defines its fallback action behavior in
`default_action_config` on `entity_handler_t`. When the screen configuration
omits `actionConfig` or specific action keys (such as web-ui's "default" option),
the device automatically fills the missing actions with the domain's defaults.
The dispatched `entity_action_context_t` contains the entity id, the complete
tile configuration and action configuration for that card, and the originating
gesture. Handlers implement domain actions through `handle_action`. There is no
registry fallback: a domain must explicitly opt in to `more-info`, optionally
through the shared `entity_handler_handle_more_info_action` helper.

The More Info screen manager retains the current Split View screen with
`auto_del=false` and owns only navigation and lifetime. A domain's
`render_more_info` implementation owns the entire visible page, including any
title, close control, layout, and entity-specific content. Its rendering
context supplies the target screen, a transparent top-layer overlay host, the
entity snapshot and Store, the tile configuration, and a `close` callback that
loads the retained TileView screen again. When a domain does not implement
`render_more_info`, the screen manager renders a minimal fallback with a close
button and the entity state.
