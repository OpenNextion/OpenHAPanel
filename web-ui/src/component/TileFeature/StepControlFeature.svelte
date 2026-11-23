<script lang="ts">
    let {
        label,
        value,
        min,
        max,
        step,
        unit = '',
        compact = false,
        roomy = false,
        narrow = false,
        disabled = false,
        readOnly = false,
        onChange,
    }: {
        label: string;
        value: number | undefined;
        min: number;
        max: number;
        step: number;
        unit?: string;
        compact?: boolean;
        roomy?: boolean;
        narrow?: boolean;
        disabled?: boolean;
        readOnly?: boolean;
        onChange?: (value: number) => void;
    } = $props();

    const precision = $derived((String(step).split('.')[1] ?? '').length);
    const fullValue = $derived(value === undefined ? unit : `${value.toFixed(precision)}${unit ? ` ${unit}` : ''}`);
    const displayValue = $derived(compact && value !== undefined ? value.toFixed(precision) : fullValue);
    const canDecrease = $derived(value !== undefined && !disabled && !readOnly && value > min);
    const canIncrease = $derived(value !== undefined && !disabled && !readOnly && value < max);

    function change(direction: -1 | 1) {
        if (value === undefined) return;
        const nextValue = Math.max(min, Math.min(max, Number((value + direction * step).toFixed(precision))));
        onChange?.(nextValue);
    }
</script>

<div class="tile-step-control" class:compact class:roomy class:narrow class:unavailable={disabled || value === undefined} role="group" aria-label={label}>
    <button type="button" disabled={!canDecrease} aria-label={`Decrease ${label}`} aria-disabled={!canDecrease} tabindex={canDecrease ? 0 : -1} onclick={(event) => { event.stopPropagation(); if (canDecrease) change(-1); }} onkeydown={(event) => event.stopPropagation()}><span aria-hidden="true">−</span></button>
    <output aria-live="off" aria-label={fullValue}>{displayValue}</output>
    <button type="button" disabled={!canIncrease} aria-label={`Increase ${label}`} aria-disabled={!canIncrease} tabindex={canIncrease ? 0 : -1} onclick={(event) => { event.stopPropagation(); if (canIncrease) change(1); }} onkeydown={(event) => event.stopPropagation()}><span aria-hidden="true">+</span></button>
</div>

<style lang="scss">
    .tile-step-control {
        display: grid;
        width: 100%;
        min-width: 0;
        height: var(--feature-height, 2.625rem);
        grid-template-columns: 2rem minmax(0, 1fr) 2rem;
        align-items: center;
        border-radius: var(--feature-border-radius, 0.75rem);
        background: var(--screen-icon-unavailable-background);
        color: var(--screen-text-primary);
    }
    button {
        display: grid;
        width: 100%;
        height: 100%;
        padding: 0;
        place-items: center;
        border: 0;
        background: transparent;
        color: inherit;
        font: inherit;
        font-size: 1.25rem;
        cursor: pointer;
    }
    button[aria-disabled='true'] { cursor: default; opacity: 0.5; }
    button:focus-visible { outline: 0.125rem solid var(--screen-focus-outline); outline-offset: -0.125rem; }
    output { min-width: 0; overflow: hidden; font-size: 0.875rem; font-variant-numeric: tabular-nums; text-align: center; text-overflow: ellipsis; white-space: nowrap; }
    .tile-step-control.unavailable { background: var(--screen-feature-disabled-background); color: var(--screen-feature-disabled-color); }
    .tile-step-control.compact { grid-template-columns: 0.5rem minmax(0, 1fr) 0.5rem; }
    .tile-step-control.compact.roomy { grid-template-columns: 1rem minmax(0, 1fr) 1rem; }
    .tile-step-control.compact output { font-size: 0.6875rem; }
    .tile-step-control.narrow { grid-template-columns: 1rem minmax(0, 1fr) 1rem; }
    .tile-step-control.narrow output { font-size: 0.75rem; }
    .tile-step-control.compact output, .tile-step-control.narrow output { display: flex; height: 100%; align-items: center; justify-content: center; line-height: 1; }
    .tile-step-control.compact button span, .tile-step-control.narrow button span { display: none; }
    .tile-step-control.compact button::before, .tile-step-control.narrow button::before { content: ''; grid-area: 1 / 1; width: 0.5rem; height: 1px; border-radius: 1px; background: currentColor; }
    .tile-step-control.compact button:last-of-type::after, .tile-step-control.narrow button:last-of-type::after { content: ''; grid-area: 1 / 1; width: 1px; height: 0.5rem; border-radius: 1px; background: currentColor; }
</style>
