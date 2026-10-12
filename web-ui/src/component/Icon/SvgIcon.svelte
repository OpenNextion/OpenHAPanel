<script lang="ts">
    import { loadMdiIcon, svgMap } from '@/constant';
    import type { SvgConfig } from '@/constant';

    interface SvgIconProps {
        /** icon name */
        name: string;
        /** icon size(px) */
        size?: number | string;
    }

    let { name, size }: SvgIconProps = $props();
    let svgConfig = $state<SvgConfig | undefined>();
    let loadGeneration = 0;

    $effect(() => {
        const requestedName = name;
        const generation = ++loadGeneration;
        const localIcon = svgMap.get(requestedName);
        if (localIcon) {
            svgConfig = localIcon;
            return;
        }

        svgConfig = undefined;
        if (!requestedName.startsWith('mdi:')) {
            console.warn(`svg icon: '${requestedName}' not found!`);
            return;
        }

        void loadMdiIcon(requestedName).then((icon) => {
            if (generation !== loadGeneration || requestedName !== name) return;
            svgConfig = icon;
            if (!icon) console.warn(`mdi icon: '${requestedName}' is not registered!`);
        });
    });
</script>

{#if svgConfig}
    <i class="sf-icon" style:font-size={size}>
        {@html svgConfig.html}
    </i>
{/if}

<style lang="scss">
    :global {
        .sf-icon {
            display: inline-flex;
            align-items: center;
            color: inherit;
            text-rendering: optimizeLegibility;
            -webkit-font-smoothing: antialiased;
            vertical-align: middle;
        }

        .sf-icon > svg {
            display: block;
            flex: 0 0 auto;
        }
    }
</style>
