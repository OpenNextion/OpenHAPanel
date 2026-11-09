<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import type { IHassScreenConfigBadge, IHassScreenConfigCard, IHassScreenConfigItem } from '@/types/screenConfig';
    import ScreenBadgeList from './ScreenBadgeList.svelte';
    import ScreenEntityList from './ScreenEntityList.svelte';

    let {
        screen,
        screenTitle,
        screens,
        editing,
        isHaConfigured,
        previewTime,
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
        onAddBadge: () => void;
        onAddEntity: () => void;
        onBadgeSettings: (entityId: string) => void;
        onEntitySettings: (entityId: string) => void;
        onReorderBadges: (badges: IHassScreenConfigBadge[]) => void;
        onReorderCards: (cards: IHassScreenConfigCard[]) => void;
    } = $props();

    const hasContent = $derived(Boolean(screen.badges?.length || screen.cards?.length));
</script>

<div class="screen-config-device-preview-frame">
    <div class="screen-config-device-preview">
        <div class="device-status">
            <strong>{screenTitle}</strong>
            <span class="device-status-icons">
                <span class="status-ha-icon"><SvgIcon name="ha.svg" /></span>
                <span class="status-wifi-icon"><SvgIcon name="wifi_4.svg" /></span>
                <b>{previewTime}</b>
            </span>
        </div>
        {#if !isHaConfigured}
            <div class="preview-empty preview-connect">
                <span class="connect-ha-logo" aria-hidden="true"><SvgIcon name="ha.svg" /></span>
                <span class="connect-ha-title">{$_('page.screen.connectHa')}</span>
                <span class="connect-ha-hint">{$_('page.screen.connectHaHint')}</span>
            </div>
        {:else}
            <ScreenBadgeList
                badges={screen.badges ?? []}
                {editing}
                screenIndex={screen.index}
                {screenTitle}
                onAdd={onAddBadge}
                onSettings={onBadgeSettings}
                onReorder={onReorderBadges}
            />
            <ScreenEntityList
                cards={screen.cards ?? []}
                {editing}
                {screenTitle}
                onAdd={onAddEntity}
                onSettings={onEntitySettings}
                onReorder={onReorderCards}
            />
            {#if !editing && !hasContent}
                <div class="preview-empty">
                    <div class="image-placeholder"><SvgIcon name="noEntity.svg" /></div>
                    <span>{$_('page.screen.noContent')}</span>
                </div>
            {/if}
        {/if}
        {#if screens.length > 1}
            <div class="screen-page-indicators" aria-hidden="true">
                {#each screens as indicatorScreen (indicatorScreen.index)}
                    <span class:active={indicatorScreen.index === screen.index} class="screen-page-indicator"></span>
                {/each}
            </div>
        {/if}
    </div>
</div>
