<script lang="ts">
    import { tick } from 'svelte';
    import { _ } from 'svelte-i18n';
    import Button from '@/component/Button/Button.svelte';
    import Modal from '@/component/Modal/Modal.svelte';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { authStore } from '@/stores/auth';
    import {
        getWifiConnectionErrorKey,
        networkStore,
        scanWifi,
        switchWifi,
        updateNetworkField,
        type WifiConnectionErrorKey,
    } from '@/stores/network';
    import type { Result } from '@/types/network';
    import { deviceStore } from '@/stores/device';
    import EThemeMode from '@/types/EThemeMode';
    import { getWifiSignalIcon } from '@/constant';
    import { copyText, validIPv4 } from '@/utils/commonUtils';

    let selected = $state<Result | null>(null);
    let ssid = $state('');
    let password = $state('');
    let showPassword = $state(false);
    let connectionModalVisible = $state(false);
    let securityTooltipBssid = $state<string | null>(null);
    let rescanLoading = $state(false);
    let ssidError = $state(false);
    let connectionError = $state<WifiConnectionErrorKey | null>(null);
    let provisioningSsid = $state('');
    let staIp = $state('');
    let successModalVisible = $state(false);
    let copiedAddress = $state<'ip' | 'mdns' | null>(null);
    let copyFailed = $state(false);
    let passwordInput: HTMLInputElement;
    let scanStarted = false;
    const SSID_MAX_LENGTH = 32;
    const loadingIcon = $derived($deviceStore.currentMode === EThemeMode.DARK ? 'loadingDark.svg' : 'loadingLight.svg');
    const scanLoading = $derived(!$authStore.sessionReady || $networkStore.scanWifiLoading);
    const mdnsDomain = $derived($networkStore.mdnsDomain?.trim() ?? '');

    $effect(() => {
        if (!$authStore.sessionReady) return;
        if (scanStarted) return;
        scanStarted = true;
        void refreshWifi();
    });

    $effect(() => {
        if (rescanLoading && !$networkStore.scanWifiLoading) rescanLoading = false;
    });

    $effect(() => {
        const wifi = $networkStore.wifi;
        const ipAddress = wifi?.ipAddress?.trim();
        if (!provisioningSsid || successModalVisible || !wifi?.connected || wifi.ssid !== provisioningSsid) return;
        if (!ipAddress || ipAddress === '0.0.0.0' || !validIPv4(ipAddress)) return;

        staIp = ipAddress;
        successModalVisible = true;
    });

    async function refreshWifi(showRescanLoading = false) {
        if ($networkStore.scanWifiLoading) return;
        if (showRescanLoading) rescanLoading = true;
        try {
            // Scan WiFi for 5 seconds.
            await scanWifi(5);
        } catch (error) {
            rescanLoading = false;
            updateNetworkField('scanWifiLoading', false);
            console.error('Failed to scan Wi-Fi:', error);
        }
    }

    function handleRefreshKeydown(event: KeyboardEvent) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        if (!$authStore.sessionReady || scanLoading) return;
        void refreshWifi(true);
    }

    function togglePasswordVisibility(event: MouseEvent) {
        event.preventDefault();
        event.stopPropagation();
        showPassword = !showPassword;
    }

    function preventPasswordEyeFocus(event: PointerEvent) {
        event.preventDefault();
        event.stopPropagation();
    }

    function openNetwork(network?: Result) {
        selected = network ?? null;
        ssid = network?.ssid ?? '';
        password = '';
        showPassword = false;
        ssidError = false;
        connectionError = null;
        securityTooltipBssid = null;
        connectionModalVisible = true;
    }

    function toggleSecurityTooltip(event: MouseEvent, bssid: string) {
        event.stopPropagation();
        securityTooltipBssid = securityTooltipBssid === bssid ? null : bssid;
    }

    async function connect() {
        if ($networkStore.connectWifiLoading) return;
        const normalizedSsid = ssid.trim();
        if (!normalizedSsid || normalizedSsid.length > SSID_MAX_LENGTH) {
            ssidError = true;
            return;
        }
        ssidError = false;
        ssid = normalizedSsid;
        const target = selected ?? { ssid: normalizedSsid, bssid: '', security: '', channel: 0, rssi: 0 };
        connectionError = null;
        try {
            const response = await switchWifi(target, password);
            connectionError = getWifiConnectionErrorKey(response?.error);
            if (connectionError === 'networkPage.wifiCredentialsIncorrect') {
                await tick();
                passwordInput?.focus();
            }
            if (response?.error === 0) {
                connectionModalVisible = false;
                provisioningSsid = ssid;
                copiedAddress = null;
                copyFailed = false;
            }
        } catch (error) {
            connectionError = 'networkPage.wifiConnectionFailed';
            updateNetworkField('connectWifiLoading', false);
            console.error('Failed to connect Wi-Fi:', error);
        }
    }

    async function copyAddress(value: string, address: 'ip' | 'mdns') {
        if (!value) return;
        try {
            await copyText(`http://${value}`);
            copiedAddress = address;
            copyFailed = false;
        } catch (error) {
            copiedAddress = null;
            copyFailed = true;
            console.error('Failed to copy device address:', error);
        }
    }
