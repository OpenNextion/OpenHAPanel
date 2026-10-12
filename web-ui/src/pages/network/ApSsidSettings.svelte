<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { networkStore, updateApSsid } from '@/stores/network';
    import Button from '@/component/Button/Button.svelte';
    import ApInUseConfirmModal from './ApInUseConfirmModal.svelte';
    const AP_SSID_MAX_LENGTH = 32;
    let ssid = $state('');
    let initialized = false;
    let loading = $state(false);
    let confirmVisible = $state(false);
    $effect(() => {
        if (!initialized && $networkStore.ap) {
            ssid = $networkStore.ap.apName;
            initialized = true;
        }
    });
    async function requestSave() {
        const normalizedSsid = ssid.trim();
        if (!normalizedSsid || normalizedSsid.length > AP_SSID_MAX_LENGTH || loading) return;
        if (($networkStore.ap?.clients.current ?? 0) > 0) {
            confirmVisible = true;
            return;
        }
        await save();
    }
    async function save() {
        const normalizedSsid = ssid.trim();
        if (!normalizedSsid || normalizedSsid.length > AP_SSID_MAX_LENGTH || loading) return;
        loading = true;
        try {
            const response = await updateApSsid(normalizedSsid);
            if (response.error === 0) {
                confirmVisible = false;
                await pop();
            }
        } catch (error) {
            console.error('Failed to update AP SSID:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container network-page network-detail">
    <NetworkPageHeader title={$_('networkPage.editApSsid')} back="/network/ap" />
    <div class="input-card">
        <input class="form-control" bind:value={ssid} maxlength={AP_SSID_MAX_LENGTH} aria-label={$_('networkPage.editApSsid')} /><Button style="width: 2.75rem; height: 100%; border: 0; padding: 0" aria-label={$_('common.clear')} onclick={() => (ssid = '')}
            ><i class="bi bi-x-circle-fill"></i></Button
        >
    </div>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" disabled={!ssid.trim()} {loading} onclick={requestSave}>{$_('common.save')}</Button>
</div>

<ApInUseConfirmModal visible={confirmVisible} {loading} confirm={save} cancel={() => (confirmVisible = false)} />
