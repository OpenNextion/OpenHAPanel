<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { dndzone, SHADOW_ITEM_MARKER_PROPERTY_NAME, type DndEvent, type Item as DndItem } from 'svelte-dnd-action';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import { getRenderableTileFeatures } from '@/constant/tileFeatureRules';
    import { getTileFeaturePosition } from '@/constant/tileFeaturePosition';
    import { haRegistryStore } from '@/stores/haRegistry';
    import { shouldShowScreenEntityIconBackground } from '@/stores/screenConfig';
    import type { IHassStateEntry } from '@/types/haRegistry';
    import type { IHassScreenConfigCard } from '@/types/screenConfig';
    import { prepareDraggedEntity } from './screenDragPreview';

    let {
        cards,
        editing,
        screenTitle,
        onAdd,
        onSettings,
        onReorder,
    }: {
        cards: IHassScreenConfigCard[];
        editing: boolean;
        screenTitle: string;
        onAdd: () => void;
        onSettings: (entityId: string) => void;
        onReorder: (cards: IHassScreenConfigCard[]) => void;
    } = $props();

    type TCardItem = DndItem & { id: string; value: IHassScreenConfigCard };
    let dndItems = $state<TCardItem[] | null>(null);
    const items: TCardItem[] = $derived(dndItems ?? cards.map((card) => ({ id: card.data.entity, value: card })));

    $effect(() => {
        if (!editing) dndItems = null;
    });

    function handleConsider(event: CustomEvent<DndEvent<TCardItem>>) {
        dndItems = event.detail.items;
    }

    function handleFinalize(event: CustomEvent<DndEvent<TCardItem>>) {
        onReorder(event.detail.items.filter((item) => !item[SHADOW_ITEM_MARKER_PROPERTY_NAME]).map((item) => item.value));
        dndItems = null;
    }

    function getEntityRowSpan(card: IHassScreenConfigCard, state: IHassStateEntry | undefined) {
        const features = card.data.features ?? [];
        const renderedCount = getRenderableTileFeatures(card.data.entity, state, features).length;
        if (!renderedCount) return 1;
        return getTileFeaturePosition(card.data.features_position) === 'inline'
            ? Math.ceil((renderedCount + 1) / 2)
            : renderedCount + 1;
    }

</script>

{#if cards.length || editing}
    <div class="screen-entities-layout">
        {#if editing}
            <button class="add-entity-button" type="button" onclick={onAdd}>
                <SvgIcon name="mdi:plus" /> <span>{$_('page.screen.addEntity')}</span>
            </button>
        {/if}
        {#if cards.length}
            <div
                class="screen-entities"
                role="list"
                aria-label={$_('page.screen.entityList', { values: { name: screenTitle } })}
                use:dndzone={{
                    items,
                    type: 'screen-entities',
                    dragDisabled: !editing,
                    dropFromOthersDisabled: true,
                    dropTargetStyle: { outline: 'none' },
                    delayTouchStart: true,
                    centreDraggedOnCursor: true,
                    useCursorForDetection: true,
                    transformDraggedElement: prepareDraggedEntity,
                }}
                onconsider={handleConsider}
                onfinalize={handleFinalize}
            >
                {#each items as item (`${item.id}-${item[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
                    {@const card = item.value}
                    {@const rowSpan = getEntityRowSpan(card, $haRegistryStore.statesById.get(card.data.entity))}
                    <div
                        class:sortable-item={editing}
                        class:full-width={card.data.full_width}
                        class:multi-row={rowSpan > 1}
                        class="screen-entity-sort-item"
                        style:--screen-entity-row-span={rowSpan}
                        role="listitem"
                    >
                        <HassEntityRow
                            entityId={card.data.entity}
                            compactPreview
                            fullWidth={card.data.full_width}
                            showIconBackground={shouldShowScreenEntityIconBackground(card.data.entity, card.data.actionConfig)}
                            features={card.data.features ?? []}
                            featuresPosition={getTileFeaturePosition(card.data.features_position)}
                            onFeatureToggle={editing ? () => onSettings(card.data.entity) : undefined}
                            onFeatureActivate={editing ? () => onSettings(card.data.entity) : undefined}
                            interactiveElement="div"
                            onclick={editing ? () => onSettings(card.data.entity) : undefined}
                        />
                    </div>
                {/each}
            </div>
        {/if}
    </div>
{/if}
