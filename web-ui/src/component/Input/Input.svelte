<script lang="ts">
    import { onDestroy, onMount, type Snippet } from 'svelte';
    import SvgIcon from '../Icon/SvgIcon.svelte';
    import { childOf } from '@/utils/commonUtils';

    interface Input {
        size: 'sm' | 'default' | 'lg'
        value: string
        prefixSnippet: Snippet
        suffixSnippet: Snippet
        allowClear: boolean
        disabled: boolean
        width: string
        maxlength: number
        minlength: number
        type: 'password'
        error: string | boolean
        trim: boolean
        [restArgs: string]: any
    }

    let {
        size = 'default',
        value = $bindable(''),
        prefixSnippet,
        suffixSnippet,
        allowClear = true,
        disabled = false,
        width,
        maxlength,
        minlength,
        type,
        error,
        trim,
        ...restArgs
    }: Partial<Input> = $props();

    let inputElement = $state<HTMLInputElement>();
    let passwordHidden = $state(true);
    let focused = $state(false);
    let rootDom = $state<HTMLElement>();
    let sizeClassName = $derived.by(() => {
        switch (size) {
            case 'sm':
                return 'form-control-sm';
            case 'lg':
                return 'form-control-lg';
            default:
                return '';
        }
    });
    let inputType = $derived.by(() => {
        if (type === 'password') {
            return passwordHidden ? 'password' : 'text';
        }
        return 'text';
    });

    const togglePassword = (event: MouseEvent) => {
        event.preventDefault();
        event.stopPropagation();
        passwordHidden = !passwordHidden;
    };

    const preventPasswordEyeFocus = (event: PointerEvent) => {
        event.preventDefault();
        event.stopPropagation();
    };

    const valueTrim = () => {
        value = value.trim();
    };

    onMount(() => {
        if (!inputElement) return;
        if (trim) {
            inputElement.addEventListener('input', valueTrim);
        }
    });

    onDestroy(() => {
        if (!inputElement) return;
        if (trim) {
            inputElement.removeEventListener('input', valueTrim);
        }
    });

    export const focus = () => {
        focused = true;
        inputElement?.focus();
        setTimeout(() => {
            document.addEventListener('click', (e) => {
                const dom = e.target as HTMLElement;
                if (dom === rootDom || childOf(rootDom!, dom)) return;
                focused = false;
            }, { once: true });
        });
    }
</script>

<span
    class={`form-control d-flex align-items-center justify-content-center sf-affix-wrapper ${sizeClassName}`}
    class:disabled
    class:error={error !== undefined && !!error}
    class:focused
    data-error={typeof error === 'string' ? error : ''}
    style:width={width}
    onclick={focus}
    role="none"
    bind:this={rootDom}
>
    {#if prefixSnippet}
        <span class="d-flex align-items-center sf-input-prefix"> {@render prefixSnippet()} </span>
    {/if}
    <span class="d-flex align-items-center justify-content-center flex-grow-1">
        <input
            bind:this={inputElement}
            type={inputType}
            class="p-0 bg-transparent flex-grow-1 form-control-plaintext sf-input"
            bind:value={value}
            maxlength={maxlength}
            minlength={minlength}
            {...restArgs}
        />
        {#if type === 'password'}
            <span class="password-eye" onpointerdown={preventPasswordEyeFocus} onclick={togglePassword} role="none">
                <SvgIcon name={passwordHidden ? 'eye.svg' : 'eyeInvisible.svg'} />
            </span>
        {:else if allowClear}
            {#if !disabled && !restArgs.readonly}
                <span class="allow-clear" class:show={value.length > 0} onclick={() => value = ''} role="none"></span>
            {/if}
        {/if}
    </span>
    {#if suffixSnippet}
        <span class="d-flex align-items-center sf-input-suffix"> {@render suffixSnippet()} </span>
    {/if}
</span>

<style lang="scss">
$error-color: #F1524B;

.sf-affix-wrapper {
    border-radius: 0.625rem;
    &.focused {
        color: var(--bs-body-color);
        background-color: var(--bs-body-bg);
        border-color: #86b7fe;
        outline: 0;
        box-shadow: 0 0 0 .25rem rgba(13, 110, 253, .25);
    }
    &.disabled {
        pointer-events: none;
        opacity: 0.75;
    }

    &.error {
        position: relative;
        border-color: $error-color !important;
        box-shadow: none !important;
        &::before {
            content: attr(data-error);
            position: absolute;
            left: 0;
            bottom: 0;
            transform: translateY(100%);
            line-height: 1rem;
            font-size: 0.75rem;
            width: 100%;
            color: $error-color;
            pointer-events: none;
            padding-top: 0.25rem;
        }
    }

    .sf-input-prefix {
        margin-right: 0.75rem;
    }
    .sf-input-suffix {
        margin-left: 0.75rem;
    }
    .sf-input {
        height: auto;
        border: none;

        &::-ms-reveal,
        &::-ms-clear {
            display: none;
        }
    }
    .password-eye {
        display: inline-flex;
        width: 2.5rem;
        flex: 0 0 2.5rem;
        align-self: stretch;
        align-items: center;
        justify-content: center;
        cursor: pointer;
    }
    .allow-clear {
        visibility: hidden;
        position: relative;
        width: 1.25rem;
        height: 1.25rem;
        border-radius: 50%;
        background-color: #969696;
        text-align: center;
        margin: 0 auto;
        cursor: pointer;

        &.show {
            visibility: visible;
        }

        &::before, &::after {
            content: '';
            position: absolute;
            top: 50%;
            left: 50%;
            width: 0.625rem;
            height: 0.125rem;
            background-color: #333;
        }
        &:before {
            transform: translate(-50%, -50%) rotate(45deg);
        }

        &:after {
            transform: translate(-50%, -50%) rotate(-45deg);
        }
        &:active {
            background-color: #999;
            transform: scale(0.9);
        }
    }
}
</style>
