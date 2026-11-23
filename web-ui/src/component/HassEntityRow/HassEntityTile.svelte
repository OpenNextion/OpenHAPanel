<script lang="ts">
    import type { getHassEntityPresentation } from '@/hooks/useHassEntity';
    import type { IHassScreenConfigFeature } from '@/types/screenConfig';
    import type { TTileFeaturePosition } from '@/constant/tileFeaturePosition';
    import { shouldShowHassEntityIconBackground } from '@/constant/hassEntity';
    import HassEntityIcon from './HassEntityIcon.svelte';
    import TileFeatureRenderer from './TileFeatureRenderer.svelte';

    let {
        entityId,
        entity,
        fullWidth,
        showIconBackground,
        onclick,
        interactiveElement,
        features,
        featuresPosition,
        onFeatureToggle,
        onFeatureActivate,
        compactPreview,
    }: {
        entityId: string;
        entity: ReturnType<typeof getHassEntityPresentation>;
        fullWidth: boolean;
        showIconBackground?: boolean;
        onclick?: () => void;
        interactiveElement: 'button' | 'div';
        features: IHassScreenConfigFeature[];
        featuresPosition: TTileFeaturePosition;
        onFeatureToggle?: (checked: boolean) => void;
        onFeatureActivate?: () => void;
        compactPreview: boolean;
    } = $props();

    const iconBackgroundVisible = $derived(showIconBackground ?? shouldShowHassEntityIconBackground(entityId));

    function handleKeydown(event: KeyboardEvent) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        onclick?.();
    }

    function handleTileClick(event: MouseEvent) {
        if (!onclick) return;
        event.stopPropagation();
        onclick();
    }
</script>

{#snippet content()}
    <HassEntityIcon {entity} showBackground={iconBackgroundVisible} showWarning={true} />
    <span class="entity-copy entity-text-secondary"><span class="entity-name">{entity.displayName}</span><span class="entity-state">{entity.displayState}</span></span>
{/snippet}

{#if features.length}
    <div
        class:full-width={fullWidth}
        class:compact-preview={compactPreview}
        class:inline-features={featuresPosition === 'inline'}
        class:interactive={Boolean(onclick)}
        class="hass-entity-row feature-tile"
        role={onclick ? 'button' : undefined}
        tabindex={onclick ? 0 : undefined}
        onclick={handleTileClick}
        onkeydown={onclick ? handleKeydown : undefined}
    >
        <div class="tile-main-row">{@render content()}</div>
        <div class="tile-feature-slot">
            <TileFeatureRenderer {entityId} {entity} {features} {fullWidth} {featuresPosition} onToggle={onFeatureToggle} onActivate={onFeatureActivate} />
        </div>
    </div>
{:else if onclick && interactiveElement === 'button'}
    <button class:full-width={fullWidth} class:compact-preview={compactPreview} class="hass-entity-row clickable" type="button" {onclick}>{@render content()}</button>
{:else if onclick}
    <div class:full-width={fullWidth} class:compact-preview={compactPreview} class="hass-entity-row clickable" role="button" tabindex="0" {onclick} onkeydown={handleKeydown}>{@render content()}</div>
{:else}
    <div class:full-width={fullWidth} class:compact-preview={compactPreview} class="hass-entity-row">{@render content()}</div>
{/if}
