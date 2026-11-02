<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import { pop } from 'svelte-spa-router';
    import { useLanguageSettings } from '@/hooks/useLanguageSettings.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    const language = useLanguageSettings();
    async function save() {
        if (await language.save()) await pop();
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.language')} />
    <SettingRowGroup>
        {#each language.options as item}
            <SettingRow title={item.label} disable={language.state.loading} onclick={() => (language.state.selected = item.value)}>
                {#snippet contentSnippet()}{#if language.state.selected === item.value}<span class="language-check"><SvgIcon name="check.svg" /></span>{/if}{/snippet}
            </SettingRow>
        {/each}
    </SettingRowGroup>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" onclick={save} loading={language.state.loading}>{$_('common.save')}</Button>
</div>

<style>
    .language-check {
        display: inline-flex;
        color: rgba(1, 119, 253, 1);
    }
</style>
