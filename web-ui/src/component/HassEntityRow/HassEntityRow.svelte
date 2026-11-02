<script lang="ts">
    import { locale } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { getHassEntityPresentation } from '@/hooks/useHassEntity';
    import { getHassEntityStateStore, haRegistryMetadataStore } from '@/stores/haRegistry';

    interface IHassEntityRowProps {
        entityId: string;
        state?: string;
        icon?: string | null;
        variant?: 'entity' | 'badge';
        fullWidth?: boolean;
        showIconBackground?: boolean;
        onclick?: () => void;
        interactiveElement?: 'button' | 'div';
    }

    let { entityId, state, icon, variant = 'entity', fullWidth = false, showIconBackground, onclick, interactiveElement = 'button' }: IHassEntityRowProps = $props();
    let entityStateStore = $derived(getHassEntityStateStore(entityId));
    const entity = $derived.by(() => {
        void $locale;
        return getHassEntityPresentation(entityId, {
            ...$haRegistryMetadataStore,
            states: $entityStateStore ? [$entityStateStore] : [],
        }, { state, icon, variant });
    });
    const iconBackgroundVisible = $derived(variant !== 'badge' && (showIconBackground ?? entity.defaultActions.iconTap !== 'nothing'));
    const badgeValue = $derived.by(() => {
        const value = entity.displayState;
        if (!entity.badgeUsesName || entity.isUnavailable) return value;
        const characters = Array.from(value);
        return characters.length > 10 ? `${characters.slice(0, 10).join('')}…` : value;
    });

    function handleInteractiveKeydown(event: KeyboardEvent) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        onclick?.();
    }
</script>

