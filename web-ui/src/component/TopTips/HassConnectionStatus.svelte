<script lang="ts">
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { ETopTipsType, useShowTopTips } from '@/hooks/useShowTopTips.svelte';

    const { topTips } = useShowTopTips();
    const authorizationInvalid = $derived(topTips.type === ETopTipsType.HASS_AUTHORIZATION_INVALID);
    const connectionInterrupted = $derived(topTips.type === ETopTipsType.HASS_CONNECTION_INTERRUPTED);
    const messageKey = $derived(
        authorizationInvalid
            ? 'homeAssistant.authorizationInvalid'
            : connectionInterrupted
              ? 'homeAssistant.connectionInterrupted'
              : 'homeAssistant.connectionFailed',
    );
</script>

<div
    class:authorization-invalid={authorizationInvalid}
    class:connection-interrupted={connectionInterrupted}
    class:connection-failed={!authorizationInvalid && !connectionInterrupted}
    class="d-flex justify-content-start align-items-center top-tips"
>
    <SvgIcon name="mdi:home-assistant" />
    {$_(messageKey)}
</div>

<style>
    .top-tips {
        padding: 0 1rem;
    }

    .authorization-invalid {
        color: #969696;
        background: rgba(225, 225, 229, 1);
    }

    :global([data-bs-theme='dark']) .authorization-invalid {
        background: var(--web-, rgba(220, 220, 220, 0.2));
    }

    .connection-interrupted {
        color: #ef961d;
        background: rgba(239, 150, 29, 0.2);
    }

    .connection-failed {
        color: #f1524b;
        background: rgba(241, 82, 75, 0.2);
    }
</style>
