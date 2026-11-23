<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { getTileFavoriteVisibleCount } from '@/constant/tileFeaturePosition';
    import { deviceStore } from '@/stores/device';

    export interface Option { value: string; label: string; icon?: string }

    let {
        label,
        value,
        options,
        variant = 'segmented',
        selectIcon,
        placeholder,
        selectLabel,
        activeColor = 'var(--screen-focus-outline)',
        trackColor = 'var(--screen-tone-ha-fan-background)',
        spansFullRow = false,
        disabled = false,
        readOnly = false,
        onSelect,
    }: {
        label: string;
        value?: string;
        options: Option[];
        variant?: 'segmented' | 'favorites' | 'select' | 'fan-controls' | 'fan-speed' | 'hvac';
        selectIcon?: string;
        placeholder?: string;
        selectLabel?: string;
        activeColor?: string;
        trackColor?: string;
        spansFullRow?: boolean;
        disabled?: boolean;
        readOnly?: boolean;
        onSelect?: (value: string) => void;
    } = $props();

    const visibleOptionCount = $derived(getTileFavoriteVisibleCount('cover', $deviceStore.screenRotation.rotation === 90, spansFullRow));
    const visibleOptions = $derived(variant === 'favorites' ? options.slice(0, visibleOptionCount) : options);
</script>

