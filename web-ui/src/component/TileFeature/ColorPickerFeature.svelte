<script lang="ts">
    let {
        label,
        value,
        disabled = false,
        readOnly = false,
        onSelect,
    }: {
        label: string;
        value: string;
        disabled?: boolean;
        readOnly?: boolean;
        onSelect?: (color: string) => void;
    } = $props();
</script>

<button
    type="button"
    class="tile-color-feature"
    class:unavailable={disabled}
    disabled={disabled || readOnly}
    aria-label={`${label}: ${value}`}
    aria-disabled={disabled || readOnly}
    tabindex={disabled || readOnly ? -1 : 0}
    onclick={(event) => { event.stopPropagation(); if (!disabled && !readOnly) onSelect?.(value); }}
    onkeydown={(event) => event.stopPropagation()}
></button>

<style lang="scss">
    .tile-color-feature {
        position: relative;
        display: block;
        width: 100%;
        min-width: 0;
        height: var(--feature-height, 2.625rem);
        padding: 0;
        overflow: hidden;
        border: 0;
        border-radius: var(--feature-border-radius, 0.75rem);
        background: linear-gradient(90deg, #00e5ed 0%, #0068ff 15%, #7b00ff 28%, #f000d0 40%, #ff002b 51%, #ff8c00 63%, #fff000 73%, #00ef27 86%, #00e5ed 100%);
        cursor: pointer;
    }
    .tile-color-feature[aria-disabled='true'] { cursor: default; }
    .tile-color-feature.unavailable { opacity: 0.5; cursor: not-allowed; }
    .tile-color-feature:focus-visible { outline: 0.125rem solid var(--screen-focus-outline); outline-offset: 0.125rem; }
</style>
