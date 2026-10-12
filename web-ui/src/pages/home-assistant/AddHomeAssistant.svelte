<script lang="ts">
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import { REG_EXP_DOMAIN, REG_EXP_IPV4, REG_EXP_PORT } from '@/constant/regRex';
    import { ROUTES_PATH } from '@/constant';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';
    import { homeAssistantStore, updateHomeassistantField } from '@/stores/homeassistant';
    import { authStore } from '@/stores/auth';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { useHomeAssistant } from '@/hooks/useHomeAssistant.svelte';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import { EMessageType, sendMessage } from '@/stores/message';
    import PageHeader from '@/component/PageHeader/PageHeader.svelte';

    const { startHomeAssistantOAuth } = useHomeAssistant();

    let protocol = $state('http://');
    let host = $state('');
    let port = $state('');
    let loading = $state(false);
    let portError = $state('');
    let awaitingScanResult = false;
    const protocolOptions = [
        { label: 'http://', value: 'http://' },
        { label: 'https://', value: 'https://' },
    ];

    // click connect
    export const connectToHaServer = async (ipAddress?: string, serverPort?: string | number) => {
        // Discovered HA items pass an explicit address. The manual connect
        // button passes no arguments and must read the latest input values.
        const resolvedIpAddress = ipAddress === undefined ? host : ipAddress;
        const resolvedPort = serverPort === undefined ? port : serverPort;
        const ip = String(resolvedIpAddress).trim();
        if (!ip) {
            sendMessage(EMessageType.ERROR, $_('homeAssistant.ipRequired'));
            return;
        }
        if (!REG_EXP_IPV4.test(ip) && !REG_EXP_DOMAIN.test(ip)) {
            sendMessage(EMessageType.ERROR, $_('homeAssistant.ipInvalid'));
            return;
        }

        const normalizedPort = String(resolvedPort ?? '').trim();
        if (normalizedPort && !REG_EXP_PORT.test(normalizedPort)) {
            portError = $_('homeAssistant.portInvalid');
            return;
        }

        portError = '';
        host = ip;
        port = normalizedPort;
        const portSegment = normalizedPort ? `:${normalizedPort}` : '';
        const hassUrl = `${protocol}${ip}${portSegment}`;
        loading = true;
        try {
            await startHomeAssistantOAuth(hassUrl);
        } finally {
            loading = false;
        }
    };

    // search local ha server
    export const getLocalHaServer = async (duration: number = 4) => {
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
    };

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

<div class="page-container ha-page">
    <PageHeader title={$_('homeAssistant.add')} back={ROUTES_PATH.HOME_ASSISTANT} />
    <section class="ha-manual">
        <h2>{$_('homeAssistant.manual')}</h2>
        <label class="ha-field-label required" for="ha-host"><SvgIcon name="require.svg" />{$_('homeAssistant.domainIp')}</label>
        <div class="ha-address-input">
            <DropDown bind:value={protocol} options={protocolOptions} ariaLabel={$_('homeAssistant.protocol')}><span>{protocol}</span></DropDown><input
                id="ha-host"
                bind:value={host}
                placeholder={$_('homeAssistant.hostPlaceholder')}
                aria-label={$_('homeAssistant.host')}
            />
        </div>
        <label class="ha-field-label" for="ha-port">{$_('homeAssistant.portNumber')}</label>
        <input
            id="ha-port"
            class="ha-port-input"
            bind:value={port}
            inputmode="numeric"
            placeholder={$_('homeAssistant.portPlaceholder')}
            aria-label={$_('homeAssistant.port')}
            aria-invalid={Boolean(portError)}
            aria-describedby={portError ? 'ha-port-error' : undefined}
            oninput={() => (portError = '')}
        />
        {#if portError}<p id="ha-port-error" class="ha-input-error">{portError}</p>{/if}
        <button class="btn ha-primary-button connect-button" type="button" onclick={() => connectToHaServer()}>{$_('homeAssistant.connect')}</button>
    </section>
    <section class="ha-discovery">
        <h2>{$_('homeAssistant.autoDiscover')}</h2>
        <div class="ha-discovery-card">
            <div class:loading={$homeAssistantStore.scanning} class="ha-scanner"><SvgIcon name="haSearch.svg" size="100px" /></div>
            <p>{$_('homeAssistant.searching')}</p>
            <div class="ha-results">
                {#each $homeAssistantStore.hassList as item (item.uuid)}<button type="button" onclick={() => connectToHaServer(item.ipAddress, item.port)} disabled={loading || $homeAssistantStore.scanning}
                        ><span class="ha-result-icon"><SvgIcon name="haBlue.svg" /></span><span
                            ><strong>{item.instanceName || item.locationName}</strong><small>{item.ipAddress}{item.port ? `:${item.port}` : ''}</small></span
                        ><i class="bi bi-box-arrow-up-right ms-auto"><SvgIcon name="blank.svg" /></i></button
                    >{/each}
            </div>
        </div>
    </section>
</div>

<style>
    .ha-field-label {
        display: flex;
        margin: 1.5rem 0 0.65rem 1rem;
        align-items: center;
        gap: 0.2rem;
        color: var(--bs-secondary-color);
        font-size: 0.95rem;
        font-weight: 400;
    }

    .ha-field-label.required :global(.sf-icon) {
        display: inline-flex;
        width: 0.55rem;
        height: 0.55rem;
        flex: 0 0 auto;
    }

    .ha-manual .ha-port-input {
        margin-top: 0;
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

    .ha-address-input {
        display: flex;
        min-width: 0;
    }

    .ha-address-input :global(.dropdown) {
        flex: 0 0 110px;
        width: auto !important;
        min-width: 0;
    }

    .ha-address-input input {
        box-sizing: border-box;
        flex: 1 1 0;
        width: auto;
        min-width: 0;
        max-width: 100%;
        border-radius: 0 1rem 1rem 0;
    }

    @media (max-width: 575.98px) {
        .ha-address-input :global(.dropdown) {
            flex-basis: 88px;
        }

        .ha-address-input input {
            padding-inline: 0.65rem;
        }
    }
</style>
