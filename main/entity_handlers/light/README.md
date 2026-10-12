# Light entity implementation plan

## Goal

Rebuild the `light` More Info page from small, independently implementable LVGL
components. The visual and interaction references are:

- `light实体效果图/`
- `.homeassistant_ui_demo/main/ui.c`

The screenshots are the visual source of truth for the 320 x 480 target. The
demo supplies proven layout, drawing, gesture, animation, and cleanup logic,
but its global entity state and local simulation must not be copied into the
components.

The final data flow is intentionally one-directional:

```text
Home Assistant / Entity Store
              |
              v
 light_entity_handler.c
  - capability parsing
  - state normalization
  - page composition
  - HA service calls
              |
       setters / callbacks
              |
              v
 light/components/*
  - LVGL objects
  - drawing and animation
  - pointer interaction
  - semantic UI callbacks
```

Only `light_entity_handler.c` may include `app.h`, access the Entity Store, read
Home Assistant attributes, or issue Home Assistant service calls. A component
must remain usable in a host test with plain values and callbacks.

## Planned directory layout

```text
main/entity_handlers/light/
|-- README.md
|-- light_entity_handler.c
|-- light_entity_handler.h
`-- components/
    |-- light_component_types.h
    |-- light_mode_bar.c
    |-- light_mode_bar.h
    |-- light_assumed_power_buttons.c
    |-- light_assumed_power_buttons.h
    |-- light_power_toggle.c
    |-- light_power_toggle.h
    |-- light_brightness_slider.c
    |-- light_brightness_slider.h
    |-- light_color_temp_slider.c
    |-- light_color_temp_slider.h
    |-- light_rgb_picker.c
    |-- light_rgb_picker.h
    |-- light_preset_colors.c
    |-- light_preset_colors.h
    |-- light_effect_selector.c
    `-- light_effect_selector.h
```

`light_component_types.h` is the only shared component contract. It contains
plain enums and value structs; it must not contain Home Assistant concepts.

## Shared component contract

All value controls distinguish a live preview from a committed value:

```c
typedef enum {
    LIGHT_CONTROL_PREVIEW,
    LIGHT_CONTROL_COMMIT,
    LIGHT_CONTROL_CANCEL,
} light_control_phase_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} light_rgb_t;

typedef enum {
    LIGHT_MODE_BRIGHTNESS,
    LIGHT_MODE_RGB,
    LIGHT_MODE_COLOR_TEMP,
} light_mode_t;
```

Rules shared by every component:

- `create(parent)` returns one root `lv_obj_t *` owned by `parent`.
- Public setters update the UI without invoking the user callback.
- Callbacks are emitted only for user interaction.
- Dragging emits `PREVIEW`; a successful release emits one `COMMIT`.
- `PRESS_LOST` restores the last committed value and emits `CANCEL` when
  applicable.
- Disabled state cancels active animations/gestures and blocks callbacks.
- Any animation must be deleted from `LV_EVENT_DELETE`.
- Components must not create timers unless the timer is deleted with the root.
- Components must not retain caller-owned temporary strings or arrays.
- Callbacks execute synchronously on the LVGL task. The caller must copy any
  callback string it needs after the callback returns.
- Components contain no entity ID, HA attribute name, cJSON object, service
  name, WebSocket request, or Entity Store handle.

The handler uses `PREVIEW` only to update adjacent UI. It decides whether and
when a `COMMIT` becomes a Home Assistant command.

## Page composition owned by the handler

`light_entity_handler.c` owns the screen-level layout:

1. Header: close button and ellipsized friendly name.
2. State block: `Off`, brightness percentage, or `不可用`, plus relative time.
3. First-page control host: exactly one of brightness, RGB, or color
   temperature.
4. First-page mode bar.
5. Optional second page: preset colors and Effect selector.
6. Page indicator dots when the second page exists.

The handler creates fixed-size hosts and passes each host to a component. A
component lays out only inside its own host and must not use screen-global
coordinates. This lets each component be developed independently without
competing over page placement.

Reference regions from the 320 x 480 screenshots are approximately:

| Region | Reference placement |
|---|---|
| Header | top 64 px |
| State and relative time | y = 78 .. 124 |
| Main slider | centered, y = 143 .. 372 |
| RGB wheel | centered, 230 px diameter |
| Mode bar | centered near y = 389 .. 438 |
| Preset colors | two rows in y = 161 .. 292 |
| Effect button | centered near y = 321 .. 376 |
| Page dots | bottom center near y = 452 .. 460 |

Exact sizes and colors should be measured from `light实体效果图/`; these
regions define ownership, not an alternative visual specification.

