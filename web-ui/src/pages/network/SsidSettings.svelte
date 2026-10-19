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
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import WifiNetworkList from '@/component/WifiNetworkList/WifiNetworkList.svelte';
    import { deviceStore } from '@/stores/device';
    import EThemeMode from '@/types/EThemeMode';
    let ssid = $state('');
    let ssidTouched = $state(false);
    let password = $state('');
    let showPassword = $state(false);
    let selected = $state<Result | null>(null);
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
    onMount(() => {
        void refreshWifi();
    });
    function selectNetwork(network: Result) {
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
    <p class="wifi-mode-hint">{$_('networkPage.wifiModeResetHint')}</p>
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
        <WifiNetworkList
            results={$networkStore.results}
            loading={$networkStore.scanWifiLoading}
            {loadingIcon}
            onRefresh={() => void refreshWifi()}
            onSelect={selectNetwork}
        />
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
    .wifi-mode-hint {
        margin: 0.75rem 0 0;
        color: #969696;
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
    .wifi-scan-list :global(.wifi-results-scroll) {
        min-height: 10rem;
    }
</style>
