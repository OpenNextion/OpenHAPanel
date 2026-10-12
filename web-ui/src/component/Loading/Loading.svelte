<script lang="ts">
    import SvgIcon from "../Icon/SvgIcon.svelte";

    interface Props {
        size: string
    }

    let {
        size
    }: Partial<Props> = $props()
</script>

<span class="d-flex d-inline-flex rotate sf-loading" style:font-size={size}>
    <SvgIcon name="loading.svg" />
</span>

<style>
    .sf-loading {
        flex: 0 0 auto;
        transform-origin: center;
        animation: sf-loading-rotate .8s linear infinite;
    }

    @keyframes sf-loading-rotate {
        to {
            transform: rotate(360deg);
        }
    }
</style>
