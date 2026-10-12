<script lang="ts">
    import type { Snippet} from 'svelte';
    interface IFormItem {
        validate?: string;
        label?: string;
        name?: string;
        required?: boolean;
        style?: string;
        children?: Snippet | undefined,
        labelSnippet?: Snippet
    }
    const { validate, label, name, required = false, style, children, labelSnippet }: IFormItem = $props();
</script>

<div class={['form-item', validate ? 'fail-status' : '']} {style}>
    {#if labelSnippet}
        {@render labelSnippet()}
    {:else if label}
        <div class="label-area">
            {#if required}
                <span class="required">*</span>
            {/if}
            <label for={name}>{label}</label>
        </div>
    {/if}
    <div>{@render children?.()}</div>
    <div class="form-item-footer">
        {#if validate}
            <div class="error-message">{validate}</div>
        {/if}
    </div>
</div>

<style lang="scss">
    .form-item {
        .label-area {
            display: flex;
            align-items: center;
            justify-content: flex-start;
            margin-bottom: 3px;
            .required {
                margin-right: 4px;
                color: #f1524b;
            }
            label {
                display: block;
                margin-bottom: 5px;
            }
        }
        .form-item-footer {
            visibility: visible;
            min-height: 20px;
            .error-message {
                height: 20px;
                margin-left: 16px;
                font-size: 14px;
                color: #f1524b;
            }
        }
    }
    .fail-status {
        :global(.error-status-input) {
            border-color: #f1524b!important;
        }
    }
</style>
