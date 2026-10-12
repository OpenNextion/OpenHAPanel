<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import Modal from '@/component/Modal/Modal.svelte';
    import PageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { ROUTES_PATH } from '@/constant';
    import { REG_EXP_DOMAIN, REG_EXP_IPV4, REG_EXP_PORT } from '@/constant/regRex';
    import { homeAssistantStore, updateHassConnectionInfo } from '@/stores/homeassistant';
    import { EWsMethods } from '@/types/websocket';
    import type { IHassDisableResponse, IHassUpdateUrlResponse } from '@/types/homeassistant';
    import sendForReply from '@/ws/sendForReply';
    import { EMessageType, sendMessage } from '@/stores/message';
    import { EHassConnectionState, getHomeAssistantConnectionState, useHomeAssistant } from '@/hooks/useHomeAssistant.svelte';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import { tick } from 'svelte';

    const { startHomeAssistantOAuth } = useHomeAssistant();

    let protocol = $state('http://');
    let host = $state('');
    let port = $state('');
    let editing = $state(false);
    let editConfirmVisible = $state(false);
    let revokeConfirmVisible = $state(false);
    let urlChangeConfirmVisible = $state(false);
    let loading = $state(false);
    let urlUpdateLoading = $state(false);
    let revokeLoading = $state(false);
    let hostError = $state('');
    let portError = $state('');
    let pendingHassUrl = $state('');
    let hostInput = $state<HTMLInputElement>();
    let disabledEditInitialized = false;
    const protocolOptions = [
        { label: 'http://', value: 'http://' },
        { label: 'https://', value: 'https://' },
    ];

    const savedHassHost = $derived.by(() => {
        const hassUrl = $homeAssistantStore.connectionInfo?.hassUrl ?? '';
        try {
            return new URL(hassUrl).hostname;
        } catch {
            return hassUrl;
        }
    });
    const connectionState = $derived(getHomeAssistantConnectionState($homeAssistantStore.connectionInfo));
    const canRevokeAuthorization = $derived(connectionState !== undefined && connectionState !== EHassConnectionState.DISABLED);
    const primaryActionText = $derived(
        connectionState === EHassConnectionState.AUTHORIZATION_INVALID || connectionState === EHassConnectionState.DISABLED
            ? 'homeAssistant.connect'
            : 'homeAssistant.editConnection',
    );

    function loadConnectionUrl() {
        const hassUrl = $homeAssistantStore.connectionInfo?.hassUrl;
        if (!hassUrl) return;
        try {
            const url = new URL(hassUrl);
            protocol = `${url.protocol}//`;
            host = url.hostname;
            port = url.port;
        } catch {
            host = hassUrl;
        }
    }

    $effect(() => {
        if (!editing) loadConnectionUrl();
    });

    $effect(() => {
        if (connectionState !== EHassConnectionState.DISABLED || disabledEditInitialized) return;
        disabledEditInitialized = true;
        void confirmEdit();
    });

    async function confirmEdit() {
        editConfirmVisible = false;
        editing = true;
        await tick();
        hostInput?.focus();
    }

    function cancelEdit() {
        editing = false;
        hostError = '';
        portError = '';
        loadConnectionUrl();
    }

    function normalizeHassUrl(value: string) {
        try {
            return new URL(value).origin.toLowerCase();
        } catch {
            return value.replace(/\/$/, '').toLowerCase();
        }
    }

    async function activateHomeAssistant() {
        const activateResponse = await sendForReply<Record<string, never>>({
            method: EWsMethods.HASS_ACTIVATE,
            data: {},
        });
        if (activateResponse.error === 0) {
            // The final enabled/connection state is updated by
            // Hass.ConnectionStatusNotify.
            editing = false;
            await pop();
        }
    }

    async function handlePrimaryAction() {
        if (loading) return;

        if (connectionState === EHassConnectionState.AUTHORIZATION_INVALID) {
            const hassUrl = $homeAssistantStore.connectionInfo?.hassUrl;
            if (!hassUrl) return;
            loading = true;
            try {
                await startHomeAssistantOAuth(hassUrl);
            } finally {
                loading = false;
            }
            return;
        }

        if (connectionState === EHassConnectionState.DISABLED) {
            // A disabled HA can be reconfigured before it is activated again.
            // Keep the existing host as the default; saveAndConnect decides
            // whether activation is enough or the URL-change confirmation is needed.
            await confirmEdit();
            return;
        }

        editConfirmVisible = true;
    }

    async function saveAndConnect() {
        const ip = host.trim();
        if (!ip || (!REG_EXP_IPV4.test(ip) && !REG_EXP_DOMAIN.test(ip))) {
            hostError = $_(ip ? 'homeAssistant.ipInvalid' : 'homeAssistant.ipRequired');
            return;
        }

        hostError = '';
        const normalizedPort = port.trim();
        if (normalizedPort && !REG_EXP_PORT.test(normalizedPort)) {
            portError = $_('homeAssistant.portInvalid');
            return;
        }
        portError = '';
        const hassUrl = `${protocol}${ip}${normalizedPort ? `:${normalizedPort}` : ''}`;
        const currentConnectionInfo = $homeAssistantStore.connectionInfo;
        const urlChanged = normalizeHassUrl(hassUrl) !== normalizeHassUrl(currentConnectionInfo?.hassUrl ?? '');

        if (urlChanged) {
            pendingHassUrl = hassUrl;
            urlChangeConfirmVisible = true;
            return;
        }

        loading = true;
        try {
            if (currentConnectionInfo?.enabled) {
                editing = false;
                await pop();
                return;
            }

            await activateHomeAssistant();
        } finally {
            loading = false;
        }
    }

    function closeUrlChangeConfirm() {
        if (urlUpdateLoading) return;
        urlChangeConfirmVisible = false;
        pendingHassUrl = '';
    }

    async function confirmUrlChange() {
        if (urlUpdateLoading || !pendingHassUrl) return;
        const hassUrl = pendingHassUrl;
        urlUpdateLoading = true;
        try {
            const updateResponse = await sendForReply<{ hassUrl: string }, IHassUpdateUrlResponse>({
                method: EWsMethods.HASS_UPDATE_URL,
                data: { hassUrl },
            });

            if (updateResponse.error === 926) {
                sendMessage(EMessageType.ERROR, $_('homeAssistant.updateUrlConnectionFailed'));
                return;
            }

            if (updateResponse.error === 931) {
                await startHomeAssistantOAuth(hassUrl);
                return;
            }

            if (updateResponse.error === 0 && updateResponse.data) {
                updateHassConnectionInfo(updateResponse.data);
                urlChangeConfirmVisible = false;
                pendingHassUrl = '';
                editing = false;
                await pop();
                return;
            }

            sendMessage(EMessageType.ERROR, $_('TIPS.OPERATION_FAILED'));
        } finally {
            urlUpdateLoading = false;
        }
    }

    async function confirmRevoke() {
        if (revokeLoading) return;
        revokeLoading = true;
        try {
            const response = await sendForReply<Record<string, never>, IHassDisableResponse>({
                method: EWsMethods.HASS_DISABLE,
                data: {},
            });
            if (response.error === 0 && response.data) {
                updateHassConnectionInfo(response.data);
                revokeConfirmVisible = false;
                sendMessage(EMessageType.WARN, $_('homeAssistant.disconnectedSuccess'));
                await pop();
            }
        } finally {
            revokeLoading = false;
        }
    }
