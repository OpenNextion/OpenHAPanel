<script lang="ts">
    import { link } from 'svelte-spa-router';
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';

    export let title: string;
    export let back = '/settings';
</script>

<a class="setting-page-title" href={back} use:link>
    <span class="page-header-back-icon"><SvgIcon name="arrowRight.svg" /></span>
    <span>{title}</span>
</a>

<style>
    .page-header-back-icon {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        transform: rotate(180deg);
    }
</style>
