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
    <div class="d-flex justify-content-center icon-container-wrapper">
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
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 0.5rem 1rem;
    background: color-mix(in srgb, var(--bs-body-bg) 92%, transparent);
    backdrop-filter: blur(14px);
    p {
        margin-bottom: 0.75rem;
        color: var(--body-text-color-highlight);
        font-size: 0.75rem;
        line-height: 1.06rem;
    }
    .icon-container {
        margin-right: 0.94rem;
        > a {
            color: unset;
        }
        &:last-of-type { margin-right: 0; }
        &.grey {
            color: var(--overview-footer-icon-color, var(--body-text-color-grey));
        }
    }
}

@media (min-width: 992px) {
    .copyright.with-sidebar {
        left: var(--sidebar-width);
        width: calc(100% - var(--sidebar-width)) !important;
    }
}
</style>
