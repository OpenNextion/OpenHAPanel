<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import { pop } from 'svelte-spa-router';
    import { deviceStore, updateDeviceName } from '@/stores/device';
    import { NAME_MAX_LENGTH } from '@/constant';
    let name = $state($deviceStore.name);
    let loading = $state(false);
    async function save() {
        if (!name.trim() || loading) return;
        loading = true;
        try {
            const result = await updateDeviceName(name.trim());
            if (result.error === 0) await pop();
        } catch (error) {
            console.error('Failed to update device name:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.deviceName')} />
    <div class="input-card"><input class="form-control" bind:value={name} maxlength={NAME_MAX_LENGTH} aria-label={$_('page.settings.deviceName')} /><button type="button" aria-label={$_('common.clear')} onclick={() => (name = '')}><i class="bi bi-x-circle-fill"></i></button></div>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" onclick={save} disabled={!name.trim()} {loading}>{$_('common.save')}</Button>
</div>
