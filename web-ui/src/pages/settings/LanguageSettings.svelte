<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import { pop } from 'svelte-spa-router';
    import { languageStore, setLanguage } from '@/stores/language';
    import ELanguage from '@/types/language';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    let selected = $state($languageStore.language);
    let loading = $state(false);
    async function save() {
        if (loading) return;
        loading = true;
        try {
            const { wsResponse } = await setLanguage(selected);
            if (wsResponse.error === 0) {
                await pop();
            }
        } catch (error) {
            console.error('Failed to update language:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.language')} />
    <SettingRowGroup>
        {#each [{ label: '简体中文', value: ELanguage.ZH_CN }, { label: 'English', value: ELanguage.EN_US }] as item}
            <SettingRow title={item.label} disable={loading} onclick={() => (selected = item.value)}>
                {#snippet contentSnippet()}{#if selected === item.value}<span class="language-check"><SvgIcon name="check.svg" /></span>{/if}{/snippet}
            </SettingRow>
        {/each}
    </SettingRowGroup>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" onclick={save} {loading}>{$_('common.save')}</Button>
</div>

<style>
    .language-check {
        display: inline-flex;
        color: rgba(1, 119, 253, 1);
    }
</style>
