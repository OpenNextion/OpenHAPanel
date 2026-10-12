<script lang="ts">
    import SvgIcon from '@/component/Icon/SvgIcon.svelte';
    import { getHassEntityMdiIcon } from '@/constant';
    import { haRegistryStore } from '@/stores/haRegistry';

    interface IHassEntityRowProps {
        entityId: string;
        name: string;
        state?: string;
        icon?: string | null;
        variant?: 'entity' | 'badge';
        onclick?: () => void;
    }

    let { entityId, name, state, icon, variant = 'entity', onclick }: IHassEntityRowProps = $props();
    const domain = $derived(entityId.split('.')[0] ?? '');
    const stateObject = $derived($haRegistryStore.states.find((item) => item.entity_id === entityId));
    const registryEntity = $derived($haRegistryStore.entities.find((item) => (item.ei ?? item.entity_id) === entityId));
    const registryEntityName = $derived(
        registryEntity?.en ?? registryEntity?.name ?? registryEntity?.ne ?? registryEntity?.original_name ?? registryEntity?.or ?? stateObject?.attributes?.friendly_name,
    );
    const displayName = $derived(name && name !== entityId ? name : (registryEntityName ?? name ?? entityId));
    const configuredIcon = $derived(icon ?? registryEntity?.ic ?? registryEntity?.icon);
    const deviceClass = $derived(typeof stateObject?.attributes?.device_class === 'string' ? stateObject.attributes.device_class : '');
    const fakeState = $derived.by(() => {
        switch (domain) {
            case 'light':
                return { label: '20%', value: 'on', tone: 'warm' };
            case 'switch':
                return { label: 'On', value: 'on', tone: 'positive' };
            case 'cover':
                return { label: 'Closed', value: 'closed', tone: 'neutral' };
            case 'scene':
                return { label: '3 days ago', value: 'unknown', tone: 'blue' };
            case 'button':
                return { label: 'Press', value: 'unknown', tone: 'blue' };
            case 'binary_sensor':
                return { label: 'Detected', value: 'on', tone: 'danger' };
            case 'sensor':
                return deviceClass === 'humidity'
                    ? { label: '65%', value: '65', tone: 'cyan' }
                    : { label: '25°C', value: '25', tone: 'cyan' };
            default:
                return { label: 'Unknown', value: 'unknown', tone: 'neutral' };
        }
    });
    const displayState = $derived(state ?? fakeState.label);
    const iconState = $derived(state ?? fakeState.value);
    const mdiIcon = $derived(getHassEntityMdiIcon(entityId, configuredIcon, deviceClass, iconState));

</script>

{#snippet content()}
    <span class="entity-icon tone-{fakeState.tone}"><SvgIcon name={mdiIcon} /></span>
    {#if variant === 'badge'}
        <strong class="badge-value">{displayState}</strong>
    {:else}
        <span class="entity-copy"><strong>{displayName}</strong><small>{displayState}</small></span>
    {/if}
{/snippet}

{#if onclick}
    <button class:badge={variant === 'badge'} class="hass-entity-row clickable" type="button" {onclick}>{@render content()}</button>
{:else}
    <div class:badge={variant === 'badge'} class="hass-entity-row">{@render content()}</div>
{/if}

<style>
    .hass-entity-row {
        display: flex;
        width: min(100%, 280px);
        height: 68px;
        min-width: 0;
        min-height: 68px;
        margin-inline: auto;
        padding: 8px 12px;
        align-items: center;
        gap: 10px;
        color: #f2f2f7;
        border: 0;
        border-radius: 12px;
        background: rgba(30, 34, 40, 1);
        font-family: MiSans, sans-serif;
        text-align: left;
    }
    .hass-entity-row.clickable {
        cursor: pointer;
    }
    .hass-entity-row.clickable:hover,
    .hass-entity-row.clickable:focus-visible {
        outline: 2px solid #249ed5;
        outline-offset: -2px;
    }
    .hass-entity-row.badge {
        width: auto;
        height: 30px;
        min-height: 30px;
        margin-inline: 0;
        padding: 0.25rem 0.65rem 0.25rem 0.35rem;
        flex: 0 1 auto;
        border-radius: 999px;
    }
    .hass-entity-row.badge .entity-icon {
        width: 1.45rem;
        height: 1.45rem;
        font-size: 1rem;
    }
    .badge-value {
        max-width: 8rem;
        overflow: hidden;
        font-size: 0.75rem;
        font-weight: 600;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .entity-icon {
        display: grid;
        width: 40px;
        height: 40px;
        flex: 0 0 auto;
        place-items: center;
        color: #697079;
        border-radius: 50%;
        background: #f2f2f7;
        font-size: 20px;
        opacity: 1;
    }
    .entity-icon.tone-warm {
        color: #ffd09a;
        background: rgba(153, 112, 76, 0.55);
    }
    .entity-icon.tone-positive {
        color: #ffc107;
        background: rgba(52, 199, 89, 0.2);
    }
    .entity-icon.tone-danger {
        color: #ff4d46;
        background: rgba(241, 82, 75, 0.18);
    }
    .entity-icon.tone-blue {
        color: #31b7eb;
        background: rgba(49, 183, 235, 0.2);
    }
    .entity-icon.tone-cyan {
        color: #19c2d8;
        background: rgba(25, 194, 216, 0.2);
    }
    .entity-icon.tone-neutral {
        color: #a8abb0;
        background: rgba(168, 171, 176, 0.18);
    }
    .entity-copy {
        display: flex;
        min-width: 0;
        flex-direction: column;
    }
    .entity-copy strong,
    .entity-copy small {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }
    .entity-copy strong {
        font-family: MiSans, sans-serif;
        font-size: 20px;
        font-style: normal;
        font-weight: 500;
        line-height: 28px;
        letter-spacing: 0;
    }
    .entity-copy small {
        color: #a8abb0;
        font-family: MiSans, sans-serif;
        font-size: 16px;
        font-style: normal;
        font-weight: 400;
        line-height: 24px;
        letter-spacing: 0;
    }
</style>
