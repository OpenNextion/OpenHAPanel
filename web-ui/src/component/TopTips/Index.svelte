<script lang="ts">
    import { useShowTopTips, ETopTipsType } from '@/hooks/useShowTopTips.svelte';
    import ResetPassword from './ResetPassword.svelte';
    import Reconnect from './Reconnect.svelte';
    import ReconnectSuccess from './ReconnectSuccess.svelte';
    import HassConnectionStatus from './HassConnectionStatus.svelte';
    import { onMount } from 'svelte';

    const { topTips } = useShowTopTips();
    let topTipsInner = $state<HTMLDivElement>();

    let Component = $derived.by(() => {
        switch (topTips.type) {
            case ETopTipsType.RESET_PASSWORD:
                return ResetPassword;
            case ETopTipsType.RECONNECT:
                return Reconnect;
            case ETopTipsType.RECONNECT_SUCCESS:
                return ReconnectSuccess;
            case ETopTipsType.HASS_AUTHORIZATION_INVALID:
            case ETopTipsType.HASS_CONNECTION_FAILED:
            case ETopTipsType.HASS_CONNECTION_INTERRUPTED:
                return HassConnectionStatus;
            default:
                return null;
        }
    });

    onMount(() => {
        const updateTopTipsHeight = () => {
            if (topTipsInner) document.documentElement.style.setProperty('--top-tips-height', `${topTipsInner.getBoundingClientRect().height}px`);
        };
        const resizeObserver = new ResizeObserver(updateTopTipsHeight);
        if (topTipsInner) resizeObserver.observe(topTipsInner);
        updateTopTipsHeight();

        return () => {
            resizeObserver.disconnect();
            document.documentElement.style.removeProperty('--top-tips-height');
        };
    });
</script>

{#if Component}
    <div class="top-tips-inner" bind:this={topTipsInner}>
        <Component />
    </div>
{/if}

<style lang="scss">
.top-tips-inner {
    width: 100%;
    min-height: var(--top-tips-min-height);
    background: var(--content-background);
    :global {
        > .top-tips {
            display: flex;
            width: 100%;
            min-height: var(--top-tips-min-height);
            padding: 0.5rem 1rem;
            align-items: center;
            justify-content: center !important;
            font-size: 0.88rem;
            line-height: 1.25rem;
            text-align: center;
            .sf-icon {
                flex: 0 0 auto;
                font-size: 1rem;
                margin-right: 0.5rem;
            }
        }
    }
}
</style>