</script>

<div class="page-container ha-edit-page">
    <PageHeader title={$_('homeAssistant.editTitle')} back={ROUTES_PATH.HOME_ASSISTANT} />
    <div class="ha-edit-form">
        <label
            ><span><b>*</b>{$_('homeAssistant.domainIp')}</span>
            <div class="address-field">
                <DropDown bind:value={protocol} options={protocolOptions} disabled={!editing} ariaLabel={$_('homeAssistant.protocol')}><span>{protocol}</span></DropDown><input
                    bind:this={hostInput}
                    bind:value={host}
                    disabled={!editing}
                    oninput={() => (hostError = '')}
                />
            </div></label
        >
        {#if hostError}<small class="text-danger">{hostError}</small>{/if}
        <label
            ><span>{$_('homeAssistant.portNumber')}</span><input
                bind:value={port}
                disabled={!editing}
                inputmode="numeric"
                aria-invalid={Boolean(portError)}
                aria-describedby={portError ? 'ha-edit-port-error' : undefined}
                oninput={() => (portError = '')}
            /></label
        >
        {#if portError}<small id="ha-edit-port-error" class="text-danger">{portError}</small>{/if}
    </div>

    {#if editing}
        <div class:single-action={connectionState === EHassConnectionState.DISABLED} class="ha-edit-actions editing-actions">
            {#if connectionState !== EHassConnectionState.DISABLED}
                <button class="secondary-action" type="button" onclick={cancelEdit}>{$_('common.cancel')}</button>
            {/if}
            <button class="primary-action" type="button" disabled={loading} onclick={saveAndConnect}
                >{$_(connectionState === EHassConnectionState.DISABLED ? 'homeAssistant.connect' : 'homeAssistant.saveAndConnect')}</button
            >
        </div>
    {:else}
        <div class="ha-edit-actions">
            <button class="primary-action" type="button" disabled={loading} onclick={handlePrimaryAction}>{$_(primaryActionText)}</button>
            {#if canRevokeAuthorization}
                <button class="danger-action" type="button" onclick={() => (revokeConfirmVisible = true)}>{$_('homeAssistant.revoke')}</button>
            {/if}
        </div>
    {/if}
</div>

<Modal modalVisible={editConfirmVisible} footer={false} setVisible={(visible) => (editConfirmVisible = visible)}>
    {#snippet customSnippet()}
        <div class="ha-confirm-dialog">
            <h2>{$_('homeAssistant.editDialogTitle')}</h2>
            <p>{$_('homeAssistant.editWarning')}</p>
            <footer>
                <button type="button" onclick={() => (editConfirmVisible = false)}>{$_('common.cancel')}</button><button class="confirm" type="button" onclick={confirmEdit}
                    >{$_('homeAssistant.confirmEdit')}</button
                >
            </footer>
        </div>
    {/snippet}
</Modal>

<Modal modalVisible={urlChangeConfirmVisible} footer={false} setVisible={(visible) => !visible && closeUrlChangeConfirm()}>
    {#snippet customSnippet()}
        <div class="ha-confirm-dialog url-change-dialog">
            <h2>{$_('homeAssistant.authorizationTitle')}</h2>
            <p>{$_('homeAssistant.urlChangeWarning', { values: { host: savedHassHost } })}</p>
            <footer>
                <button type="button" disabled={urlUpdateLoading} onclick={closeUrlChangeConfirm}>{$_('common.cancel')}</button><button
                    class="confirm"
                    type="button"
                    disabled={urlUpdateLoading}
                    onclick={confirmUrlChange}
                    >{#if urlUpdateLoading}<span class="spinner-border spinner-border-sm" aria-hidden="true"></span>{$_('homeAssistant.connectingHa')}{:else}{$_(
                            'common.confirm',
                        )}{/if}</button
                >
            </footer>
        </div>
    {/snippet}
</Modal>

<Modal modalVisible={revokeConfirmVisible} footer={false} setVisible={(visible) => (revokeConfirmVisible = visible)}>
    {#snippet customSnippet()}
        <div class="ha-confirm-dialog">
            <h2>{$_('homeAssistant.revokeDialogTitle')}</h2>
            <p>{$_('homeAssistant.revokeWarning')}</p>
            <footer>
                <button type="button" disabled={revokeLoading} onclick={() => (revokeConfirmVisible = false)}>{$_('common.cancel')}</button><button
                    class="confirm"
                    type="button"
                    disabled={revokeLoading}
                    onclick={confirmRevoke}
                    >{#if revokeLoading}<span class="spinner-border spinner-border-sm" aria-hidden="true"></span>{/if}{$_('homeAssistant.confirmRevoke')}</button
                >
            </footer>
        </div>
    {/snippet}
</Modal>

<style>
    .ha-edit-page {
        width: 100%;
        max-width: 960px;
        margin-inline: auto;
    }
    .ha-edit-form {
        display: grid;
        gap: 1rem;
        margin-top: 1.75rem;
    }
    .ha-edit-form label {
        display: grid;
        gap: 0.45rem;
        color: var(--bs-secondary-color);
        font-size: 0.8rem;
    }
    .ha-edit-form label > span {
        padding-left: 0.75rem;
    }
    .ha-edit-form b {
        margin-right: 0.2rem;
        color: #ef5b5b;
    }
    .ha-edit-form input {
        min-height: 50px;
        color: var(--bs-body-color);
        border: 0;
        outline: 0;
        background: transparent;
    }
    .ha-edit-form label > input,
    .address-field {
        padding-inline: 1rem;
        border: 1px solid transparent;
        border-radius: 0.8rem;
        background: var(--body-inner-level-2);
        transition:
            border-color 0.18s ease,
            box-shadow 0.18s ease;
    }
    .ha-edit-form label > input {
        overflow: hidden;
    }
    .ha-edit-form label > input:not(:disabled):focus,
    .address-field:focus-within {
        border-color: #249ed5;
        box-shadow: 0 0 0 1px rgba(36, 158, 213, 0.2);
    }
    .address-field {
        display: grid;
        grid-template-columns: 82px minmax(0, 1fr);
    }
    .address-field :global(.dropdown) {
        width: 100%;
        min-height: 50px;
        color: var(--bs-body-color);
        background: transparent;
    }
    .address-field :global(.dropdown-trigger) {
        min-height: 50px;
        padding-right: 0.35rem;
    }
    .address-field :global(.dropdown.disabled) {
        cursor: default;
        opacity: 1;
    }
    .ha-edit-form input:disabled {
        cursor: default;
        opacity: 1;
    }
    .ha-edit-actions {
        display: grid;
        width: min(320px, 100%);
        grid-template-columns: 100%;
        justify-content: center;
        justify-items: center;
        gap: 1rem;
        margin: clamp(3rem, 9vh, 5.5rem) auto 0;
    }
    .ha-edit-actions button {
        width: 100%;
        min-height: 44px;
        padding: 0.6rem 1.25rem;
        font-weight: 500;
        line-height: 1.2;
        transition:
            filter 0.18s ease,
            opacity 0.18s ease;
        border-radius: 999px;
    }
    .ha-edit-actions button:not(:disabled):hover {
        filter: brightness(1.06);
    }
    .ha-edit-actions button:disabled {
        cursor: not-allowed;
        opacity: 0.6;
    }
    .primary-action {
        color: white;
        border: 0;
        background: var(--primary-button-bg);
    }
    .danger-action {
        color: rgba(255, 82, 75, 1);
        border: 0;
        background: var(--web-, rgba(241, 82, 75, 0.2));
    }
    :global([data-bs-theme='dark']) .danger-action {
        background: rgba(241, 82, 75, 0.25);
    }
    .editing-actions {
        width: min(700px, 100%);
        grid-template-columns: repeat(2, minmax(180px, 320px));
        justify-content: space-between;
        column-gap: clamp(1.5rem, 8vw, 4.5rem);
    }
    .editing-actions.single-action {
        grid-template-columns: minmax(180px, 320px);
        justify-content: center;
    }
    .secondary-action {
        color: var(--bs-body-color);
        border: 1px solid var(--bs-secondary-color);
        background: transparent;
    }
    .ha-confirm-dialog {
        width: min(340px, calc(100vw - 2rem));
        padding: 1.5rem 1.5rem 1.4rem;
        color: var(--bs-body-color);
        border-radius: 1.25rem;
        background: var(--body-inner-level-2);
        box-shadow: 0 1rem 2.5rem rgba(0, 0, 0, 0.28);
    }
    .ha-confirm-dialog h2 {
        margin: 0 0 0.85rem;
        font-size: 1.05rem;
        font-weight: 600;
        line-height: 1.4;
        text-align: center;
    }
    .ha-confirm-dialog p {
        margin: 0 0 1.25rem;
        color: var(--bs-body-color);
        font-size: 0.92rem;
        line-height: 1.55;
    }
    .ha-confirm-dialog footer {
        display: flex;
        flex-direction: column-reverse;
        gap: 0.65rem;
    }
    .ha-confirm-dialog footer button {
        width: 100%;
        min-height: 40px;
        padding: 0.5rem 1rem;
        border: 1px solid var(--bs-border-color);
        border-radius: 999px;
        background: transparent;
        color: var(--bs-body-color);
        font-weight: 500;
        line-height: 1.2;
    }
    .ha-confirm-dialog footer .confirm {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        gap: 0.45rem;
        color: white;
        border-color: var(--primary-button-bg);
        background: var(--primary-button-bg);
    }
    .ha-confirm-dialog footer button:disabled {
        cursor: not-allowed;
        opacity: 0.6;
    }
    @media (max-width: 600px) {
        .ha-edit-page {
            padding-inline: 1rem;
        }
        .ha-edit-actions {
            width: min(310px, 100%);
            margin-top: 2.75rem;
        }
        .editing-actions {
            grid-template-columns: 1fr;
        }
        .ha-confirm-dialog {
            padding: 1.35rem 1.25rem 1.25rem;
        }
    }
</style>