## Component 1: mode switching bar

Files: `light_mode_bar.c/.h`

Responsibilities:

- Render the rounded dark toolbar.
- Render the power button and separator.
- Conditionally render brightness, RGB, and color-temperature buttons.
- Center the toolbar according to the number of visible buttons.
- Show the active mode using the white selected treatment from the screenshots.
- Show power state without deciding what turning on or off means.
- Apply the grey disabled appearance used by unavailable entities.
- Emit either a power-click event or a requested `light_mode_t`.

Suggested API shape:

```c
lv_obj_t *light_mode_bar_create(lv_obj_t *parent);
void light_mode_bar_set_modes(lv_obj_t *obj, uint32_t mode_mask);
void light_mode_bar_set_active_mode(lv_obj_t *obj, light_mode_t mode);
void light_mode_bar_set_power(lv_obj_t *obj, bool on);
void light_mode_bar_set_disabled(lv_obj_t *obj, bool disabled);
void light_mode_bar_set_event_cb(lv_obj_t *obj,
                                 light_mode_bar_event_cb_t callback,
                                 void *user_data);
```

Reference implementation: the mode row created by `build_light()` in
`.homeassistant_ui_demo/main/ui.c`. Port its dynamic button-count and selected
mode behavior, not its global `selected`/`states` access.

## Component 2: brightness slider

Files: `light_brightness_slider.c/.h`

Responsibilities:

- Render the solid-color vertical pill slider.
- Display the dark off track and colored active fill.
- Render the white horizontal indicator.
- Accept a configurable active color so RGB lights can tint the fill.
- Support tap, hold, and drag.
- Animate a tap to its destination; dragging follows immediately.
- Accept state values in the range 0 .. 100. Pointer/encoder adjustment is
  clamped to 1 .. 100; turning the light off belongs to the power control.
- Optionally show the temporary percentage bubble used by the demo behavior.
- Emit preview/commit/cancel callbacks; never decide that zero means
  `light.turn_off`.

Suggested API shape:

```c
lv_obj_t *light_brightness_slider_create(lv_obj_t *parent);
void light_brightness_slider_set_value(lv_obj_t *obj,
                                       uint8_t percent,
                                       lv_anim_enable_t animate);
void light_brightness_slider_set_active_color(lv_obj_t *obj, lv_color_t color);
void light_brightness_slider_set_power(lv_obj_t *obj, bool on);
void light_brightness_slider_set_disabled(lv_obj_t *obj, bool disabled);
void light_brightness_slider_set_change_cb(
    lv_obj_t *obj,
    light_brightness_change_cb_t callback,
    void *user_data);
```

Reference implementation: `make_slider()`, `slider_event()`,
`slider_pointer()`, `slider_sync()`, and the non-temperature branch of
`slider_paint()` in `.homeassistant_ui_demo/main/ui.c`.

This must be a separate implementation from the color-temperature slider. The
two controls may follow the same gesture contract, but their paint rules and
indicators are intentionally different.

## Component 3: color-temperature slider

Files: `light_color_temp_slider.c/.h`

Responsibilities:

- Render the warm-to-cool vertical gradient.
- Accept entity-specific minimum and maximum Kelvin values.
- Render the wide white marker and grey center line shown in the screenshots.
- Map pointer position to Kelvin and clamp it to the configured range.
- Support tap animation and immediate drag.
- Emit preview/commit/cancel callbacks in Kelvin.
- Hide or disable its marker when requested by the handler.
- Apply unavailable styling without losing its configured range.

Suggested API shape:

```c
lv_obj_t *light_color_temp_slider_create(lv_obj_t *parent);
void light_color_temp_slider_set_range(lv_obj_t *obj,
                                       uint16_t min_kelvin,
                                       uint16_t max_kelvin);
void light_color_temp_slider_set_value(lv_obj_t *obj,
                                       uint16_t kelvin,
                                       lv_anim_enable_t animate);
void light_color_temp_slider_set_marker_visible(lv_obj_t *obj, bool visible);
void light_color_temp_slider_set_disabled(lv_obj_t *obj, bool disabled);
void light_color_temp_slider_set_change_cb(
    lv_obj_t *obj,
    light_color_temp_change_cb_t callback,
    void *user_data);
```

Reference implementation: the temperature branch of `slider_paint()`, the
Kelvin branch of `slider_pointer()`, and the gradient stop generation in
`make_slider()` from `.homeassistant_ui_demo/main/ui.c`.

Keep draw work bounded. A pre-generated target-size gradient asset or a small,
fixed number of gradient bands is preferable to allocating a runtime canvas.

