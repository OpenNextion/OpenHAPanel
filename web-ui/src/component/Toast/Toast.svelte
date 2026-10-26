<script>
    import { messageStore, EMessageType } from '@/stores/message';
    import { fade } from 'svelte/transition';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { useShowTopTips } from '@/hooks/useShowTopTips.svelte';

    const { topTips } = useShowTopTips();

    const iconMap = {
        [EMessageType.SUCCESS]: 'success.svg',
        [EMessageType.ERROR]: 'error.svg',
        [EMessageType.WARN]: 'warn.svg',
    };
</script>

{#if $messageStore.showMessage}
    <div class:has-top-tips={topTips.type !== undefined} class="toast-container d-flex justify-content-center align-items-center" transition:fade>
        <div class="toast">
            <SvgIcon name={iconMap[$messageStore.type] || iconMap[EMessageType.SUCCESS]} />
            <div class="message">{$messageStore.message}</div>
        </div>
    </div>
{/if}

<style lang="scss">
    .toast-container {
        position: fixed;
        left: 50%;
        top: 1.5rem;
        transform: translateX(-50%);
        z-index: 9999;
        opacity: 0.9;
        width: calc(100% - 2rem);
        display: flex;
        justify-content: center;
        &.has-top-tips {
            top: calc(var(--top-tips-height, 2.5rem) + 1.5rem);
        }
        .toast {
            width: auto;
            max-width: 21.875rem;
            min-width: 0;
            background-color: #1a1a1a;
            color: #fff;
            padding: 1rem 1.5rem;
            border-radius: 0.5rem;
            display: flex;
            align-items: center;
            gap: 0.5rem;
            box-shadow: 0 0.125rem 0.25rem rgba(0, 0, 0, 0.2);
            :global(.sf-icon) {
                flex-shrink: 0;
            }
            .message {
                width: 100%;
                min-width: 0;
                font-size: 1rem;
                color: #ffffff;
                max-width: 21.875rem;
                overflow-wrap: anywhere;
            }
        }
    }
    @media screen and (min-width: 48rem) {
        .toast-container {
            width: max-content;
            max-width: calc(100% - 2rem);
            .toast {
                width: auto;
                max-width: 50rem;
                .message {
                    max-width: 70vw;
                    white-space: nowrap;
                    overflow: hidden;
                    text-overflow: ellipsis;
                }
            }
        }
    }
</style>
