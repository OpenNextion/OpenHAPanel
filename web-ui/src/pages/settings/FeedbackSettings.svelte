<script lang="ts">
    import { _ } from 'svelte-i18n';
    import PageHeader from '@/component/PageHeader/PageHeader.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import { HOME_ASSISTANT_ENTITY_SUPPORT, NEXTION_GUIDE_AND_FAQ, ROUTES_PATH, SETTINGS_FEEDBACK } from '@/constant';
</script>

<div class="page-container feedback-settings-page">
    <PageHeader title={$_('page.settings.feedback')} back={ROUTES_PATH.SETTINGS} />

    <div class="feedback-links">
        <SettingRowGroup>
            <SettingRow title={$_('page.settings.haEntitySupport')} titleCol={9} contentCol={3} href={HOME_ASSISTANT_ENTITY_SUPPORT} trailingIcon="blank.svg" />
            <SettingRow title={$_('page.settings.nextionGuideFaq')} titleCol={9} contentCol={3} href={NEXTION_GUIDE_AND_FAQ} trailingIcon="blank.svg" />
        </SettingRowGroup>

        <SettingRow title={$_('page.settings.problemFeedback')} href={SETTINGS_FEEDBACK} trailingIcon="blank.svg" />
    </div>
</div>

<style>
    .feedback-settings-page {
        max-width: 1080px;
    }

    .feedback-links {
        display: grid;
        gap: 1.25rem;
        margin-top: 1.25rem;
    }
</style>
