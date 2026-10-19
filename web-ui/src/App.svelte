<script lang="ts">
    import { onMount } from 'svelte';
    import Router, { replace, router } from 'svelte-spa-router';
    import { _ } from 'svelte-i18n';
    import { protectedRoutes, publicRoutes } from './routes';
    import { ROUTES_PATH } from './constant/route';
    import Toast from '@/component/Toast/Toast.svelte';
    import { initWebsocket } from '@/ws/initWebsocket';
    import { authStore, updateAuthField } from '@/stores/auth';
    import { captureHassOAuthCallback, homeAssistantStore, pendingHassOAuthCallback } from '@/stores/homeassistant';
    import TopTips from '@/component/TopTips/Index.svelte';
    import { getHassConnectionTopTipsType, HASS_TOP_TIPS, useShowTopTips } from '@/hooks/useShowTopTips.svelte';
    import { isNil } from 'lodash-es';
    import Sidebar from '@/layout/Sidebar.svelte';
    import Footer from '@/layout/Footer.svelte';
    import Header from '@/layout/Header.svelte';
    import GuestSidebar from '@/layout/GuestSidebar.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { networkStore, wifiSetupRequired } from '@/stores/network';
    import WifiSetup from '@/pages/setup/WifiSetup.svelte';
    import FirmwareUpdateOverlay from '@/component/FirmwareUpdateOverlay/FirmwareUpdateOverlay.svelte';
    import { deviceStore } from '@/stores/device';
    import EThemeMode from '@/types/EThemeMode';

    const { topTips, setTopTips } = useShowTopTips();
    let nextionLogo = $derived($deviceStore.currentMode === EThemeMode.DARK ? 'nextionLogoMobileDark.svg' : 'nextionLogoMobileNormal.svg');
    let hasTopTips = $derived.by(() => !isNil(topTips.type));
    function normalizeRoute(path: string) {
        const normalized = path.split('?')[0].replace(/\/+$/, '');
        return normalized || ROUTES_PATH.ROOT;
    }
    function getBrowserRoute() {
        const hashRoute = window.location.hash.replace(/^#/, '');
        return normalizeRoute(hashRoute.startsWith('/') ? hashRoute : window.location.pathname);
    }
    let isWifiSetupRoute = $derived.by(
        () => normalizeRoute(router.location) === ROUTES_PATH.WIFI_SETUP || getBrowserRoute() === ROUTES_PATH.WIFI_SETUP,
    );
    let showWifiSetup = $derived.by(
        () =>
            isWifiSetupRoute ||
            $authStore.initialSetup ||
            ($authStore.loggedIn && $authStore.sessionReady && $wifiSetupRequired),
    );

    let protectedAppStatusReady = $derived.by(
        () =>
            !$authStore.loggedIn ||
            Boolean($pendingHassOAuthCallback) ||
            normalizeRoute(router.location) === ROUTES_PATH.HOME_ASSISTANT_OAUTH_CALLBACK ||
            getBrowserRoute() === ROUTES_PATH.HOME_ASSISTANT_OAUTH_CALLBACK ||
            ($authStore.sessionReady && $networkStore.infoReady && $homeAssistantStore.connectionInfo !== null),
    );
    let sidebarOpen = $state(false);
    let homeAssistantSetupRedirectHandled = $state(false);
    captureHassOAuthCallback();
    $effect(() => {
        const connectionInfo = $homeAssistantStore.connectionInfo;
        if (!connectionInfo) return;

        const type = getHassConnectionTopTipsType(connectionInfo);
        const currentType = topTips.type;
        if (type !== undefined) {
            if (currentType !== type && (currentType === undefined || HASS_TOP_TIPS.has(currentType))) setTopTips({ type });
        } else if (currentType !== undefined && HASS_TOP_TIPS.has(currentType)) {
            setTopTips({ type: undefined });
        }
    });
    $effect(() => {
        const loggedIn = $authStore.loggedIn;
        const handoffPending = $authStore.handoffPending;
        const currentPath = router.location;

        if (!loggedIn) {
            homeAssistantSetupRedirectHandled = false;
        }

        const oauthCallbackPending = Boolean($pendingHassOAuthCallback);
        const initialSetupPending = $authStore.initialSetup && showWifiSetup;
        const authenticatedWifiSetupRequired = loggedIn && $authStore.sessionReady && $wifiSetupRequired;
        const configuredWifiSetupRoute =
            loggedIn &&
            $authStore.sessionReady &&
            $networkStore.infoReady &&
            !$wifiSetupRequired &&
            !$authStore.initialSetup &&
            currentPath === ROUTES_PATH.WIFI_SETUP;
        const connectionInfo = $homeAssistantStore.connectionInfo;
        const homeAssistantSetupRequired =
            loggedIn &&
            $authStore.sessionReady &&
            $networkStore.infoReady &&
            !$wifiSetupRequired &&
            connectionInfo !== null &&
            connectionInfo.configured === false &&
            !homeAssistantSetupRedirectHandled;

        if (authenticatedWifiSetupRequired) {
            if (!$authStore.initialSetup) updateAuthField('initialSetup', true);
            if (currentPath !== ROUTES_PATH.WIFI_SETUP) void replace(ROUTES_PATH.WIFI_SETUP);
        } else if (configuredWifiSetupRoute) {
            void replace(ROUTES_PATH.ROOT);
        } else if (
            !loggedIn &&
            !handoffPending &&
            currentPath !== ROUTES_PATH.LOGIN &&
            currentPath !== ROUTES_PATH.WIFI_SETUP &&
            !initialSetupPending
        ) {
            void replace(ROUTES_PATH.LOGIN);
        } else if (loggedIn && oauthCallbackPending && currentPath !== ROUTES_PATH.HOME_ASSISTANT_OAUTH_CALLBACK) {
            void replace(ROUTES_PATH.HOME_ASSISTANT_OAUTH_CALLBACK);
        } else if (homeAssistantSetupRequired) {
            homeAssistantSetupRedirectHandled = true;
            if (!currentPath.startsWith(ROUTES_PATH.HOME_ASSISTANT)) {
                void replace(ROUTES_PATH.HOME_ASSISTANT);
            }
        } else if (loggedIn && $networkStore.infoReady && currentPath === ROUTES_PATH.LOGIN) {
            void replace(ROUTES_PATH.ROOT);
        }
    });

    onMount(async () => {
        const initRes = await initWebsocket();
        console.log('websocket init result: ', initRes);
    });
</script>

<svelte:head><title>{$_('app.name')} · {$_('app.subtitle')}</title></svelte:head>
{#if hasTopTips}
    <div class:with-sidebar={($authStore.loggedIn && !showWifiSetup) || sidebarOpen} class="app-top-tips"><TopTips /></div>
{/if}
{#if showWifiSetup || !$authStore.loggedIn}
    <GuestSidebar open={sidebarOpen} hasTopTips={hasTopTips} onClose={() => (sidebarOpen = false)} />
    <div class:has-top-tips={hasTopTips} class="auth-topbar" aria-hidden="true"></div>
    <div class:has-top-tips={hasTopTips} class="auth-topbar-controls">
        <div class="auth-brand-logo" aria-hidden="true"><SvgIcon name={nextionLogo} /></div>
        <button class="auth-sidebar-toggle" type="button" aria-label={$_('common.openMenu')} onclick={() => (sidebarOpen = !sidebarOpen)}>
            <SvgIcon name="sidebarCollapse.svg" />
        </button>
    </div>
    <main
        class:has-top-tips={hasTopTips}
        class:guest-sidebar-open={sidebarOpen}
        class:wifi-setup-shell={showWifiSetup}
        class="auth-shell"
    >
        {#if showWifiSetup}
            <WifiSetup />
        {:else}
            <Router routes={publicRoutes} />
        {/if}
    </main>
{:else}
    <div class:has-top-tips={hasTopTips} class="app-shell">
        <Sidebar open={sidebarOpen} onClose={() => (sidebarOpen = false)} />
        <section class="workspace">
            <Header onMenuClick={() => (sidebarOpen = !sidebarOpen)} />
            <main class="content-area">
                {#if protectedAppStatusReady}
                    <Router routes={protectedRoutes} />
                {/if}
            </main>
        </section>
    </div>
{/if}
<Footer withSidebar={($authStore.loggedIn && !showWifiSetup) || sidebarOpen} />
<Toast />
<FirmwareUpdateOverlay />

<style>
    .auth-topbar {
        position: fixed;
        inset: 0 0 auto;
        z-index: 1030;
        height: var(--topbar-height);
        background: var(--bs-body-bg);
        box-shadow: 0 0.25rem 1.25rem rgba(20, 25, 45, 0.035);
    }

    .auth-topbar.has-top-tips {
        top: var(--top-tips-height);
    }
</style>
