<script lang="ts">
    import { _, t } from 'svelte-i18n';
    import { get } from 'svelte/store';
    import { onMount, tick } from 'svelte';
    import { pop } from 'svelte-spa-router';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import {
        getWifiConnectionErrorKey,
        networkStore,
        scanWifi,
        switchWifi,
        updateNetworkField,
        type WifiConnectionErrorKey,
    } from '@/stores/network';
    import type { Result } from '@/types/network';
    import { EMessageType, sendMessage } from '@/stores/message';
    import Button from '@/component/Button/Button.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { deviceStore } from '@/stores/device';
    import EThemeMode from '@/types/EThemeMode';
    import { getWifiSignalIcon } from '@/constant';
    let ssid = $state('');
    let ssidTouched = $state(false);
    let password = $state('');
    let showPassword = $state(false);
    let selected = $state<Result | null>(null);
    let securityTooltipBssid = $state<string | null>(null);
    let connectionError = $state<WifiConnectionErrorKey | null>(null);
    let passwordInput: HTMLInputElement;
    const SSID_MAX_LENGTH = 32;
    const loadingIcon = $derived($deviceStore.currentMode === EThemeMode.DARK ? 'loadingDark.svg' : 'loadingLight.svg');
    $effect(() => {
        if (!ssidTouched && !ssid && $networkStore.wifi?.ssid) ssid = $networkStore.wifi.ssid;
    });

    async function refreshWifi() {
        updateNetworkField('scanWifiLoading', true);
        try {
            await scanWifi(5);
        } catch (error) {
            updateNetworkField('scanWifiLoading', false);
            console.error('Failed to scan Wi-Fi:', error);
        }
    }
    function handleRefreshKeydown(event: KeyboardEvent) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        if (!$networkStore.scanWifiLoading) void refreshWifi();
    }

    onMount(() => {
        void refreshWifi();
    });
    function selectNetwork(network: Result) {
        securityTooltipBssid = null;
        selected = network;
        ssidTouched = true;
        ssid = network.ssid;
        connectionError = null;
    }
    function updateSsid(value: string) {
        ssidTouched = true;
        ssid = value;
        connectionError = null;
        if (selected && selected.ssid !== value) selected = null;
    }
    function toggleSecurityTooltip(event: MouseEvent, bssid: string) {
        event.stopPropagation();
        securityTooltipBssid = securityTooltipBssid === bssid ? null : bssid;
    }
    function togglePasswordVisibility(event?: KeyboardEvent) {
        if (event && event.key !== 'Enter' && event.key !== ' ') return;
        event?.preventDefault();
        showPassword = !showPassword;
    }
    async function connect() {
        const normalizedSsid = ssid.trim();
        if (!normalizedSsid || normalizedSsid.length > SSID_MAX_LENGTH || $networkStore.connectWifiLoading) return;
        const target = selected?.ssid === normalizedSsid ? selected : { ssid: normalizedSsid, bssid: '', security: '', channel: 0, rssi: 0 };
        connectionError = null;
        try {
            const response = await switchWifi(target, password);
            connectionError = getWifiConnectionErrorKey(response?.error);
            if (connectionError === 'networkPage.wifiCredentialsIncorrect') {
                await tick();
                passwordInput?.focus();
            }
            if (response?.error === 0) {
                sendMessage(EMessageType.SUCCESS, get(t)('networkPage.connectSuccess'));
                await pop();
            }
        } catch (error) {
            connectionError = 'networkPage.wifiConnectionFailed';
            console.error('Failed to connect Wi-Fi:', error);
        } finally {
            updateNetworkField('connectWifiLoading', false);
        }
    }
</script>

