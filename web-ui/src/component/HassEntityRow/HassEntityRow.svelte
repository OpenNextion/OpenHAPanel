<script lang="ts">
    import './HassEntityRow.scss';
    import { locale } from 'svelte-i18n';
    import { getHassEntityPresentation, hassEntityRelativeTimeClock, usesHassEntityRelativeTime } from '@/hooks/useHassEntity';
    import { getRenderableTileFeatures } from '@/constant/tileFeatureRules';
    import type { TTileFeaturePosition } from '@/constant/tileFeaturePosition';
    import { getHassEntityStateStore, haRegistryMetadataStore } from '@/stores/haRegistry';
    import type { IHassScreenConfigFeature } from '@/types/screenConfig';
    import HassEntityBadge from './HassEntityBadge.svelte';
    import HassEntityTile from './HassEntityTile.svelte';

    interface IHassEntityRowProps {
        entityId: string;
        state?: string;
        icon?: string | null;
        variant?: 'entity' | 'badge';
        fullWidth?: boolean;
        showIconBackground?: boolean;
        onclick?: () => void;
        interactiveElement?: 'button' | 'div';
        features?: IHassScreenConfigFeature[];
        featuresPosition?: TTileFeaturePosition;
        onFeatureToggle?: (checked: boolean) => void;
        onFeatureActivate?: () => void;
        compactPreview?: boolean;
    }

    let { entityId, state: entityState, icon, variant = 'entity', fullWidth = false, showIconBackground, onclick, interactiveElement = 'button', features = [], featuresPosition = 'bottom', onFeatureToggle, onFeatureActivate, compactPreview = false }: IHassEntityRowProps = $props();
    let entityStateStore = $derived(getHassEntityStateStore(entityId));
    let relativeTimeNow = $state(Date.now());
    $effect(() => {
        const stateObject = $entityStateStore;
        const displayStateObject = stateObject && entityState !== undefined ? { ...stateObject, state: entityState } : stateObject;
        if (!usesHassEntityRelativeTime(entityId, displayStateObject, variant)) return;
        return hassEntityRelativeTimeClock.subscribe((now) => {
            relativeTimeNow = now;
        });
    });
    const entity = $derived.by(() => {
        void $locale;
        void relativeTimeNow;
        return getHassEntityPresentation(entityId, {
            ...$haRegistryMetadataStore,
            states: $entityStateStore ? [$entityStateStore] : [],
        }, { state: entityState, icon, variant });
    });
    const supportedFeatures = $derived(getRenderableTileFeatures(entityId, $entityStateStore, features));
</script>

{#if variant === 'badge'}
    <HassEntityBadge {entity} {onclick} {interactiveElement} {compactPreview} />
{:else}
    <HassEntityTile {entityId} {entity} {fullWidth} {showIconBackground} {onclick} {interactiveElement} features={supportedFeatures} {featuresPosition} {onFeatureToggle} {onFeatureActivate} {compactPreview} />
{/if}
