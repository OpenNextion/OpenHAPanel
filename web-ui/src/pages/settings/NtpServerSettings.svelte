<script lang="ts">
    import { onMount } from 'svelte';
    import { _ } from 'svelte-i18n';
    import { pop } from 'svelte-spa-router';
    import Button from '@/component/Button/Button.svelte';
    import { useNtpServersHooks } from '@/hooks/useNtpServer.svelte';
    import type { NtpServers } from '@/types/device';
    import { NTP_SERVER_MAX_LENGTH, REG_EXP_DOMAIN, ROUTES_PATH } from '@/constant';
    import SettingPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { deviceStore } from '@/stores/device';

    let { params }: { params: Record<string, string> } = $props();
    const { timeInfo, isEmptyHook, setNtpServers, setTzInfo, updateNtpServers } = useNtpServersHooks();
    const field = $derived((params.field === 'server2' ? 'server2' : 'server1') as keyof NtpServers);
    const label = $derived(field === 'server1' ? 'settingsDetail.ntp1' : 'settingsDetail.ntp2');
    const defaultServers: Record<keyof NtpServers, string> = {
        server1: import.meta.env.VITE_DEFAULT_NTP_SERVER_1 || 'pool.ntp.org',
        server2: import.meta.env.VITE_DEFAULT_NTP_SERVER_2 || 'time.google.com',
    };
    let value = $state('');
    let normalizedValue = $derived(value.trim());
    let serverValid = $derived(REG_EXP_DOMAIN.test(normalizedValue));

    onMount(() => {
        if (isEmptyHook()) {
            setNtpServers($deviceStore.ntpServers);
            setTzInfo($deviceStore.tzInfo);
        }
        value = timeInfo.ntpServers[field];
    });

    async function save() {
        if (!serverValid) return;
        updateNtpServers({ [field]: normalizedValue });
        await pop();
    }
</script>

<div class="page-container settings-detail">
    <SettingPageHeader title={$_(label)} back={ROUTES_PATH.SETTINGS_TIME} />
    <div class="input-card">
        <input
            class="form-control"
            bind:value
            maxlength={NTP_SERVER_MAX_LENGTH}
            aria-label={$_(label)}
            aria-invalid={Boolean(normalizedValue && !serverValid)}
            aria-describedby={normalizedValue && !serverValid ? 'ntp-server-error' : undefined}
        /><button type="button" aria-label={$_('common.clear')} onclick={() => (value = '')}
            ><i class="bi bi-x-circle-fill"></i></button
        >
    </div>
    {#if normalizedValue && !serverValid}<small id="ntp-server-error" class="d-block text-danger mt-2">{$_('settingsDetail.ntpInvalid')}</small>{/if}
    <button class="restore-default" type="button" onclick={() => (value = defaultServers[field])}>{$_('settingsDetail.restoreDefault')}</button>
    <Button type="primary" class="settings-save" style="width: min(70%, 560px)" onclick={save} disabled={!serverValid}>{$_('common.save')}</Button>
</div>

<style>
    .restore-default {
        display: block;
        margin-top: 0.65rem;
        padding: 0;
        color: var(--body-text-color-blue, #0199df);
        border: 0;
        background: transparent;
        font-size: 0.85rem;
    }
</style>
