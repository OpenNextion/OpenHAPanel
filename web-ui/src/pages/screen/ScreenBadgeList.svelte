<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { dndzone, SHADOW_ITEM_MARKER_PROPERTY_NAME, type DndEvent, type Item as DndItem } from 'svelte-dnd-action';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import { MAX_SCREEN_BADGE_COUNT, shouldShowScreenEntityIconBackground } from '@/stores/screenConfig';
    import type { IHassScreenConfigBadge } from '@/types/screenConfig';
    import { prepareDraggedBadge } from './screenDragPreview';

    let {
        badges,
        editing,
        screenIndex,
        screenTitle,
        onAdd,
        onSettings,
        onReorder,
    }: {
        badges: IHassScreenConfigBadge[];
        editing: boolean;
        screenIndex: number;
        screenTitle: string;
        onAdd: () => void;
        onSettings: (entityId: string) => void;
        onReorder: (badges: IHassScreenConfigBadge[]) => void;
    } = $props();

    type TBadgeItem = DndItem & { id: string; value: IHassScreenConfigBadge };
    let dndItems = $state<TBadgeItem[] | null>(null);
    const items: TBadgeItem[] = $derived(dndItems ?? badges.map((badge) => ({ id: badge.data.entity, value: badge })));

    $effect(() => {
        if (!editing) dndItems = null;
    });

    function handleConsider(event: CustomEvent<DndEvent<TBadgeItem>>) {
        dndItems = event.detail.items;
    }

    function handleFinalize(event: CustomEvent<DndEvent<TBadgeItem>>) {
        onReorder(event.detail.items.filter((item) => !item[SHADOW_ITEM_MARKER_PROPERTY_NAME]).map((item) => item.value));
        dndItems = null;
    }
</script>

{#if editing || badges.length}
    <div class="screen-badges">
        {#if editing && badges.length < MAX_SCREEN_BADGE_COUNT}
            <button class="add-badge-button" type="button" onclick={onAdd}><SvgIcon name="mdi:plus" /> <span>{$_('page.screen.addBadge')}</span></button>
        {/if}
        <div
            class="screen-badge-items"
            role="list"
            aria-label={$_('page.screen.badgeList', { values: { name: screenTitle } })}
            use:dndzone={{
                items,
                type: 'screen-badges',
                dragDisabled: !editing,
                morphDisabled: true,
                dropFromOthersDisabled: true,
                dropTargetStyle: { outline: 'none' },
                delayTouchStart: 250,
                transformDraggedElement: prepareDraggedBadge,
            }}
            onconsider={handleConsider}
            onfinalize={handleFinalize}
        >
            {#each items as item (`${item.id}-${item[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
                {@const badge = item.value}
                <div class:sortable-item={editing} class="screen-badge-sort-item" role="listitem">
                    <HassEntityRow
                        variant="badge"
                        compactPreview
                        entityId={badge.data.entity}
                        showIconBackground={shouldShowScreenEntityIconBackground(badge.data.entity, badge.data.actionConfig)}
                        interactiveElement="div"
                        onclick={editing ? () => onSettings(badge.data.entity) : undefined}
                    />
                </div>
            {/each}
        </div>
    </div>
{/if}
