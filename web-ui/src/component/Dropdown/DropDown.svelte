<script lang="ts">
    import { tick, type Snippet } from 'svelte';
    import SvgIcon from '../Icon/SvgIcon.svelte';

    interface Option {
        label: string
        value: any
    }

    interface DropDownProps {
        value: any
        options: Option[]
        disabled: boolean
        children: Snippet
        optionSnippet: Snippet<[option: Option, active: boolean]>
        onchange: (value: any, previousValue?: any) => void
        id: string
        ariaLabel: string
        searchable: boolean
        searchValue: string
        searchPlaceholder: string
        noOptionsText: string
        onsearch: (value: string) => void
    }

    let {
        value = $bindable(),
        options = [],
        disabled = false,
        children,
        optionSnippet,
        onchange,
        id,
        ariaLabel,
        searchable = false,
        searchValue = $bindable(''),
        searchPlaceholder = '',
        noOptionsText = '',
        onsearch,
    }: Partial<DropDownProps> = $props();

    let selectedLabel = $derived.by(() => options.find(option => option.value === value)?.label ?? value);
    let expanded = $state(false);
    let menuElement = $state<HTMLUListElement>();
    let searchInput = $state<HTMLInputElement>();

    $effect(() => {
        if (!expanded) return;
        void tick().then(() => {
            searchInput?.focus();
            menuElement?.querySelector<HTMLElement>('.dropdown-item.active')?.scrollIntoView({ block: 'nearest' });
        });
    });

    function openSearch() {
        if (disabled) return;
        expanded = true;
    }

    function updateSearch(value: string) {
        searchValue = value;
        expanded = true;
        onsearch?.(value);
    }

    const handleSelected = (event: MouseEvent, _value: any) => {
        event.preventDefault();
        event.stopPropagation();
        const prevValue = value;
        expanded = false;
        if (prevValue === _value) return;
        value = _value;
        onchange?.(_value, prevValue);
    };
</script>

<div class="dropdown" class:disabled>
    {#if searchable}
        {#if children && !expanded}
            <button
                {id}
                class="dropdown-trigger custom-searchable-trigger"
                type="button"
                aria-expanded="false"
                aria-label={ariaLabel}
                {disabled}
                onclick={openSearch}
            >
                {@render children()}
            </button>
        {:else}
            <div class="dropdown-trigger searchable-trigger" role="combobox" aria-expanded={expanded} aria-label={ariaLabel} aria-controls={id ? `${id}-menu` : undefined}>
                <SvgIcon name="mdi:magnify" />
                <input
                    bind:this={searchInput}
                    {id}
                    value={searchValue}
                    placeholder={searchPlaceholder || String(selectedLabel ?? '')}
                    {disabled}
                    onfocus={openSearch}
                    oninput={(event) => updateSearch(event.currentTarget.value)}
                    onkeydown={(event) => {
                        if (event.key === 'Escape') expanded = false;
                    }}
                />
                {#if searchValue}
                    <button
                        type="button"
                        class="searchable-clear"
                        aria-label="Clear search"
                        onclick={(event) => {
                            event.stopPropagation();
                            updateSearch('');
                            void tick().then(() => searchInput?.focus());
                        }}
                    >
                        <SvgIcon name="mdi:close" />
                    </button>
                {/if}
                <button type="button" class="searchable-arrow arrow" aria-label={ariaLabel} {disabled} onclick={() => (expanded = !expanded)}>
                    <SvgIcon name="arrowRight.svg" />
                </button>
            </div>
        {/if}
    {:else if children}
        <button
            {id}
            class="h-100 w-100 d-flex align-items-center justify-content-center dropdown-toggle dropdown-trigger"
            type="button"
            aria-expanded={expanded}
            aria-label={ariaLabel}
            {disabled}
            onclick={() => (expanded = !expanded)}
        >
            <div class="h-100 d-flex align-items-center flex-grow-1 label-content">
                {@render children()}
            </div>
            <div class="h-100 d-flex align-items-center justify-content-center arrow">
                <i class="d-flex" style="color: var(--body-text-color-grey);">
                    <SvgIcon name="arrowRight.svg" />
                </i>
            </div>
        </button>
    {:else}
        <button
            class="btn btn-secondary dropdown-toggle"
            type="button"
            aria-expanded={expanded}
            aria-label={ariaLabel}
            {disabled}
            onclick={() => (expanded = !expanded)}
        >
            { selectedLabel }
        </button>
    {/if}
    {#if expanded}
        <button
            class="dropdown-shadow"
            type="button"
            aria-label="Close dropdown"
            onclick={(event) => {
                event.preventDefault();
                event.stopPropagation();
                expanded = false;
            }}
        ></button>
        <ul bind:this={menuElement} id={id ? `${id}-menu` : undefined} class="show w-100 dropdown-menu">
            {#each options as option, index (`${option.label}-${index}`)}
                <li class="dropdown-item" class:active={option.value === value} onclick={(event) => handleSelected(event, option.value)} data-value={option.value} role="none">
                    {#if optionSnippet}
                        {@render optionSnippet(option, option.value === value)}
                    {:else}
                        { option.label }
                    {/if}
                </li>
            {:else}
                {#if noOptionsText}<li class="dropdown-empty" role="status">{noOptionsText}</li>{/if}
            {/each}
        </ul>
    {/if}
</div>

<style lang="scss">
.dropdown {
    width: 10rem;
    cursor: pointer;
    &.disabled {
        cursor: default;
        opacity: 0.7;
    }
    .dropdown-trigger {
        padding: 0;
        color: inherit;
        border: 0;
        background: transparent;
        text-align: left;
    }
    .searchable-trigger {
        display: flex;
        min-height: 40px;
        padding: 0 0.65rem;
        align-items: center;
        gap: 0.5rem;
        background: transparent;
    }
    .custom-searchable-trigger {
        width: 100%;
        min-height: 56px;
    }
    .searchable-trigger input {
        min-width: 0;
        flex: 1;
        border: 0;
        outline: 0;
        background: transparent;
        color: inherit;
    }
    .searchable-arrow,
    .searchable-clear {
        display: inline-flex;
        padding: 0;
        border: 0;
        background: transparent;
        color: var(--body-text-color-grey);
    }
    .searchable-clear {
        flex: 0 0 auto;
        font-size: 1rem;
    }
    .searchable-arrow :global(.sf-icon) {
        transition: transform 0.25s;
        transform: rotate(90deg);
    }
    .searchable-trigger[aria-expanded="true"] .searchable-arrow :global(.sf-icon) {
        transform: rotate(-90deg);
    }
    .dropdown-toggle {
        &::after {
            display: none;
        }
        .arrow {
            :global {
                .sf-icon {
                    transition: all 0.25s;
                    transform: rotate(90deg);
                }
            }
        }
        &[aria-expanded="true"] {
            .arrow {
                :global {
                    .sf-icon {
                        transform: rotate(-90deg);
                    }
                }
            }
        }
        .label-content {
            width: calc(100% - 1rem);
        }
        .arrow {
            width: 1rem;
        }
    }
    .dropdown-shadow {
        position: fixed;
        inset: 0;
        z-index: 999;
        padding: 0;
        border: 0;
        background: transparent;
        cursor: auto;
    }
    .dropdown-menu {
        --bs-dropdown-link-active-color: var(--body-text-color-highlight);
        z-index: 1000;
    }
    .dropdown-empty {
        padding: 0.75rem;
        color: var(--bs-secondary-color);
        cursor: default;
        text-align: center;
    }
}
</style>
