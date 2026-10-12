<script lang="ts">
    import { _ } from 'svelte-i18n';
    import { firmwareUpdateStore } from '@/stores/firmwareUpdate';
</script>

{#if $firmwareUpdateStore.operationBlocked}
    <div class="firmware-update-overlay" role="alertdialog" aria-modal="true" aria-label={$_('firmwareUpdate.operationBlocked')}>
        <div class="firmware-update-message">{$_('firmwareUpdate.operationBlocked')}</div>
    </div>
{/if}

<style>
    .firmware-update-overlay {
        position: fixed;
        inset: 0;
        z-index: 3000;
        display: grid;
        padding: 1rem;
        place-items: center;
        background: rgba(20, 22, 25, 0.48);
        backdrop-filter: blur(1px);
    }
    .firmware-update-message {
        display: flex;
        width: min(22rem, calc(100vw - 2rem));
        min-height: 13rem;
        padding: 2rem;
        align-items: center;
        justify-content: center;
        color: #666;
        border-radius: 1.25rem;
        background: #fff;
        box-shadow: 0 1rem 2.5rem rgba(0, 0, 0, 0.18);
        font-size: 1rem;
        font-weight: 500;
        text-align: center;
    }
</style>
