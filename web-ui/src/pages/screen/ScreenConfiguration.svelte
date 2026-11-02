<script lang="ts">
    import { cloneDeep } from 'lodash-es';
    import dayjs from 'dayjs';
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import {
        dndzone,
        dragHandle,
        dragHandleZone,
        SHADOW_ITEM_MARKER_PROPERTY_NAME,
        type DndEvent,
        type Item as DndItem,
    } from 'svelte-dnd-action';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import EntitySettingsModal from './EntitySettingsModal.svelte';
    import EntitySelectionModal from './EntitySelectionModal.svelte';
    import type { TEntityPickerMode } from '@/hooks/useScreenConfiguration';
    import { EMessageType, sendMessage } from '@/stores/message';
    import {
        MAX_SCREEN_BADGE_COUNT,
        removeScreenConfig,
        screenConfigStore,
        shouldShowScreenEntityIconBackground,
        updateScreenConfigs,
        updateScreenTitle,
    } from '@/stores/screenConfig';
    import type { IHassScreenConfigBadge, IHassScreenConfigCard, IHassScreenConfigItem } from '@/types/screenConfig';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';
    import { REG_EXP_CHINESE_CHARACTERS } from '@/constant/regRex';
    import { deviceStore } from '@/stores/device';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { finialOffset, timerStore } from '@/stores/timer';

    let editing = $state(false);
    let saving = $state(false);
    let entityModalVisible = $state(false);
    let pickerMode = $state<TEntityPickerMode>('entity');
    let activeScreenIndex = $state<number | null>(null);
    let entitySettingsVisible = $state(false);
    let settingsMode = $state<TEntityPickerMode>('entity');
    let selectedEntityScreenIndex = $state<number | null>(null);
    let selectedEntityId = $state('');
    let screenSettingsVisible = $state(false);
    let editingScreenIndex = $state<number | null>(null);
    let screenTitleCache = $state('');
    let addScreenModalVisible = $state(false);
    let newScreenTitle = $state('');
    let leaveConfirmVisible = $state(false);
    let pendingNavigationHash = $state('');
    let editingSnapshot: IHassScreenConfigItem[] | null = null;
    let dndItemsByZone = $state<Record<string, TDndScreenItem[]>>({});
    let dndScreenItems = $state<TDndScreenConfigItem[] | null>(null);
    let draggedScreenId = $state<string | null>(null);
    let mobilePreviewScale = $state(1);
    let currentRouteHash = window.location.hash;
    let allowNextNavigation = false;
    const MAX_SCREEN_COUNT = 5;
    const MAX_SCREEN_TITLE_LENGTH = 20;

    type TSortableScreenItem = IHassScreenConfigBadge | IHassScreenConfigCard;
    type TDndScreenItem = DndItem & { id: string; value: TSortableScreenItem };
    type TDndScreenConfigItem = DndItem & { id: string; screen: IHassScreenConfigItem };

    type TScreenTitleMessages = { page: { screen: { defaultSplit: string } } };
    const defaultScreenTranslations = import.meta.glob<TScreenTitleMessages>('../../assets/i18n/*.json', { eager: true, import: 'default' });
    const DEFAULT_SCREEN_TITLE_ALIASES = new Set([
        'default',
        ...Object.values(defaultScreenTranslations).map((messages) => messages.page.screen.defaultSplit),
    ]);

    const DEFAULT_SCREEN: IHassScreenConfigItem = $derived({
        index: 0,
        title: $_('page.screen.defaultSplit'),
        icon: '',
        type: 'sections',
        columns: 1,
        badges: [],
        cards: [],
        features: [],
    });

    const currentTimeInMs = $derived(Number($timerStore.timers.currentTime?.value ?? ($deviceStore.currentTime || dayjs().valueOf())));
    const previewTime = $derived(dayjs.utc(currentTimeInMs).utcOffset($finialOffset).format('HH:mm'));
    const isLandscape = $derived($deviceStore.screenRotation.rotation === 90);
    const isHaConfigured = $derived($homeAssistantStore.connectionInfo?.configured === true);
    const desktopGridColumns = $derived(isLandscape ? 2 : 3);
    const displayedScreens = $derived.by(() => {
        if (editing) return $screenConfigStore.screens;
        return $screenConfigStore.screens.length ? $screenConfigStore.screens : [DEFAULT_SCREEN];
    });
    function getScreenTitle(screen: IHassScreenConfigItem) {
        return screen.index === 0 && DEFAULT_SCREEN_TITLE_ALIASES.has(screen.title) ? $_('page.screen.defaultSplit') : screen.title;
    }

    function isConfigured(screen: IHassScreenConfigItem) {
        return Boolean(screen.badges?.length || screen.cards?.length || screen.features?.length);
    }

    function getDndZoneKey(screenIndex: number, mode: 'badge' | 'entity') {
        return `${screenIndex}:${mode}`;
    }

    function getDndItems<T extends TSortableScreenItem>(
        screenIndex: number,
        mode: 'badge' | 'entity',
        items: T[],
    ): (DndItem & { id: string; value: T })[] {
        const key = getDndZoneKey(screenIndex, mode);
        const current = dndItemsByZone[key];
        if (current) return current as (DndItem & { id: string; value: T })[];
        return items.map((item) => ({ id: item.data.entity, value: item })) as (DndItem & { id: string; value: T })[];
    }

    function handleDndConsider(event: CustomEvent<DndEvent<TDndScreenItem>>, screenIndex: number, mode: 'badge' | 'entity') {
        dndItemsByZone[getDndZoneKey(screenIndex, mode)] = event.detail.items;
    }

    function handleDndFinalize(event: CustomEvent<DndEvent<TDndScreenItem>>, screenIndex: number, mode: 'badge' | 'entity') {
        const key = getDndZoneKey(screenIndex, mode);
        const orderedItems = event.detail.items
            .filter((item) => !item[SHADOW_ITEM_MARKER_PROPERTY_NAME])
            .map((item) => item.value);

        updateScreenConfigs(
            $screenConfigStore.screens.map((screen) => {
                if (screen.index !== screenIndex) return screen;
                return mode === 'badge'
                    ? { ...screen, badges: orderedItems as IHassScreenConfigBadge[] }
                    : { ...screen, cards: orderedItems as IHassScreenConfigCard[] };
            }),
        );
        delete dndItemsByZone[key];
    }

    function getDndScreenItems(screens: IHassScreenConfigItem[]): TDndScreenConfigItem[] {
        return dndScreenItems ?? screens.map((screen) => ({ id: String(screen.index), screen }));
    }

    function handleScreenDndConsider(event: CustomEvent<DndEvent<TDndScreenConfigItem>>) {
        dndScreenItems = event.detail.items;
        if (event.detail.info.trigger === 'dragStarted') {
            draggedScreenId = event.detail.info.id;
            if (window.matchMedia('(max-width: 52rem)').matches) {
                window.scrollTo(0, 0);
            }
        }
    }

    function handleScreenDndFinalize(event: CustomEvent<DndEvent<TDndScreenConfigItem>>) {
        const orderedScreens = event.detail.items
            .filter((item) => !item[SHADOW_ITEM_MARKER_PROPERTY_NAME])
            .map((item, index) => ({ ...item.screen, index }));
        updateScreenConfigs(orderedScreens);
        dndScreenItems = null;
        draggedScreenId = null;
    }

    function prepareDraggedScreenCard(element?: HTMLElement) {
        if (!element) return;
        element.classList.add('screen-card-dragged-preview');
        element.querySelector<HTMLElement>('.screen-card')?.classList.add('screen-card-dragged-content-preview');

        // The library moves its preview to document.body, so it no longer inherits
        // sizing variables defined on .screen-config-page.
        const page = document.querySelector<HTMLElement>('.screen-config-page');
        if (!page) return;
        const pageStyles = getComputedStyle(page);
        for (const variable of [
            '--screen-preview-scale',
            '--screen-preview-width',
            '--screen-preview-height',
            '--screen-card-width',
            '--screen-card-height',
            '--screen-grid-columns',
        ]) {
            const value = pageStyles.getPropertyValue(variable).trim();
            if (value) element.style.setProperty(variable, value);
        }
    }

    function prepareDraggedBadge(element?: HTMLElement) {
        if (!element) return;
        element.classList.add('screen-badge-dragged-preview');
        // The original device preview is CSS-scaled on desktop, but the drag clone is
        // moved to the document root. Its fixed width is therefore too small for the
        // unscaled badge contents and makes the badge flex-shrink while dragging.
        element.style.setProperty('width', 'max-content', 'important');
        const scale = setDraggedItemPreviewScale(element);
        const row = element.querySelector<HTMLElement>('.hass-entity-row');
        if (!row) return;
        row.classList.add('screen-item-dragged-content-preview');
        row.style.transform = `scale(${scale * 1.2})`;
    }

    function prepareDraggedEntity(element?: HTMLElement) {
        if (!element) return;
        element.classList.add('screen-entity-dragged-preview');
        const scale = setDraggedItemPreviewScale(element);
        const row = element.querySelector<HTMLElement>('.hass-entity-row');
        if (!row) return;
        const rootFontSize = Number.parseFloat(getComputedStyle(document.documentElement).fontSize) || 16;
        row.style.width = `${element.getBoundingClientRect().width / scale / rootFontSize}rem`;
        row.style.maxWidth = 'none';
        row.classList.add('screen-item-dragged-content-preview');
        row.style.transform = `scale(${scale * 1.1})`;
    }

    function setDraggedItemPreviewScale(element: HTMLElement) {
        const scale = getComputedStyle(document.querySelector<HTMLElement>('.screen-config-page') ?? document.documentElement)
            .getPropertyValue('--screen-preview-scale')
            .trim() || '1';
        element.style.setProperty('--screen-preview-scale', scale);
        return Number(scale) || 1;
    }

    function updateMobilePreviewScale(page: HTMLElement) {
        if (!window.matchMedia('(max-width: 52rem)').matches) {
            mobilePreviewScale = 1;
            return;
        }
        const rootFontSize = Number.parseFloat(getComputedStyle(document.documentElement).fontSize) || 16;
        const availableWidth = Math.max(0, page.clientWidth - rootFontSize * 1.5);
        const previewWidthInRem = isLandscape ? 30 : 20.1875;
        mobilePreviewScale = Math.min(1, availableWidth / (rootFontSize * previewWidthInRem));
    }

    function startEditing() {
        if (!isHaConfigured) {
            sendMessage(EMessageType.WARN, $_('page.screen.connectHaFirst'));
            return;
        }
        dndItemsByZone = {};
        dndScreenItems = null;
        draggedScreenId = null;
        editingSnapshot = cloneDeep($screenConfigStore.screens);
        if (!$screenConfigStore.screens.length) {
            updateScreenConfigs([DEFAULT_SCREEN]);
        }
        editing = true;
    }

    function openEntityPicker(mode: TEntityPickerMode, screenIndex: number) {
        if (
            mode === 'badge' &&
            ($screenConfigStore.screens.find((screen) => screen.index === screenIndex)?.badges?.length ?? 0) >= MAX_SCREEN_BADGE_COUNT
        )
            return;
        pickerMode = mode;
        activeScreenIndex = screenIndex;
        entityModalVisible = true;
    }

    function closeEntityPicker() {
        entityModalVisible = false;
        activeScreenIndex = null;
    }

    function openEntitySettings(mode: TEntityPickerMode, screenIndex: number, entityId: string) {
        settingsMode = mode;
        selectedEntityScreenIndex = screenIndex;
        selectedEntityId = entityId;
        entitySettingsVisible = true;
    }

    function closeEntitySettings() {
        entitySettingsVisible = false;
        selectedEntityScreenIndex = null;
        selectedEntityId = '';
    }

    function openAddScreenModal() {
        if ($screenConfigStore.screens.length >= MAX_SCREEN_COUNT) return;
        newScreenTitle = '';
        addScreenModalVisible = true;
    }

    function closeAddScreenModal() {
        addScreenModalVisible = false;
        newScreenTitle = '';
    }

    function updateNewScreenTitle(value: string) {
        newScreenTitle = value.replace(REG_EXP_CHINESE_CHARACTERS, '').slice(0, MAX_SCREEN_TITLE_LENGTH);
    }

    function addScreen() {
        const screens = $screenConfigStore.screens;
        const title = newScreenTitle.replace(REG_EXP_CHINESE_CHARACTERS, '').trim();
        if (screens.length >= MAX_SCREEN_COUNT || !title) return;
        const nextIndex = screens.reduce((max, screen) => Math.max(max, screen.index), -1) + 1;
        updateScreenConfigs([
            ...screens,
            {
                index: nextIndex,
                title,
                icon: '',
                type: 'sections',
                columns: 1,
                badges: [],
                cards: [],
                features: [],
            },
        ]);
        closeAddScreenModal();
    }

    function openScreenSettings(screen: IHassScreenConfigItem) {
        editingScreenIndex = screen.index;
        screenTitleCache = getScreenTitle(screen).slice(0, MAX_SCREEN_TITLE_LENGTH);
        screenSettingsVisible = true;
    }

    function closeScreenSettings() {
        screenSettingsVisible = false;
        editingScreenIndex = null;
        screenTitleCache = '';
    }

    function saveScreenSettings() {
        const title = screenTitleCache.replace(REG_EXP_CHINESE_CHARACTERS, '').trim();
        if (editingScreenIndex === null || !title) return;
        updateScreenTitle(editingScreenIndex, title);
        closeScreenSettings();
    }

    function updateScreenTitleCache(value: string) {
        screenTitleCache = value.replace(REG_EXP_CHINESE_CHARACTERS, '').slice(0, MAX_SCREEN_TITLE_LENGTH);
    }

    function deleteScreen() {
        if (editingScreenIndex === null || $screenConfigStore.screens.length <= 1) return;
        removeScreenConfig(editingScreenIndex);
        closeScreenSettings();
    }

    async function finishEditing() {
        if (saving) return;
        saving = true;
        try {
            const screens: IHassScreenConfigItem[] = $screenConfigStore.screens.map((screen) => ({ ...screen, columns: isLandscape ? 2 : 1 }));
            const response = await sendForReply<{ screens: IHassScreenConfigItem[] }>({
                method: EWsMethods.SET_SCREEN_SETTING,
                data: { screens },
            });
            if (response.error === 0) {
                updateScreenConfigs(screens);
                editing = false;
                editingSnapshot = null;
                dndItemsByZone = {};
                dndScreenItems = null;
                draggedScreenId = null;
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
            } else {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
            }
        } catch (error) {
            console.error('Failed to save screen configuration:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            saving = false;
        }
    }

    function requestNavigation(hash: string) {
        if (!hash || hash === currentRouteHash) return;
        pendingNavigationHash = hash;
        leaveConfirmVisible = true;
    }

    function continueEditing() {
        leaveConfirmVisible = false;
        pendingNavigationHash = '';
    }

    function discardAndLeave() {
        if (editingSnapshot) updateScreenConfigs(editingSnapshot);
        editingSnapshot = null;
        dndItemsByZone = {};
        dndScreenItems = null;
        draggedScreenId = null;
        editing = false;
        leaveConfirmVisible = false;
        allowNextNavigation = true;
        const targetHash = pendingNavigationHash;
        pendingNavigationHash = '';
        if (targetHash) window.location.hash = targetHash;
    }

    onMount(() => {
        const page = document.querySelector<HTMLElement>('.screen-config-page');
        const resizeObserver = page ? new ResizeObserver(() => updateMobilePreviewScale(page)) : null;
        if (page && resizeObserver) {
            resizeObserver.observe(page);
            updateMobilePreviewScale(page);
        }
        const restoreCurrentRoute = () => {
            window.history.replaceState(window.history.state, '', `${window.location.pathname}${window.location.search}${currentRouteHash}`);
        };
        const handleRouteLink = (event: MouseEvent) => {
            if (!editing || allowNextNavigation || event.defaultPrevented || event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
            const anchor = (event.target as Element | null)?.closest('a');
            if (!anchor) return;
            const target = new URL(anchor.href, window.location.href);
            if (target.origin !== window.location.origin || !target.hash.startsWith('#/') || target.hash === currentRouteHash) return;
            event.preventDefault();
            event.stopImmediatePropagation();
            requestNavigation(target.hash);
        };
        const handleRouteChange = () => {
            const nextHash = window.location.hash;
            if (allowNextNavigation || !editing) {
                currentRouteHash = nextHash;
                allowNextNavigation = false;
                return;
            }
            if (nextHash === currentRouteHash) return;
            restoreCurrentRoute();
            requestNavigation(nextHash);
        };
        const handleBeforeUnload = (event: BeforeUnloadEvent) => {
            if (!editing) return;
            event.preventDefault();
            event.returnValue = '';
        };

        document.addEventListener('click', handleRouteLink, true);
        window.addEventListener('popstate', handleRouteChange, true);
        window.addEventListener('hashchange', handleRouteChange, true);
        window.addEventListener('beforeunload', handleBeforeUnload);
        return () => {
            resizeObserver?.disconnect();
            document.removeEventListener('click', handleRouteLink, true);
            window.removeEventListener('popstate', handleRouteChange, true);
            window.removeEventListener('hashchange', handleRouteChange, true);
            window.removeEventListener('beforeunload', handleBeforeUnload);
        };
    });

    $effect(() => {
        const page = document.querySelector<HTMLElement>('.screen-config-page');
        if (page) updateMobilePreviewScale(page);
    });
</script>

<div
    class:landscape={isLandscape}
    class="page-container screen-config-page"
    style={`--mobile-preview-scale: ${mobilePreviewScale}`}
>
    <header class="screen-config-header">
        <div class="page-heading">
            <h1>{$_('page.screen.title')}</h1>
            <p class="text-body-secondary mb-0">{$_('page.screen.subtitle')}</p>
        </div>
        <button class="screen-edit-button" type="button" disabled={saving} onclick={editing ? finishEditing : startEditing}>
            {#if saving}<span class="spinner-border spinner-border-sm" aria-hidden="true"></span>{/if}
            {$_(editing ? 'page.screen.done' : 'page.screen.edit')}
        </button>
    </header>

    <div class:editing class:has-add-screen={editing && $screenConfigStore.screens.length < MAX_SCREEN_COUNT} class="screen-grid-layout">
        <div
            class:sorting={draggedScreenId !== null}
            class="screen-grid"
            role="list"
            aria-label={$_('page.screen.title')}
            use:dragHandleZone={{
                items: getDndScreenItems(displayedScreens),
                type: 'screen-card-shell',
                dragDisabled: !editing,
                dropFromOthersDisabled: true,
                dropTargetStyle: { outline: 'none' },
                delayTouchStart: 250,
                transformDraggedElement: prepareDraggedScreenCard,
            }}
            onconsider={handleScreenDndConsider}
            onfinalize={handleScreenDndFinalize}
        >
        {#each getDndScreenItems(displayedScreens) as screenItem (`${screenItem.id}-${screenItem[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
            {@const screen = screenItem.screen}
            <div
                class:dragging={draggedScreenId === String(screen.index)}
                class="screen-card-shell"
                data-is-dnd-shadow-item-hint={screenItem[SHADOW_ITEM_MARKER_PROPERTY_NAME]}
            >
                <article class="screen-card">
                <div class:editing class="screen-card-title" use:dragHandle aria-label={$_('page.screen.reorderScreen', { values: { name: getScreenTitle(screen) } })}>
                    <div class="screen-card-title-main">
                        <span>{getScreenTitle(screen)}</span>
                        {#if editing}
                            <button
                                type="button"
                                aria-label={$_('page.screen.rename')}
                                onmousedown={(event) => event.stopPropagation()}
                                ontouchstart={(event) => event.stopPropagation()}
                                onkeydown={(event) => event.stopPropagation()}
                                onclick={() => openScreenSettings(screen)}
                            ><SvgIcon name="edit.svg" /></button>
                        {/if}
                    </div>
                    <div aria-hidden="true">
                        {#if editing}
                            <span class="screen-card-drag-handle">
                                <SvgIcon name="sort.svg" />
                            </span>
                        {/if}
                    </div>
                </div>
                <div class="device-preview-frame">
                        <div class="device-preview" style={`--screen-columns: ${isLandscape ? 2 : 1}`}>
                        <div class="device-status">
                            <strong>{getScreenTitle(screen)}</strong><span class="device-status-icons"
                                ><SvgIcon name="ha.svg" /><SvgIcon name="wifi_4.svg" /><b>{previewTime}</b></span
                            >
                        </div>
                        {#if !isHaConfigured}
                            <div class="preview-empty preview-connect">
                                <span class="connect-ha-logo" aria-hidden="true"><SvgIcon name="ha.svg" /></span>
                                <span class="connect-ha-title">{$_('page.screen.connectHa')}</span>
                                <span class="connect-ha-hint">{$_('page.screen.connectHaHint')}</span>
                            </div>
                        {:else}
                        {#if editing || screen.badges?.length}
                            <div class="screen-badges">
                                {#if editing && (screen.badges?.length ?? 0) < MAX_SCREEN_BADGE_COUNT}
                                    <button class="add-badge-button" type="button" onclick={() => openEntityPicker('badge', screen.index)}><SvgIcon name="mdi:plus" /> <span>{$_('page.screen.addBadge')}</span></button>
                                {/if}
                                <div
                                    class="screen-badge-items"
                                    role="list"
                                    aria-label={$_('page.screen.badgeList', { values: { name: getScreenTitle(screen) } })}
                                    use:dndzone={{
                                        items: getDndItems(screen.index, 'badge', screen.badges ?? []),
                                        type: 'screen-badges',
                                        dragDisabled: !editing,
                                        // Keep the floating badge at its original size; flex wrapping can
                                        // otherwise shrink the shadow placeholder and morph the preview.
                                        morphDisabled: true,
                                        dropFromOthersDisabled: true,
                                        dropTargetStyle: { outline: 'none' },
                                        delayTouchStart: 250,
                                        transformDraggedElement: prepareDraggedBadge,
                                    }}
                                    onconsider={(event) => handleDndConsider(event, screen.index, 'badge')}
                                    onfinalize={(event) => handleDndFinalize(event, screen.index, 'badge')}
                                >
                                    {#each getDndItems(screen.index, 'badge', screen.badges ?? []) as item (`${item.id}-${item[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
                                        {@const badge = item.value as IHassScreenConfigBadge}
                                        <div class:sortable-item={editing} class="screen-badge-sort-item" role="listitem">
                                            <HassEntityRow
                                                variant="badge"
                                                entityId={badge.data.entity}
                                                showIconBackground={shouldShowScreenEntityIconBackground(badge.data.entity, badge.data.actionConfig)}
                                                interactiveElement="div"
                                                onclick={editing ? () => openEntitySettings('badge', screen.index, badge.data.entity) : undefined}
                                            />
                                        </div>
                                    {/each}
                                </div>
                            </div>
                        {/if}
                        {#if editing && !isLandscape}
                            <div class="edit-actions">
                                <button class="add-entity-button" type="button" onclick={() => openEntityPicker('entity', screen.index)}>
                                    <SvgIcon name="mdi:plus" /> <span>{$_('page.screen.addEntity')}</span>
                                </button>
                            </div>
                        {/if}
                        {#if screen.cards?.length || (editing && isLandscape)}
                            <div class="screen-entities-layout" style={`--screen-columns: ${isLandscape ? 2 : 1}`}>
                                {#if editing && isLandscape}
                                    <button class="add-entity-button landscape-add-entity" type="button" onclick={() => openEntityPicker('entity', screen.index)}>
                                        <SvgIcon name="mdi:plus" /> <span>{$_('page.screen.addEntity')}</span>
                                    </button>
                                {/if}
                                {#if screen.cards?.length}
                                    <div
                                        class="screen-entities"
                                        role="list"
                                        aria-label={$_('page.screen.entityList', { values: { name: getScreenTitle(screen) } })}
                                        style={`--screen-columns: ${isLandscape ? 2 : 1}`}
                                        use:dndzone={{
                                            items: getDndItems(screen.index, 'entity', screen.cards ?? []),
                                            type: 'screen-entities',
                                            dragDisabled: !editing,
                                            dropFromOthersDisabled: true,
                                            dropTargetStyle: { outline: 'none' },
                                            delayTouchStart: true,
                                            centreDraggedOnCursor: true,
                                            useCursorForDetection: true,
                                            transformDraggedElement: prepareDraggedEntity,
                                        }}
                                        onconsider={(event) => handleDndConsider(event, screen.index, 'entity')}
                                        onfinalize={(event) => handleDndFinalize(event, screen.index, 'entity')}
                                    >
                                        {#each getDndItems(screen.index, 'entity', screen.cards ?? []) as item (`${item.id}-${item[SHADOW_ITEM_MARKER_PROPERTY_NAME] ? 'shadow' : 'item'}`)}
                                            {@const card = item.value as IHassScreenConfigCard}
                                            <div class:sortable-item={editing} class:full-width={card.data.full_width} class="screen-entity-sort-item" role="listitem">
                                                <HassEntityRow
                                                    entityId={card.data.entity}
                                                    fullWidth={card.data.full_width || !isLandscape}
                                                    showIconBackground={shouldShowScreenEntityIconBackground(card.data.entity, card.data.actionConfig)}
                                                    interactiveElement="div"
                                                    onclick={editing ? () => openEntitySettings('entity', screen.index, card.data.entity) : undefined}
                                                />
                                            </div>
                                        {/each}
                                    </div>
                                {/if}
                            </div>
                        {/if}
                        {#if !editing && !isConfigured(screen)}
                            <div class="preview-empty">
                                <div class="image-placeholder"><SvgIcon name="noEntity.svg" /></div>
                                <span>{$_('page.screen.noContent')}</span>
                            </div>
                        {/if}
                        {/if}
                        {#if displayedScreens.length > 1}
                            <div class="screen-page-indicators" aria-hidden="true">
                                {#each displayedScreens as indicatorScreen (indicatorScreen.index)}
                                    <span class:active={indicatorScreen.index === screen.index} class="screen-page-indicator"></span>
                                {/each}
                            </div>
                        {/if}
                    </div>
                </div>
                </article>
            </div>
        {/each}
        </div>

        {#if editing && $screenConfigStore.screens.length < MAX_SCREEN_COUNT}
            <div
                class="add-screen-card"
                role="button"
                tabindex="0"
                style={`--add-screen-column: ${$screenConfigStore.screens.length % desktopGridColumns}; --add-screen-row: ${Math.floor($screenConfigStore.screens.length / desktopGridColumns)}`}
                onclick={openAddScreenModal}
                onkeydown={(event) => {
                    if (event.key === 'Enter' || event.key === ' ') {
                        event.preventDefault();
                        openAddScreenModal();
                    }
                }}
            >
                <span class="add-screen-icon" aria-hidden="true">+</span><span>{$_('page.screen.addSplit')}</span>
            </div>
        {/if}
    </div>
</div>

<EntitySettingsModal
    visible={entitySettingsVisible}
    mode={settingsMode}
    screenIndex={selectedEntityScreenIndex}
    entityId={selectedEntityId}
    onclose={closeEntitySettings}
/>

<Modal modalVisible={addScreenModalVisible} footer={false} setVisible={(visible) => !visible && closeAddScreenModal()}>
    {#snippet customSnippet()}
        <form
            class="add-screen-dialog"
            onsubmit={(event) => {
                event.preventDefault();
                addScreen();
            }}
        >
            <h2>{$_('page.screen.addSplit')}</h2>
            <label for="new-screen-title">{$_('page.screen.splitName')}</label>
            <input
                id="new-screen-title"
                value={newScreenTitle}
                maxlength={MAX_SCREEN_TITLE_LENGTH}
                placeholder={$_('page.screen.splitNamePlaceholder')}
                oninput={(event) => updateNewScreenTitle(event.currentTarget.value)}
            />
            <footer>
                <button type="button" onclick={closeAddScreenModal}>{$_('common.cancel')}</button>
                <button class="save-new-screen-button" type="submit" disabled={!newScreenTitle.trim()}>{$_('common.save')}</button>
            </footer>
        </form>
    {/snippet}
</Modal>

<Modal modalVisible={leaveConfirmVisible} footer={false}>
    {#snippet customSnippet()}
        <div class="leave-editing-dialog" role="alertdialog" aria-modal="true" aria-label={$_('page.screen.leaveTitle')}>
            <h2>{$_('page.screen.leaveTitle')}</h2>
            <p>{$_('page.screen.leaveWarning')}</p>
            <footer>
                <button type="button" onclick={discardAndLeave}>{$_('page.screen.discardAndLeave')}</button>
                <button class="continue-editing-button" type="button" onclick={continueEditing}>{$_('page.screen.continueEditing')}</button>
            </footer>
        </div>
    {/snippet}
</Modal>

<Modal modalVisible={screenSettingsVisible} footer={false} setVisible={(visible) => !visible && closeScreenSettings()}>
    {#snippet customSnippet()}
        <form
            class="add-screen-dialog"
            aria-label={$_('page.screen.editSplit')}
            onsubmit={(event) => {
                event.preventDefault();
                saveScreenSettings();
            }}
        >
            <h2>{screenTitleCache}</h2>
            <label for="edit-screen-title">{$_('page.screen.splitName')}</label>
            <input id="edit-screen-title" value={screenTitleCache} maxlength={MAX_SCREEN_TITLE_LENGTH} oninput={(event) => updateScreenTitleCache(event.currentTarget.value)} />
            <footer>
                <button class="delete-screen-button" type="button" disabled={$screenConfigStore.screens.length <= 1} onclick={deleteScreen}>{$_('page.screen.deleteSplit')}</button>
                <button class="save-screen-button" type="submit" disabled={!screenTitleCache.trim()}>{$_('common.save')}</button>
            </footer>
        </form>
    {/snippet}
</Modal>

<EntitySelectionModal visible={entityModalVisible} mode={pickerMode} screenIndex={activeScreenIndex} onclose={closeEntityPicker} />

<style lang="scss">
    .screen-config-page {
        --screen-preview-width: 20rem;
        --screen-preview-height: 30rem;
        --screen-preview-scale: 0.67;
        --screen-card-width: 14.875rem;
        --screen-card-height: 24rem;
        --screen-grid-width: 46.25rem;
        --screen-grid-height: 49.25rem;
        --screen-grid-gap: 0.8125rem;
        --screen-grid-columns: 3;
        width: var(--screen-grid-width);
        max-width: none;
    }
    .screen-config-page.landscape {
        --screen-preview-width: 30rem;
        --screen-preview-height: 20rem;
        --screen-preview-scale: 0.67;
        --screen-card-width: 22.8125rem;
        --screen-card-height: 17.3125rem;
        --screen-grid-width: 46.4375rem;
        --screen-grid-height: 46.25rem;
        --screen-grid-columns: 2;
    }
    .screen-config-header {
        display: flex;
        align-items: flex-start;
        justify-content: space-between;
        gap: 2rem;
        border-bottom: 0.0625rem solid var(--bs-border-color);
        margin-bottom: 2rem;
    }
    .screen-edit-button {
        min-width: 9.375rem;
        padding: 0.65rem 2rem;
        color: white;
        border: 0;
        border-radius: 999rem;
        background: var(--primary-button-bg);
    }
    .screen-grid-layout {
        position: relative;
        width: var(--screen-grid-width);
        min-height: var(--screen-grid-height);
    }
    .screen-grid {
        display: grid;
        width: var(--screen-grid-width);
        min-height: var(--screen-grid-height);
        grid-template-columns: repeat(var(--screen-grid-columns, 3), minmax(0, 1fr));
        grid-auto-rows: var(--screen-card-height);
        gap: var(--screen-grid-gap);
        align-content: start;
        align-items: stretch;
    }
    .screen-card {
        width: 100%;
        height: var(--screen-card-height);
        padding: calc(1.1rem * var(--screen-preview-scale));
        border: 0;
        border-radius: 0.75rem;
        background: var(--body-inner-level-2);
    }
    .screen-card-shell {
        width: 100%;
        min-width: 0;
        height: var(--screen-card-height);
    }
    :global(.screen-badge-dragged-preview) > :global(.hass-entity-row) {
        width: max-content;
        max-width: none;
        transform-origin: top left;
    }
    :global(.screen-entity-dragged-preview) > :global(.hass-entity-row) {
        max-width: none;
        transform-origin: top left;
    }
    :global(.screen-card-dragged-content-preview) {
        scale: 1.1;
        box-shadow: 0 0 0 0.125rem rgba(1, 119, 253, 1);
        border-radius: 0.75rem;
    }
    :global(.screen-item-dragged-content-preview) {
        box-shadow: 0 0 0 0.125rem rgba(1, 119, 253, 1);
    }
    .screen-card {
        background: var(--web-, rgba(48, 49, 50, 1));
    }
    .screen-card-title {
        display: flex;
        min-width: 0;
        align-items: center;
        justify-content: space-between;
        gap: 0.75rem;
        margin-bottom: calc(1.5rem * var(--screen-preview-scale));
        color: white;
        font-size: calc(1.5rem * var(--screen-preview-scale));
        line-height: 1.5;
    }
    .screen-card-title.editing {
        cursor: grab;
    }
    .screen-card-title-main {
        display: flex;
        min-width: 0;
        align-items: center;
        gap: 0.4rem;
    }
    .screen-card-title-main > span {
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        color: #fff;
    }
    .screen-card-title > div[aria-hidden='true'] {
        display: flex;
        align-self: stretch;
    }
    .screen-card-title button {
        display: inline-flex;
        flex: 0 0 auto;
        height: 1.5rem;
        padding: 0;
        align-items: center;
        justify-content: center;
        border: 0;
        background: transparent;
        color: var(--bs-body-color);
        font-size: 1.35rem;
        line-height: 1;
    }
    .screen-card-title .screen-card-drag-handle {
        display: inline-flex;
        width: 2rem;
        align-items: center;
        align-self: stretch;
        justify-content: center;
        border-radius: 0.4rem;
        cursor: grab;
        touch-action: none;
    }
    .screen-card-drag-handle:active {
        cursor: grabbing;
    }
    .screen-card-drag-handle :global(svg) {
        width: 1.25rem;
        height: 1.25rem;
    }
    .add-screen-dialog {
        width: min(32.5rem, calc(100vw - 2rem));
        padding: 1.25rem 1.25rem 2rem;
        border-radius: 1.5rem;
        background: var(--body-inner-level-2);
        color: var(--bs-body-color);
    }
    .add-screen-dialog h2 {
        margin: 0 0 2.75rem;
        font-size: 1.375rem;
        font-weight: 600;
        line-height: 1.5;
        text-align: center;
    }
    .add-screen-dialog label {
        display: block;
        margin-bottom: 0.625rem;
        font-size: 1rem;
        font-weight: 500;
    }
    .add-screen-dialog input {
        width: 100%;
        height: 3.5rem;
        padding: 0 1.125rem;
        outline: 0;
        color: var(--bs-body-color);
        border: 0.0625rem solid var(--bs-border-color);
        border-radius: 0.75rem;
        background: transparent;
    }
    .add-screen-dialog input:focus {
        border-color: var(--primary-button-bg);
    }
    .add-screen-dialog footer {
        display: flex;
        margin-top: 4.5rem;
        justify-content: center;
        gap: 1.75rem;
    }
    .add-screen-dialog footer button {
        width: 8.875rem;
        height: 3rem;
        padding: 0.5rem 1rem;
        color: var(--bs-body-color);
        border: 0.0625rem solid var(--bs-border-color);
        border-radius: 999rem;
        background: transparent;
    }
    .add-screen-dialog footer .save-new-screen-button {
        color: #fff;
        border-color: var(--primary-button-bg);
        background: var(--primary-button-bg);
    }
    .add-screen-dialog footer .save-new-screen-button:disabled {
        cursor: not-allowed;
        opacity: 0.5;
    }
    .leave-editing-dialog {
        width: min(21.25rem, calc(100vw - 2rem));
        padding: 1.5rem;
        border-radius: 1.25rem;
        background: var(--body-inner-level-2);
        color: var(--bs-body-color);
        box-shadow: 0 1rem 2.5rem rgba(0, 0, 0, 0.28);
    }
    .leave-editing-dialog h2 {
        margin: 0 0 0.85rem;
        font-size: 1.05rem;
        font-weight: 600;
        line-height: 1.4;
        text-align: center;
    }
    .leave-editing-dialog p {
        margin: 0;
        font-size: 0.92rem;
        line-height: 1.55;
        text-align: center;
    }
    .leave-editing-dialog footer {
        display: flex;
        margin-top: 1.5rem;
        flex-direction: column-reverse;
        gap: 0.65rem;
    }
    .leave-editing-dialog footer button {
        width: 100%;
        min-height: 2.5rem;
        padding: 0.5rem 1rem;
        border: 0.0625rem solid var(--bs-border-color);
        border-radius: 999rem;
        background: transparent;
        color: var(--bs-body-color);
        font-weight: 500;
        line-height: 1.2;
    }
    .leave-editing-dialog footer .continue-editing-button {
        color: white;
        border-color: var(--primary-button-bg);
        background: var(--primary-button-bg);
    }
    .add-screen-dialog footer .delete-screen-button {
        color: rgba(241, 82, 75, 1);
        border-color: transparent;
        background: rgba(241, 82, 75, 0.25);
    }
    .delete-screen-button:disabled {
        cursor: not-allowed;
        opacity: 0.5;
    }
    .add-screen-dialog footer .save-screen-button {
        color: #fff;
        border-color: var(--primary-button-bg);
        background: var(--primary-button-bg);
    }
    .save-screen-button:disabled {
        opacity: 0.5;
    }
    .device-preview-frame {
        position: relative;
        left: 50%;
        width: calc(var(--screen-preview-width, 20rem) * var(--screen-preview-scale));
        height: calc(var(--screen-preview-height, 30rem) * var(--screen-preview-scale));
        overflow: hidden;
        transform: translateX(-50%);
    }
    .device-preview {
        position: relative;
        display: flex;
        width: var(--screen-preview-width, 20rem);
        height: var(--screen-preview-height, 30rem);
        overflow: hidden;
        flex-direction: column;
        color: white;
        background: #000;
        font-family: MiSans, sans-serif;
        transform: scale(var(--screen-preview-scale));
        transform-origin: top left;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
    }
    .device-status {
        display: flex;
        width: var(--screen-preview-width, 20rem);
        height: 3rem;
        box-sizing: border-box;
        padding: 0.5rem 0.75rem;
        gap: 0.625rem;
        align-items: center;
        justify-content: space-between;
    }
    .device-status > strong {
        overflow: hidden;
        font-family: MiSans, sans-serif;
        font-size: 1rem;
        font-weight: 400;
        line-height: 1.5rem;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .device-status-icons {
        display: flex;
        margin-left: auto;
        gap: 0.35rem;
        align-items: center;
    }
    .device-status-icons :global(.sf-icon) {
        display: inline-flex;
        flex: 0 0 auto;
    }
    .device-status-icons :global(svg) {
        width: 1.5rem;
        height: 1.5rem;
    }
    .device-status-icons b {
        font-family: MiSans, sans-serif;
        font-size: 1.25rem;
        font-weight: 500;
        line-height: 1.75rem;
    }
    .screen-page-indicators {
        position: absolute;
        z-index: 1;
        right: 0;
        bottom: 0;
        left: 0;
        display: flex;
        height: 1.5rem;
        align-items: center;
        justify-content: center;
        gap: 0.4rem;
        pointer-events: none;
    }
    .screen-page-indicator {
        width: 0.4rem;
        height: 0.4rem;
        flex: 0 0 auto;
        border-radius: 50%;
        background: rgba(255, 255, 255, 0.25);
    }
    .screen-page-indicator.active {
        background: #fff;
    }
    .screen-badges {
        display: flex;
		justify-content: start;
        width: 100%;
        max-width: 100%;
        padding: 0 0.75rem 0.5rem;
        box-sizing: border-box;
        flex: 0 0 auto;
        flex-wrap: wrap;
        align-items: center;
        gap: 0.25rem;
    }
    .screen-config-page.landscape .screen-badges,
    .screen-config-page.landscape .screen-entities-layout {
        padding-right: 1.25rem;
        padding-left: 1.25rem;
    }
    .screen-badge-items {
        display: flex;
        min-width: 0;
        flex: 1 1 auto;
        flex-wrap: wrap;
        align-items: center;
        gap: 0.25rem;
    }
    .screen-badge-sort-item {
        display: inline-flex;
        min-width: 0;
        flex: 0 0 auto;
        align-items: center;
        gap: 0.125rem;
    }
    .sortable-item {
        cursor: grab;
    }
    .sortable-item:active {
        cursor: grabbing;
    }
    .sortable-item :global(.hass-entity-row) {
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
    }
    .screen-badge-sort-item :global(.hass-entity-row) {
        width: auto;
        margin-inline: 0;
    }
    .add-badge-button {
        position: relative;
        display: inline-flex;
        width: auto;
        height: 1.875rem;
        padding: 0 0.625rem;
        flex: 0 0 auto;
        align-items: center;
        justify-content: center;
        gap: 0.25rem;
        color: #f2f2f7;
        border: 0;
        border-radius: 999rem;
        background: #000;
        font-size: 0.875rem;
        line-height: 1;
        white-space: nowrap;
    }
    .add-badge-button :global(.sf-icon) {
        width: 1rem;
        height: 1rem;
        flex: 0 0 auto;
        font-size: 1rem;
    }
    .preview-empty {
        position: absolute;
        inset: 0;
        display: flex;
        box-sizing: border-box;
        padding: 1.5rem;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 1.25rem;
        text-align: center;
        font-size: 0.8rem;
        pointer-events: none;
    }
    .preview-connect {
        bottom: 5rem;
        padding: 1rem;
        gap: 0;
    }
    .screen-config-page.landscape .preview-connect {
        bottom: 3rem;
    }
    .connect-ha-logo {
        display: grid;
        width: 3.75rem;
        height: 3.75rem;
        margin-bottom: 1rem;
        flex: 0 0 auto;
        place-items: center;
        border-radius: 1.25rem;
        background: linear-gradient(135deg, #20d5dc, #0086d1);
    }
    .connect-ha-logo :global(svg) {
        width: 2.5rem;
        height: 2.5rem;
    }
    .connect-ha-logo :global(path) {
        fill: #fff;
    }
    .connect-ha-title {
        font-size: 1.25rem;
        line-height: 1.75rem;
    }
    .connect-ha-hint {
        margin-top: 0.5rem;
        font-size: 1rem;
        line-height: 1.5rem;
    }
    .image-placeholder {
        display: grid;
        width: 9.0625rem;
        height: 6.5625rem;
        margin-bottom: 0.5rem;
        place-items: center;
    }
    .image-placeholder :global(.sf-icon),
    .image-placeholder :global(svg) {
        width: 100%;
        height: 100%;
    }
    .edit-actions {
        margin-bottom: 0.75rem;
        padding: 0 1.25rem;
    }
    .add-entity-button {
        position: relative;
        display: flex;
        width: 100%;
        height: 4.25rem;
        padding: 0.25rem 0.5rem;
        align-items: center;
        justify-content: center;
        gap: 0.625rem;
        color: white;
        border: 0;
        border-radius: 0.75rem;
        background: #000;
        font-size: 1.5rem;
        line-height: 1;
        opacity: 1;
    }
    .add-badge-button::after,
    .add-entity-button::after {
        position: absolute;
        z-index: 1;
        box-sizing: border-box;
        border: 0.0625rem dashed rgba(1, 119, 253, 1);
        border-radius: inherit;
        content: '';
        inset: 0;
        pointer-events: none;
    }
    .add-entity-button :global(.sf-icon) {
        display: inline-flex;
        width: 1.75rem;
        height: 1.75rem;
        flex: 0 0 auto;
        align-items: center;
        justify-content: center;
        font-size: 1.75rem;
    }
    .screen-entities-layout {
        display: grid;
        min-height: 0;
        padding: 0 0.75rem;
        flex: 1 1 auto;
        grid-template-columns: repeat(var(--screen-columns, 1), minmax(0, 1fr));
        align-content: start;
        gap: 0.5rem;
        overflow-y: auto;
        -webkit-overflow-scrolling: touch;
        overscroll-behavior: contain;
        touch-action: pan-y;
        scrollbar-width: none;
    }
    .screen-entities-layout::-webkit-scrollbar {
        display: none;
    }
    .screen-entities {
        display: grid;
        /* Keep the drop zone as tall as its cards while the parent scrolls. */
        min-height: max-content;
        max-height: none;
        width: 100%;
        padding: 0;
        grid-column: 1 / -1;
        grid-template-columns: repeat(var(--screen-columns, 1), minmax(0, 1fr));
        align-content: start;
        gap: 0.5rem;
        overflow: visible;
    }
    .screen-entity-sort-item {
        width: 100%;
        position: relative;
        min-width: 0;
        padding: 0;
        color: inherit;
        border: 0;
        background: transparent;
        font: inherit;
        text-align: left;
        border-radius: 0.65rem;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
    }
    .screen-entity-sort-item.full-width {
        grid-column: span var(--screen-columns, 1);
    }
    .add-screen-card {
        position: absolute;
        cursor: pointer;
        top: calc(var(--add-screen-row) * (var(--screen-card-height) + var(--screen-grid-gap)));
        left: calc(var(--add-screen-column) * (var(--screen-card-width) + var(--screen-grid-gap)));
        display: flex;
        width: var(--screen-card-width);
        height: var(--screen-card-height);
        padding: 0;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: calc(1rem * var(--screen-preview-scale));
        color: #fff;
        background: rgba(48, 49, 50, 1);
        font-size: calc(1.5rem * var(--screen-preview-scale));
        font-weight: 600;
        border-radius: 0.75rem;
    }
    .add-screen-icon {
        display: inline-flex;
        width: calc(4rem * var(--screen-preview-scale));
        height: calc(4rem * var(--screen-preview-scale));
        align-items: center;
        justify-content: center;
        border-radius: 50%;
        background: #0177fd;
        color: #fff;
        font-size: calc(3.25rem * var(--screen-preview-scale));
        font-weight: 300;
        line-height: 1;
    }
    @media (max-width: 52rem) {
        .screen-config-page {
            --screen-preview-scale: var(--mobile-preview-scale, 1);
            --screen-card-width: 100%;
            --screen-card-height: auto;
            width: 100%;
        }
        .screen-config-page:not(.landscape) {
            --screen-preview-width: 20.1875rem;
            --screen-preview-height: 30.25rem;
        }
        .screen-config-page.landscape {
            --screen-preview-scale: var(--mobile-preview-scale, 1);
            --screen-card-width: 100%;
            --screen-card-height: auto;
            --screen-grid-width: 100%;
            --screen-grid-height: auto;
            --screen-grid-columns: 1;
            width: 100%;
        }
        .screen-config-header {
            align-items: center;
        }
        .screen-grid-layout {
            width: 100%;
            min-height: 0;
        }
        .screen-grid {
            width: 100%;
            min-height: 0;
            grid-template-columns: var(--screen-card-width);
            grid-auto-rows: auto;
            justify-content: center;
        }
        .screen-grid-layout.has-add-screen .screen-grid {
            padding-top: 5rem;
        }
        .screen-card-shell {
            display: block;
            width: var(--screen-card-width);
            height: auto;
        }
        .screen-config-page.landscape .screen-card-shell,
        .screen-config-page.landscape .screen-card {
            width: 100%;
        }
        .screen-grid.sorting .device-preview-frame,
        :global(.screen-card-dragged-preview) .device-preview-frame {
            display: none;
        }
        .screen-card-shell.dragging .screen-card > :not(.screen-card-title),
        :global(.screen-card-dragged-preview) .screen-card > :not(.screen-card-title) {
            display: none;
        }
        .screen-card-shell.dragging .screen-card,
        :global(.screen-card-dragged-preview) .screen-card {
            height: auto;
        }
        .screen-card-shell.dragging .screen-card-title,
        :global(.screen-card-dragged-preview) .screen-card-title {
            margin-bottom: 0;
        }
        .screen-card {
            height: auto;
            padding: 0.75rem;
        }
        .screen-card-title {
            margin-bottom: 1.25rem;
            font-size: 1.25rem;
        }
        .device-preview-frame {
            left: auto;
            width: calc(var(--screen-preview-width) * var(--screen-preview-scale));
            height: calc(var(--screen-preview-height) * var(--screen-preview-scale));
            margin-inline: auto;
            transform: none;
        }
        .device-preview {
            transform: scale(var(--screen-preview-scale));
        }
        .screen-grid-layout.editing .screen-entities-layout {
            min-height: 0;
            flex: 1 1 auto;
            grid-template-rows: minmax(0, 1fr);
            overflow: hidden;
        }
        .screen-config-page.landscape .screen-grid-layout.editing .screen-entities-layout {
            grid-template-rows: auto minmax(0, 1fr);
        }
        .screen-grid-layout.editing .screen-entities {
            min-height: 0;
            max-height: 100%;
            overflow-y: auto;
            -webkit-overflow-scrolling: touch;
            overscroll-behavior: contain;
            touch-action: pan-y;
            scrollbar-width: none;
        }
        .screen-grid-layout.editing .screen-entities::-webkit-scrollbar {
            display: none;
        }
        .device-status {
            width: 100%;
            height: 3rem;
            padding: 0.5rem 0.75rem;
            gap: 0.625rem;
        }
        .device-status > strong {
            font-size: 1rem;
            line-height: 1.5rem;
        }
        .device-status-icons :global(svg) {
            width: 1.5rem;
            height: 1.5rem;
        }
        .device-status-icons b {
            font-size: 1.25rem;
            line-height: 1.75rem;
        }
        .edit-actions {
            margin-bottom: 0.75rem;
            padding: 0 1.25rem;
        }
        .edit-actions button {
            height: 4.25rem;
            padding: 0.25rem 0.5rem;
            gap: 0.625rem;
            border-radius: 0.75rem;
            font-size: 1.5rem;
        }
        .screen-entities {
            max-height: none;
            gap: 0.5rem;
        }
        .add-screen-card {
            top: 0;
            left: 50%;
            transform: translateX(-50%);
            height: 4.25rem;
            gap: 0.25rem;
            font-size: 0.75rem;
        }
        .add-screen-icon {
            width: 2rem;
            height: 2rem;
            font-size: 1.75rem;
        }
    }
    @media (max-width: 52rem) {
        .screen-config-page .screen-card-shell,
        .screen-config-page .screen-card {
            width: 100%;
        }
    }
</style>
