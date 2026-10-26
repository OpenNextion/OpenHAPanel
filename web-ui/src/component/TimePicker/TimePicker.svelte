<script lang="ts">
    import { tick } from 'svelte';
    import { _ } from 'svelte-i18n';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';

    interface TimePickerProps {
        value: string;
        label: string;
        disabled?: boolean;
        onchange: (value: string) => void;
    }

    let { value, label, disabled = false, onchange }: TimePickerProps = $props();
    let open = $state(false);
    let draftHour = $state(20);
    let draftMinute = $state(0);
    let hourColumn = $state<HTMLDivElement>();
    let minuteColumn = $state<HTMLDivElement>();

    const formattedTime = $derived(value || '00:00');
    const hourOptions = Array.from({ length: 24 }, (_, hour) => hour);
    const minuteOptions = Array.from({ length: 60 }, (_, minute) => minute);

    function setDraftFromValue() {
        const [hour, minute] = formattedTime.split(':').map(Number);
        draftHour = Number.isInteger(hour) && hour >= 0 && hour <= 23 ? hour : 0;
        draftMinute = Number.isInteger(minute) && minute >= 0 && minute <= 59 ? minute : 0;
    }

    function scrollOptionToTop(column: HTMLDivElement, value: number) {
        const option = column.querySelector<HTMLButtonElement>(`[data-time-option="${value}"]`);
        if (!option) return;
        const optionTop = option.getBoundingClientRect().top - column.getBoundingClientRect().top + column.scrollTop;
        column.scrollTop = optionTop;
    }

    async function togglePicker() {
        if (disabled) return;
        if (open) {
            open = false;
            return;
        }
        setDraftFromValue();
        open = true;
        await tick();
        if (hourColumn) scrollOptionToTop(hourColumn, draftHour);
        if (minuteColumn) scrollOptionToTop(minuteColumn, draftMinute);
    }

    function confirm() {
        onchange(`${String(draftHour).padStart(2, '0')}:${String(draftMinute).padStart(2, '0')}`);
        open = false;
    }

    function handleKeydown(event: KeyboardEvent) {
        if (event.key === 'Escape') open = false;
    }
</script>

<div class:open class="time-picker">
    <button
        class="time-picker-trigger"
        type="button"
        aria-label={label}
        aria-expanded={open}
        disabled={disabled}
        onclick={togglePicker}
        onkeydown={handleKeydown}
    >
        <span>{formattedTime}</span>
        <SvgIcon name="mdi:clock-outline" />
    </button>
    {#if open}
        <button class="time-picker-backdrop" type="button" aria-label={$_('common.cancel')} onclick={() => (open = false)}></button>
        <div class="time-picker-popover" role="dialog" aria-label={label}>
            <div class="time-picker-columns">
                <div bind:this={hourColumn} class="time-picker-column" role="listbox" aria-label={`${label} hour`}>
                    {#each hourOptions as hour (hour)}
                        <button
                            data-time-option={hour}
                            class:selected={draftHour === hour}
                            type="button"
                            role="option"
                            aria-selected={draftHour === hour}
                            onclick={() => (draftHour = hour)}
                        >{String(hour).padStart(2, '0')}</button>
                    {/each}
                </div>
                <div bind:this={minuteColumn} class="time-picker-column" role="listbox" aria-label={`${label} minute`}>
                    {#each minuteOptions as minute (minute)}
                        <button
                            data-time-option={minute}
                            class:selected={draftMinute === minute}
                            type="button"
                            role="option"
                            aria-selected={draftMinute === minute}
                            onclick={() => (draftMinute = minute)}
                        >{String(minute).padStart(2, '0')}</button>
                    {/each}
                </div>
            </div>
            <footer>
                <button class="confirm-time" type="button" onclick={confirm}>{$_('common.confirm')}</button>
                <button class="cancel-time" type="button" onclick={() => (open = false)}>{$_('common.cancel')}</button>
            </footer>
        </div>
    {/if}
</div>

<style>
    .time-picker {
        position: relative;
        z-index: 0;
        min-width: 0;
    }
    .time-picker.open {
        z-index: 20;
    }
    .time-picker-trigger {
        display: flex;
        width: 100%;
        min-height: 2.5rem;
        padding: 0.35rem 0.625rem;
        align-items: center;
        justify-content: space-between;
        gap: 0.5rem;
        color: var(--body-text-color-highlight);
        border: 1px solid var(--bs-border-color);
        border-radius: 0.375rem;
        background: var(--bs-tertiary-bg);
        font: inherit;
        text-align: left;
    }
    .time-picker-trigger :global(.sf-icon) {
        color: var(--body-text-color-grey);
        font-size: 1rem;
    }
    .time-picker.open .time-picker-trigger {
        border-color: var(--primary-button-bg);
        box-shadow: 0 0 0 0.125rem color-mix(in srgb, var(--primary-button-bg) 20%, transparent);
    }
    .time-picker-trigger:disabled {
        opacity: 0.5;
        cursor: not-allowed;
    }
    .time-picker-backdrop {
        position: fixed;
        z-index: 1;
        inset: 0;
        padding: 0;
        border: 0;
        background: transparent;
    }
    .time-picker-popover {
        position: absolute;
        z-index: 2;
        top: calc(100% + 0.25rem);
        left: 0;
        overflow: hidden;
        color: var(--bs-body-color);
        border: 1px solid var(--bs-border-color);
        border-radius: 0.5rem;
        background: var(--body-inner-level-2);
        box-shadow: 0 0.35rem 0.9rem rgba(0, 0, 0, 0.18);
    }
    .time-picker-columns {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 0.25rem;
        padding: 0.4rem;
    }
    .time-picker-column {
        display: flex;
        max-height: 12.5rem;
        min-height: 0;
        flex-direction: column;
        overflow-y: auto;
        scrollbar-width: none;
        overscroll-behavior: contain;
    }
    .time-picker-column::-webkit-scrollbar {
        display: none;
    }
    .time-picker-column + .time-picker-column {
        border-left: 1px solid var(--bs-border-color);
    }
    .time-picker-column button {
        min-height: 2rem;
        margin: 0 0.1rem;
        padding: 0 0.2rem;
        flex: 0 0 auto;
        color: var(--bs-body-color);
        border: 0;
        border-radius: 0.2rem;
        background: transparent;
        font: inherit;
        font-size: 0.8125rem;
    }
    .time-picker-column button:hover {
        background: color-mix(in srgb, var(--primary-button-bg) 10%, transparent);
    }
    .time-picker-column button.selected {
        background: var(--web-20, rgba(1, 119, 253, 0.2));
    }
    .time-picker-popover footer {
        display: flex;
        padding: 0.4rem;
        justify-content: space-between;
        gap: 0.35rem;
        border-top: 1px solid var(--bs-border-color);
    }
    .time-picker-popover footer button {
        min-width: 0;
        min-height: 1.8rem;
        padding: 0.2rem 0.55rem;
        flex: 1 1 0;
        color: var(--bs-body-color);
        border: 1px solid var(--bs-border-color);
        border-radius: 999px;
        background: transparent;
        font: inherit;
        font-size: 0.75rem;
    }
    .time-picker-popover footer .confirm-time {
        color: #fff;
        border-color: var(--primary-button-bg);
        background: var(--primary-button-bg);
    }
</style>
