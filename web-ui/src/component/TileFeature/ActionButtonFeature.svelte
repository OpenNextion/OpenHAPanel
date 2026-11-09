<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';

    interface Props {
        /** Visible and accessible label. Translation belongs to the caller. */
        label: string;
        icon?: string;
        backgroundColor?: string;
        textColor?: string;
        disabled?: boolean;
        readOnly?: boolean;
        busy?: boolean;
        onActivate: () => void;
    }

    let {
        label,
        icon,
        backgroundColor = 'var(--screen-tone-ha-inactive-background)',
        textColor = 'var(--screen-text-primary)',
        disabled = false,
        readOnly = false,
        busy = false,
        onActivate,
    }: Props = $props();
</script>

<button
    class="tile-action-feature"
    type="button"
    aria-busy={busy}
    aria-disabled={readOnly || disabled || busy}
    disabled={disabled || busy}
    tabindex={readOnly ? -1 : 0}
    style={`--action-background: ${backgroundColor}; --action-color: ${textColor}`}
    onpointerdown={(event) => event.stopPropagation()}
    onkeydown={(event) => event.stopPropagation()}
    onclick={(event) => { event.stopPropagation(); if (!readOnly) onActivate(); }}
>
    {#if busy}<span class="action-spinner" aria-hidden="true"></span>{:else if icon}<SvgIcon name={icon} />{/if}
    <span>{label}</span>
</button>

<style lang="scss">
    .tile-action-feature {
        display: flex;
        width: 100%;
        min-height: var(--feature-height, 2.625rem);
        padding: var(--feature-control-padding, 0.5rem 0.75rem);
        align-items: center;
        justify-content: center;
        gap: var(--feature-control-gap, 0.5rem);
        border: 0;
        border-radius: var(--feature-border-radius, 0.75rem);
        background: var(--action-background);
        color: var(--action-color);
        font: inherit;
        font-weight: 600;
        line-height: 1.25;
        cursor: pointer;
        touch-action: manipulation;
    }

    .tile-action-feature:focus-visible {
        outline: 0.125rem solid var(--screen-focus-outline);
        outline-offset: 0.125rem;
    }

    .tile-action-feature:disabled {
        cursor: not-allowed;
        opacity: 0.5;
    }

    .tile-action-feature[aria-disabled='true']:not(:disabled) {
        cursor: default;
    }

    .tile-action-feature :global(svg) {
        width: var(--feature-icon-size, 1.25rem);
        height: var(--feature-icon-size, 1.25rem);
    }

    .action-spinner {
        width: var(--feature-icon-size, 1.125rem);
        height: var(--feature-icon-size, 1.125rem);
        border: 0.125rem solid currentColor;
        border-right-color: transparent;
        border-radius: 50%;
        animation: action-spin 800ms linear infinite;
    }

    @keyframes action-spin {
        to { transform: rotate(360deg); }
    }

    @media (prefers-reduced-motion: reduce) {
        .action-spinner { animation-duration: 2s; }
    }
</style>
