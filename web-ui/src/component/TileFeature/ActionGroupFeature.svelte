<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';

    export interface Action { id: string; label: string; icon: string; disabled?: boolean }

    let {
        label,
        actions,
        activeColor = 'var(--screen-text-primary)',
        disabled = false,
        readOnly = false,
        onAction,
    }: {
        label: string;
        actions: Action[];
        activeColor?: string;
        disabled?: boolean;
        readOnly?: boolean;
        onAction?: (id: string) => void;
    } = $props();
</script>

<div class="tile-action-group" class:unavailable={disabled} role="group" aria-label={label} style={`--action-color: ${activeColor}`}>
    {#each actions as action (action.id)}
        <button type="button" aria-label={action.label} aria-disabled={disabled || readOnly || action.disabled} disabled={disabled || action.disabled} tabindex={disabled || readOnly || action.disabled ? -1 : 0} onclick={(event) => { event.stopPropagation(); if (!disabled && !readOnly && !action.disabled) onAction?.(action.id); }} onkeydown={(event) => event.stopPropagation()}><SvgIcon name={action.icon} /></button>
    {/each}
</div>

<style lang="scss">
    .tile-action-group { display: flex; width: 100%; min-width: 0; height: var(--feature-height, 2.625rem); gap: 0.2rem; }
    button { display: grid; min-width: 0; height: 100%; flex: 1; place-items: center; border: 0; border-radius: var(--feature-border-radius, 0.75rem); background: var(--screen-icon-unavailable-background); color: var(--action-color); cursor: pointer;padding:0 }
    button[aria-disabled='true'] { cursor: default; }
    button:disabled { color: var(--screen-icon-unavailable); cursor: not-allowed; }
    .tile-action-group.unavailable button { background: var(--screen-feature-disabled-background); color: var(--screen-feature-disabled-color); }
    button:focus-visible { outline: 0.125rem solid var(--screen-focus-outline); }
    button :global(svg) { width: 1.25rem; height: 1.25rem; }
</style>