{#snippet content()}
    <span
        class:unavailable={entity.isUnavailable}
        class:no-icon-background={!iconBackgroundVisible}
        class="entity-icon tone-{entity.preview.tone}"
        style={entity.iconStyle}
    >
        <SvgIcon name={entity.icon} />
        {#if entity.climateActionIndicator}
            <span class="climate-action-indicator action-{entity.climateActionIndicator.tone}" aria-hidden="true">
                <SvgIcon name={entity.climateActionIndicator.icon} />
            </span>
        {/if}
        {#if entity.isUnavailable && variant !== 'badge'}<span class="entity-warning" aria-hidden="true">!</span>{/if}
    </span>
    {#if variant === 'badge'}
        <strong class:unavailable={entity.isUnavailable} class="badge-value">{badgeValue}</strong>
    {:else}
        <span class="entity-copy"><strong>{entity.displayName}</strong><small>{entity.displayState}</small></span>
    {/if}
{/snippet}

{#if onclick}
    {#if interactiveElement === 'button'}
        <button class:badge={variant === 'badge'} class:full-width={fullWidth} class="hass-entity-row clickable" type="button" {onclick}>{@render content()}</button>
    {:else}
        <div
            class:badge={variant === 'badge'}
            class:full-width={fullWidth}
            class="hass-entity-row clickable"
            role="button"
            tabindex="0"
            {onclick}
            onkeydown={handleInteractiveKeydown}
        >
            {@render content()}
        </div>
    {/if}
{:else}
    <div class:badge={variant === 'badge'} class:full-width={fullWidth} class="hass-entity-row">{@render content()}</div>
{/if}

<style>
    .hass-entity-row {
        display: flex;
        width: min(100%, 17.5rem);
        min-width: 0;
        margin-inline: auto;
        padding: 0.5rem 0.75rem;
        align-items: center;
        gap: 0.625rem;
        color: #f2f2f7;
        border: 0.0625rem solid rgba(84, 84, 88, 0.65);
        border-radius: 0.75rem;
        background: rgba(153, 153, 153, 0.2);
        font-family: MiSans, sans-serif;
        text-align: left;
    }
    .hass-entity-row.full-width {
        width: 100%;
        max-width: 100%;
        margin-inline: 0;
    }
    .hass-entity-row.clickable {
        cursor: pointer;
    }
    .hass-entity-row.clickable:hover,
    .hass-entity-row.clickable:focus-visible {
        outline: 0.125rem solid #249ed5;
        outline-offset: -0.125rem;
    }
    .hass-entity-row.badge {
        width: auto;
        margin-inline: 0;
        padding: 0.2rem 0.6rem 0.2rem 0.3rem;
        flex: 0 1 auto;
        gap: 0rem;
        border: 0;
        border-radius: 0.83625rem;
        background: rgb(29, 29, 30);
    }
    .hass-entity-row.badge .entity-icon {
        display: inline-grid;
        width: 1.45rem;
        height: 1.45rem;
        aspect-ratio: 1;
        border-radius: 50%;
        font-size: 1rem;
    }
    .badge-value {
        max-width: 8rem;
        overflow: hidden;
        font-size: 0.75rem;
        font-weight: 600;
        line-height: 1.125rem;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .badge-value.unavailable {
        color: #909090;
    }
    .entity-icon {
        position: relative;
        display: grid;
        width: 2.5rem;
        height: 2.5rem;
        flex: 0 0 auto;
        place-items: center;
        color: var(--entity-icon-color, #697079);
        border-radius: 50%;
        background-color: var(--entity-icon-background, #f2f2f7);
        font-size: 1.25rem;
        opacity: 1;
    }
    .entity-icon.tone-warm {
        --entity-icon-color: #ffc107;
        --entity-icon-background: rgba(255, 193, 7, 0.2);
    }
    .entity-icon.tone-positive {
        --entity-icon-color: #4caf50;
        --entity-icon-background: rgba(76, 175, 80, 0.2);
    }
    .entity-icon.tone-danger {
        --entity-icon-color: #f44336;
        --entity-icon-background: rgba(241, 82, 75, 0.18);
    }
    .entity-icon.tone-blue {
        --entity-icon-color: #2196f3;
        --entity-icon-background: rgba(68, 140, 203, 0.22);
    }
    .entity-icon.tone-cyan {
        --entity-icon-color: #00bcd4;
        --entity-icon-background: rgba(68, 140, 203, 0.22);
    }
    .entity-icon.tone-ha-orange {
        --entity-icon-color: #ff9800;
        --entity-icon-background: rgba(255, 152, 0, 0.2);
    }
    .entity-icon.tone-ha-deep-orange {
        --entity-icon-color: #ff6f22;
        --entity-icon-background: rgba(255, 111, 34, 0.2);
    }
    .entity-icon.tone-neutral {
        --entity-icon-color: #a8abb0;
        --entity-icon-background: rgba(168, 171, 176, 0.18);
    }
    .entity-icon.tone-ha-inactive {
        --entity-icon-color: #9e9e9e;
        --entity-icon-background: rgba(158, 158, 158, 0.18);
    }
    .entity-icon.tone-ha-cover {
        --entity-icon-color: #926bc7;
        --entity-icon-background: rgba(146, 107, 199, 0.2);
    }
    .entity-icon.tone-ha-fan {
        --entity-icon-color: #00bcd4;
        --entity-icon-background: rgba(0, 188, 212, 0.2);
    }
    .entity-icon.tone-ha-sensor {
        --entity-icon-color: #44739e;
        --entity-icon-background: rgba(68, 115, 158, 0.2);
    }
    .hass-entity-row.badge .entity-icon.tone-ha-sensor {
        --entity-icon-color: #009ac7;
    }
    .entity-icon.tone-ha-battery-high {
        --entity-icon-color: #4caf50;
        --entity-icon-background: rgba(76, 175, 80, 0.2);
    }
    .entity-icon.tone-ha-battery-medium {
        --entity-icon-color: #ff9800;
        --entity-icon-background: rgba(255, 152, 0, 0.2);
    }
    .entity-icon.tone-ha-battery-low {
        --entity-icon-color: #f44336;
        --entity-icon-background: rgba(244, 67, 54, 0.2);
    }
    .entity-icon.unavailable {
        --entity-icon-color: #bdbdbd;
        --entity-icon-background: rgba(142, 142, 147, 0.2);
    }
    :global([data-bs-theme='dark']) .entity-icon.unavailable {
        --entity-icon-color: #6f6f6f;
    }
    .entity-icon.no-icon-background {
        background-color: transparent;
    }
    .climate-action-indicator {
        position: absolute;
        top: 0.05rem;
        right: 0.05rem;
        display: grid;
        width: 1.125rem;
        height: 1.125rem;
        place-items: center;
        color: #fff;
        border-radius: 50%;
        background: var(--climate-action-color);
        font-size: 0.75rem;
    }
    .climate-action-indicator.action-heating,
    .climate-action-indicator.action-drying {
        --climate-action-color: #ff9800;
    }
    .climate-action-indicator.action-cooling {
        --climate-action-color: rgb(60, 135, 236);
    }
    .climate-action-indicator.action-fan {
        --climate-action-color: #00bcd4;
    }
    .entity-warning {
        position: absolute;
        top: -0.125rem;
        right: -0.125rem;
        display: grid;
        width: 0.875rem;
        aspect-ratio: 1;
        place-items: center;
        color: #fff;
        border-radius: 50%;
        background: #ff9f0a;
        font-family: Arial, sans-serif;
        font-size: 0.6875rem;
        font-weight: 700;
        line-height: 1;
    }
    .entity-copy {
        display: flex;
        min-width: 0;
        flex-direction: column;
    }
    .entity-copy strong,
    .entity-copy small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .entity-copy strong {
        font-family: MiSans, sans-serif;
        font-size: 1.25rem;
        font-style: normal;
        font-weight: 500;
        line-height: 1.75rem;
        letter-spacing: 0;
    }
    .entity-copy small {
        color: #a8abb0;
        font-family: MiSans, sans-serif;
        font-size: 1rem;
        font-style: normal;
        font-weight: 400;
        line-height: 1.5rem;
        letter-spacing: 0;
    }
</style>
