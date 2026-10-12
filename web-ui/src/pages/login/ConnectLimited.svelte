<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { t } from 'svelte-i18n';
</script>

<div class="connected-limit">
    <div class="d-flex justify-content-center icon">
        <SvgIcon name="empty.svg" />
    </div>
    <div class="text">
        <p>{$t('LOGIN.CONNECT_LIMITED')}</p>
        <p>{$t('LOGIN.PLE_CLOSE_CLIENT')}</p>
    </div>
</div>

<style lang="scss">
    .connected-limit {
        .icon {
            margin-bottom: 0.75rem;
            :global(.sf-icon) {
                font-size: 15rem;
            }
        }
        .text {
            p {
                color: var(--body-text-color-highlight);
                font-size: 1rem;
                line-height: 1.5rem;
                text-align: center;
                padding: 0 1.5rem;
                margin-bottom: 0;
            }
        }
    }
</style>
