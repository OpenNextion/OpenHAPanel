<script lang="ts">
    import { _ } from 'svelte-i18n';
    import DropDown from '@/component/Dropdown/DropDown.svelte';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { currentLanDevice, lanDeviceStore } from '@/stores/lanDevice';

    interface Props {
        guest?: boolean;
    }

    let { guest = false }: Props = $props();
    const deviceOptions = $derived($lanDeviceStore.devices.map((device) => ({ label: device.name, value: device.ip })));

    function switchDevice(ip: string) {
        if (!ip?.trim()) return;
        window.location.href = /^https?:\/\//i.test(ip) ? ip : `http://${ip}`;
    }
</script>

<div class:guest class="device-switcher">
    <DropDown value={$currentLanDevice?.ip} options={deviceOptions} disabled={deviceOptions.length === 0} onchange={switchDevice} ariaLabel={$_('common.switchDevice')}>
        <span class="device-copy"><strong>{$currentLanDevice?.name || '--'}</strong><span class="device-ip">{$currentLanDevice?.ip || '--'}</span></span>
        {#snippet optionSnippet(option, active)}
            <span class="device-option-copy">
                <span class="device-option-name">{option.label || '--'}</span>
                <span class="device-option-ip">{option.value}</span>
            </span>
            {#if active}<span class="device-option-check" aria-hidden="true"><SvgIcon name="check.svg" /></span>{/if}
        {/snippet}
    </DropDown>
</div>

<style lang="scss">
    .device-switcher {
        --device-menu-offset: 4.5rem;
        --device-menu-background: rgba(249, 249, 249, 1);
        min-width: 0;
        flex: 1 1 auto;
    }
    :global([data-bs-theme='dark']) .device-switcher {
        --device-menu-background: rgba(28, 29, 29, 1);
    }
    .device-switcher :global(.dropdown) {
        width: 100%;
        min-width: 0;
    }
    .device-switcher :global(.dropdown-toggle) {
        min-height: 2.5rem;
    }
    .device-copy {
        min-width: 0;
        text-align: left;
    }
    .device-copy strong {
        font-size: 1rem;
        font-weight: 400;
    }
    .device-copy .device-ip {
        margin-top: 0.25rem;
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        font-weight: 400;
    }
    .device-switcher :global(.dropdown-menu) {
        left: calc(-1 * var(--device-menu-offset));
        width: calc(100% + var(--device-menu-offset) + 1.25rem) !important;
        max-height: 18rem;
        overflow-y: auto;
        margin: 0;
        padding: 0;
        border: 0;
        border-radius: 0 0 0.625rem 0.625rem;
        background: var(--device-menu-background);
        box-shadow: none;
    }
    .device-switcher :global(.dropdown-item) {
        display: flex;
        min-width: 0;
        min-height: 3.25rem;
        padding: 0.75rem 1.25rem;
        align-items: center;
        justify-content: space-between;
        gap: 0.5rem;
        color: var(--bs-body-color);
        font-size: 1rem;
    }
    .device-switcher :global(.dropdown-item.active) {
        background: transparent;
        color: var(--bs-body-color);
    }
    .device-switcher :global(.dropdown-item:hover) {
        background: var(--web-10, rgba(1, 119, 253, 0.1));
    }
    .device-option-copy {
        display: flex;
        min-width: 0;
        flex-direction: column;
        gap: 0.25rem;
    }
    .device-option-name {
        display: block;
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .device-option-ip {
        overflow: hidden;
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .device-option-check {
        flex: 0 0 auto;
        color: var(--primary-button-bg);
        font-size: 0.875rem;
    }
</style>
