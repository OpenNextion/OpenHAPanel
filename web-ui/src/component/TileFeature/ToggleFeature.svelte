<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';

    interface Props {
        /** Controlled state. The caller decides how it maps to an entity. */
        checked: boolean;
        /** Accessible name, for example the entity name or a translated action label. */
        label: string;
        /** Already resolved icon for the current entity and state. */
        icon: string;
        /** Colors are supplied by the caller; this component has no entity-specific palette. */
        trackColor?: string;
        activeColor?: string;
        inactiveColor?: string;
        activeIconColor?: string;
        inactiveIconColor?: string;
        disabled?: boolean;
        busy?: boolean;
        readOnly?: boolean;
        onToggle?: (nextChecked: boolean) => void;
    }

    let {
        checked,
        label,
        icon,
        trackColor = 'var(--screen-tile-background)',
        activeColor = 'var(--screen-focus-outline)',
        inactiveColor = 'var(--screen-icon-unavailable-background)',
        activeIconColor = 'var(--screen-text-primary)',
        inactiveIconColor = 'var(--screen-text-secondary)',
        disabled = false,
        busy = false,
        readOnly = false,
        onToggle,
    }: Props = $props();
</script>

<button
    class="tile-toggle-feature"
    type="button"
    aria-label={label}
    aria-pressed={checked}
    aria-busy={busy}
    aria-disabled={readOnly || disabled || busy}
    disabled={disabled || busy}
    tabindex={readOnly ? -1 : 0}
    style={`--toggle-track-color: ${trackColor}; --toggle-active-color: ${activeColor}; --toggle-inactive-color: ${inactiveColor}; --toggle-active-icon-color: ${activeIconColor}; --toggle-inactive-icon-color: ${inactiveIconColor}`}
    onpointerdown={(event) => event.stopPropagation()}
    onkeydown={(event) => event.stopPropagation()}
    onclick={(event) => { event.stopPropagation(); if (!readOnly) onToggle?.(!checked); }}
>
    <span class:checked class="toggle-thumb" aria-hidden="true"><SvgIcon name={icon} /></span>
</button>

<style lang="scss">
    .tile-toggle-feature {
        position: relative;
        display: block;
        width: 100%;
        height: var(--feature-height, 2.625rem);
        padding: 0;
        overflow: hidden;
        border: 0;
        border-radius: var(--feature-border-radius, 0.75rem);
        background: var(--toggle-track-color);
        cursor: pointer;
        touch-action: manipulation;
    }

    .tile-toggle-feature:focus-visible {
        outline: 0.125rem solid var(--screen-focus-outline);
        outline-offset: 0.125rem;
    }

    .tile-toggle-feature:disabled {
        cursor: not-allowed;
        opacity: 0.5;
    }

    .tile-toggle-feature[aria-disabled='true']:not(:disabled) {
        cursor: default;
    }

    .toggle-thumb {
        position: absolute;
        top: 0;
        left: 0;
        display: grid;
        width: 50%;
        height: 100%;
        place-items: center;
        border-radius: inherit;
        background: var(--toggle-inactive-color);
        color: var(--toggle-inactive-icon-color);
        font-size: var(--feature-icon-size, 1.375rem);
        transition: transform 160ms ease, background-color 160ms ease, color 160ms ease;
    }

    .toggle-thumb.checked {
        background: var(--toggle-active-color);
        color: var(--toggle-active-icon-color);
        transform: translateX(100%);
    }

    .toggle-thumb :global(svg) {
        width: var(--feature-icon-size, 1.375rem);
        height: var(--feature-icon-size, 1.375rem);
    }

    @media (prefers-reduced-motion: reduce) {
        .toggle-thumb { transition: none; }
    }
</style>
