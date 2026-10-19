<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { initializeWifiIpDraft, networkStore, setWifiConfig, updateWifiIpDraftMode, wifiIpDraft, wifiIpDraftMode } from '@/stores/network';
    import { EIPAcquisition } from '@/types/network';
    import Button from '@/component/Button/Button.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { REG_EXP_IPV4 } from '@/constant/regRex';
    import { EMessageType, sendMessage } from '@/stores/message';
    let initialized = false;
    let loading = $state(false);
    let showValidationError = $state(false);
    $effect(() => {
        if (!initialized && $networkStore.wifi) {
            initializeWifiIpDraft($networkStore.wifi);
            initialized = true;
        }
    });
    const fields = [
        { key: 'ipAddress', path: 'address', label: 'networkPage.ipAddress' },
        { key: 'subnetMask', path: 'netmask', label: 'networkPage.netmask' },
        { key: 'gateway', path: 'gateway', label: 'networkPage.gateway' },
        { key: 'dns1', path: 'dns1', label: 'networkPage.dns1' },
        { key: 'dns2', path: 'dns2', label: 'networkPage.dns2' },
    ] as const;
    let staticIpValid = $derived.by(() => fields.every((field) => REG_EXP_IPV4.test($wifiIpDraft[field.key].trim())));
    async function save() {
        if (loading) return;
        if ($wifiIpDraftMode === EIPAcquisition.STATIC && !staticIpValid) {
            showValidationError = true;
            return;
        }
        const savedMode = $wifiIpDraftMode;
        const currentWifi = $networkStore.wifi;
        const currentMode = currentWifi && (currentWifi.ipAcquisition ?? (currentWifi.dhcpEnabled ? EIPAcquisition.DHCP : EIPAcquisition.STATIC));
        const unchanged = currentMode === savedMode &&
            (savedMode === EIPAcquisition.DHCP || fields.every(({ key }) => $wifiIpDraft[key].trim() === currentWifi?.[key]));
        if (unchanged) {
            await pop();
            return;
        }
        loading = true;
        try {
            const targetIp = $wifiIpDraft.ipAddress.trim();
            const response = await setWifiConfig({
                dhcpEnabled: savedMode === EIPAcquisition.DHCP,
                ipAcquisition: savedMode,
                ...(savedMode === EIPAcquisition.STATIC ? $wifiIpDraft : {}),
            });
            if (response.error === 0) {
                initializeWifiIpDraft($networkStore.wifi, true);
                if (savedMode === EIPAcquisition.STATIC && REG_EXP_IPV4.test(window.location.hostname)) {
                    const target = new URL(window.location.href);
                    target.hostname = targetIp;
                    if (target.hostname !== window.location.hostname) {
                        target.hash = '#/';
                        window.location.replace(target.toString());
                        return;
                    }
                }
                await pop();
            } else if (response.error === 402 && savedMode === EIPAcquisition.STATIC) {
                sendMessage(EMessageType.ERROR, $_('networkPage.staticIpConflict'));
            }
        } catch (error) {
            console.error('Failed to update Wi-Fi IP settings:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.ipSettings')} back="/network/wlan" />
    <SettingRowGroup>
        <SettingRow title="DHCP" onclick={() => updateWifiIpDraftMode(EIPAcquisition.DHCP)}>{#snippet contentSnippet()}{#if $wifiIpDraftMode === EIPAcquisition.DHCP}<SvgIcon name="check.svg" />{/if}{/snippet}</SettingRow>
        <SettingRow title={$_('networkPage.manualIp')} onclick={() => updateWifiIpDraftMode(EIPAcquisition.STATIC)}>{#snippet contentSnippet()}{#if $wifiIpDraftMode === EIPAcquisition.STATIC}<SvgIcon name="check.svg" />{/if}{/snippet}</SettingRow>
    </SettingRowGroup>
    {#if $wifiIpDraftMode === EIPAcquisition.STATIC}
        <SettingRowGroup>
            {#each fields as field}<SettingRow title={$_(field.label)} content={$wifiIpDraft[field.key] || '--'} route={`/network/wlan/ip/${field.path}`} />{/each}
        </SettingRowGroup>
        {#if showValidationError && !staticIpValid}<small class="d-block text-danger mb-3">{$_('networkPage.staticIpv4Invalid')}</small>{/if}
    {/if}
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={save}>{$_('common.save')}</Button>
</div>
