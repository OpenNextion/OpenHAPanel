<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { deviceStore, updateDebugLog } from '@/stores/device';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    let enabled = $state($deviceStore.debugLogEnabled);
    let loading = $state(false);
    async function toggleDebug() {
        if (loading) return;
        const previous = !enabled;
        loading = true;
        try {
            const result = await updateDebugLog(enabled);
            if (result.error !== 0) enabled = previous;
        } catch (error) {
            console.error('Failed to update Debug log:', error);
            enabled = previous;
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.debugLog')} />
    <SettingRow title={$_('settingsDetail.enableDebug')} tips={$_('settingsDetail.debugHint')}>
        {#snippet contentSnippet()}<span class="form-check form-switch"><input class="form-check-input" type="checkbox" bind:checked={enabled} disabled={loading} onchange={toggleDebug} /></span>{/snippet}
    </SettingRow>
</div>
