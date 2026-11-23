<script lang="ts">
    import SvgIcon from '../component/Icon/SvgIcon.svelte';
    import { NEXTION_TECH, FACEBOOK, X, YOUTUBE, TIKTOK } from '@/constant';

    let { withSidebar = false }: { withSidebar?: boolean } = $props();

    const medias = [
        { href: NEXTION_TECH, icon: 'itead.svg' },
        { href: FACEBOOK, icon: 'facebook.svg', color: 'grey' },
        { href: X, icon: 'x.svg', color: 'grey' },
        { href: YOUTUBE, icon: 'youtube.svg', color: 'grey' },
        { href: TIKTOK, icon: 'tiktok.svg', color: 'grey' }
    ]
</script>

<footer class:with-sidebar={withSidebar} class="w-100 text-center copyright">
    <p>Copyright © 2026 ITEAD Intelligent Systems Co.,Ltd</p>
    <div class="icon-container-wrapper">
        {#each medias as item (item.href)}
            <span class="icon-container" class:grey={item.color === 'grey'}>
                <a href={item.href} target="_blank" rel="noreferrer">
                    <SvgIcon name={item.icon} />
                </a>
            </span>
        {/each}
    </div>
</footer>

<style lang="scss">
.copyright {
    position: fixed;
    right: 0;
    bottom: 0;
    left: 0;
    z-index: 1020;
    display: flex;
    height: var(--footer-height);
    flex-direction: row;
    align-items: center;
    justify-content: center;
    gap: 1rem;
    padding: 0.5rem 1rem;
    background: var(--web-translucent-surface-92);
    backdrop-filter: blur(14px);
    p {
        margin: 0;
        color: var(--body-text-color-highlight);
        font-size: 0.75rem;
        line-height: 1.06rem;
    }
    .icon-container-wrapper {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 0.94rem;
    }
    .icon-container {
        display: flex;
        align-items: center;
        justify-content: center;
        > a {
            display: flex;
            align-items: center;
            justify-content: center;
            color: unset;
        }
        &.grey {
            color: var(--overview-footer-icon-color, var(--body-text-color-grey));
        }
    }
}

@media (max-width: 767.98px) {
    .copyright {
        flex-direction: column;
        gap: 0.5rem;
    }
}

@media (min-width: 992px) {
    .copyright.with-sidebar {
        left: var(--sidebar-width);
        width: calc(100% - var(--sidebar-width)) !important;
    }
}
</style>
