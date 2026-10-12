<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { ROUTES_PATH } from '@/constant';
    let enabled = false;
    let start = '20:00';
    let end = '07:00';
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.nightMode')} back={ROUTES_PATH.SETTINGS_DISPLAY} />
    <SettingRow title={$_('settingsDetail.enableNight')} tips={$_('settingsDetail.nightHint')}>
        {#snippet contentSnippet()}<span class="form-check form-switch"><input class="form-check-input" type="checkbox" bind:checked={enabled} /></span>{/snippet}
    </SettingRow>
    <SettingRowGroup>
        <SettingRow title={$_('settingsDetail.from')} disable={!enabled}>
            {#snippet contentSnippet()}<input id="night-start" class="form-control" type="time" bind:value={start} disabled={!enabled} />{/snippet}
        </SettingRow>
        <SettingRow title={$_('settingsDetail.to')} disable={!enabled}>
            {#snippet contentSnippet()}<input id="night-end" class="form-control" type="time" bind:value={end} disabled={!enabled} />{/snippet}
        </SettingRow>
    </SettingRowGroup>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)">{$_('common.save')}</Button>
</div>
