<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import Button from '@/component/Button/Button.svelte';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { ROUTES_PATH } from '@/constant';
    import { deviceStore, updateDeviceField } from '@/stores/device';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';

    const OPTIONS = [30, 60, 120, 180, 240, 300, 0];
    let selectedIdleTime = $state($deviceStore.idleTime);
    let loading = $state(false);
    const hasChanges = $derived(selectedIdleTime !== $deviceStore.idleTime);

    function formatIdleTime(idleTime: number) {
        if (idleTime === 0) return $_('settingsDetail.never');
        if (idleTime < 60) return `${idleTime} s`;
        return `${idleTime / 60} min`;
    }

    async function save() {
        if (loading) return;
        loading = true;
        try {
            const response = await sendForReply<{ idleTime: number }>({
                method: EWsMethods.SET_IDLE_TIME,
                data: { idleTime: selectedIdleTime },
            });
            if (response.error === 0) {
                updateDeviceField('idleTime', selectedIdleTime);
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
                await pop();
            } else {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
            }
        } catch (error) {
            console.error('Failed to set screen lock idle time:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail screen-lock-settings">
    <SettingPageHeader title={$_('settingsDetail.screenLock')} back={ROUTES_PATH.SETTINGS_DISPLAY} />
    <section class="screen-lock-card">
        <header>
            <h2>{$_('settingsDetail.screenLock')}</h2>
            <p>{$_('settingsDetail.screenLockHint')}</p>
        </header>
        <div class="screen-lock-options">
        {#each OPTIONS as idleTime}
                <button type="button" disabled={loading} onclick={() => (selectedIdleTime = idleTime)}>
                    <span>{formatIdleTime(idleTime)}</span>
                    {#if selectedIdleTime === idleTime}<span class="selected-option"><SvgIcon name="check.svg" /></span>{/if}
                </button>
        {/each}
        </div>
    </section>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} disabled={!hasChanges} onclick={save}>{$_('common.save')}</Button>
</div>

<style>
    .screen-lock-card {
        padding: 1rem;
        overflow: hidden;
        border-radius: 1rem;
        background: var(--body-inner-level-2);
    }
    .screen-lock-card header {
        padding-bottom: 1.5rem;
        border-bottom: 1px solid var(--bs-border-color);
    }
    .screen-lock-card h2 {
        margin: 0;
        color: var(--body-text-color-highlight);
        font-size: 1.25rem;
        font-weight: 400;
        line-height: 1.4;
    }
    .screen-lock-card p {
        margin: 0.15rem 0 0;
        color: var(--body-text-color-grey);
        font-size: 0.875rem;
        line-height: 1.5;
    }
    .screen-lock-options {
        padding-top: 1.5rem;
    }
    .screen-lock-options button {
        display: flex;
        width: 100%;
        min-height: 3.125rem;
        padding: 0;
        align-items: center;
        justify-content: space-between;
        color: var(--body-text-color-highlight);
        border: 0;
        background: transparent;
        font-size: 1.125rem;
        text-align: left;
    }
    .screen-lock-options button:disabled {
        cursor: wait;
        opacity: 0.7;
    }
    .selected-option {
        display: inline-flex;
        flex: 0 0 auto;
        color: rgba(1, 119, 253, 1);
        font-size: 1.5rem;
    }
</style>
