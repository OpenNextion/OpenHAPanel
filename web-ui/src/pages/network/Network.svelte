<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { networkStore } from '@/stores/network';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { ROUTES_PATH } from '@/constant';
</script>

<div class="page-container network-page">
    <div class="page-heading connection-title">
        <h1>{$_('networkPage.title')}</h1>
    </div>
    <SettingRowGroup margin={20}>
        <SettingRow title={$_('networkPage.apSettings')} content={$networkStore.ap?.defaultEnabled ? $_('common.enabled') : $_('common.disabled')} route={ROUTES_PATH.NETWORK_AP} />
        <SettingRow title={$_('networkPage.wlanSettings')} route={ROUTES_PATH.NETWORK_WLAN}>
            {#snippet contentSnippet()}<span class="network-wlan-content d-flex flex-column"><strong>{$networkStore.wifi?.connected ? $_('networkPage.connected') : $_('networkPage.disconnected')}</strong><small class="network-wlan-ssid">{$networkStore.wifi?.ssid || '--'}</small></span>{/snippet}
        </SettingRow>
    </SettingRowGroup>
</div>

<style>
    .network-wlan-content {
        min-width: 0;
        max-width: 100%;
        text-align: right;
    }

    .network-wlan-content strong,
    .network-wlan-ssid {
        min-width: 0;
        max-width: 100%;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
</style>
