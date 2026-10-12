<script lang="ts">
    import { _ } from 'svelte-i18n';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { networkStore } from '@/stores/network';
    import { EIPAcquisition } from '@/types/network';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { ROUTES_PATH } from '@/constant';
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.wlanSettings')} back="/network" />
    <SettingRowGroup>
        <SettingRow title="SSID" content={$networkStore.wifi?.ssid || '--'} route={ROUTES_PATH.NETWORK_WLAN_SSID} />
        <SettingRow
            title={$_('networkPage.ipSettings')}
            content={$networkStore.wifi?.ipAcquisition === EIPAcquisition.STATIC || $networkStore.wifi?.dhcpEnabled === false ? $_('networkPage.manualIp') : 'DHCP'}
            route={ROUTES_PATH.NETWORK_WLAN_IP}
        />
    </SettingRowGroup>
</div>
