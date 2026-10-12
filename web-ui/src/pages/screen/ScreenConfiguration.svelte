<script lang="ts">
    import { cloneDeep, isEqual } from 'lodash-es';
    import dayjs from 'dayjs';
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import HassEntityRow from '@/component/HassEntityRow/HassEntityRow.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import EntityCardSettingsModal from './EntityCardSettingsModal.svelte';
    import EntityPickerModal from './EntityPickerModal.svelte';
    import { getScreenEntityName, type TEntityPickerMode } from '@/hooks/useScreenConfiguration';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { isHomeAssistantAvailable } from '@/hooks/useHomeAssistant.svelte';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { removeScreenConfig, screenConfigStore, updateScreenConfigs, updateScreenTitle } from '@/stores/screenConfig';
    import type { IHassScreenConfigItem } from '@/types/screenConfig';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';
    import { REG_EXP_CHINESE_CHARACTERS } from '@/constant/regRex';
    import { deviceStore } from '@/stores/device';
    import { finialOffset, timerStore } from '@/stores/timer';

    let editing = $state(false);
    let saving = $state(false);
    let entityModalVisible = $state(false);
    let pickerMode = $state<TEntityPickerMode>('entity');
    let activeScreenIndex = $state<number | null>(null);
    let cardSettingsVisible = $state(false);
    let selectedCardScreenIndex = $state<number | null>(null);
    let selectedCardEntityId = $state('');
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

    const DEFAULT_SCREEN: IHassScreenConfigItem = {
        index: 0,
        title: 'default',
        icon: '',
        type: 'sections',
        badges: [],
        cards: [],
        features: [],
    };

    const configured = $derived(Boolean($homeAssistantStore.connectionInfo?.configured));
    const homeAssistantAvailable = $derived(isHomeAssistantAvailable($homeAssistantStore.connectionInfo));
    const currentTimeInMs = $derived(Number($timerStore.timers.currentTime?.value ?? ($deviceStore.currentTime || dayjs().valueOf())));
    const previewTime = $derived(dayjs.utc(currentTimeInMs).utcOffset($finialOffset).format('HH:mm'));
    const displayedScreens = $derived.by(() => {
        if (!configured) return [DEFAULT_SCREEN];
        if (editing) return $screenConfigStore.screens;
        return $screenConfigStore.screens.length ? $screenConfigStore.screens : [DEFAULT_SCREEN];
    });
    function getScreenTitle(screen: IHassScreenConfigItem) {
        return screen.title === DEFAULT_SCREEN.title ? $_('page.screen.defaultSplit') : screen.title;
    }

    function isConfigured(screen: IHassScreenConfigItem) {
        return Boolean(screen.badges?.length || screen.cards?.length || screen.features?.length);
    }

    function ensureHomeAssistantAvailable() {
        if (homeAssistantAvailable) return true;
        sendMessage(EMessageType.ERROR, $_('page.screen.haConnectionAbnormal'));
        return false;
    }

    function hasCardChanges() {
        if (!editingSnapshot) return false;
        const serializeCards = (screens: IHassScreenConfigItem[]) =>
            screens
                .map((screen) => screen.cards ?? [])
                .filter((cards) => cards.length > 0)
                .map((cards) => JSON.stringify(cards))
                .sort();
        return !isEqual(serializeCards(editingSnapshot), serializeCards($screenConfigStore.screens));
    }

    function startEditing() {
        if (!configured) {
            sendMessage(EMessageType.ERROR, $_('page.screen.connectHaFirst'));
            return;
        }
        editingSnapshot = cloneDeep($screenConfigStore.screens);
        if (!$screenConfigStore.screens.length) {
            updateScreenConfigs([DEFAULT_SCREEN]);
        }
        editing = true;
    }

    function openEntityPicker(mode: TEntityPickerMode, screenIndex: number) {
        if (!ensureHomeAssistantAvailable()) return;
        pickerMode = mode;
        activeScreenIndex = screenIndex;
        entityModalVisible = true;
    }

    function closeEntityPicker() {
        entityModalVisible = false;
        activeScreenIndex = null;
    }

    function openCardSettings(screenIndex: number, entityId: string) {
        if (!ensureHomeAssistantAvailable()) return;
        selectedCardScreenIndex = screenIndex;
        selectedCardEntityId = entityId;
        cardSettingsVisible = true;
    }

    function closeCardSettings() {
        cardSettingsVisible = false;
        selectedCardScreenIndex = null;
        selectedCardEntityId = '';
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
                badges: [],
                cards: [],
                features: [],
            },
        ]);
        closeAddScreenModal();
    }

    function openScreenSettings(screen: IHassScreenConfigItem) {
        if (!configured) {
            sendMessage(EMessageType.ERROR, $_('page.screen.connectHaFirst'));
            return;
        }
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
        if (!homeAssistantAvailable && hasCardChanges()) {
            sendMessage(EMessageType.ERROR, $_('page.screen.haConnectionAbnormal'));
            return;
        }
        saving = true;
        try {
            const screens: IHassScreenConfigItem[] = $screenConfigStore.screens;
            const response = await sendForReply<{ screens: IHassScreenConfigItem[] }>({
                method: EWsMethods.SET_SCREEN_SETTING,
                data: { screens },
            });
            if (response.error === 0) {
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

<div class="page-container screen-config-page">
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

    <div class:editing class="screen-grid-layout">
        <div class="screen-grid">
        {#each displayedScreens as screen (screen.index)}
            <div class="screen-card-shell">
                <article class="screen-card">
                <div class="screen-card-title">
                    <div class="screen-card-title-main">
                        <span>{getScreenTitle(screen)}</span>
                        {#if editing}
                            <button type="button" aria-label={$_('page.screen.rename')} onclick={() => openScreenSettings(screen)}><SvgIcon name="edit.svg" /></button>
                        {/if}
                    </div>
                </div>
                <div class="device-preview-frame">
                    <div class="device-preview">
                        <div class="device-status">
                            {#if configured}<strong>{getScreenTitle(screen)}</strong>{/if}<span class="device-status-icons"
                                >{#if configured}<SvgIcon name="ha.svg" />{/if}<SvgIcon name="wifi_4.svg" /><b>{previewTime}</b></span
                            >
                        </div>
                        <!--{#if editing || screen.badges?.length}
                        <div class="screen-badges">
                            {#each screen.badges ?? [] as badge, badgeIndex (`${badge.data.entity}-${badgeIndex}`)}
                                <HassEntityRow
                                    variant="badge"
                                    entityId={badge.data.entity}
                                    name={badge.data.name || getScreenEntityName(badge.data.entity)}
                                />
                            {/each}
                            {#if editing}
                                <button class="add-badge-button" type="button" onclick={() => openEntityPicker('badge', screen.index)}><i class="bi bi-plus"></i> Add Badge</button>
                            {/if}
                        </div>
                    {/if}-->
                        {#if editing}
                            <div class="edit-actions">
                                <button type="button" onclick={() => openEntityPicker('entity', screen.index)}><i class="bi bi-plus"></i> Add Entity</button>
                            </div>
                        {/if}
                        {#if screen.cards?.length}
                            <div class="screen-entities">
                                {#each screen.cards as card, cardIndex (`${card.data.entity}-${cardIndex}`)}
                                    <div class="screen-entity-sort-item" role="listitem">
                                        <HassEntityRow
                                            entityId={card.data.entity}
                                            name={getScreenEntityName(card.data.entity)}
                                            onclick={editing ? () => openCardSettings(screen.index, card.data.entity) : undefined}
                                        />
                                    </div>
                                {/each}
                            </div>
                        {/if}
                        {#if !editing && !configured}
                            <div class="preview-empty connect-ha">
                                <span class="connect-ha-icon"><SvgIcon name="haHeaderActive.svg" /></span>
                                <strong>{$_('page.screen.connectHa')}</strong>
                                <span>{$_('page.screen.connectHaHint')}</span>
                            </div>
                        {:else if !editing && !isConfigured(screen)}
                            <div class="preview-empty">
                                <div class="image-placeholder"><SvgIcon name="noEntity.svg" /></div>
                                <span>{$_('page.screen.noContent')}</span>
                            </div>
                        {/if}
                    </div>
                </div>
                </article>
            </div>
        {/each}
        </div>

        {#if editing && $screenConfigStore.screens.length < MAX_SCREEN_COUNT}
            <button
                class="add-screen-card"
                type="button"
                style={`--add-screen-column: ${$screenConfigStore.screens.length % 3}; --add-screen-row: ${Math.floor($screenConfigStore.screens.length / 3)}`}
                onclick={openAddScreenModal}
            >
                <span class="add-screen-icon" aria-hidden="true">+</span><span>{$_('page.screen.addSplit')}</span>
            </button>
        {/if}
    </div>
</div>

<EntityCardSettingsModal visible={cardSettingsVisible} screenIndex={selectedCardScreenIndex} entityId={selectedCardEntityId} onclose={closeCardSettings} />

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

<EntityPickerModal visible={entityModalVisible} mode={pickerMode} screenIndex={activeScreenIndex} onclose={closeEntityPicker} />

<style lang="scss">
    .screen-config-page {
        --screen-preview-scale: 0.67;
        --screen-card-width: 238px;
        --screen-card-height: 384px;
        --screen-grid-width: 740px;
        --screen-grid-height: 788px;
        --screen-grid-gap: 13px;
        width: var(--screen-grid-width);
        max-width: none;
    }
    .screen-config-header {
        display: flex;
        align-items: flex-start;
        justify-content: space-between;
        gap: 2rem;
        border-bottom: 1px solid var(--bs-border-color);
        margin-bottom: 2rem;
    }
    .screen-edit-button {
        min-width: 150px;
        padding: 0.65rem 2rem;
        color: white;
        border: 0;
        border-radius: 999px;
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
        grid-template-columns: repeat(3, var(--screen-card-width));
        grid-auto-rows: var(--screen-card-height);
        gap: var(--screen-grid-gap);
        align-content: start;
        align-items: stretch;
    }
    .screen-card,
    .add-screen-card {
        width: var(--screen-card-width);
        height: var(--screen-card-height);
        padding: calc(1.1rem * var(--screen-preview-scale));
        border: 0;
        border-radius: 0.75rem;
        background: var(--body-inner-level-2);
    }
    .screen-card-shell {
        width: var(--screen-card-width);
        height: var(--screen-card-height);
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
        margin-bottom: calc(1rem * var(--screen-preview-scale));
        color: white;
        font-size: calc(1.5rem * var(--screen-preview-scale));
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
    .screen-card-title button {
        display: inline-flex;
        flex: 0 0 auto;
        padding: 0;
        align-items: center;
        border: 0;
        background: transparent;
        color: var(--bs-body-color);
        font-size: 1.35rem;
    }
    .add-screen-dialog {
        width: min(520px, calc(100vw - 2rem));
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
        border: 1px solid var(--bs-border-color);
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
        border: 1px solid var(--bs-border-color);
        border-radius: 999px;
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
        width: min(340px, calc(100vw - 2rem));
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
        min-height: 40px;
        padding: 0.5rem 1rem;
        border: 1px solid var(--bs-border-color);
        border-radius: 999px;
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
        width: calc(320px * var(--screen-preview-scale));
        height: calc(480px * var(--screen-preview-scale));
        overflow: hidden;
        transform: translateX(-50%);
    }
    .device-preview {
        width: 320px;
        height: 480px;
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
        width: 320px;
        height: 48px;
        box-sizing: border-box;
        padding: 8px 12px;
        gap: 10px;
        align-items: center;
        justify-content: space-between;
    }
    .device-status > strong {
        overflow: hidden;
        font-family: MiSans, sans-serif;
        font-size: 16px;
        font-weight: 400;
        line-height: 24px;
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
        width: 24px;
        height: 24px;
    }
    .device-status-icons b {
        font-family: MiSans, sans-serif;
        font-size: 20px;
        font-weight: 500;
        line-height: 28px;
    }
    .preview-empty {
        display: flex;
        min-height: 270px;
        padding: 1.5rem;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 1.25rem;
        text-align: center;
        font-size: 0.8rem;
    }
    .connect-ha {
        width: 100%;
        height: calc(480px - 33px);
        min-height: 0;
        padding: 24px 20px;
        gap: 8px;
    }
    .connect-ha-icon {
        display: inline-flex;
        flex: 0 0 60px;
        width: 60px;
        height: 60px;
        margin-bottom: 16px;
    }
    .connect-ha-icon :global(.sf-icon),
    .connect-ha-icon :global(svg) {
        width: 100%;
        height: 100%;
    }
    .connect-ha strong {
        width: 100%;
        font-size: 24px;
        font-weight: 500;
        line-height: 1.2;
        white-space: nowrap;
    }
    .connect-ha > span:last-child {
        max-width: 280px;
        font-size: 18px;
        line-height: 1.35;
    }
    .image-placeholder {
        display: grid;
        width: 145px;
        height: 105px;
        margin-bottom: 0.5rem;
        place-items: center;
    }
    .image-placeholder :global(.sf-icon),
    .image-placeholder :global(svg) {
        width: 100%;
        height: 100%;
    }
    .edit-actions {
        margin-bottom: 12px;
        padding: 0 20px;
    }
    .edit-actions button {
        display: flex;
        width: 100%;
        height: 68px;
        padding: 4px 8px;
        align-items: center;
        justify-content: center;
        gap: 10px;
        color: white;
        border: 1px dashed rgba(1, 119, 253, 1);
        border-radius: 12px;
        background: #000;
        font-size: 24px;
        line-height: 1;
        opacity: 1;
    }
    .edit-actions button i {
        font-size: 28px;
        line-height: 1;
    }
    .screen-entities {
        display: grid;
        max-height: calc(480px - 48px);
        padding: 0 0.35rem;
        gap: 8px;
        overflow-y: auto;
        overscroll-behavior: contain;
        scrollbar-width: thin;
        scrollbar-color: rgba(255, 255, 255, 0.24) transparent;
    }
    .edit-actions + .screen-entities {
        max-height: 308px;
    }
    .screen-entity-sort-item {
        position: relative;
        min-width: 0;
        border-radius: 0.65rem;
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        user-select: none;
    }
    .screen-entities::-webkit-scrollbar {
        width: 0.3rem;
    }
    .screen-entities::-webkit-scrollbar-track {
        background: transparent;
    }
    .screen-entities::-webkit-scrollbar-thumb {
        border-radius: 999px;
        background: rgba(255, 255, 255, 0.24);
    }
    .add-screen-card {
        position: absolute;
        top: calc(var(--add-screen-row) * (var(--screen-card-height) + var(--screen-grid-gap)));
        left: calc(var(--add-screen-column) * (var(--screen-card-width) + var(--screen-grid-gap)));
        display: flex;
        padding: 0;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: calc(1rem * var(--screen-preview-scale));
        color: #fff;
        background: rgba(48, 49, 50, 1);
        font-size: calc(1.5rem * var(--screen-preview-scale));
        font-weight: 600;
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
    @media (max-width: 700px) {
        .screen-config-page {
            --mobile-device-preview-width: 18rem;
            --mobile-device-preview-height: 27rem;
            --screen-card-width: 19.75rem;
            --screen-card-height: auto;
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
        .screen-grid-layout.editing .screen-grid {
            padding-top: 5rem;
        }
        .screen-card-shell {
            display: block;
            width: var(--screen-card-width);
            height: auto;
        }
        .screen-card {
            height: auto;
            padding: 0.875rem;
        }
        .screen-card-title {
            margin-bottom: 0.75rem;
            font-size: 1.25rem;
        }
        .device-preview-frame {
            left: auto;
            width: var(--mobile-device-preview-width);
            height: var(--mobile-device-preview-height);
            transform: none;
        }
        .device-preview {
            width: var(--mobile-device-preview-width);
            height: var(--mobile-device-preview-height);
            transform: none;
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
        .connect-ha {
            height: calc(var(--mobile-device-preview-height) - 3rem);
            padding: 1.5rem 1.25rem;
            gap: 0.5rem;
        }
        .connect-ha-icon {
            width: 3.75rem;
            height: 3.75rem;
            margin-bottom: 1rem;
            flex-basis: 3.75rem;
        }
        .connect-ha strong {
            font-size: 1.5rem;
        }
        .connect-ha > span:last-child {
            max-width: 17.5rem;
            font-size: 1.125rem;
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
        .edit-actions button i {
            font-size: 1.75rem;
        }
        .screen-entities {
            max-height: calc(var(--mobile-device-preview-height) - 3rem);
            gap: 0.5rem;
        }
        .edit-actions + .screen-entities {
            max-height: calc(var(--mobile-device-preview-height) - 8rem);
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
    @media (max-width: 340px) {
        .screen-config-page {
            --mobile-device-preview-width: 16.5rem;
            --mobile-device-preview-height: 24.75rem;
            --screen-card-width: 18.25rem;
        }
    }
</style>
