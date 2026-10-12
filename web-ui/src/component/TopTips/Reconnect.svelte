<script lang="ts">
    import SvgIcon from '../Icon/SvgIcon.svelte';
    import { t } from 'svelte-i18n';
</script>

<div class="d-flex justify-content-start align-items-center top-tips">
    <SvgIcon name="iWarn.svg" />
    { $t('TIPS.CONNECT_LOST') }
</div>

<style lang="scss">
$color: #F1524B;

.top-tips {
    background: rgba($color, 0.2);
    color: $color;
    padding: 0 1rem;
}
</style>