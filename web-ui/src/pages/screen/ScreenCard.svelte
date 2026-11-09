<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { dragHandle } from 'svelte-dnd-action';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import type { IHassScreenConfigBadge, IHassScreenConfigCard, IHassScreenConfigItem } from '@/types/screenConfig';
    import DeviceScreenPreview from './DeviceScreenPreview.svelte';

    let {
        screen,
        screenTitle,
        screens,
        editing,
        isHaConfigured,
        previewTime,
        onRename,
        onAddBadge,
        onAddEntity,
        onBadgeSettings,
        onEntitySettings,
        onReorderBadges,
        onReorderCards,
    }: {
        screen: IHassScreenConfigItem;
        screenTitle: string;
        screens: IHassScreenConfigItem[];
        editing: boolean;
        isHaConfigured: boolean;
        previewTime: string;
        onRename: () => void;
        onAddBadge: () => void;
        onAddEntity: () => void;
        onBadgeSettings: (entityId: string) => void;
        onEntitySettings: (entityId: string) => void;
        onReorderBadges: (badges: IHassScreenConfigBadge[]) => void;
        onReorderCards: (cards: IHassScreenConfigCard[]) => void;
    } = $props();
</script>

<article class="screen-card">
    <div class:editing class="screen-card-title" use:dragHandle aria-label={$_('page.screen.reorderScreen', { values: { name: screenTitle } })}>
        <div class="screen-card-title-main">
            <span>{screenTitle}</span>
            {#if editing}
                <button
                    type="button"
                    aria-label={$_('page.screen.rename')}
                    onmousedown={(event) => event.stopPropagation()}
                    ontouchstart={(event) => event.stopPropagation()}
                    onkeydown={(event) => event.stopPropagation()}
                    onclick={onRename}
                ><SvgIcon name="edit.svg" /></button>
            {/if}
        </div>
        <div aria-hidden="true">
            {#if editing}
                <span class="screen-card-drag-handle"><SvgIcon name="sort.svg" /></span>
            {/if}
        </div>
    </div>
    <DeviceScreenPreview
        {screen}
        {screenTitle}
        {screens}
        {editing}
        {isHaConfigured}
        {previewTime}
        {onAddBadge}
        {onAddEntity}
        {onBadgeSettings}
        {onEntitySettings}
        {onReorderBadges}
        {onReorderCards}
    />
</article>