## Component 4: RGB color picker

Files: `light_rgb_picker.c/.h`

Responsibilities:

- Render the circular RGB color wheel.
- Convert pointer position into a normalized RGB value.
- Restore the marker position from an RGB value.
- Clamp the marker to the valid color-wheel radius.
- While dragging, show the enlarged color preview circle above the finger.
- Restore the marker with the short release animation.
- Invalidate only the old/new marker areas rather than the whole screen.
- Emit RGB preview/commit/cancel callbacks.
- Never turn on a light or send `rgb_color` itself.

Suggested API shape:

```c
lv_obj_t *light_rgb_picker_create(lv_obj_t *parent);
void light_rgb_picker_set_value(lv_obj_t *obj, light_rgb_t rgb);
void light_rgb_picker_set_marker_visible(lv_obj_t *obj, bool visible);
void light_rgb_picker_set_disabled(lv_obj_t *obj, bool disabled);
void light_rgb_picker_set_change_cb(lv_obj_t *obj,
                                    light_rgb_change_cb_t callback,
                                    void *user_data);
```

Reference implementation: `wheel_marker_area()`, `wheel_damage()`,
`wheel_scale_anim()`, `wheel_release()`, and `wheel_event()` in
`.homeassistant_ui_demo/main/ui.c`.

Do not copy the demo's 320 px and 400 px ARGB resources wholesale. Generate a
single target-size 230 px wheel matching the configured control
and keep it in flash-backed assets.

## Component 5: preset colors

Files: `light_preset_colors.c/.h`

Responsibilities:

- Render the two rows of four circular preset colors from the target image.
- Support a default eight-color palette and an optional caller-provided
  palette.
- Provide pressed feedback and a selected treatment if required.
- Emit the selected RGB value and index.
- Apply disabled/unavailable styling.
- Never issue a light command.

Suggested API shape:

```c
lv_obj_t *light_preset_colors_create(lv_obj_t *parent);
void light_preset_colors_set_palette(lv_obj_t *obj,
                                     const light_rgb_t *colors,
                                     size_t count);
void light_preset_colors_set_selected(lv_obj_t *obj, int selected_index);
void light_preset_colors_set_disabled(lv_obj_t *obj, bool disabled);
void light_preset_colors_set_select_cb(lv_obj_t *obj,
                                       light_preset_select_cb_t callback,
                                       void *user_data);
```

There is no equivalent eight-color component in the demo. Use
`light实体效果图/支持开关+亮度+彩光+色温(右边侧屏).png` as the
layout reference. Demo button/ripple helpers may still be reused conceptually.

## Component 6: Effect selector

Files: `light_effect_selector.c/.h`

Responsibilities:

- Render the rounded Effect button and sparkle icon.
- Display the current effect when the target design calls for it.
- Open an anchored dropdown/list above the button.
- Populate the dropdown from caller-provided strings.
- Mark or focus the current selection.
- Close on selection, outside click, disable, or object deletion.
- Emit only the selected effect string/index.
- Copy effect options into component-owned storage or LVGL-owned dropdown
  options; never retain transient caller strings.
- Never issue an HA service call.

Suggested API shape:

```c
lv_obj_t *light_effect_selector_create(lv_obj_t *parent);
bool light_effect_selector_set_options(lv_obj_t *obj,
                                       const char *const *options,
                                       size_t count);
void light_effect_selector_set_selected(lv_obj_t *obj, size_t index);
void light_effect_selector_set_power(lv_obj_t *obj, bool on);
void light_effect_selector_set_disabled(lv_obj_t *obj, bool disabled);
void light_effect_selector_set_change_cb(lv_obj_t *obj,
                                         light_effect_change_cb_t callback,
                                         void *user_data);
```

Reference implementation: `effects_menu()`, `menu_outside()`, and
`close_menu()` in `.homeassistant_ui_demo/main/ui.c`. The exact dropdown
geometry comes from the two right-page Effect screenshots.

## Component 7: power-only toggle

Files: `light_power_toggle.c/.h`

For lights without brightness, RGB, or color-temperature controls, render the
vertical power toggle in the main control area and omit the mode bar. The
component owns its animation and visual state and emits only an on/off change;
the handler remains responsible for issuing the Home Assistant service call.

## Component 8: assumed-state power buttons

Files: `light_assumed_power_buttons.c/.h`

For a power-only light whose `assumed_state` attribute is true, or whose state
is `unknown`, render separate turn-on and turn-off buttons instead of a switch.
The buttons issue explicit semantic callbacks because the device cannot confirm
that its physical state changed. An unknown state initially selects neither
button; a known assumed state highlights the matching command.