</script>

<div class="wifi-setup-page">
    <header>
        <h1>{$_('networkPage.setupTitle')}</h1>
        <p>{$_('networkPage.setupSubtitle')}</p>
    </header>

    <section class="wifi-card">
        <div class="wifi-list-heading">
            <span>
                {#if scanLoading}<span class="wifi-heading-loading"><SvgIcon name={loadingIcon} /></span>{/if}
                {$_(scanLoading ? 'networkPage.scanning' : 'networkPage.nearbyWifi')}
            </span>
            <span
                class="wifi-refresh"
                role="button"
                tabindex="0"
                aria-disabled={!$authStore.sessionReady || scanLoading}
                aria-label={$_('networkPage.refresh')}
                onclick={() => $authStore.sessionReady && !scanLoading && void refreshWifi(true)}
                onkeydown={handleRefreshKeydown}><SvgIcon name="refresh.svg" /></span
            >
        </div>

        <div class="wifi-results-scroll">
            {#if $networkStore.results.length}
                <SettingRowGroup>
                    {#each $networkStore.results as item (item.bssid)}
                        <div class="wifi-setting-row" class:tooltip-open={securityTooltipBssid === item.bssid}>
                            <SettingRow titleCol={9} contentCol={3} onclick={() => openNetwork(item)}>
                                {#snippet titleSnippet()}
                                    <span class="wifi-row-title">
                                        <span class="wifi-signal-icon"><SvgIcon name={getWifiSignalIcon(item.rssi)} /></span>
                                        <span class="wifi-row-copy">
                                            <strong>{item.ssid || $_('networkPage.hiddenNetwork')}</strong>
                                            <small><span>RSSI {item.rssi} dBm</span><span>Channel {item.channel}</span></small>
                                        </span>
                                    </span>
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
            {:else if !scanLoading}
                <p class="empty-result">{$_('networkPage.noWifi')}</p>
            {/if}
        </div>
    </section>

    <Button class="manual-button" onclick={() => openNetwork()}>{$_('networkPage.addWifiManually')}</Button>
</div>

<Modal modalVisible={rescanLoading} footer={false} maskClosable={false}>
    {#snippet customSnippet()}
        <div class="rescan-loading-dialog" role="status" aria-live="polite">
            <span class="rescan-loading-icon"><SvgIcon name="loadingLight.svg" /></span>
            <span>{$_('networkPage.rescanning')}</span>
        </div>
    {/snippet}
</Modal>

<Modal
    modalVisible={connectionModalVisible}
    footer={false}
    setVisible={(visible) => {
        if (!$networkStore.connectWifiLoading) connectionModalVisible = visible;
    }}
>
    {#snippet customSnippet()}
        <div class="connection-modal">
            <h2>
                {selected ? $_('networkPage.joinWifi', { values: { ssid } }) : $_('networkPage.addWifiManually')}
            </h2>
            <div class="connection-fields">
                {#if !selected}
                    <div class="connection-field">
                        <label for="setup-ssid">SSID</label>
                        <input
                            id="setup-ssid"
                            class="form-control"
                            bind:value={ssid}
                            maxlength={SSID_MAX_LENGTH}
                            placeholder={$_('networkPage.ssidPlaceholder')}
                            disabled={$networkStore.connectWifiLoading}
                            aria-invalid={ssidError ? 'true' : undefined}
                            aria-describedby={ssidError ? 'setup-ssid-error' : undefined}
                            oninput={() => (ssidError = false)}
                        />
                        {#if ssidError}<p id="setup-ssid-error" class="wifi-connection-error" role="alert">{$_('networkPage.ssidRequired')}</p>{/if}
                    </div>
                {/if}
                <div class="connection-field">
                    <label for="setup-password">{$_('networkPage.password')}</label>
                    <div class="password-input">
                        <input
                            id="setup-password"
                            class="form-control"
                            type={showPassword ? 'text' : 'password'}
                            bind:value={password}
                            bind:this={passwordInput}
                            placeholder={$_('networkPage.passwordPlaceholder')}
                            disabled={$networkStore.connectWifiLoading}
                            aria-invalid={connectionError ? 'true' : undefined}
                            aria-describedby={connectionError ? 'setup-password-error' : undefined}
                            oninput={() => (connectionError = null)}
                        />
                        <span class="password-eye" onpointerdown={preventPasswordEyeFocus} onclick={togglePasswordVisibility} role="none">
                            <SvgIcon name={showPassword ? 'eyeInvisible.svg' : 'eye.svg'} />
                        </span>
                    </div>
                    {#if connectionError}<p id="setup-password-error" class="wifi-connection-error" role="alert">{$_(connectionError)}</p>{/if}
                </div>
            </div>
            <div class="modal-actions">
                <Button disabled={$networkStore.connectWifiLoading} onclick={() => (connectionModalVisible = false)}>{$_('common.cancel')}</Button>
                <Button type="primary" disabled={$networkStore.connectWifiLoading} loading={$networkStore.connectWifiLoading} onclick={connect}>{$_('networkPage.connect')}</Button>
            </div>
        </div>
    {/snippet}
</Modal>

<Modal modalVisible={successModalVisible} footer={false} maskClosable={false}>
    {#snippet customSnippet()}
        <div class="provisioning-success-modal" role="dialog" aria-modal="true" aria-labelledby="provisioning-success-title">
            <span class="success-icon" aria-hidden="true"><SvgIcon name="success.svg" /></span>
            <h2 id="provisioning-success-title">{$_('networkPage.setupSuccessTitle')}</h2>
            <p class="setup-step">{$_('networkPage.setupSuccessStepOne')}</p>
            <div class="device-addresses">
                <div class="device-address-row">
                    <strong>{staIp}</strong>
                    <button type="button" onclick={() => copyAddress(staIp, 'ip')}>
                        {$_(copiedAddress === 'ip' ? 'networkPage.addressCopied' : 'networkPage.copyAddress')}
                    </button>
                </div>
                <div class="device-address-row">
                    <strong>{mdnsDomain || '--'}</strong>
                    <button type="button" disabled={!mdnsDomain} onclick={() => copyAddress(mdnsDomain, 'mdns')}>
                        {$_(copiedAddress === 'mdns' ? 'networkPage.addressCopied' : 'networkPage.copyAddress')}
                    </button>
                </div>
            </div>
            {#if copyFailed}<p class="copy-error" role="alert">{$_('networkPage.copyAddressFailed')}</p>{/if}
            <p class="setup-step setup-step-two">
                {$_('networkPage.setupSuccessStepTwo', { values: { ssid: provisioningSsid } })}
            </p>
        </div>
    {/snippet}
</Modal>

<style lang="scss">
    .wifi-setup-page {
        display: flex;
        flex-direction: column;
        width: min(100%, 1080px);
        height: calc(100dvh - var(--footer-height) - var(--auth-header-space) - clamp(2rem, 8vw, 6rem));
        min-height: 0;
        margin: 0 auto;
        overflow: hidden;
    }
    header {
        margin-bottom: 1rem;
        h1 {
            margin: 0 0 0.75rem;
            font-size: 1.25rem;
            font-weight: 700;
        }
        p {
            margin: 0;
            color: var(--body-text-color-grey);
        }
    }
    .wifi-card {
        display: flex;
        flex: 1 1 auto;
        flex-direction: column;
        min-height: 0;
        padding: 0.75rem 1rem 0;
        overflow: visible;
        border-radius: 1rem;
        background: var(--bs-tertiary-bg);
		margin-bottom: 2rem;
    }
    .wifi-list-heading {
        display: flex;
        align-items: center;
        gap: 0.75rem;
        min-height: 2.5rem;
    }
    .wifi-list-heading > span {
        display: flex;
        align-items: center;
        gap: 0.75rem;
    }
    .wifi-refresh {
        width: 2.5rem;
        height: 2.5rem;
        margin-left: auto;
        justify-content: center;
        cursor: pointer;
    }
    .wifi-refresh[aria-disabled='true'] {
        cursor: default;
        opacity: 0.65;
    }
    .wifi-heading-loading {
        display: inline-flex;
        width: 1.25rem;
        height: 1.25rem;
    }
    .wifi-heading-loading :global(.sf-icon),
    .wifi-heading-loading :global(svg) {
        width: 100%;
        height: 100%;
    }
    .wifi-results-scroll {
        flex: 1 1 auto;
        min-height: 0;
        overflow-y: auto;
        scrollbar-gutter: stable;
    }
    .wifi-results-scroll::-webkit-scrollbar {
        width: 0.45rem;
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
    .wifi-row-copy strong,
    .wifi-row-copy small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .wifi-row-copy strong {
        font-size: 0.9rem;
        font-weight: 500;
    }
    .wifi-row-copy small {
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
    .empty-result {
        padding: 2rem;
        text-align: center;
        color: var(--body-text-color-grey);
    }
    :global(.manual-button) {
        width: min(36%, 350px) !important;
        height: 45px;
        min-height: 45px;
        margin: 1.5rem auto 0;
    }
    .connection-modal {
        width: min(88vw, 28rem);
        padding: 1.5rem;
        border-radius: 0.9rem;
        background: var(--bs-body-bg);
    }
    .provisioning-success-modal {
        width: min(88vw, 28rem);
        padding: 1.75rem;
        color: var(--bs-body-color);
        border-radius: 0.9rem;
        background: var(--bs-body-bg);
        box-shadow: 0 1rem 3rem rgba(0, 0, 0, 0.25);
        text-align: left;
    }
    .success-icon {
        display: flex;
        width: 3rem;
        height: 3rem;
        margin: 0 auto 0.75rem;
        justify-content: center;
    }
    .success-icon :global(.sf-icon),
    .success-icon :global(svg) {
        width: 100%;
        height: 100%;
    }
    .provisioning-success-modal h2 {
        margin: 0 0 1.5rem;
        font-size: 1.35rem;
        font-weight: 600;
        text-align: center;
    }
    .setup-step {
        margin: 0;
        color: var(--bs-body-color);
        font-size: 1rem;
        line-height: 1.55;
    }
    .device-addresses {
        display: grid;
        gap: 0.75rem;
        margin: 1rem 0 1.5rem;
    }
    .device-address-row {
        display: grid;
        grid-template-columns: minmax(0, 1fr) auto;
        align-items: center;
        gap: 0.75rem;
    }
    .device-address-row strong {
        overflow-wrap: anywhere;
        font-size: 1rem;
        user-select: text;
    }
    .device-address-row button {
        min-width: 5.25rem;
        min-height: 2.25rem;
        padding: 0.35rem 0.75rem;
        color: #fff;
        border: 0;
        border-radius: 0.45rem;
        background: var(--primary-button-bg);
        font-size: 0.85rem;
    }
    .device-address-row button:disabled {
        cursor: not-allowed;
        opacity: 0.5;
    }
    .copy-error {
        margin: -0.75rem 0 1rem;
        color: var(--bs-danger, #dc3545);
        font-size: 0.8rem;
    }
    .setup-step-two {
        margin-top: 0.25rem;
        overflow-wrap: anywhere;
        word-break: break-word;
    }
    .rescan-loading-dialog {
        display: flex;
        width: max-content;
        max-width: calc(100vw - 2rem);
        padding: 1rem 1.5rem;
        align-items: center;
        justify-content: center;
        gap: 0.9rem;
        color: #fff;
        border-radius: 0.75rem;
        background: #4a4a4a;
        box-shadow: 0 0.5rem 1.5rem rgba(0, 0, 0, 0.2);
        font-size: 1rem;
        font-weight: 500;
    }
    .rescan-loading-icon {
        display: inline-flex;
        flex: 0 0 1.5rem;
        width: 1.5rem;
        height: 1.5rem;
        animation: rescan-loading-spin 0.8s linear infinite;
    }
    .rescan-loading-icon :global(.sf-icon),
    .rescan-loading-icon :global(svg) {
        width: 100%;
        height: 100%;
    }
    @keyframes rescan-loading-spin {
        to {
            transform: rotate(360deg);
        }
    }
    .connection-modal h2 {
        margin-bottom: 1.25rem;
        font-size: 1.15rem;
        text-align: center;
    }
    .connection-fields {
        display: grid;
        gap: 1rem;
    }
    .connection-field label {
        display: block;
        margin: 0 0 0.35rem;
        font-size: 0.85rem;
    }
    .connection-field > .form-control,
    .password-input {
        min-height: 3rem;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.625rem;
        background: var(--body-inner-level-2);
    }
    .connection-field > .form-control {
        color: var(--bs-body-color);
    }
    .connection-field > .form-control:disabled {
        color: var(--bs-body-color);
        background: var(--body-inner-level-2);
        opacity: 1;
    }
    .password-input {
        display: flex;
        overflow: hidden;
    }
    .password-input input {
        min-width: 0;
        border: 0;
        border-radius: 0;
        background: transparent;
        color: var(--bs-body-color);
        box-shadow: none;
    }
    .wifi-connection-error {
        margin: 0.4rem 0 0;
        color: var(--bs-danger, #dc3545);
        font-size: 0.8rem;
        line-height: 1.4;
    }
    .password-eye {
        display: inline-flex;
        width: 2.75rem;
        flex: 0 0 2.75rem;
        align-self: stretch;
        align-items: center;
        justify-content: center;
        border: 0;
        background: transparent;
        color: var(--bs-body-color);
        cursor: pointer;
    }
    .modal-actions {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 0.75rem;
        margin-top: 1.5rem;
    }
    @media (max-width: 576px) {
        :global(.manual-button) {
            width: 80% !important;
        }
    }
    .modal-actions {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 0.75rem;
        margin-top: 1.5rem;
    }
    @media (max-width: 576px) {
        :global(.manual-button) {
            width: 80% !important;
        }
    }
</style>
