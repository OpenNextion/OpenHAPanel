<script lang="ts">
    let {
        checked,
        disabled = false,
        ariaLabel,
        onchange,
    }: {
        checked: boolean;
        disabled?: boolean;
        ariaLabel: string;
        onchange?: (event: Event & { currentTarget: HTMLInputElement }) => void;
    } = $props();
</script>

<label class="setting-switch" class:disabled>
    <input type="checkbox" role="switch" {checked} {disabled} aria-label={ariaLabel} {onchange} />
    <span class="track" aria-hidden="true"><span class="thumb"></span></span>
</label>

<style lang="scss">
    .setting-switch {
        --switch-track-off: #e2e2e2;
        --switch-thumb-off: #bbb;
        --switch-track-on: #dbe3ff;
        --switch-thumb-on: var(--primary-button-bg, #0177fd);
        position: relative;
        display: block;
        width: 3.125rem;
        height: 1.625rem;
        flex: 0 0 auto;
        cursor: pointer;
    }

    .setting-switch.disabled { cursor: not-allowed; }

    input {
        position: absolute;
        z-index: 1;
        inset: 0;
        width: 100%;
        height: 100%;
        margin: 0;
        opacity: 0;
        cursor: inherit;
    }

    .track {
        display: block;
        width: 100%;
        height: 100%;
        border-radius: 999px;
        background: var(--switch-track-off);
        pointer-events: none;
        transition: background-color 0.18s ease;
    }

    .thumb {
        position: absolute;
        top: 1px;
        left: 1px;
        width: 1.5rem;
        height: 1.5rem;
        border-radius: 50%;
        background: var(--switch-thumb-off);
        transition: background-color 0.18s ease, transform 0.18s ease;
    }

    input:checked + .track { background: var(--switch-track-on); }
    input:checked + .track .thumb { background: var(--switch-thumb-on); transform: translateX(1.5rem); }
    input:focus-visible + .track { outline: 2px solid var(--switch-thumb-on); outline-offset: 2px; }
    input:disabled + .track { opacity: 0.5; }
</style>
