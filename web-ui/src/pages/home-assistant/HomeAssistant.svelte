<script lang="ts">
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import { link } from 'svelte-spa-router';
    import { authStore } from '@/stores/auth';
    import { homeAssistantStore } from '@/stores/homeassistant';
    import { ROUTES_PATH } from '@/constant';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { EHassConnectionState, getHomeAssistantConnectionState, useHomeAssistant } from '@/hooks/useHomeAssistant.svelte';

    const { getConnectionInfo } = useHomeAssistant();
    const connectionState = $derived(getHomeAssistantConnectionState($homeAssistantStore.connectionInfo));
    const authorized = $derived(connectionState !== undefined);
    const connectionStatusText = $derived.by(() => {
        switch (connectionState) {
            case EHassConnectionState.AUTHORIZATION_INVALID:
                return 'homeAssistant.authorizationInvalidStatus';
            case EHassConnectionState.DISABLED:
                return 'homeAssistant.authorizedOffline';
            case EHassConnectionState.INTERRUPTED:
                return 'homeAssistant.authorizedDisconnected';
            default:
                return 'homeAssistant.connectedNormally';
        }
    });
    const connectionManagePath = $derived(
        connectionState === EHassConnectionState.AUTHORIZATION_INVALID
            ? ROUTES_PATH.HOME_ASSISTANT_ADD
            : ROUTES_PATH.HOME_ASSISTANT_EDIT,
    );

    function formatAuthorizedAt(timestamp?: number) {
        if (!timestamp) return '--';
        return new Intl.DateTimeFormat(undefined, {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hour12: false,
        }).format(new Date(timestamp));
    }

    function formatHassUrl(url?: string) {
        if (!url) return '';
        return url.replace(/^https?:\/\//, '').replace(/\/$/, '');
    }

    function formatDuration(timestamp?: number) {
        if (!timestamp) return '--';
        const totalMinutes = Math.max(0, Math.floor((Date.now() - timestamp) / 60_000));
        const days = Math.floor(totalMinutes / 1440);
        const hours = Math.floor((totalMinutes % 1440) / 60);
        const minutes = totalMinutes % 60;
        return `${days} Days ${hours}h ${minutes}min`;
    }

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

<div class="page-container ha-page">
    {#if authorized}
        <h1 class="connection-title">{$_('homeAssistant.title')}</h1>
        <article class="ha-connection-card">
            <header>
                <span class="connection-logo"><SvgIcon name="haBlue.svg" /></span>
                <div class="connection-name">
                    <strong>Home Assistant</strong>
                    {#if $homeAssistantStore.connectionInfo?.hassUrl}
                        <a href={$homeAssistantStore.connectionInfo.hassUrl} target="_blank" rel="noreferrer">
                            <span>{formatHassUrl($homeAssistantStore.connectionInfo.hassUrl)}</span><SvgIcon name="blank.svg" />
                        </a>
                    {/if}
                </div>
                <div class="connection-actions">
                    <span
                        class:authorization-invalid={connectionState === EHassConnectionState.AUTHORIZATION_INVALID}
                        class:online={connectionState === EHassConnectionState.CONNECTED}
                        class:interrupted={connectionState === EHassConnectionState.INTERRUPTED}
                        class:disabled={connectionState === EHassConnectionState.DISABLED}
                        class="connection-status"
                    >
                        {$_(connectionStatusText)}
                    </span>
                    <a class="connection-arrow" href={connectionManagePath} use:link aria-label={$_('homeAssistant.manage')}
                        ><SvgIcon name="arrowRight.svg" /></a
                    >
                </div>
            </header>
            <dl>
                <div>
                    <dt>{$_('homeAssistant.authorizedAt')}</dt>
                    <dd>{formatAuthorizedAt($homeAssistantStore.connectionInfo?.tokenGeneratedAt)}</dd>
                </div>
                <div>
                    <dt>{$_('homeAssistant.connectedDuration')}</dt>
                    <dd>{formatDuration($homeAssistantStore.connectionInfo?.tokenGeneratedAt)}</dd>
                </div>
            </dl>
        </article>
    {:else}
        <div class="page-heading connection-title">
            <h1>{$_('homeAssistant.title')}</h1>
        </div>
        <div class="ha-permission-card">
            <span class="ha-logo"><i class="bi bi-house-gear-fill"><SvgIcon name="addHA.svg" /></i></span>
            <h2>{$_('homeAssistant.permissionTitle')}</h2>
            <p>{$_('homeAssistant.permissionDescription')}</p>
            <a class="btn ha-primary-button" href="/home-assistant/add" use:link>{$_('homeAssistant.add')}</a>
        </div>
    {/if}
</div>

<style>
    .ha-page {
        display: flex;
        width: min(100%, 57.5rem);
        height: min(41.9375rem, calc(100dvh - var(--topbar-height) - var(--footer-height) - clamp(1.25rem, 4vw, 3rem) - 3rem));
        min-height: 26.875rem;
        flex-direction: column;
        border-radius: 1.25rem;
    }
    .page-heading {
        display: flex;
        margin: 0 0 0.75rem 0.75rem;
        align-items: baseline;
        gap: 0.35rem;
    }
    .page-heading h1 {
        margin: 0;
        color: var(--bs-secondary-color);
        font-family: 'PingFang SC', sans-serif;
        font-size: 0.75rem;
        font-style: normal;
        font-weight: 400;
        line-height: 1.41;
        letter-spacing: 0;
        text-transform: capitalize;
    }
    .ha-permission-card {
        flex: 1 1 auto;
        min-height: 26.875rem;
        background: rgba(255, 255, 255, 1);
    }
    :global([data-bs-theme='dark']) .ha-permission-card {
        background: rgba(48, 49, 50, 1);
    }
    .connection-title {
        margin: 0.2rem 0 1rem;
        color: var(--bs-secondary-color);
        font-family: 'PingFang SC', sans-serif;
        font-size: 0.75rem;
        font-style: normal;
        font-weight: 400;
        line-height: 1.41;
        letter-spacing: 0;
        text-transform: capitalize;
    }
    .ha-connection-card {
        overflow: hidden;
        padding: 1.35rem 1.4rem 1.15rem;
        border: 0;
        border-radius: 1.5rem;
        background: rgb(255, 255, 255);
    }
    .ha-connection-card header {
        display: flex;
        align-items: center;
        gap: 1rem;
        padding-bottom: 1.2rem;
        border-bottom: 1px solid var(--bs-border-color);
    }
    .connection-logo {
        display: grid;
        flex: 0 0 auto;
        place-items: center;
        width: 4rem;
        height: 4rem;
    }
    .connection-logo :global(svg) {
        width: 100%;
        height: 100%;
    }
    .connection-name {
        display: grid;
        min-width: 0;
        gap: 0.35rem;
    }
    .connection-name strong {
        color: var(--bs-emphasis-color);
        font-size: 1.45rem;
        font-weight: 600;
    }
    .connection-name a {
        display: flex;
        align-items: center;
        gap: 0.4rem;
        min-width: 0;
        overflow: hidden;
        color: var(--bs-secondary-color);
        font-size: 1rem;
        text-decoration: none;
    }
    .connection-name a > span:first-child {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .connection-name a :global(.sf-icon) {
        flex: 0 0 auto;
    }
    .connection-status {
        padding: 0.25rem 0.55rem;
        color: #fff;
        border-radius: 999px;
        background: rgba(239, 150, 29, 1);
        font-size: 0.7rem;
    }
    .connection-status.online {
        background: rgba(52, 199, 89, 1);
    }
    .connection-status.authorization-invalid {
        background: rgba(150, 150, 150, 1);
    }
    .connection-status.interrupted {
        background: rgba(239, 150, 29, 1);
    }
    .connection-status.disabled {
        background: rgba(241, 82, 75, 1);
    }
    .connection-actions {
        display: flex;
        align-items: center;
        margin-left: auto;
        gap: 0.35rem;
    }
    .connection-arrow {
        padding: 0.4rem;
        color: var(--bs-secondary-color);
        border: 0;
        background: transparent;
        font-size: 1.25rem;
    }
    .connection-arrow:disabled {
        opacity: 0.55;
    }
    .ha-connection-card dl {
        display: grid;
        gap: 0.75rem;
        margin: 1.2rem 0 0;
    }
    .ha-connection-card dl div {
        display: flex;
        justify-content: space-between;
        gap: 1rem;
    }
    .ha-connection-card dt {
        color: var(--bs-body-color);
        font-weight: 400;
    }
    .ha-connection-card dd {
        margin: 0;
        text-align: right;
    }
    :global([data-bs-theme='dark']) .ha-connection-card {
        background: rgb(42, 43, 44);
    }
    @media (max-width: 37.5rem) {
        .ha-page {
            width: 100%;
            height: auto;
            min-height: calc(100dvh - var(--topbar-height) - var(--footer-height) - 4.25rem);
        }
        .connection-title {
            margin-bottom: 0.7rem;
            font-size: 0.75rem;
        }
        .ha-connection-card {
            padding: 0.8rem;
            border-radius: 1.25rem;
        }
        .ha-connection-card header {
            flex-wrap: wrap;
            gap: 0.7rem;
            padding-bottom: 1rem;
        }
        .connection-logo {
            width: 2.75rem;
            height: 2.75rem;
        }
        .connection-name strong {
            font-size: 1.1rem;
        }
        .connection-name a {
            font-size: 0.75rem;
        }
        .connection-actions {
            flex: 0 0 100%;
            justify-content: flex-start;
            margin-left: 0;
            gap: 0.45rem;
        }
        .connection-status {
            display: inline-flex;
            flex: 0 0 auto;
            align-items: center;
            padding: 0.35rem 0.7rem;
            color: #fff;
            background: rgba(239, 150, 29, 1);
            font-size: 0.75rem;
            white-space: nowrap;
        }
        .connection-status.online {
            color: #fff;
            background: rgba(52, 199, 89, 1);
        }
        .connection-status.authorization-invalid {
            background: rgba(150, 150, 150, 1);
        }
        .connection-status.interrupted {
            background: rgba(239, 150, 29, 1);
        }
        .connection-status.disabled {
            background: rgba(241, 82, 75, 1);
        }
        .connection-arrow {
            display: grid;
            width: 1.75rem;
            height: 1.75rem;
            padding: 0;
            place-items: center;
            color: var(--bs-body-color);
        }
        .ha-connection-card dl {
            gap: 0.4rem;
            margin: 1rem 0 0;
            font-size: 0.85rem;
        }
    }
</style>
