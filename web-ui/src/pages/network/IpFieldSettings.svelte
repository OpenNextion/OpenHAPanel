<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { initializeWifiIpDraft, networkStore, updateWifiIpDraft, wifiIpDraft, type WifiIpDraft } from '@/stores/network';
    import Button from '@/component/Button/Button.svelte';
    import { REG_EXP_IPV4 } from '@/constant/regRex';
    let { params }: { params: Record<string, string> } = $props();
    type WifiIpField = keyof WifiIpDraft;
    const fieldConfig: Record<string, { label: string; field: WifiIpField }> = {
        address: { label: 'networkPage.ipAddress', field: 'ipAddress' },
        netmask: { label: 'networkPage.netmask', field: 'subnetMask' },
        gateway: { label: 'networkPage.gateway', field: 'gateway' },
        dns1: { label: 'networkPage.dns1', field: 'dns1' },
        dns2: { label: 'networkPage.dns2', field: 'dns2' },
    };
    const config = $derived(fieldConfig[params.field] ?? fieldConfig.address);
    let value = $state('');
    let initialized = false;
    let showError = $state(false);
    let valueValid = $derived(REG_EXP_IPV4.test(value.trim()));
    $effect(() => {
        if (!initialized && $networkStore.wifi) {
            initializeWifiIpDraft($networkStore.wifi);
            value = $wifiIpDraft[config.field] || '';
            initialized = true;
        }
    });
    function save() {
        showError = true;
        if (!valueValid) return;
        updateWifiIpDraft(config.field, value.trim());
        void pop();
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_(config.label)} back="/network/wlan/ip" />
    <div class="network-form">
        <input class="form-control ip-field-input" bind:value inputmode="decimal" aria-label={$_(config.label)} oninput={() => (showError = true)} />
        {#if showError && !valueValid}<small class="text-danger">{$_('networkPage.ipv4Invalid')}</small>{/if}
    </div>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" disabled={!valueValid} onclick={save}>{$_('common.save')}</Button>
</div>
