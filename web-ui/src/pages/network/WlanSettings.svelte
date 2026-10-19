<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { push } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { initializeWifiIpDraft, networkStore } from '@/stores/network';
    import { EIPAcquisition } from '@/types/network';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { ROUTES_PATH } from '@/constant';
    function openIpSettings() {
        if ($networkStore.wifi) initializeWifiIpDraft($networkStore.wifi, true);
        void push(ROUTES_PATH.NETWORK_WLAN_IP);
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.wlanSettings')} back="/network" />
    <SettingRowGroup>
        <SettingRow title="SSID" content={$networkStore.wifi?.ssid || '--'} route={ROUTES_PATH.NETWORK_WLAN_SSID} />
        <SettingRow
            title={$_('networkPage.ipSettings')}
            content={$networkStore.wifi?.ipAcquisition === EIPAcquisition.STATIC || $networkStore.wifi?.dhcpEnabled === false ? $_('networkPage.manualIp') : 'DHCP'}
            route={ROUTES_PATH.NETWORK_WLAN_IP}
            onclick={openIpSettings}
        />
    </SettingRowGroup>
</div>