{#if variant === 'select'}
    <label class="tile-option-select" class:with-icon={!!selectIcon} class:with-selected-display={!!selectLabel && options.some((option) => option.value === value)} class:unavailable={disabled}>
        <span class="visually-hidden">{label}</span>
        {#if selectIcon}<span class="select-leading-icon" aria-hidden="true"><SvgIcon name={selectIcon} /></span>{/if}
        <select value={value ?? ''} disabled={disabled || readOnly} onchange={(event) => { event.stopPropagation(); onSelect?.(event.currentTarget.value); }} onclick={(event) => event.stopPropagation()}>
            {#if !options.some((option) => option.value === value)}<option value="">{placeholder ?? (selectIcon ? '' : label)}</option>{/if}
            {#each options as option (option.value)}<option value={option.value}>{option.label}</option>{/each}
        </select>
        {#if selectLabel && options.some((option) => option.value === value)}
            <span class="select-value-display" aria-hidden="true">
                <small>{selectLabel}</small>
                <span>{options.find((option) => option.value === value)?.label}</span>
            </span>
        {/if}
        {#if selectIcon}<span class="select-chevron" aria-hidden="true"><SvgIcon name="mdi:chevron-down" /></span>{/if}
    </label>
{:else}
    <div class="tile-option-group" class:favorite-positions={variant === 'favorites'} class:fan-controls={variant === 'fan-controls'} class:fan-speed={variant === 'fan-speed'} class:hvac={variant === 'hvac'} class:unavailable={disabled} role="group" aria-label={label} style={`--option-active-color: ${activeColor}; --option-count: ${visibleOptions.length}; --fan-speed-track-color: ${trackColor}`}>
        {#each visibleOptions as option (option.value)}
            <button type="button" disabled={disabled || readOnly} class:selected={option.value === value} aria-label={option.label} aria-pressed={option.value === value} aria-disabled={disabled || readOnly} tabindex={disabled || readOnly ? -1 : 0} onclick={(event) => { event.stopPropagation(); if (!disabled && !readOnly) onSelect?.(option.value); }} onkeydown={(event) => event.stopPropagation()}>
                {#if option.icon}<SvgIcon name={option.icon} />{:else}<span>{option.label}</span>{/if}
            </button>
        {/each}
    </div>
{/if}

<style lang="scss">
    .tile-option-group { display: flex; width: 100%; min-width: 0; min-height: var(--feature-height, 2.625rem); padding: 0.2rem; gap: 0.15rem; border-radius: var(--feature-border-radius, 0.75rem); background: var(--screen-icon-unavailable-background); overflow-x: auto; }
    .tile-option-group button { display: grid; min-width: 2rem; min-height: 2rem; padding: 0.2rem 0.35rem; flex: 1 0 auto; place-items: center; border: 0; border-radius: 0.55rem; background: transparent; color: var(--screen-text-primary); font: inherit; font-size: 0.75rem; cursor: pointer; }
    .tile-option-group button.selected { background: var(--option-active-color); color: #fff; }
    .tile-option-group.unavailable { background: var(--screen-feature-disabled-background); }
    .tile-option-group.unavailable button { color: var(--screen-feature-disabled-color); cursor: not-allowed; }
    .tile-option-group.unavailable button.selected { background: var(--screen-feature-disabled-fill); color: #fff; }
    .tile-option-group.favorite-positions { display: flex; height: var(--feature-height, 2.625rem); min-height: 0; padding: 0; gap: 0; border-radius: 0.625rem; overflow: visible; }
    .tile-option-group.favorite-positions button { width: auto; min-width: 0; min-height: 0; padding: 0 0.125rem; flex: 1 1 0; border-radius: 0.75rem; font-size: 12px; font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .tile-option-group.favorite-positions.unavailable { --favorite-disabled-color: var(--screen-cover-favorite-disabled-color, var(--screen-feature-disabled-color)); background: var(--screen-cover-favorite-disabled-background); }
    .tile-option-group.favorite-positions.unavailable button { color: var(--favorite-disabled-color); }
    .tile-option-group.favorite-positions.unavailable button.selected { background: var(--favorite-disabled-color); color: #fff; }
    .tile-option-group.fan-controls { display: grid; height: var(--feature-height, 2.625rem); min-height: 0; padding: 0; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 0; overflow: hidden; }
    .tile-option-group.fan-controls button { min-width: 0; min-height: 0; padding: 0; border-radius: inherit; }
    .tile-option-group.fan-speed { display: grid; height: var(--feature-height, 2.625rem); min-height: 0; padding: 0; grid-template-columns: repeat(var(--option-count), minmax(0, 1fr)); gap: 0; overflow: hidden; background: var(--fan-speed-track-color); }
    .tile-option-group.fan-speed button { min-width: 0; min-height: 0; padding: 0; border-radius: inherit; font-weight: 500; }
    .tile-option-group.fan-speed.unavailable { background: var(--screen-feature-disabled-background); }
    .tile-option-group.hvac { display: grid; height: var(--feature-height, 2.625rem); min-height: 0; padding: 0; grid-template-columns: repeat(var(--option-count), minmax(0, 1fr)); gap: 0; overflow: hidden; }
    .tile-option-group.hvac button { width: 100%; min-width: 0; min-height: 0; padding: 0 1px; border-radius: inherit; }
    .tile-option-group.hvac :global(.sf-icon) { width: 100%; max-width: 1.15rem; min-width: 0; }
    .tile-option-group.hvac :global(svg) { width: 100%; height: auto; aspect-ratio: 1; flex: 0 1 auto; }
    .tile-option-group button[aria-disabled='true'] { cursor: default; }
    .tile-option-group :global(svg) { width: 1.15rem; height: 1.15rem; }
    .tile-option-select { position: relative; display: block; width: 100%; }
    .tile-option-select select { width: 100%; min-width: 0; height: var(--feature-height, 2.625rem); padding: 0 0.75rem; border: 0; border-radius: var(--feature-border-radius, 0.75rem); background: var(--screen-icon-unavailable-background); color: var(--screen-text-primary); font: inherit; }
    .tile-option-select.with-icon select { padding-left: 1.75rem; padding-right: 1.375rem; appearance: none; -webkit-appearance: none; }
    .tile-option-select.with-selected-display select { color: transparent; }
    .tile-option-select.with-selected-display select option { color: var(--screen-text-primary); }
    .select-value-display { position: absolute; top: 0; right: 1.375rem; bottom: 0; left: 1.75rem; display: flex; min-width: 0; flex-direction: column; justify-content: center; overflow: hidden; pointer-events: none; }
    .select-value-display small { overflow: hidden; color: var(--screen-text-primary); font-size: 0.625rem; line-height: 0.75rem; text-overflow: ellipsis; white-space: nowrap; }
    .select-value-display > span { overflow: hidden; color: var(--screen-text-primary); font-size: 0.75rem; line-height: 0.875rem; text-overflow: ellipsis; white-space: nowrap; }
    .select-leading-icon, .select-chevron { position: absolute; z-index: 1; top: 50%; display: grid; place-items: center; color: var(--screen-text-primary); pointer-events: none; transform: translateY(-50%); }
    .select-leading-icon { left: 0.375rem; }
    .select-chevron { right: 0.375rem; }
    .select-leading-icon :global(svg) { width: 1rem; height: 1rem; }
    .select-chevron :global(svg) { width: 0.75rem; height: 0.75rem; }
    .tile-option-select select:disabled { opacity: 1; }
    .tile-option-select.unavailable select { background: var(--screen-feature-disabled-background); color: var(--screen-feature-disabled-color); cursor: not-allowed; }
    .tile-option-select.unavailable .select-leading-icon,
    .tile-option-select.unavailable .select-chevron { color: var(--screen-feature-disabled-color); }
    .tile-option-group.unavailable button[aria-disabled='true'] { cursor: not-allowed; }
    .visually-hidden { position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%); white-space: nowrap; }
</style>
