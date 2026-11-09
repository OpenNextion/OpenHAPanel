<script lang="ts">
    import { _ } from 'svelte-i18n';
    import Button from '@/component/Button/Button.svelte';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { deviceStore, updateDeviceField, updateTheme } from '@/stores/device';
    import { EMessageType, sendMessage } from '@/stores/message';
    import type { IScreenThemeNotify, TScreenTheme } from '@/types/device';
    import EThemeMode from '@/types/EThemeMode';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';

    const webThemeOptions = [
        { label: 'settingsDetail.lightMode', value: EThemeMode.NORMAL },
        { label: 'settingsDetail.darkMode', value: EThemeMode.DARK },
    ];
    const screenThemeOptions: { label: string; value: TScreenTheme }[] = [
        { label: 'settingsDetail.lightMode', value: 'light' },
        { label: 'settingsDetail.darkMode', value: 'dark' },
    ];

    let followSystem = $state($deviceStore.followSystem);
    let selectedWebTheme = $state($deviceStore.currentMode);
    let selectedScreenTheme = $state<TScreenTheme | null>($deviceStore.screenTheme);
    let screenThemeTouched = $state(false);
    let loading = $state(false);

    $effect(() => {
        if (!loading && !screenThemeTouched) selectedScreenTheme = $deviceStore.screenTheme;
    });

    async function save() {
        if (loading) return;
        loading = true;
        try {
            if (screenThemeTouched && selectedScreenTheme && selectedScreenTheme !== $deviceStore.screenTheme) {
                const response = await sendForReply<IScreenThemeNotify>({
                    method: EWsMethods.SET_SCREEN_THEME,
                    data: { theme: selectedScreenTheme },
                });
                if (response.error !== 0) {
                    sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
                    return;
                }
                updateDeviceField('screenTheme', selectedScreenTheme);
            }

            const result = await updateTheme({ currentMode: selectedWebTheme, followSystem });
            if (result.error !== 0) {
                sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
                return;
            }
            selectedWebTheme = $deviceStore.currentMode;
            followSystem = $deviceStore.followSystem;
            screenThemeTouched = false;
            sendMessage(EMessageType.SUCCESS, $_('TIPS.SAVED'));
        } catch (error) {
            console.error('Failed to save theme settings:', error);
            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail dark-mode-settings">
    <SettingPageHeader title={$_('page.settings.darkMode')} />
    <SettingRowGroup category={$_('settingsDetail.webConsole')}>
        <SettingRow titleCol={10} contentCol={2}>
            {#snippet titleSnippet()}
                <div class="theme-follow-copy">
                    <span>{$_('page.settings.darkMode')}</span>
                    <small>{$_('settingsDetail.followHint')}</small>
                </div>
            {/snippet}
            {#snippet contentSnippet()}
                <span class="form-check form-switch">
                    <input
                        class="form-check-input"
                        type="checkbox"
                        aria-label={$_('settingsDetail.followSystem')}
                        bind:checked={followSystem}
                        disabled={loading}
                    />
                </span>
            {/snippet}
        </SettingRow>
    </SettingRowGroup>

    <div class="web-theme-options">
        <SettingRowGroup>
            {#each webThemeOptions as option (option.value)}
                <SettingRow title={$_(option.label)} disable={loading || followSystem} onclick={() => (selectedWebTheme = option.value)}>
                    {#snippet contentSnippet()}
                        {#if !followSystem && selectedWebTheme === option.value}<span class="theme-check" aria-hidden="true"><SvgIcon name="check.svg" /></span>{/if}
                    {/snippet}
                </SettingRow>
            {/each}
        </SettingRowGroup>
    </div>

    <div class="screen-theme-options">
        <SettingRowGroup category={$_('settingsDetail.screenDevice')}>
            {#each screenThemeOptions as option (option.value)}
                <SettingRow
                    title={$_(option.label)}
                    disable={loading}
                    onclick={() => {
                        selectedScreenTheme = option.value;
                        screenThemeTouched = true;
                    }}
                >
                    {#snippet contentSnippet()}
                        {#if selectedScreenTheme === option.value}<span class="theme-check" aria-hidden="true"><SvgIcon name="check.svg" /></span>{/if}
                    {/snippet}
                </SettingRow>
            {/each}
        </SettingRowGroup>
    </div>

    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" {loading} onclick={save}>{$_('common.save')}</Button>
</div>

<style>
    .web-theme-options {
        margin-top: 1.25rem;
    }
    .screen-theme-options {
        margin-top: 1.5rem;
    }
    .theme-follow-copy {
        display: flex;
        min-width: 0;
        flex-direction: column;
        gap: 0.25rem;
    }
    .theme-follow-copy small {
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        line-height: 1.125rem;
    }
    .theme-check {
        display: inline-flex;
        color: var(--primary-button-bg);
    }
</style>
