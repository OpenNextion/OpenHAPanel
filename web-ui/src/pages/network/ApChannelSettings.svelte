<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { networkStore, setApConfig } from '@/stores/network';
    import Button from '@/component/Button/Button.svelte';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import ApInUseConfirmModal from './ApInUseConfirmModal.svelte';
    import { EMessageType, sendMessage } from '@/stores/message';
    let channel = $state(1);
    let bandwidth = $state(20);
    let initialized = false;
    let loading = $state(false);
    let confirmVisible = $state(false);
    const channelOptions = Array.from({ length: 13 }, (_, index) => {
        const value = index + 1;
        return { value, label: `${value} (${2407 + value * 5} MHz)` };
    });
    const bandwidthOptions = [
        { value: 20, label: '20 MHz' },
        { value: 40, label: '40 MHz' },
    ];
    const channelLabel = $derived(channelOptions.find((item) => item.value === channel)?.label ?? String(channel));
    const bandwidthLabel = $derived(bandwidthOptions.find((item) => item.value === bandwidth)?.label ?? `${bandwidth} MHz`);
    $effect(() => {
        if (!initialized && $networkStore.ap) {
            channel = $networkStore.ap.channel;
            bandwidth = $networkStore.ap.bandwidth;
            initialized = true;
        }
    });
    async function requestSave() {
        if (loading) return;
        if (($networkStore.ap?.clients.current ?? 0) > 0) {
            confirmVisible = true;
            return;
        }
        await save();
    }
    async function save() {
        if (loading) return;
        loading = true;
        try {
            const response = await setApConfig({ channel, bandwidth });
            if (response.error === 0) {
                confirmVisible = false;
                sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
                await pop();
            } else {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
            }
        } catch (error) {
            console.error('Failed to update AP channel:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.channel')} back="/network/ap" />
    <div class="network-form">
        <label for="ap-channel">{$_('networkPage.channelNumber')}</label>
        <DropDown id="ap-channel" bind:value={channel} options={channelOptions} ariaLabel={$_('networkPage.channelNumber')}><span>{channelLabel}</span></DropDown>
        <label for="ap-bandwidth">{$_('networkPage.bandwidth')}</label>
        <DropDown id="ap-bandwidth" bind:value={bandwidth} options={bandwidthOptions} ariaLabel={$_('networkPage.bandwidth')}><span>{bandwidthLabel}</span></DropDown>
    </div>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={requestSave}>{$_('common.save')}</Button>
</div>

<ApInUseConfirmModal visible={confirmVisible} {loading} confirm={save} cancel={() => (confirmVisible = false)} />

<style>
    .network-form :global(.dropdown) {
        width: 100%;
        min-height: 48px;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.65rem;
        background: var(--bs-body-bg);
    }
    .network-form :global(.dropdown-toggle) {
        min-height: 48px;
        padding: 0.5rem 0.85rem;
    }
</style>
