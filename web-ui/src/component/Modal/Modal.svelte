<script lang="ts">
    import Button from '@/component/Button/Button.svelte';
    import type { Snippet } from 'svelte';
    import { t } from 'svelte-i18n';

    interface IModalParams {
        title?: string;
        subTitleSnippet?: Snippet;
        subTitle?: string;
        footer?: boolean | null;
        cancelBtn?: boolean | null;
        modalVisible: boolean;
        confirm?: any;
        confirmLoading?: boolean
        cancel?: any;
        content?: Snippet;
        tips?: string;
        confirmTitle?: string;
        customSnippet?: Snippet;
        maskClosable?: boolean;
        mobileSheet?: boolean;
        setVisible?: (visible: boolean) => void
    }

    const {
        confirmTitle,
        title,
        subTitle,
        footer = true,
        cancelBtn = false,
        modalVisible = false,
        confirm,
        confirmLoading,
        cancel,
        content,
        tips,
        customSnippet,
        subTitleSnippet,
        maskClosable = false,
        mobileSheet = false,
        setVisible
    }: IModalParams = $props();

    let _confirmTitle = $derived.by(() => confirmTitle ?? $t('common.confirm'));

    const handleMaskClose = () => {
        if (maskClosable) {
            setVisible?.(false);
        }
    };
</script>

{#if modalVisible}
    <div class:mobile-sheet={mobileSheet} class="modal-mask" onclick={handleMaskClose} role="none">
        {#if customSnippet}
            <div class="fixed-center" onclick={(e) => e.stopPropagation()} role="none">
                {@render customSnippet()}
            </div>
        {:else}
            <div class="content fixed-center" onclick={(e) => e.stopPropagation()} role="none">
                {#if title || subTitle}
                    <div class="default-content">
                        <div class="title">{title}</div>
                        {#if subTitleSnippet}
                            {@render subTitleSnippet()}
                        {:else}
                            <div class="subTitle">{subTitle}</div>
                        {/if}
                        <div class="tips">{tips}</div>
                    </div>
                {:else if content}
                    {@render content()}
                {/if}

                {#if footer}
                    <div class="footer">
                        <Button title={_confirmTitle} onclick={confirm} type="primary" loading={confirmLoading} />
                        {#if cancelBtn}
                            <div class="cancel-button">
                                <Button title={$t('common.cancel')} onclick={cancel} />
                            </div>
                        {/if}
                    </div>
                {/if}
            </div>
        {/if}
    </div>
{/if}

<style lang="scss" scoped>
    .modal-mask {
        position: fixed;
        top: 0;
        right: 0;
        left: 0;
        bottom: 0;
        background: rgba(0, 0, 0, 0.6);
        user-select: none;
        z-index: 9998;

        .fixed-center{
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
        }
        .content {
            width: 311px;
            text-align: center;
            background-color: var(--body-inner-level-2);
            border-radius: 12px;
            overflow: hidden;
            color: var(--bs-body-color);
            .default-content {
                padding: 24px;
                color: var(--bs-body-color);
                text-align: center;
                .title {
                    font-weight: 500;
                    font-size: 18px;
                }
                .subTitle {
                    margin: 8px 0;
                    font-weight: 400;
                    font-size: 16px;
                    color: var(--bs-secondary-color);
                }
                .tips {
                    font-size: 14px;
                    color: var(--body-text-color-grey);
                }
            }
        }
        .footer {
            display: flex;
            justify-content: space-between;
            flex-direction: column;
            align-items: center;
            padding: 0 24px 24px 24px;
            .cancel-button {
                width: 100%;
                margin-top: 12px;
                :global(.default-button) {
                    color: var(--bs-body-color);
                    border-color: var(--bs-border-color);
                    background-color: transparent;
                }
            }
        }
    }
    @media (max-width: 700px) {
        .modal-mask.mobile-sheet .fixed-center {
            top: auto;
            right: 0;
            bottom: 0;
            left: 0;
            width: 100%;
            transform: none;
            animation: mobile-sheet-enter 0.24s ease-out both;
        }
    }
    @keyframes mobile-sheet-enter {
        from {
            transform: translateY(100%);
        }
        to {
            transform: translateY(0);
        }
    }
    @media (prefers-reduced-motion: reduce) {
        .modal-mask.mobile-sheet .fixed-center {
            animation: none;
        }
    }
</style>
