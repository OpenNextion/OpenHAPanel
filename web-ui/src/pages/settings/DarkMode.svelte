<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { deviceStore, updateTheme } from '@/stores/device';
    import EThemeMode from '@/types/EThemeMode';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    let followSystem = $state($deviceStore.followSystem);
    let selected = $state($deviceStore.currentMode);
    let loading = $state(false);
    async function saveTheme(currentMode = selected, nextFollowSystem = followSystem) {
        if (loading) return;
        loading = true;
        try {
            const result = await updateTheme({ currentMode, followSystem: nextFollowSystem });
            if (result.error === 0) return;
            selected = $deviceStore.currentMode;
            followSystem = $deviceStore.followSystem;
        } catch (error) {
            console.error('Failed to update theme:', error);
            selected = $deviceStore.currentMode;
            followSystem = $deviceStore.followSystem;
        } finally {
            loading = false;
        }
    }
    function select(theme: EThemeMode) {
        if (followSystem || loading) return;
        selected = theme;
        void saveTheme(theme, followSystem);
    }
    function toggleFollowSystem() {
        void saveTheme(selected, followSystem);
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.darkMode')} />
    <div class="follow-system-setting">
        <SettingRow title={$_('settingsDetail.followSystem')} margin={0}>
            {#snippet contentSnippet()}<span class="form-check form-switch"
                    ><input class="form-check-input" type="checkbox" bind:checked={followSystem} disabled={loading} onchange={toggleFollowSystem} /></span
                >{/snippet}
        </SettingRow>
        <p>{$_('settingsDetail.followHint')}</p>
    </div>
    <SettingRowGroup>
        {#each [{ label: 'settingsDetail.lightMode', value: EThemeMode.NORMAL }, { label: 'settingsDetail.darkMode', value: EThemeMode.DARK }] as item}
            <SettingRow title={$_(item.label)} disable={loading || followSystem} onclick={() => select(item.value)}>
                {#snippet contentSnippet()}{#if !followSystem && selected === item.value}<span class="theme-check"><SvgIcon name="check.svg" /></span>{/if}{/snippet}
            </SettingRow>
        {/each}
    </SettingRowGroup>
</div>

<style>
    .theme-check {
        display: inline-flex;
        color: rgba(1, 119, 253, 1);
    }
    .follow-system-setting {
        margin-bottom: 1.25rem;
    }
    .follow-system-setting p {
        margin: 0.5rem 0.75rem 0;
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        line-height: 1.4;
    }
</style>
