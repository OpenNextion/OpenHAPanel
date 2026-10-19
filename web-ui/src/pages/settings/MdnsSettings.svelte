<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import Button from '@/component/Button/Button.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { pop } from 'svelte-spa-router';
    import { networkStore, updateMdnsDomain } from '@/stores/network';
    import { ERROR_CODE, MDNS_DOMAIN_INPUT_MAX_LENGTH } from '@/constant';
    import { REG_EXP_MDNS_DOMAIN_FORMAT } from '@/constant/regRex';
    import { EMessageType, sendMessage } from '@/stores/message';
    let hostname = $state($networkStore.mdnsDomain.replace(/\.local$/i, ''));
    let loading = $state(false);
    let normalizedHostname = $derived.by(() => {
        const value = hostname.trim();
        return value ? `${value}.local` : value;
    });
    let hostnameValid = $derived(REG_EXP_MDNS_DOMAIN_FORMAT.test(normalizedHostname));
    async function save() {
        if (!hostnameValid || loading) return;
        const targetDomain = normalizedHostname;
        loading = true;
        try {
            const result = await updateMdnsDomain(targetDomain);
            if (result.error === 0) {
                if (/\.local\.?$/i.test(window.location.hostname)) {
                    const target = new URL(window.location.href);
                    target.hash = '#/';
                    // The responder may discover a collision after the save reply.
                    await new Promise((resolve) => setTimeout(resolve, 2000));
                    target.hostname = $networkStore.mdnsDomain || targetDomain;
                    if (target.hostname !== window.location.hostname) {
                        window.location.replace(target.toString());
                        return;
                    }
                }
                await pop();
            } else if (result.error === ERROR_CODE.MDNS_DOMAIN_CONFLICT) sendMessage(EMessageType.ERROR, $_('TIPS.MDNS_NAME_CONFLICT'));
        } catch (error) {
            console.error('Failed to update mDNS domain:', error);
        } finally {
            loading = false;
        }
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_('page.settings.mdns')} />
    <div class="input-card mdns-input-card">
        <input class="form-control" bind:value={hostname} maxlength={MDNS_DOMAIN_INPUT_MAX_LENGTH} aria-label={$_('page.settings.mdns')} /><button
            class="clear-hostname"
            type="button"
            aria-label={$_('common.clear')}
            onclick={() => (hostname = '')}><SvgIcon name="close.svg" /></button
        >
        <span class="mdns-suffix" aria-hidden="true">.local</span>
    </div>
    {#if hostname && !hostnameValid}<small class="d-block text-danger mt-2">{$_('settingsDetail.mdnsInvalid')}</small>{/if}
    <small class="d-block text-body-secondary mt-2">{$_('settingsDetail.mdnsRestartHint')}</small>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" onclick={save} disabled={!hostnameValid} {loading}>{$_('common.save')}</Button>
</div>

<style>
    .mdns-input-card {
        width: 100%;
        height: 3.625rem;
        padding: 0;
        overflow: hidden;
        border: 1px solid var(--bs-border-color);
        border-radius: 0.75rem;
        outline: 0;
        background: var(--body-inner-level-2);
        box-shadow: none;
    }
    .mdns-input-card:focus-within {
        border-color: var(--primary-button-bg);
    }
    .mdns-input-card input {
        min-width: 0;
        height: 100%;
        padding: 0 1.125rem;
        color: var(--bs-body-color);
        font-size: 1rem;
    }
    .mdns-input-card .clear-hostname {
        width: 3.25rem;
        height: 100%;
        padding: 0;
        flex: 0 0 3.25rem;
        color: var(--body-text-color-grey);
        font-size: 1.15rem;
    }
    .clear-hostname :global(.sf-icon),
    .clear-hostname :global(svg) {
        width: 1.5rem;
        height: 1.5rem;
    }
    .mdns-suffix {
        display: inline-flex;
        height: 100%;
        padding: 0 1rem;
        align-items: center;
        border-left: 1px solid var(--bs-border-color);
        color: var(--bs-body-color);
        font-size: 1rem;
        white-space: nowrap;
    }
</style>
