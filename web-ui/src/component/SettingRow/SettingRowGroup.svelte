<script lang="ts">
    import { setContext, type Snippet } from 'svelte';

    interface SettingRowGroup {
        /** margin */
        margin: number;
        /** tips */
        tips: string;
        /** tips snippet */
        tipsSnippet: Snippet;
        /** default snippet */
        children: Snippet,
        /** category */
        category: string
    }

    let {
        margin = 0,
        tips = '',
        tipsSnippet = undefined,
        children = undefined,
        category = ''
    }: Partial<SettingRowGroup> = $props();

    setContext('setting-row-margin', () => margin);
</script>

<div class="setting-row-group" class:no-margin={margin === 0} >
    {#if category}
        <div class="category">{ category }</div>
    {/if}
    <div class="group-content">{@render children?.()}</div>

    <div class="container-fluid group-tips">
        {#if tipsSnippet}
            {@render tipsSnippet()}
        {:else if tips}
            {tips}
        {/if}
    </div>
</div>

<style lang="scss">
    $border-radius: 0.63rem;

    .setting-row-group {
        &.no-margin {
            .group-content {
                border-radius: $border-radius $border-radius;
                background-color: var(--body-inner-level-2);
            }
            :global {
                .setting-row {
                    .setting-row-top {
                        background: none;
                        border-radius: 0;
                    }
                }
            }
        }

        .category {
            color: var(--body-text-color-grey);
            font-size: 12px;
            margin: 0 0 8px 12px;
        }

        .group-tips {
            margin-top: 0.25rem;
            font-size: 0.75rem;
            line-height: 1.06rem;
            color: var(--body-text-color-grey);
        }
    }
</style>
