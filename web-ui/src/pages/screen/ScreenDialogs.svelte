<script lang="ts">
    import { _ } from 'svelte-i18n';
    import Modal from '@/component/Modal/Modal.svelte';

    let {
        addScreenVisible,
        leaveConfirmVisible,
        screenSettingsVisible,
        newScreenTitle,
        screenTitleCache,
        maxScreenTitleLength,
        canDeleteScreen,
        onCloseAddScreen,
        onNewScreenTitleChange,
        onAddScreen,
        onDiscardAndLeave,
        onContinueEditing,
        onCloseScreenSettings,
        onScreenTitleChange,
        onDeleteScreen,
        onSaveScreenSettings,
    }: {
        addScreenVisible: boolean;
        leaveConfirmVisible: boolean;
        screenSettingsVisible: boolean;
        newScreenTitle: string;
        screenTitleCache: string;
        maxScreenTitleLength: number;
        canDeleteScreen: boolean;
        onCloseAddScreen: () => void;
        onNewScreenTitleChange: (value: string) => void;
        onAddScreen: () => void;
        onDiscardAndLeave: () => void;
        onContinueEditing: () => void;
        onCloseScreenSettings: () => void;
        onScreenTitleChange: (value: string) => void;
        onDeleteScreen: () => void;
        onSaveScreenSettings: () => void;
    } = $props();
</script>

<Modal modalVisible={addScreenVisible} footer={false} setVisible={(visible) => !visible && onCloseAddScreen()}>
    {#snippet customSnippet()}
        <form class="add-screen-dialog" onsubmit={(event) => { event.preventDefault(); onAddScreen(); }}>
            <h2>{$_('page.screen.addSplit')}</h2>
            <label for="new-screen-title">{$_('page.screen.splitName')}</label>
            <input
                id="new-screen-title"
                value={newScreenTitle}
                maxlength={maxScreenTitleLength}
                placeholder={$_('page.screen.splitNamePlaceholder')}
                oninput={(event) => onNewScreenTitleChange(event.currentTarget.value)}
            />
            <footer>
                <button type="button" onclick={onCloseAddScreen}>{$_('common.cancel')}</button>
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
                <button type="button" onclick={onDiscardAndLeave}>{$_('page.screen.discardAndLeave')}</button>
                <button class="continue-editing-button" type="button" onclick={onContinueEditing}>{$_('page.screen.continueEditing')}</button>
            </footer>
        </div>
    {/snippet}
</Modal>

<Modal modalVisible={screenSettingsVisible} footer={false} setVisible={(visible) => !visible && onCloseScreenSettings()}>
    {#snippet customSnippet()}
        <form class="add-screen-dialog" aria-label={$_('page.screen.editSplit')} onsubmit={(event) => { event.preventDefault(); onSaveScreenSettings(); }}>
            <h2>{screenTitleCache}</h2>
            <label for="edit-screen-title">{$_('page.screen.splitName')}</label>
            <input id="edit-screen-title" value={screenTitleCache} maxlength={maxScreenTitleLength} oninput={(event) => onScreenTitleChange(event.currentTarget.value)} />
            <footer>
                <button class="delete-screen-button" type="button" disabled={!canDeleteScreen} onclick={onDeleteScreen}>{$_('page.screen.deleteSplit')}</button>
                <button class="save-screen-button" type="submit" disabled={!screenTitleCache.trim()}>{$_('common.save')}</button>
            </footer>
        </form>
    {/snippet}
</Modal>
