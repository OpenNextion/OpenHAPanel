<script lang="ts">
    import './ScreenConfiguration.scss';
    import { cloneDeep } from 'lodash-es';
    import dayjs from 'dayjs';
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import ScreenGrid from './ScreenGrid.svelte';
    import ScreenDialogs from './ScreenDialogs.svelte';
    import EntitySettingsModal from './EntitySettingsModal.svelte';
    import EntitySelectionModal from './EntitySelectionModal.svelte';
    import type { TEntityPickerMode } from '@/hooks/useScreenConfiguration';
    import { EMessageType, sendMessage } from '@/stores/message';
    import {
        MAX_SCREEN_BADGE_COUNT,
        removeScreenConfig,
        screenConfigStore,
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
    let currentRouteHash = window.location.hash;
    let allowNextNavigation = false;
    const MAX_SCREEN_COUNT = 5;
    const MAX_SCREEN_TITLE_LENGTH = 20;

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
    });

    const currentTimeInMs = $derived(Number($timerStore.timers.currentTime?.value ?? ($deviceStore.currentTime || dayjs().valueOf())));
    const previewTime = $derived(dayjs.utc(currentTimeInMs).utcOffset($finialOffset).format('HH:mm'));
    const isLandscape = $derived($deviceStore.screenRotation.rotation === 90);
    const isHaConfigured = $derived($homeAssistantStore.connectionInfo?.configured === true);
    const displayedScreens = $derived.by(() => {
        if (editing) return $screenConfigStore.screens;
        return $screenConfigStore.screens.length ? $screenConfigStore.screens : [DEFAULT_SCREEN];
    });
    function getScreenTitle(screen: IHassScreenConfigItem) {
        return screen.index === 0 && DEFAULT_SCREEN_TITLE_ALIASES.has(screen.title) ? $_('page.screen.defaultSplit') : screen.title;
    }

    function startEditing() {
        if (!isHaConfigured) {
            sendMessage(EMessageType.WARN, $_('page.screen.connectHaFirst'));
            return;
        }
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

    function reorderBadges(screenIndex: number, badges: IHassScreenConfigBadge[]) {
        updateScreenConfigs($screenConfigStore.screens.map((screen) => screen.index === screenIndex ? { ...screen, badges } : screen));
    }

    function reorderCards(screenIndex: number, cards: IHassScreenConfigCard[]) {
        updateScreenConfigs($screenConfigStore.screens.map((screen) => screen.index === screenIndex ? { ...screen, cards } : screen));
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
        editing = false;
        leaveConfirmVisible = false;
        allowNextNavigation = true;
        const targetHash = pendingNavigationHash;
        pendingNavigationHash = '';
        if (targetHash) window.location.hash = targetHash;
    }

    onMount(() => {
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
            document.removeEventListener('click', handleRouteLink, true);
            window.removeEventListener('popstate', handleRouteChange, true);
            window.removeEventListener('hashchange', handleRouteChange, true);
            window.removeEventListener('beforeunload', handleBeforeUnload);
        };
    });
</script>

<div class:landscape={isLandscape} class="page-container screen-config-page">
    <header class="screen-config-header">
        <div class="page-heading">
            <span class="page-heading-title">{$_('page.screen.title')}</span>
            <p class="text-body-secondary mb-0">{$_('page.screen.subtitle')}</p>
        </div>
        <button class="screen-edit-button" type="button" disabled={saving} onclick={editing ? finishEditing : startEditing}>
            {#if saving}<span class="spinner-border spinner-border-sm" aria-hidden="true"></span>{/if}
            {$_(editing ? 'page.screen.done' : 'page.screen.edit')}
        </button>
    </header>

    <ScreenGrid
        screens={displayedScreens}
        {editing}
        {isLandscape}
        {isHaConfigured}
        {previewTime}
        canAddScreen={editing && $screenConfigStore.screens.length < MAX_SCREEN_COUNT}
        {getScreenTitle}
        onAddScreen={openAddScreenModal}
        onRenameScreen={openScreenSettings}
        onAddBadge={(screenIndex) => openEntityPicker('badge', screenIndex)}
        onAddEntity={(screenIndex) => openEntityPicker('entity', screenIndex)}
        onBadgeSettings={(screenIndex, entityId) => openEntitySettings('badge', screenIndex, entityId)}
        onEntitySettings={(screenIndex, entityId) => openEntitySettings('entity', screenIndex, entityId)}
        onReorderScreens={updateScreenConfigs}
        onReorderBadges={reorderBadges}
        onReorderCards={reorderCards}
    />
</div>

<EntitySettingsModal
    visible={entitySettingsVisible}
    mode={settingsMode}
    screenIndex={selectedEntityScreenIndex}
    entityId={selectedEntityId}
    onclose={closeEntitySettings}
/>

<ScreenDialogs
    addScreenVisible={addScreenModalVisible}
    {leaveConfirmVisible}
    {screenSettingsVisible}
    {newScreenTitle}
    {screenTitleCache}
    maxScreenTitleLength={MAX_SCREEN_TITLE_LENGTH}
    canDeleteScreen={$screenConfigStore.screens.length > 1}
    onCloseAddScreen={closeAddScreenModal}
    onNewScreenTitleChange={updateNewScreenTitle}
    onAddScreen={addScreen}
    onDiscardAndLeave={discardAndLeave}
    onContinueEditing={continueEditing}
    onCloseScreenSettings={closeScreenSettings}
    onScreenTitleChange={updateScreenTitleCache}
    onDeleteScreen={deleteScreen}
    onSaveScreenSettings={saveScreenSettings}
/>

<EntitySelectionModal visible={entityModalVisible} mode={pickerMode} screenIndex={activeScreenIndex} onclose={closeEntityPicker} />
