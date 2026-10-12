<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { networkStore, setApConfig } from '@/stores/network';
    import Button from '@/component/Button/Button.svelte';
    import ApInUseConfirmModal from './ApInUseConfirmModal.svelte';
    import { REG_EXP_IPV4, REG_EXP_IPV4_NETMASK } from '@/constant/regRex';
    let address = $state('');
    let netmask = $state('');
    let initialized = false;
    let loading = $state(false);
    let showErrors = $state(false);
    let confirmVisible = $state(false);
    let addressValid = $derived(REG_EXP_IPV4.test(address.trim()));
    let netmaskValid = $derived(REG_EXP_IPV4_NETMASK.test(netmask.trim()));
    $effect(() => {
        if (!initialized && $networkStore.ap) {
            address = $networkStore.ap.gateway;
            netmask = $networkStore.ap.subnetMask;
            initialized = true;
        }
    });
    async function requestSave() {
        if (loading) return;
        showErrors = true;
        if (!addressValid || !netmaskValid) return;
        if (($networkStore.ap?.clients.current ?? 0) > 0) {
            confirmVisible = true;
            return;
        }
        await save();
    }
    async function save() {
        if (loading || !addressValid || !netmaskValid) return;
        loading = true;
        try {
            const response = await setApConfig({ gateway: address.trim(), subnetMask: netmask.trim() });
            if (response.error === 0) {
                confirmVisible = false;
                await pop();
            }
        } catch (error) {
            console.error('Failed to update AP IPv4 settings:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.ipv4Lan')} back="/network/ap" />
    <div class="network-form">
        <label for="ap-ip">{$_('networkPage.ipAddress')}</label><input id="ap-ip" class="form-control" bind:value={address} inputmode="decimal" />
        {#if showErrors && !addressValid}<small class="text-danger">{$_('networkPage.ipv4Invalid')}</small>{/if}
        <label for="ap-netmask">{$_('networkPage.netmask')}</label><input id="ap-netmask" class="form-control" bind:value={netmask} inputmode="decimal" />
        {#if showErrors && !netmaskValid}<small class="text-danger">{$_('networkPage.netmaskInvalid')}</small>{/if}
    </div>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={requestSave}>{$_('common.save')}</Button>
</div>

<ApInUseConfirmModal visible={confirmVisible} {loading} confirm={save} cancel={() => (confirmVisible = false)} />
