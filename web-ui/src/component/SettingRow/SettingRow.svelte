<script lang=ts>
    import { getContext } from 'svelte';
    import type { Snippet } from 'svelte';
    import SvgIcon from '../Icon/SvgIcon.svelte';
    import { push } from 'svelte-spa-router';

    interface SettingRow {
        title: string
        titleSnippet: Snippet
        content: string
        contentSnippet: Snippet
        route: string
        tips: string
        tipsSnippet: Snippet
        footer: string
        footerSnippet: Snippet
        disable: boolean
        margin: number | string
        titleCol: number
        contentCol: number
        category: string | undefined
        href: string
        trailingIcon: string
        onclick: (args?: Partial<Pick<SettingRow, 'route' | 'href'>>) => void
    }

    let {
        title = '',
        titleSnippet,
        content = '',
        contentSnippet,
        route,
        tips = '',
        tipsSnippet,
        footer = '',
        footerSnippet,
        disable = false,
        margin = 20,
        titleCol = 6,
        contentCol = 6,
        category,
        href = '',
        trailingIcon = 'arrowRight.svg',
        onclick
    }: Partial<SettingRow> = $props()

    const marginContext = getContext<number | (() => number) | null>('setting-row-margin');
    let _margin = $derived.by(() => (typeof marginContext === 'function' ? marginContext() : marginContext) ?? margin);
    let _titleCol = $derived.by(() => getContext<number | null>('setting-row-title-col') ?? titleCol);
    let _contentCol = $derived.by(() => getContext<number | null>('setting-row-content-col') ?? contentCol);
    let safeTitleCol = $derived.by(() => Math.max(1, Math.min(11, Number(_titleCol) || 6)));
    let safeContentCol = $derived.by(() => Math.max(1, Math.min(11, Number(_contentCol) || 12 - safeTitleCol)));
    let titleColClass = $derived.by(() => `col-${safeTitleCol}`);
    let contentColClass = $derived.by(() => `col-${safeContentCol}`);
    let cursor = $derived.by(() => {
        if (onclick || route || href) return 'pointer';
        return 'default';
    });

    const handleOnclick = () => {
        if(disable) return;
        if (onclick) {
            onclick({ route, href });
        } else if (route) {
            push(route);
        } else if (href) {
            window.open(href, '_blank');
        }
    };
</script>

<div
    class="container-fluid p-0 setting-row"
    data-disable={disable}
    style:--margin={typeof _margin === 'string' ? margin : `${_margin}px`}
    style:cursor={cursor}
>
    {#if category}
        <div class="category text-truncate w-100">{category}</div>
    {/if}

    <div class="container-fluid setting-row-top">
        <div class="row align-items-center main-content text-truncate {cursor !== 'pointer' ? 'no-active' : ''}" onclick={handleOnclick} role="none">
            <div class={`${titleColClass} p-0 title-wrapper`}>
                {#if titleSnippet}
                    {@render titleSnippet()}
                {:else}
                    <span class="d-block w-100 m-0 text-truncate" >{ title }</span>
                {/if}
            </div>
            <div class={`${contentColClass} p-0 d-flex justify-content-end align-items-center content-wrapper`}>
                <div class="flex-grow-1 d-flex justify-content-end align-items-center text-end content" style={`width: ${route || href ? 'calc(100% - 1.5rem)' : '100%'}`}>
                    {#if contentSnippet}
                        {@render contentSnippet()}
                    {:else}
                        <span class="text-truncate w-100">{ content }</span>
                    {/if}
                </div>
                {#if route || href}
                    <span class="ps-2 arrow">
                        <SvgIcon name={trailingIcon} />
                    </span>
                {/if}
            </div>
        </div>
        {#if (footerSnippet || footer)}
            <div class="row sub-content">
                <div class="col-12">
                    {#if footerSnippet}
                        {@render footerSnippet()}
                    {:else}
                        { footer }
                    {/if}
                </div>
            </div>
        {/if}
    </div>

    {#if tips || tipsSnippet}
        <div class="setting-row-tips">
            {#if tipsSnippet}
                {@render tipsSnippet()}
            {:else if tips}
                { tips }
            {/if}
        </div>
    {/if}
</div>

<style lang="scss">
$background: var(--body-inner-level-2);
$border-radius: 0.63rem;
$padding: 0.75rem;
.setting-row {
    margin-bottom: var(--margin);
    &:last-of-type { margin-bottom: 0; }

    &[data-disable=true] {
        opacity: 0.75;
        cursor: not-allowed;
        pointer-events: none;
    }

    .category{
        color: var(--body-text-color-grey);
        font-size: 0.75rem;
        margin: 0 0 0.5rem 0.75rem;
    }

    .setting-row-top {
        background-color: $background;
        border-radius: $border-radius;
        .no-active{
            &:active{
                background-color: $background !important;
                transition: none !important;
            }
        }
        .main-content {
            padding: $padding;
            .title-wrapper {
                font-size: 1rem;
                line-height: 1.38rem;
                color: var(--body-text-color-highlight);
            }
            .content-wrapper {
                .content {
                    font-size: 0.88rem;
                    color: var(--body-text-color-grey);
                }
                .arrow {
                    color: #999;
                    height: 100%;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                }
            }
            &:active {
                background-color: var(--setting-row-active-bg);
                border-radius: $border-radius;
                transition: $background 0.18s;
            }
        }
        .sub-content {
            > .col-12 {
                padding: $padding;
                &:empty {
                    display: none;
                }
            }
        }
    }

    .setting-row-tips {
        // margin-top: 0.25rem;
        font-size: 0.75rem;
        line-height: 1.06rem;
        color: var(--body-text-color-grey);
        margin: 0.25rem 0 1.25rem 0.75rem;
    }
}
</style>
