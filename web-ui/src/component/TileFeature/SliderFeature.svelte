<script lang="ts">
    interface Props {
        /** Controlled numeric value; conversion from HA state belongs to the caller. */
        value: number;
        label: string;
        min?: number;
        max?: number;
        step?: number;
        showLabel?: boolean;
        /** Optional formatted value, including any unit, supplied by the caller. */
        valueText?: string;
        activeColor?: string;
        trackColor?: string;
        disabled?: boolean;
        onValueInput: (value: number) => void;
        onValueCommit?: (value: number) => void;
    }

    let {
        value,
        label,
        min = 0,
        max = 100,
        step = 1,
        showLabel = true,
        valueText,
        activeColor = 'var(--screen-focus-outline)',
        trackColor = 'var(--screen-icon-unavailable-background)',
        disabled = false,
        onValueInput,
        onValueCommit,
    }: Props = $props();

    const progress = $derived(max > min ? Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100)) : 0);
</script>

<label
    class:disabled
    class="tile-slider-feature"
    style={`--slider-progress: ${progress}%; --slider-active-color: ${activeColor}; --slider-track-color: ${trackColor}`}
>
    <span class:visually-hidden={!showLabel} class="slider-label">{label}</span>
    {#if valueText !== undefined}<output>{valueText}</output>{/if}
    <input
        type="range"
        {min}
        {max}
        {step}
        {value}
        {disabled}
        aria-valuetext={valueText}
        onpointerdown={(event) => event.stopPropagation()}
        onkeydown={(event) => event.stopPropagation()}
        onclick={(event) => event.stopPropagation()}
        oninput={(event) => { event.stopPropagation(); onValueInput(Number(event.currentTarget.value)); }}
        onchange={(event) => { event.stopPropagation(); onValueCommit?.(Number(event.currentTarget.value)); }}
    />
</label>

<style lang="scss">
    .tile-slider-feature {
        display: grid;
        width: 100%;
        min-width: 0;
        padding: var(--feature-control-padding, 0.5rem 0.75rem);
        grid-template-columns: minmax(0, 1fr) auto;
        align-items: center;
        gap: var(--feature-control-gap, 0.25rem 0.5rem);
        border-radius: var(--feature-border-radius, 0.75rem);
        background: var(--screen-tile-background);
        color: var(--screen-text-primary);
        font: inherit;
    }

    .tile-slider-feature.disabled { opacity: 0.5; }

    .slider-label {
        min-width: 0;
        overflow: hidden;
        font-size: 0.875rem;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .slider-label.visually-hidden {
        position: absolute;
        width: 0.0625rem;
        height: 0.0625rem;
        padding: 0;
        overflow: hidden;
        clip-path: inset(50%);
        white-space: nowrap;
    }

    output {
        color: var(--screen-text-secondary);
        font-size: 0.875rem;
        font-variant-numeric: tabular-nums;
    }

    input {
        display: block;
        width: 100%;
        height: var(--feature-height, 2.625rem);
        margin: 0;
        grid-column: 1 / -1;
        appearance: none;
        -webkit-appearance: none;
        border-radius: 999rem;
        background: linear-gradient(to right,
            var(--slider-active-color) 0 var(--slider-progress),
            var(--slider-track-color) var(--slider-progress) 100%);
        cursor: pointer;
        touch-action: pan-y;
    }

    input:focus-visible {
        outline: 0.125rem solid var(--screen-focus-outline);
        outline-offset: 0.125rem;
    }

    input:disabled { cursor: not-allowed; }

    input::-webkit-slider-thumb {
        width: 1.5rem;
        height: 1.5rem;
        appearance: none;
        -webkit-appearance: none;
        border: 0.125rem solid var(--screen-text-primary);
        border-radius: 50%;
        background: var(--slider-active-color);
    }

    input::-moz-range-thumb {
        width: 1.25rem;
        height: 1.25rem;
        border: 0.125rem solid var(--screen-text-primary);
        border-radius: 50%;
        background: var(--slider-active-color);
    }
</style>
