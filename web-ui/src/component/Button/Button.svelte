<script lang="ts">
    import EButtonType from '@/types/components';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import type { Snippet } from 'svelte';
    import Loading from '@/component/Loading/Loading.svelte';

    type ButtonStyleType = EButtonType | `${EButtonType}`;

    interface IButtonStyle {
        title?: string;
        style?: string;
        type?: ButtonStyleType;
        iconUrl?: string;
        buttonType?: 'button' | 'reset' | 'submit' | null | undefined;
        disabled?: boolean;
        children?: Snippet;
        class?: string;
        [restArgs: string]: any;
        loading?: boolean;
    }

    const defaultStyle: string = `width: 100%`;
    let {
        title = 'Confirm',
        style = defaultStyle,
        loading = false,
        type = EButtonType.DEFAULT,
        iconUrl,
        buttonType = 'button',
        disabled = false,
        children,
        class: className = '',
        ...restArgs
    }: IButtonStyle = $props();

    const typeMap = {
        [EButtonType.PRIMARY]: 'primary-button',
        [EButtonType.BLUE]: 'blue-button',
        [EButtonType.DEFAULT]: 'default-button',
        [EButtonType.DANGER]: 'danger-button',
        [EButtonType.BORDER]: 'border-button',
        [EButtonType.DELETE]: 'delete-button',
        [EButtonType.BORDER_GHOST]: 'border-ghost-button',
    };

    const currentBtn = $derived.by(() => typeMap[type as EButtonType] || typeMap[EButtonType.DEFAULT]);
    const loadingClass = $derived.by(() => (loading ? 'loading' : ''));
</script>

<button type={buttonType} class={['btn', 'base-button', currentBtn, 'd-flex', 'align-items-center', 'justify-content-center', loadingClass, className]} {style} disabled={disabled || loading} {...restArgs}>
    {#if loading}
        <Loading size="20px" />
    {:else if iconUrl}
        <SvgIcon name={iconUrl} />
    {/if}
    <span class:ms-2={!!iconUrl}>
        {#if children}
            {@render children()}
        {:else}
            {title}
        {/if}
    </span>
</button>

<style lang="scss" scoped>
    .base-button {
        width: 100%;
        margin: 0 auto;
        border-radius: 1.88rem;
        height: 45px;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        &.loading {
            pointer-events: none;
            opacity: 0.7;
        }
        :global(.sf-loading) {
            margin-right: 0.25rem;
        }
    }
    .default-button {
        color: var(--body-text-color, var(--bs-body-color));
        background-color: var(--default-btn-bg, var(--body-inner-level-2));
        border: 1px solid #969696;
    }
    .danger-button {
        border: 1px solid #f1524b;
        color: #f1524b;
        background-color: #ffffff;
    }
    .primary-button,
    .blue-button {
        background: var(--primary-button-bg);
        border: 1px solid var(--primary-button-bg);
        color: #ffffff;
    }
    .border-button {
        background-color: transparent;
        border: 1px solid #0199df;
        color: #0199df;
    }
    .delete-button {
        background-color: transparent;
        border: 1px solid #f1524b;
        color: #f1524b;
    }
    .border-ghost-button {
        background-color: transparent;
        border: 1px solid #969696;
        color: var(--body-text-color, var(--bs-body-color));
    }
</style>
