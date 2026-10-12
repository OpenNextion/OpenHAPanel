<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { onMount } from 'svelte';
    import { link } from 'svelte-spa-router';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { ROUTES_PATH } from '@/constant/route';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { networkStore } from '@/stores/network';
    import { authStore } from '@/stores/auth';
    import { useHomeAssistant } from '@/hooks/useHomeAssistant.svelte';
    import { deviceStore } from '@/stores/device';
    import EThemeMode from '@/types/EThemeMode';

    interface HeaderProps {
        onMenuClick?: () => void;
    }

    const { onMenuClick = () => undefined }: HeaderProps = $props();
    const { getConnectionInfo } = useHomeAssistant();
    const nextionLogo = $derived(
        $deviceStore.currentMode === EThemeMode.DARK ? 'nextionLogoMobileDark.svg' : 'nextionLogoMobileNormal.svg',
    );

    onMount(() => {
        let requested = false;
        const unsubscribe = authStore.subscribe(({ sessionReady }) => {
            if (!sessionReady || requested) return;
            requested = true;
            void getConnectionInfo();
        });
        return unsubscribe;
    });
</script>

<header class="topbar">
    <span class="mobile-brand" aria-hidden="true"><SvgIcon name={nextionLogo} /></span>
    <button class="btn btn-sm icon-button menu-button" type="button" onclick={onMenuClick} aria-label={$_('common.openMenu')}>
        <SvgIcon name="sidebarCollapse.svg" />
    </button>
    <nav class="topbar-statuses ms-auto" aria-label={$_('nav.network')}>
        <a
            href={ROUTES_PATH.HOME_ASSISTANT}
            use:link
            class:active={$homeAssistantStore.connected}
            class="topbar-status-item status-icon"
            aria-label={$_('nav.homeAssistant')}
            title={$_('nav.homeAssistant')}><SvgIcon name={$homeAssistantStore.connected ? 'haHeaderActive.svg' : 'haHeader.svg'} /></a
        >
        <div class="topbar-network-statuses">
            <a
                href={ROUTES_PATH.NETWORK_WLAN}
                use:link
                class:active={Boolean($networkStore.wifi?.connected)}
                class="topbar-status-item status-icon"
                aria-label="Wi-Fi"
                title="Wi-Fi"><SvgIcon name="wifi.svg" /></a
            >
            <a
                href={ROUTES_PATH.NETWORK_AP}
                use:link
                class:active={Boolean($networkStore.ap?.enabled)}
                class="topbar-status-item status-ap"
                aria-label="AP"
                title="AP"><span>AP</span></a
            >
        </div>
    </nav>
</header>

<style>
    .mobile-brand {
        display: none;
    }

    @media (max-width: 575.98px) {
        .mobile-brand {
            display: inline-flex;
            width: 5.875rem;
            height: 1.875rem;
        }
        .mobile-brand :global(.sf-icon),
        .mobile-brand :global(svg) {
            width: 100%;
            height: 100%;
        }
        .menu-button {
            margin-right: 0;
            margin-left: auto;
        }
        .topbar-statuses {
            display: none;
        }
    }
</style>
