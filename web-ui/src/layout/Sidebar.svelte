<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { link } from 'svelte-spa-router';
    import active from 'svelte-spa-router/active';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { ROUTES_PATH } from '@/constant/route';
    import { currentLanDevice } from '@/stores/lanDevice';

    interface SidebarProps {
        open?: boolean;
        onClose?: () => void;
    }

    const { open = false, onClose = () => undefined }: SidebarProps = $props();

    const menu = [
        { path: ROUTES_PATH.ROOT, icon: 'screenConfig.svg', label: 'nav.screen' },
        { path: ROUTES_PATH.HOME_ASSISTANT, icon: 'ha.svg', label: 'nav.homeAssistant' },
        { path: ROUTES_PATH.NETWORK, icon: 'networkMode.svg', label: 'nav.network' },
        { path: ROUTES_PATH.SETTINGS, icon: 'settings.svg', label: 'nav.settings' },
    ];
</script>

<aside class:open class="sidebar">
    <div class="device-brand">
        <span class="device-logo"><SvgIcon name="nextionLogo.svg" /></span>
        <div class="device-copy"><strong>{$currentLanDevice?.name || '--'}</strong><span>{$currentLanDevice?.ip || '--'}</span></div>
        <i class="bi bi-chevron-down ms-auto text-body-tertiary"></i>
    </div>
    <nav class="sidebar-nav" aria-label={$_('nav.main')}>
        {#each menu as item}
            <a href={item.path} use:link use:active={item.path} onclick={onClose}><SvgIcon name={item.icon} /><span>{$_(item.label)}</span></a>
        {/each}
    </nav>
</aside>

{#if open}<button class="sidebar-backdrop" aria-label={$_('common.closeMenu')} onclick={onClose}></button>{/if}
