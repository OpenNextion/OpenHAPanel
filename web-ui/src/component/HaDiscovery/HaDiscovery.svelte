<script lang="ts">
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { authStore } from '@/stores/auth';
    import { homeAssistantStore, updateHomeassistantField } from '@/stores/homeassistant';
    import type { IHassInstanceItem } from '@/types/homeassistant';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';

    interface IHaDiscoveryProps {
        onServerSelect: (item: IHassInstanceItem) => void | Promise<void>;
        busy?: boolean;
    }

    let { onServerSelect, busy = false }: IHaDiscoveryProps = $props();
    let awaitingScanResult = false;

    async function getLocalHaServer(duration: number = 4) {
        if (awaitingScanResult || $homeAssistantStore.scanning) return;
        awaitingScanResult = true;
        updateHomeassistantField('scanning', true);
        try {
            const result = await sendForReply({
                method: EWsMethods.SCAN_HA_SERVER,
                data: { duration },
            });
            if (result.error !== 0) {
                awaitingScanResult = false;
                updateHomeassistantField('scanning', false);
            }
        } catch (error) {
            awaitingScanResult = false;
            updateHomeassistantField('scanning', false);
            console.error('Failed to start Home Assistant discovery', error);
        }
    }

    onMount(() => {
        let scanStarted = false;
        let previousScanning = false;
        let retryTimer: ReturnType<typeof setTimeout> | null = null;
        let active = true;

        const unsubscribeHomeAssistant = homeAssistantStore.subscribe(({ scanning, hassList }) => {
            if (previousScanning && !scanning && awaitingScanResult) {
                awaitingScanResult = false;
                if (hassList.length === 0 && active) {
                    retryTimer = setTimeout(() => {
                        retryTimer = null;
                        if (active) void getLocalHaServer();
                    }, 0);
                }
            }
            previousScanning = scanning;
        });
        const unsubscribeAuth = authStore.subscribe(({ sessionReady }) => {
            if (!sessionReady || scanStarted) return;
            scanStarted = true;
            void getLocalHaServer();
        });

        return () => {
            active = false;
            awaitingScanResult = false;
            if (retryTimer) clearTimeout(retryTimer);
            unsubscribeAuth();
            unsubscribeHomeAssistant();
        };
    });
</script>

<section class="ha-discovery">
    <div class="ha-discovery-heading">
        <h2>{$_('homeAssistant.autoDiscover')}</h2>
        <button
            type="button"
            class="ha-discovery-refresh"
            aria-label={$_('networkPage.refresh')}
            title={$_('networkPage.refresh')}
            disabled={busy || $homeAssistantStore.scanning}
            onclick={() => void getLocalHaServer()}><SvgIcon name="refresh.svg" /></button
        >
    </div>
    <div class="ha-discovery-card">
        <div class:loading={$homeAssistantStore.scanning} class="ha-scanner"><SvgIcon name="haSearch.svg" size="100px" /></div>
        <p>{$_('homeAssistant.searching')}</p>
        <div class="ha-results">
            {#each $homeAssistantStore.hassList as item (item.uuid)}<button type="button" onclick={() => onServerSelect(item)} disabled={busy || $homeAssistantStore.scanning}
                    ><span class="ha-result-icon"><SvgIcon name="haBlue.svg" /></span><span
                        ><strong>{item.instanceName || item.locationName}</strong><small>{item.ipAddress}{item.port ? `:${item.port}` : ''}</small></span
                    ><i class="bi bi-box-arrow-up-right ms-auto"><SvgIcon name="blank.svg" /></i></button
                >{/each}
        </div>
    </div>
</section>

<style>
    .ha-discovery-heading {
        display: flex;
        align-items: center;
        justify-content: space-between;
        margin: .4rem 0 .8rem;
    }

    .ha-discovery-heading h2 {
        margin: 0 0 0 .8rem;
    }

    .ha-discovery-refresh {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        width: 2rem;
        height: 2rem;
        margin-right: .8rem;
        padding: 0;
        border: 0;
        background: transparent;
        cursor: pointer;
    }

    .ha-discovery-refresh:disabled {
        opacity: .5;
        cursor: not-allowed;
    }

    .ha-discovery-card {
        background: rgba(255, 255, 255, 1);
    }

    :global([data-bs-theme='dark']) .ha-discovery-card {
        background: rgba(48, 49, 50, 1);
    }

    .ha-result-icon {
        display: inline-flex;
        width: 2.5rem;
        height: 2.5rem;
        flex: 0 0 auto;
    }

    .ha-result-icon :global(.sf-icon),
    .ha-result-icon :global(svg) {
        width: 100%;
        height: 100%;
    }
</style>
