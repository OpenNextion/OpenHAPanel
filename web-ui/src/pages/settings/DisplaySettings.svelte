<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { ROUTES_PATH } from '@/constant';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { deviceStore, updateDeviceField } from '@/stores/device';

    let brightness = $state($deviceStore.brightness);
    let loading = $state(false);

    $effect(() => {
        if (!loading && brightness !== $deviceStore.brightness) brightness = $deviceStore.brightness;
    });

    function updateBrightness(value: number) {
        brightness = value;
        updateDeviceField('brightness', value);
    }

    function formatIdleTime(idleTime: number) {
        if (idleTime === 0) return $_('settingsDetail.never');
        if (idleTime < 60) return `${idleTime} s`;
        return `${idleTime / 60} min`;
    }

    async function save() {
        if (loading) return;
        loading = true;
        try {
            const response = await sendForReply<{ brightness: number }>({
                method: EWsMethods.SET_BRIGHTNESS,
                data: { brightness },
            });
            if (response.error === 0) {
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
            } else {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
            }
        } catch (error) {
            console.error('Failed to set display brightness:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.display')} />
    <SettingRow title={$_('settingsDetail.brightness')} content={`${brightness}%`}>
        {#snippet footerSnippet()}<div class="brightness-control" style={`--brightness: ${brightness}%`}><i class="bi bi-sun-fill"></i><input id="display-brightness" type="range" min="1" max="100" value={brightness} oninput={(event) => updateBrightness(Number(event.currentTarget.value))} /></div>{/snippet}
    </SettingRow>
    <SettingRowGroup>
        <!--<SettingRow title={$_('page.settings.nightMode')} content={$_('common.disabled')} route={ROUTES_PATH.SETTINGS_NIGHT_MODE} />-->
        <SettingRow title={$_('settingsDetail.screenLock')} content={formatIdleTime($deviceStore.idleTime)} route={ROUTES_PATH.SETTINGS_SCREEN_LOCK} />
    </SettingRowGroup>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={save}>{$_('common.save')}</Button>
</div>