## Responsibilities retained by light_entity_handler.c

The integrator owns all logic that crosses the UI/HA boundary:

- Parse `state`, `brightness`, `color_mode`, `supported_color_modes`,
  `color_temp_kelvin`, legacy mired attributes, RGB/HS/XY attributes,
  `effect`, and `effect_list`.
- Convert HA values into the plain values accepted by components.
- Decide which mode buttons and which second-page components exist.
- Select the initial mode and preserve the selected mode during live updates.
- Build the header, state text, relative-time label, two-page container, mode
  host, component hosts, and page dots.
- Translate component callbacks into `light.turn_on` or `light.turn_off`.
- Build service data for `brightness_pct`, `color_temp_kelvin`, `rgb_color`,
  and `effect`.
- Decide that brightness zero means off and that manipulating color or color
  temperature while off should turn the light on.
- Maintain optimistic preview state and reconcile it with authoritative HA
  state updates.
- Suppress server-side intermediate values while a control is actively being
  dragged.
- Avoid rebuilding or deleting an active control in the middle of a gesture.
- Disable every component for `unknown`, `unavailable`, removed, or otherwise
  unavailable entities.
- Close the page and release all handler-owned state.

The current parameterless `app_ha_call_entity_service()` is insufficient for
brightness, color temperature, RGB, and Effect. The integrator may extend the
application service wrapper, but component implementations must not depend on
that work.

## Capability-to-layout rules

The layout is capability driven, never entity-name driven:

| Capabilities | First page | Second page |
|---|---|---|
| on/off only | power state | none |
| brightness | brightness slider + power/brightness bar | none |
| brightness + color temperature | brightness or temperature slider | none unless effects exist |
| brightness + RGB | brightness or RGB picker | preset colors, plus Effect if supported |
| brightness + RGB + color temperature | all three selectable modes | preset colors, plus Effect if supported |
| effects without RGB | supported first-page controls | Effect selector only |

Create page dots only when the second page exists. Unsupported controls must
not be created and merely hidden; capability changes are handled by the
handler rebuilding the page at a safe point.

## Multi-developer integration rules

Before parallel component work starts, the integrator should land only:

1. This README.
2. `components/light_component_types.h` with final shared types.
3. Empty public headers with the agreed API signatures.
4. Optional host-test scaffolding and target-size parent containers.

After that, each component can be assigned as an independent work package.
Component developers must modify only their own `.c/.h` pair and any explicitly
assigned asset. Changes to shared types or public signatures require agreement
with the integrator first.

Each component handoff should include:

- A plain LVGL construction example with no HA dependency.
- Initial value, disabled value, boundary values, press, drag, release, and
  deletion behavior.
- Confirmation that programmatic setters do not trigger callbacks.
- Confirmation that no timer/animation survives root deletion.
- The screenshot(s) used for its visual comparison.

`light_entity_handler.c` should be integrated only after individual component
APIs are stable. It is the sole place where the pieces are composed and where
Home Assistant behavior is tested.

## Explicit non-goals for components

- No Home Assistant history fetching or history rendering.
- No entity capability discovery.
- No entity-name-specific Floor lamp or Bar lamp branches.
- No WebSocket calls, service JSON, retries, or notifications.
- No ownership of the More Info screen transition or close operation.
- No full-screen layout decisions.
- No copying of `.homeassistant_ui_demo` globals or simulated entity records.

History can be added later as a separate handler-owned page using the existing
shared history components; it must not be coupled to any of the six controls
above.

## Current framework status

The initial integration skeleton is now present. All six public component APIs
exist, each component creates a minimal visible LVGL control, owns its private
context, emits semantic callbacks, and frees that context on deletion.

`light_entity_handler.c` currently:

- parses the basic light capability/state attributes;
- creates the first page and optional second page;
- swaps the active control through the mode bar;
- preserves the user-selected mode across Home Assistant state refreshes;
- wires preview/commit callbacks;
- sends basic power, brightness, Kelvin, RGB, preset, and Effect commands;
- disables controls when the entity is unavailable.

Most remaining visuals are deliberately placeholders. Component owners are
expected to replace only the internals of their assigned `.c` file while
keeping the public header contract stable. The mode switching bar, brightness
slider, color-temperature slider, and RGB picker have been ported from the
Floor lamp behavior. The RGB picker uses the target-size flash-backed color
wheel, restores marker position from the current RGB value, and includes the
enlarged drag preview and release animation. Effect popup geometry, page-dot
synchronization, fine animation timing, and incremental HA refresh
reconciliation still require their detailed passes.
