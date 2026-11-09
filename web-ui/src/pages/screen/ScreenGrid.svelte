<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { dragHandleZone, SHADOW_ITEM_MARKER_PROPERTY_NAME, type DndEvent, type Item as DndItem } from 'svelte-dnd-action';
    import type { IHassScreenConfigBadge, IHassScreenConfigCard, IHassScreenConfigItem } from '@/types/screenConfig';
    import { prepareDraggedScreenCard } from './screenDragPreview';
    import ScreenCard from './ScreenCard.svelte';

    let {
        screens,
        editing,
        isLandscape,
        isHaConfigured,
        previewTime,
        canAddScreen,
        getScreenTitle,
        onAddScreen,
        onRenameScreen,
        onAddBadge,
        onAddEntity,
        onBadgeSettings,
        onEntitySettings,
        onReorderScreens,
        onReorderBadges,
        onReorderCards,
    }: {
        screens: IHassScreenConfigItem[];
        editing: boolean;
        isLandscape: boolean;
        isHaConfigured: boolean;
        previewTime: string;
        canAddScreen: boolean;
        getScreenTitle: (screen: IHassScreenConfigItem) => string;
        onAddScreen: () => void;
        onRenameScreen: (screen: IHassScreenConfigItem) => void;
        onAddBadge: (screenIndex: number) => void;
        onAddEntity: (screenIndex: number) => void;
        onBadgeSettings: (screenIndex: number, entityId: string) => void;
        onEntitySettings: (screenIndex: number, entityId: string) => void;
        onReorderScreens: (screens: IHassScreenConfigItem[]) => void;
        onReorderBadges: (screenIndex: number, badges: IHassScreenConfigBadge[]) => void;
        onReorderCards: (screenIndex: number, cards: IHassScreenConfigCard[]) => void;
    } = $props();

    type TScreenItem = DndItem & { id: string; screen: IHassScreenConfigItem };
    let dndItems = $state<TScreenItem[] | null>(null);
    let draggedScreenId = $state<string | null>(null);
    const items: TScreenItem[] = $derived(dndItems ?? screens.map((screen) => ({ id: String(screen.index), screen })));
    const desktopGridColumns = $derived(isLandscape ? 2 : 3);

    $effect(() => {
        if (!editing) {
            dndItems = null;
            draggedScreenId = null;
        }
    });

    function handleConsider(event: CustomEvent<DndEvent<TScreenItem>>) {
        dndItems = event.detail.items;
        if (event.detail.info.trigger === 'dragStarted') {
            draggedScreenId = event.detail.info.id;
            if (window.matchMedia('(max-width: 52rem)').matches) window.scrollTo(0, 0);
        }
    }

    function handleFinalize(event: CustomEvent<DndEvent<TScreenItem>>) {
        onReorderScreens(event.detail.items
            .filter((item) => !item[SHADOW_ITEM_MARKER_PROPERTY_NAME])
            .map((item, index) => ({ ...item.screen, index })));
        dndItems = null;
        draggedScreenId = null;
    }
</script>

<div class:editing class:has-add-screen={canAddScreen} class="screen-grid-layout">
    <div
        class:sorting={draggedScreenId !== null}
        class="screen-grid"
        role="list"
        aria-label={$_('page.screen.title')}
        use:dragHandleZone={{
            items,
            type: 'screen-card-shell',
            dragDisabled: !editing,
            dropFromOthersDisabled: true,
            dropTargetStyle: { outline: 'none' },
            delayTouchStart: 250,
            transformDraggedElement: prepareDraggedScreenCard,
        }}
        onconsider={handleConsider}
        onfinalize={handleFinalize}
    >
        {#each items as item (`${item.id}-${item[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
            {@const screen = item.screen}
            <div
                class:dragging={draggedScreenId === String(screen.index)}
                class="screen-card-shell"
                data-is-dnd-shadow-item-hint={item[SHADOW_ITEM_MARKER_PROPERTY_NAME]}
            >
                <ScreenCard
                    {screen}
                    screenTitle={getScreenTitle(screen)}
                    {screens}
                    {editing}
                    {isHaConfigured}
                    {previewTime}
                    onRename={() => onRenameScreen(screen)}
                    onAddBadge={() => onAddBadge(screen.index)}
                    onAddEntity={() => onAddEntity(screen.index)}
                    onBadgeSettings={(entityId) => onBadgeSettings(screen.index, entityId)}
                    onEntitySettings={(entityId) => onEntitySettings(screen.index, entityId)}
                    onReorderBadges={(badges) => onReorderBadges(screen.index, badges)}
                    onReorderCards={(cards) => onReorderCards(screen.index, cards)}
                />
            </div>
        {/each}
    </div>

    {#if canAddScreen}
        <div
            class="add-screen-card"
            role="button"
            tabindex="0"
            style={`--add-screen-column: ${screens.length % desktopGridColumns}; --add-screen-row: ${Math.floor(screens.length / desktopGridColumns)}`}
            onclick={onAddScreen}
            onkeydown={(event) => {
                if (event.key === 'Enter' || event.key === ' ') {
                    event.preventDefault();
                    onAddScreen();
                }
            }}
        >
            <span class="add-screen-icon" aria-hidden="true">+</span><span>{$_('page.screen.addSplit')}</span>
        </div>
    {/if}
</div>
