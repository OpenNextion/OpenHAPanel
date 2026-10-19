<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { getWifiSignalIcon } from '@/constant';
    import type { Result } from '@/types/network';
    import SettingRow from '@/component/SettingRow/SettingRow.svelte';
    import SettingRowGroup from '@/component/SettingRow/SettingRowGroup.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';

    interface Props {
        results: Result[];
        loading: boolean;
        loadingIcon: string;
        onRefresh: () => void;
        onSelect: (network: Result) => void;
    }

    let { results, loading, loadingIcon, onRefresh, onSelect }: Props = $props();
    let securityTooltipBssid = $state<string | null>(null);

    function handleRefreshKeydown(event: KeyboardEvent) {
        if (event.key !== 'Enter' && event.key !== ' ') return;
        event.preventDefault();
        if (!loading) onRefresh();
    }

    function toggleSecurityTooltip(event: MouseEvent, bssid: string) {
        event.stopPropagation();
        securityTooltipBssid = securityTooltipBssid === bssid ? null : bssid;
    }

    function selectNetwork(network: Result) {
        securityTooltipBssid = null;
        onSelect(network);
    }
</script>

<div class="wifi-network-list">
    <div class="wifi-list-heading">
        <span>
            {#if loading}<span class="wifi-heading-loading"><SvgIcon name={loadingIcon} /></span>{/if}
            {$_(loading ? 'networkPage.scanning' : 'networkPage.nearbyWifi')}
        </span>
        <span
            class="wifi-refresh"
            role="button"
            tabindex="0"
            aria-disabled={loading}
            aria-label={$_('networkPage.refresh')}
            onclick={() => !loading && onRefresh()}
            onkeydown={handleRefreshKeydown}><SvgIcon name="refresh.svg" /></span
        >
    </div>

    <div class="wifi-results-scroll">
        {#if results.length}
            <SettingRowGroup>
                {#each results as item (item.bssid)}
                    <div class="wifi-setting-row" class:tooltip-open={securityTooltipBssid === item.bssid}>
                        <SettingRow titleCol={9} contentCol={3} onclick={() => selectNetwork(item)}>
                            {#snippet titleSnippet()}
                                <span class="wifi-row-title">
                                    <span class="wifi-signal-icon"><SvgIcon name={getWifiSignalIcon(item.rssi)} /></span>
                                    <span class="wifi-row-copy">
                                        <strong>{item.ssid || $_('networkPage.hiddenNetwork')}</strong>
                                        <small><span>RSSI {item.rssi}dBm</span><span>Channel {item.channel}</span></small>
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
        {:else if !loading}
            <p class="wifi-empty-result">{$_('networkPage.noWifi')}</p>
        {/if}
    </div>
</div>

<style lang="scss">
    .wifi-network-list {
        display: flex;
        flex: 1 1 0;
        flex-direction: column;
        height: 0;
        min-height: 0;
        border-radius: 0.63rem;
        background: var(--body-inner-level-2);
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
        display: flex;
        width: 2.5rem;
        height: 2.5rem;
        margin-left: auto;
        align-items: center;
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
        flex: 1 1 0;
        height: 0;
        min-height: 0;
        overflow-y: auto;
        overscroll-behavior: contain;
        touch-action: pan-y;
        -webkit-overflow-scrolling: touch;
        scrollbar-gutter: stable;
        background: var(--body-inner-level-2);
        border-radius: 0 0 0.63rem 0.63rem;
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
    .wifi-row-copy strong,
    .wifi-row-copy small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .wifi-row-copy strong {
        font-size: 0.9rem;
        font-weight: 600;
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
    .wifi-empty-result {
        padding: 1.5rem 1rem;
        margin: 0;
        color: var(--body-text-color-grey);
        text-align: center;
    }
</style>
