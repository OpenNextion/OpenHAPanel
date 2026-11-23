<script lang="ts">
    import { _ } from 'svelte-i18n';
    import NetworkPageHeader from '@/component/PageHeader/PageHeader.svelte';
    import { networkStore, setApConfig } from '@/stores/network';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SettingSwitch from '@/component/SettingSwitch/SettingSwitch.svelte';
    import ApInUseConfirmModal from './ApInUseConfirmModal.svelte';
    import { ROUTES_PATH } from '@/constant';
    let loadingField = $state<'defaultEnabled' | 'internetThroughAp' | 'hideSsid' | null>(null);
    let confirmVisible = $state(false);
    let pendingConfig = $state<{
        field: 'defaultEnabled' | 'internetThroughAp' | 'hideSsid';
        checked: boolean;
        input: HTMLInputElement;
    } | null>(null);

    const applyConfig = async (
        field: 'defaultEnabled' | 'internetThroughAp' | 'hideSsid',
        checked: boolean,
        input: HTMLInputElement,
    ) => {
        if (loadingField) return;
        loadingField = field;
        try {
            const response = await setApConfig({ [field]: checked });
            input.checked = response.error === 0 ? checked : !checked;
        } catch (error) {
            console.error(`Failed to update AP ${field}:`, error);
            input.checked = !checked;
        } finally {
            loadingField = null;
        }
    };

    const updateConfig = async (field: 'defaultEnabled' | 'internetThroughAp' | 'hideSsid', event: Event) => {
        const input = event.currentTarget as HTMLInputElement;
        const checked = input.checked;
        if (($networkStore.ap?.clients.current ?? 0) > 0) {
            input.checked = !checked;
            pendingConfig = { field, checked, input };
            confirmVisible = true;
            return;
        }
        await applyConfig(field, checked, input);
    };

    async function confirmConfig() {
        const pending = pendingConfig;
        if (!pending) return;
        await applyConfig(pending.field, pending.checked, pending.input);
        confirmVisible = false;
        pendingConfig = null;
    }

    function cancelConfig() {
        confirmVisible = false;
        pendingConfig = null;
    }
</script>

<div class="page-container network-page network-detail ap-settings">
    <NetworkPageHeader title={$_('networkPage.apSettings')} back="/network" />
    <SettingRowGroup>
        <SettingRow title="AP" titleCol={10} contentCol={2}>
            {#snippet titleSnippet()}<span class="ap-row-title"><span>AP</span><small>{$_('networkPage.apHint')}</small></span>{/snippet}
            {#snippet contentSnippet()}<SettingSwitch ariaLabel="AP" checked={$networkStore.ap?.defaultEnabled ?? false} disabled={loadingField !== null} onchange={(event) => updateConfig('defaultEnabled', event)} />{/snippet}
        </SettingRow>
        <!--<SettingRow title={$_('networkPage.internetConnection')} disable={!$networkStore.ap?.enabled}>
            {#snippet contentSnippet()}<SettingSwitch ariaLabel={$_('networkPage.internetConnection')} checked={$networkStore.ap?.internetThroughAp ?? false} disabled={!$networkStore.ap?.enabled || loadingField !== null} onchange={(event) => updateConfig('internetThroughAp', event)} />{/snippet}
        </SettingRow>-->
    </SettingRowGroup>
    <SettingRowGroup>
        <SettingRow title="SSID" content={$networkStore.ap?.apName || '--'} route={ROUTES_PATH.NETWORK_AP_SSID} />
        <SettingRow title={$_('networkPage.changePassword')} content={$networkStore?.ap?.password || ''} route={ROUTES_PATH.NETWORK_AP_PASSWORD} />
        <!--<SettingRow title={$_('networkPage.hideNetwork')}>
            {#snippet contentSnippet()}<SettingSwitch ariaLabel={$_('networkPage.hideNetwork')} checked={$networkStore.ap?.hideSsid ?? false} disabled={loadingField !== null} onchange={(event) => updateConfig('hideSsid', event)} />{/snippet}
        </SettingRow>-->
        <SettingRow title={$_('networkPage.ipv4Lan')} route={ROUTES_PATH.NETWORK_AP_IPV4} />
        <SettingRow title={$_('networkPage.channel')} route={ROUTES_PATH.NETWORK_AP_CHANNEL} />
    </SettingRowGroup>
</div>

<ApInUseConfirmModal visible={confirmVisible} loading={loadingField !== null} confirm={confirmConfig} cancel={cancelConfig} />

<style>
    .ap-settings > :global(.setting-row-group:not(:last-child)) {
        margin-bottom: 1.25rem;
    }

    .ap-row-title {
        display: flex;
        flex-direction: column;
        gap: 0.25rem;
    }

    .ap-row-title small {
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        line-height: 1.25;
    }

</style>
