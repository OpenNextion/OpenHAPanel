<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { deviceStore } from '@/stores/device';
    import { networkStore } from '@/stores/network';
    import Modal from '@/component/Modal/Modal.svelte';
    import sendForReply from '@/ws/sendForReply';
    import { EWsMethods } from '@/types/websocket';
    import { ERebootType } from '@/types/device';
    import { getWebsocket } from '@/ws/initWebsocket';
    import { wsReconnectManager } from '@/ws/wsReconnect';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { ROUTES_PATH } from '@/constant';

    let modalVisible = $state(false);
    let confirmLoading = $state(false);
    let restartSuccess = $state(false);
    let current = $state(ERebootType.ESP32);

    const confirmReboot = () => {
        if (confirmLoading) return;
        const target = current;
        try {
            // 当前固件重启后不会响应请求，因此不等待该 Promise。
            confirmLoading = true;
            restartSuccess = false;
            void sendForReply<{ target: ERebootType }>({
                method: EWsMethods.REBOOT_DEVICE,
                data: { target },
            }).catch(() => undefined);
        } finally {
            setTimeout(() => {
                confirmLoading = false;
                modalVisible = false;
                getWebsocket()?.disconnect();
                wsReconnectManager.start();
            }, 2000);
        }
    };
    const systemItems = [
        { label: 'page.settings.time', path: ROUTES_PATH.SETTINGS_TIME },
        //{ label: 'page.settings.language', path: ROUTES_PATH.SETTINGS_LANGUAGE },
        { label: 'page.settings.display', path: ROUTES_PATH.SETTINGS_DISPLAY },
        { label: 'page.settings.darkMode', path: ROUTES_PATH.SETTINGS_DARK_MODE },
        //{ label: 'page.settings.debugLog', path: ROUTES_PATH.SETTINGS_DEBUG },
    ];
</script>

<div class="page-container settings-page">
    <div class="settings-groups">
        <SettingRowGroup category={$_('page.settings.basic')} margin={20}>
            <SettingRow title={$_('page.settings.deviceName')} content={$deviceStore.name || '--'} route={ROUTES_PATH.SETTINGS_DEVICE_NAME} />
            <SettingRow title={$_('page.settings.mdns')} content={$networkStore.mdnsDomain || '--'} route={ROUTES_PATH.SETTINGS_MDNS} />
        </SettingRowGroup>
        <SettingRowGroup category={$_('page.settings.system')}>
            {#each systemItems as item}
                <SettingRow title={$_(item.label)} route={item.path} />
            {/each}
        </SettingRowGroup>
        <SettingRow title={$_('page.settings.feedback')} route={ROUTES_PATH.SETTINGS_FEEDBACK} margin={0} />
        <SettingRow title={$_('page.settings.restartDevice')} margin={0}>
            {#snippet contentSnippet()}<button class="reboot-button" type="button" onclick={() => (modalVisible = true)}>{$_('page.settings.restart')}</button>{/snippet}
        </SettingRow>
    </div>
</div>

<Modal
    {modalVisible}
    {confirmLoading}
    title={$_('settingsDetail.restartConfirmTitle')}
    subTitle={$_('settingsDetail.restartConfirmHint')}
    confirmTitle={$_('page.settings.restart')}
    cancelBtn={true}
    confirm={confirmReboot}
    cancel={() => (modalVisible = false)}
    setVisible={(visible) => (modalVisible = visible)}
/>

<style>
    .settings-groups {
        display: flex;
        flex-direction: column;
        gap: 1.25rem;
    }
    .reboot-button {
        display: inline-flex;
        width: 6.5rem;
        height: 2.5rem;
        padding: 0.5rem 1rem;
        gap: 0.5rem;
        align-items: center;
        justify-content: center;
        color: #fff;
        border: 0;
        border-radius: 1.8125rem;
        background: rgba(1, 119, 253, 1);
    }
</style>
