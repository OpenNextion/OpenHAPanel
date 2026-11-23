<script lang="ts">
    interface Props {
        /** Controlled numeric value; conversion from HA state belongs to the caller. */
        value: number;
        label: string;
        min?: number;
        max?: number;
        step?: number;
        /** Every variant uses a filled track; cover uses HA's inverted direction. */
        variant?: 'default' | 'filled' | 'cover' | 'color-temp';
        /** Optional formatted value, including any unit, supplied by the caller. */
        valueText?: string;
        activeColor?: string;
        trackColor?: string;
        disabled?: boolean;
        readOnly?: boolean;
        showActivePart?: boolean;
        onValueInput?: (value: number) => void;
        onValueCommit?: (value: number) => void;
    }

    let {
        value,
        label,
        min = 0,
        max = 100,
        step = 1,
        variant = 'default',
        valueText,
        activeColor = 'var(--screen-tone-ha-inactive-color)',
        trackColor = 'var(--screen-icon-unavailable-background)',
        disabled = false,
        readOnly = false,
        showActivePart = true,
        onValueInput,
        onValueCommit,
    }: Props = $props();

    const progress = $derived(max > min ? Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100)) : 0);
    const visualProgress = $derived(variant === 'cover' ? 100 - progress : progress);
    const inputValue = $derived(variant === 'cover' ? min + max - value : value);
    function actualValue(input: number) { return variant === 'cover' ? min + max - input : input; }
</script>

<label
    class:disabled
    class:default={variant === 'default'}
    class:cover={variant === 'cover'}
    class:color-temp={variant === 'color-temp'}
    class:at-min={variant === 'filled' && progress === 0}
    class="tile-slider-feature"
    style={`--slider-progress: ${visualProgress}%; --slider-active-color: ${activeColor}; --slider-track-color: ${trackColor}`}
>
    <span class="slider-label visually-hidden">{label}</span>
    {#if showActivePart && variant !== 'color-temp'}
        <span class="filled-progress" aria-hidden="true"><span class="filled-grip"></span></span>
    {/if}
    <input
        type="range"
        {min}
        {max}
        {step}
        value={inputValue}
        disabled={disabled || readOnly}
        tabindex={readOnly ? -1 : 0}
        aria-disabled={disabled || readOnly}
        aria-valuetext={valueText}
        onpointerdown={(event) => event.stopPropagation()}
        onkeydown={(event) => event.stopPropagation()}
        onclick={(event) => event.stopPropagation()}
        oninput={(event) => { event.stopPropagation(); if (readOnly) { event.currentTarget.value = String(inputValue); return; } onValueInput?.(actualValue(Number(event.currentTarget.value))); }}
        onchange={(event) => { event.stopPropagation(); if (!readOnly) onValueCommit?.(actualValue(Number(event.currentTarget.value))); }}
    />
</label>

<style lang="scss">
    .tile-slider-feature {
        position: relative;
        display: block;
        width: 100%;
        min-width: 0;
        height: var(--feature-height, 2.625rem);
        padding: 0;
        overflow: hidden;
        border-radius: var(--feature-border-radius, 0.75rem);
        background: var(--slider-track-color);
        color: var(--screen-text-primary);
        font: inherit;
    }

    .tile-slider-feature.disabled { cursor: not-allowed; }

    .filled-progress {
        position: absolute;
        inset-block: 0;
        left: 0;
        display: flex;
        width: clamp(2.25rem, calc(var(--slider-progress) + 0.75rem), 100%);
        padding-right: 0.5rem;
        align-items: center;
        justify-content: flex-end;
        border-radius: inherit;
        background: var(--slider-active-color);
        pointer-events: none;
    }

    .filled-grip {
        width: 0.25rem;
        height: 1.375rem;
        border-radius: 999rem;
        background: #fff;
    }

    .tile-slider-feature.at-min .filled-progress {
        width: 0.875rem;
        padding-right: 0;
        justify-content: center;
    }

    .tile-slider-feature.default .filled-progress,
    .tile-slider-feature.cover .filled-progress {
        width: clamp(0.875rem, calc(var(--slider-progress) + 0.875rem), 100%);
        padding-right: 0.3125rem;
    }

    .tile-slider-feature.color-temp {
        background: linear-gradient(90deg, #ff941f 0%, #ffb87f 34%, #ffe0c9 68%, #fff 100%);
    }

    .tile-slider-feature input {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        margin: 0;
        opacity: 0;
        cursor: pointer;
        touch-action: pan-y;
    }

    .tile-slider-feature:focus-within {
        outline: 0.125rem solid var(--screen-focus-outline);
        outline-offset: 0.125rem;
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

    input:disabled { cursor: not-allowed; }
</style>
