<script lang="ts">
    import { onMount } from 'svelte';
    import { homeAssistantWebSocket } from '@/wsForHA';
    import { haHistoryStore } from '@/stores/haHistory';

    let { entityId, label, points = [], valueText, lineColor = 'var(--screen-tone-ha-sensor-color)' }: { entityId: string; label: string; points?: number[]; valueText?: string; lineColor?: string } = $props();

    $effect(() => {
        const currentEntityId = entityId;
        return homeAssistantWebSocket.subscribeEntityHistory(currentEntityId);
    });

    let graphElement: HTMLDivElement;
    let graphWidth = $state(0);
    let graphNow = $state(Date.now());
    onMount(() => {
        const observer = new ResizeObserver(([entry]) => { graphWidth = entry.contentRect.width; });
        const clockTimer = window.setInterval(() => { graphNow = Date.now(); }, 60_000);
        observer.observe(graphElement);
        return () => { observer.disconnect(); window.clearInterval(clockTimer); };
    });

    const historyEntry = $derived($haHistoryStore.entities[entityId]);

    const graphPaths = $derived.by(() => {
        const windowEnd = graphNow;
        const windowStart = windowEnd - 24 * 60 * 60 * 1000;
        const allPoints = (historyEntry?.points ?? []).filter((point) => Number.isFinite(point.value) && point.timestamp <= windowEnd);
        const visibleStart = allPoints.findIndex((point) => point.timestamp >= windowStart);
        let visible = visibleStart < 0 ? [] : allPoints.slice(visibleStart);
        if (visibleStart > 0 && (visible.length === 0 || visible[0].timestamp > windowStart)) {
            visible.unshift({ timestamp: windowStart, value: allPoints[visibleStart - 1].value });
        }
        if (!visible.length && allPoints.length) visible = [{ ...allPoints[allPoints.length - 1], timestamp: windowStart }];
        const source = visible.length ? visible : points.map((value, index) => ({
            timestamp: windowStart + (points.length > 1 ? index / (points.length - 1) : 1) * (windowEnd - windowStart),
            value,
        }));
        if (!source.length) return { line: '', area: '', opacity: '0.1' };

        const sampled = sampleTrend(source, windowStart, windowEnd, graphWidth, 24);
        const samples = sampled.length ? sampled : source;
        const values = samples.map((point) => point.value);
        const minValue = Math.min(...values);
        const maxValue = Math.max(...values);
        const dataRange = maxValue - minValue || minValue * 0.1;
        let max = maxValue + dataRange * 0.1;
        let min = minValue - dataRange * 0.1;
        let origin = 1;
        if (max < 0) origin = 0;
        else if (min < 0) origin = max / (max - min || 1);
        else min = Math.max(0, min);
        const range = max - min || 1;
        const coords = samples.map((point) => ({
            x: Math.max(0, Math.min(1, (point.timestamp - windowStart) / (windowEnd - windowStart))) * 100,
            y: 1.2 + (1 - (point.value - min) / range) * 33.6,
        }));
        let line = `M 0 ${coords[0].y} L ${coords[0].x} ${coords[0].y}`;
        for (let i = 0; i < coords.length; i++) {
            const current = coords[i];
            const next = coords[i + 1];
            const endX = next ? (current.x + next.x) / 2 : current.x;
            const endY = next ? (current.y + next.y) / 2 : current.y;
            line += ` Q ${current.x} ${current.y} ${endX} ${endY}`;
        }
        line += ` L 100 ${coords.at(-1)!.y}`;
        return { line, area: `${line} L 100 ${35 * origin} L 0 ${35 * origin} Z` };
    });

    function sampleTrend(source: { timestamp: number; value: number }[], startMs: number, endMs: number, width: number, hours: number) {
        const details = Math.max(10, width / 5, hours);
        if (source.length <= details) return source;
        const span = Math.floor((endMs - startMs) / 1000);
        const desired = Math.ceil(span / details);
        const frames = [1,2,3,5,10,15,20,30,60,120,180,300,600,900,1200,1800,3600,7200,10800,14400,21600,28800,43200,86400];
        let step = frames.filter((size) => size <= desired).at(-1) ?? 1;
        if (desired >= 86400) step = Math.floor(desired / 86400) * 86400;
        while (true) {
            const result: typeof source = [];
            for (let first = 0; first < source.length;) {
                const frame = Math.floor(source[first].timestamp / 1000 / step);
                let end = first + 1;
                while (end < source.length && Math.floor(source[end].timestamp / 1000 / step) === frame) end++;
                let low = first; let high = first;
                for (let index = first + 1; index < end; index++) {
                    if (source[index].value < source[low].value) low = index;
                    if (source[index].value > source[high].value) high = index;
                }
                if (low < high) result.push(source[low], source[high]);
                else if (high < low) result.push(source[high], source[low]);
                else result.push(source[low]);
                first = end;
            }
            if (result.length <= 384 || step > span) return result;
            step *= 2;
        }
    }
</script>

<div bind:this={graphElement} class="tile-trend-graph" role="img" aria-label={`${label}${valueText ? `: ${valueText}` : ''}`} style={`--trend-line-color: ${lineColor}`}>
    {#if graphPaths.line}
        <svg viewBox="0 0 100 36" preserveAspectRatio="none" aria-hidden="true">
            <path class="trend-area" d={graphPaths.area} />
            <path class="trend-line" d={graphPaths.line} />
        </svg>
    {:else}
        <span class="trend-baseline" aria-hidden="true"></span>
    {/if}
</div>

<style lang="scss">
    .tile-trend-graph { display: flex; box-sizing: border-box; width: 100%; min-width: 0; height: var(--feature-height, 3.5rem); min-height: 0; padding-inline: 0.5rem; align-items: center; pointer-events: none; }
    svg { width: 100%; height: 100%; }
    .trend-area { fill: var(--trend-line-color); opacity: 0.1; }
    :global([data-screen-theme='dark']) .trend-area { opacity: 0.25; }
    .trend-line { fill: none; stroke: var(--trend-line-color); stroke-width: 2; opacity: 0.8; vector-effect: non-scaling-stroke; }
    .trend-baseline { width: 100%; height: 1px; margin-top: auto; margin-bottom: 0.375rem; background: var(--trend-line-color); }
</style>