<div class="page-container network-page network-detail ssid-page">
    <NetworkPageHeader title="SSID" back="/network/wlan" />
    <div class="network-form">
        <label for="ssid">SSID</label><input id="ssid" class="form-control" maxlength={SSID_MAX_LENGTH} value={ssid} oninput={(event) => updateSsid(event.currentTarget.value)} /><small>{$_('networkPage.only24g')}</small>
        <label for="wifi-password">{$_('networkPage.password')}</label>
        <div class="password-input">
            <input
                id="wifi-password"
                class="form-control"
                type={showPassword ? 'text' : 'password'}
                bind:value={password}
                bind:this={passwordInput}
                aria-invalid={connectionError ? 'true' : undefined}
                aria-describedby={connectionError ? 'wifi-password-error' : undefined}
                oninput={() => (connectionError = null)}
            /><span
                class="input-icon-action"
                role="button"
                tabindex="0"
                onclick={() => togglePasswordVisibility()}
                onkeydown={togglePasswordVisibility}
                aria-label={$_('networkPage.togglePassword')}><SvgIcon name={showPassword ? 'eyeInvisible.svg' : 'eye.svg'} /></span
            >
        </div>
        {#if connectionError}<p id="wifi-password-error" class="wifi-connection-error" role="alert">{$_(connectionError)}</p>{/if}
    </div>
    <Button
        type="primary"
        class="settings-save"
        style="width: min(70%, 560px); height: 45px; min-height: 45px; padding: 0 2rem"
        disabled={!ssid}
        loading={$networkStore.connectWifiLoading}
        onclick={connect}>{$_('networkPage.connect')}</Button
    >
    <div class="wifi-scan-list">
        <div class="wifi-list-heading">
            <span>
                {#if $networkStore.scanWifiLoading}<span class="wifi-heading-loading"><SvgIcon name={loadingIcon} /></span>{/if}
                {$_(
                    $networkStore.scanWifiLoading ? 'networkPage.scanning' : 'networkPage.nearbyWifi',
                )}
            </span>
            <span
                class="wifi-refresh"
                role="button"
                tabindex="0"
                aria-disabled={$networkStore.scanWifiLoading}
                aria-label={$_('networkPage.refresh')}
                onclick={() => !$networkStore.scanWifiLoading && void refreshWifi()}
                onkeydown={handleRefreshKeydown}><SvgIcon name="refresh.svg" /></span
            >
        </div>
        <div class="wifi-results-scroll">
            {#if $networkStore.results.length}
                <SettingRowGroup>
                    {#each $networkStore.results as item (item.bssid)}
                        <div class="wifi-setting-row" class:tooltip-open={securityTooltipBssid === item.bssid}>
                            <SettingRow titleCol={9} contentCol={3} onclick={() => selectNetwork(item)}>
                                {#snippet titleSnippet()}
                                    <span class="wifi-row-title"
                                        ><span class="wifi-signal-icon"><SvgIcon name={getWifiSignalIcon(item.rssi)} /></span><span class="wifi-row-copy"
                                            ><strong>{item.ssid || $_('networkPage.hiddenNetwork')}</strong><small
                                                ><span>RSSI {item.rssi} dBm</span><span>Channel {item.channel}</span></small
                                            ></span
                                        ></span
                                    >
                                {/snippet}
                                {#snippet contentSnippet()}
                                    <span class="wifi-row-content">
                                        <span class="security-tooltip-wrap">
                                            <button
                                                type="button"
                                                class="security-info"
                                                aria-label={item.security}
                                                aria-expanded={securityTooltipBssid === item.bssid}
                                                onclick={(event) => toggleSecurityTooltip(event, item.bssid)}><SvgIcon name="i.svg" /></button
                                            >
                                            {#if securityTooltipBssid === item.bssid}<span class="security-tooltip" role="tooltip">{item.security || '--'}</span>{/if}
                                        </span>
                                    </span>
                                {/snippet}
                            </SettingRow>
                        </div>
                    {/each}
                </SettingRowGroup>
            {/if}
            {#if !$networkStore.scanWifiLoading && !$networkStore.results.length}<p class="text-center text-body-secondary py-4 mb-0">{$_('networkPage.noWifi')}</p>{/if}
        </div>
    </div>
</div>

<style>
    .ssid-page {
        display: flex;
        flex-direction: column;
        height: calc(100dvh - var(--topbar-height) - var(--footer-height) - clamp(1.25rem, 4vw, 3rem) - 3rem);
        min-height: 48rem;
        overflow: hidden;
    }
    .ssid-page .network-form .form-control:focus {
        border-color: #86b7fe;
        outline: 0;
        box-shadow: none;
    }
    .wifi-connection-error {
        margin: 0.4rem 0 0;
        color: var(--bs-danger, #dc3545);
        font-size: 0.8rem;
        line-height: 1.4;
    }
    .wifi-scan-list {
        display: flex;
        flex: 1 1 auto;
        flex-direction: column;
        min-height: 0;
        margin-top: 2rem;
    }
    .wifi-results-scroll {
        flex: 1 1 auto;
        min-height: 10rem;
        overflow-y: auto;
        overscroll-behavior: contain;
        scrollbar-gutter: stable;
    }
    .wifi-results-scroll::-webkit-scrollbar {
        width: 0.45rem;
    }
    .wifi-results-scroll::-webkit-scrollbar-track {
        background: transparent;
    }
    .wifi-results-scroll::-webkit-scrollbar-thumb {
        border-radius: 999px;
        background: var(--bs-secondary-bg);
    }
    .wifi-row-title {
        display: flex;
        align-items: center;
        gap: 0.75rem;
        min-width: 0;
    }
    .wifi-signal-icon {
        display: inline-flex;
        flex: 0 0 1.5rem;
        width: 1.5rem;
        height: 1.5rem;
        filter: invert(1);
    }
    .wifi-signal-icon :global(.sf-icon),
    .wifi-signal-icon :global(svg) {
        width: 100%;
        height: 100%;
    }
    :global([data-bs-theme='dark']) .wifi-signal-icon {
        filter: none;
    }
    .wifi-row-copy {
        display: grid;
        min-width: 0;
    }
    .wifi-row-title strong,
    .wifi-row-title small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .wifi-row-title strong {
        font-size: 0.9rem;
        font-weight: 600;
    }
    .wifi-row-title small {
        display: flex;
        gap: 0.85rem;
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
    }
    .wifi-row-content {
        display: flex;
        align-items: center;
        justify-content: flex-end;
        gap: 0.65rem;
        width: 100%;
    }
    .wifi-setting-row {
        position: relative;
        z-index: 0;
    }
    .wifi-setting-row.tooltip-open {
        z-index: 30;
    }
    .wifi-setting-row :global(.setting-row .main-content),
    .wifi-setting-row :global(.setting-row .content-wrapper),
    .wifi-setting-row :global(.setting-row .content) {
        overflow: visible;
    }
    .security-tooltip-wrap {
        position: relative;
        display: inline-flex;
    }
    .security-info {
        display: grid;
        width: 1.75rem;
        height: 1.75rem;
        padding: 0;
        place-items: center;
        color: var(--body-text-color-grey);
        border: 0;
        background: transparent;
        font-size: 1rem;
    }
    .security-info :global(.sf-icon) {
        display: inline-flex;
    }
    .security-tooltip {
        position: absolute;
        right: 0;
        bottom: calc(100% + 0.4rem);
        z-index: 20;
        width: max-content;
        max-width: min(260px, 70vw);
        padding: 0.4rem 0.6rem;
        color: white;
        border-radius: 0.4rem;
        background: #202124;
        box-shadow: 0 0.25rem 0.8rem rgba(0, 0, 0, 0.25);
        font-size: 0.75rem;
        white-space: normal;
    }
    .wifi-setting-row:first-child .security-tooltip {
        top: calc(100% + 0.4rem);
        bottom: auto;
    }
</style>
