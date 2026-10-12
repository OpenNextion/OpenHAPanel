<script lang="ts">
    import { onMount } from 'svelte';
    import { get } from 'svelte/store';
    import { _, t } from 'svelte-i18n';
    import { replace } from 'svelte-spa-router';
    import { ROUTES_PATH } from '@/constant';
    import { EWsMethods } from '@/types/websocket';
    import sendForReply from '@/ws/sendForReply';
    import { homeAssistantStore, pendingHassOAuthCallback, updateHomeassistantField } from '@/stores/homeassistant';

    let submitError = $state('');

    onMount(async () => {
        const callback = get(pendingHassOAuthCallback);
        if (!callback) {
            submitError = get(t)('homeAssistant.oauthCallbackMissing');
            return;
        }

        // Consume the authorization code once. It must never be retried blindly.
        pendingHassOAuthCallback.set(null);
        updateHomeassistantField('authorizationResult', null);
        updateHomeassistantField('oauthCompleting', true);
        try {
            const response = await sendForReply({
                method: EWsMethods.HASS_COMPLETE_OAUTH,
                data: callback,
            });
            if (response.error !== 0) {
                submitError = get(t)('homeAssistant.oauthSubmitFailed');
                updateHomeassistantField('oauthCompleting', false);
            }
        } catch {
            submitError = get(t)('homeAssistant.oauthSubmitFailed');
            updateHomeassistantField('oauthCompleting', false);
        }
    });

    $effect(() => {
        if ($homeAssistantStore.authorizationResult?.status === 'authorized') {
            void replace(ROUTES_PATH.HOME_ASSISTANT);
        }
    });
</script>

<div class="page-container ha-oauth-callback">
    <i class="bi" class:bi-hourglass-split={$homeAssistantStore.oauthCompleting} class:bi-exclamation-circle={Boolean(submitError || $homeAssistantStore.authorizationResult?.status === 'error')}></i>
    <h1>{$_('homeAssistant.oauthCompleting')}</h1>
    {#if submitError}
        <p>{submitError}</p>
    {:else if $homeAssistantStore.authorizationResult?.status === 'error'}
        <p>{$_('homeAssistant.oauthFailed')}: {$homeAssistantStore.authorizationResult.reason}</p>
    {:else}
        <p>{$_('homeAssistant.oauthCompletingHint')}</p>
    {/if}
</div>

<style>
    .ha-oauth-callback { display: grid; min-height: 55vh; place-content: center; justify-items: center; text-align: center; }
    .ha-oauth-callback > i { margin-bottom: 1rem; color: #249ed5; font-size: 3rem; }
    .ha-oauth-callback h1 { font-size: 1.5rem; }
    .ha-oauth-callback p { color: var(--bs-secondary-color); }
</style>
