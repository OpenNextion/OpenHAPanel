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
    <div class:has-top-tips={topTips.type !== undefined} class="toast-container d-flex justity-content-center align-items-center" transition:fade>
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
        top: 40%;
        transform: translate(-50%, -50%);
        z-index: 9999;
        opacity: 0.9;
        width: 80vw;
        display: flex;
        justify-content: center;
        &.has-top-tips {
            top: calc(var(--top-tips-height) + 1rem);
            transform: translateX(-50%);
        }
        .toast {
            width: auto;
            max-width: 350px;
            background-color: #1a1a1a;
            color: #fff;
            padding: 1rem 1.5rem;
            border-radius: 0.5rem;
            display: flex;
            align-items: center;
            gap: 0.5rem;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
            .message {
                width: 100%;
                font-size: 1rem;
                color: #ffffff;
                max-width: 350px;
            }
        }
    }
    @media screen and (min-width: 768px) {
        .toast-container {
            top: 40px;
            width: unset;
            .toast {
                width: auto;
                max-width: 800px !important;
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
