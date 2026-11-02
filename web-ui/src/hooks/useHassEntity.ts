import { get } from 'svelte/store';
import { locale, t } from 'svelte-i18n';
import {
    getHassEntityMdiIcon,
    HASS_ENTITY_SUPPORT_CONFIG,
    isHassEntitySupported,
    type THassEntityDisplayMode,
    type IHassEntityPreviewConfig,
    type IHassEntitySupportConfig,
} from '@/constant';
import { haRegistryStore } from '@/stores/haRegistry';
import type {
    IHassAreaRegistryEntry,
    IHassDeviceRegistryEntry,
    IHassEntityRegistryDisplayEntry,
    IHassStateEntry,
} from '@/types/haRegistry';
import type { THassScreenAction } from '@/types/screenConfig';

export type THassActionSelection = 'default' | THassScreenAction;

interface IHassEntityRegistryData {
    states: IHassStateEntry[];
    statesById?: ReadonlyMap<string, IHassStateEntry>;
    entities: IHassEntityRegistryDisplayEntry[];
    devices: IHassDeviceRegistryEntry[];
    areas: IHassAreaRegistryEntry[];
}

interface IHassEntityPresentationOptions {
    state?: string;
    icon?: string | null;
    variant?: THassEntityDisplayMode;
}

const FALLBACK_PREVIEW: IHassEntityPreviewConfig = { tone: 'neutral' };

const STATE_TRANSLATION_KEYS: Readonly<Record<string, string>> = {
    on: 'on',
    off: 'off',
    open: 'open',
    closed: 'closed',
    opening: 'opening',
    closing: 'closing',
    unknown: 'unknown',
    unavailable: 'unavailable',
};

const CLIMATE_MODE_TRANSLATION_KEYS: Readonly<Record<string, string>> = {
    heat: 'heat',
    cool: 'cool',
    heat_cool: 'heatCool',
    auto: 'auto',
    dry: 'dryMode',
    fan_only: 'fanOnly',
    idle: 'idle',
};

const CLIMATE_ACTION_INDICATORS = {
    heating: { icon: 'mdi:fire', tone: 'heating' },
    preheating: { icon: 'mdi:fire', tone: 'heating' },
    cooling: { icon: 'mdi:snowflake', tone: 'cooling' },
    defrosting: { icon: 'mdi:snowflake', tone: 'cooling' },
    drying: { icon: 'mdi:water-percent', tone: 'drying' },
    fan: { icon: 'mdi:fan', tone: 'fan' },
} as const;

function translateHassState(key: string) {
    return get(t)(`hassEntityState.${key}`);
}

function getDisplayLocale() {
    return get(locale) || 'en';
}

function getAttributeString(attributes: Record<string, unknown> | undefined, key: string) {
    const value = attributes?.[key];
    return typeof value === 'string' || typeof value === 'number' ? String(value) : '';
}

function getAttributeNumber(attributes: Record<string, unknown> | undefined, key: string) {
    const value = attributes?.[key];
    if (typeof value !== 'number' && (typeof value !== 'string' || !value.trim())) return null;
    const number = Number(value);
    return Number.isFinite(number) ? number : null;
}

function formatUnit(value: string, unit: string) {
    if (!unit) return value;
    return `${value}${unit.startsWith('°') || unit === '%' ? '' : ' '}${unit}`;
}

function formatSensorState(state: string, unit: string, displayPrecision?: number) {
    if (!unit) return state;
    const value = Number(state);
    if (!Number.isFinite(value)) return formatUnit(state, unit);

    // HA uses the registry precision when available; otherwise it keeps the
    // decimal places present in the state string when formatting a number.
    const fractionDigits = displayPrecision !== undefined && Number.isInteger(displayPrecision) && displayPrecision >= 0
        ? displayPrecision
        : state.includes('.') ? state.split('.')[1].length : 0;
    try {
        const formattedValue = new Intl.NumberFormat(getDisplayLocale(), {
            minimumFractionDigits: fractionDigits,
            maximumFractionDigits: fractionDigits,
        }).format(value);
        return formatUnit(formattedValue, unit);
    } catch {
        return formatUnit(state, unit);
    }
}

function supportsLightBrightness(attributes: Record<string, unknown> | undefined) {
    const colorModes = attributes?.supported_color_modes;
    if (Array.isArray(colorModes) && colorModes.length) {
        return colorModes.some((mode) => typeof mode === 'string' && mode !== 'onoff' && mode !== 'unknown');
    }
    return getAttributeNumber(attributes, 'brightness') !== null;
}

function supportsCoverPosition(attributes: Record<string, unknown> | undefined) {
    const features = getAttributeNumber(attributes, 'supported_features');
    return features !== null
        ? (features & 4) !== 0
        : getAttributeNumber(attributes, 'current_position') !== null;
}

function supportsFanSpeed(attributes: Record<string, unknown> | undefined) {
    const features = getAttributeNumber(attributes, 'supported_features');
    return features !== null && (features & 1) !== 0;
}

function parseHassDate(value: string) {
    const dateOnly = /^(\d{4})-(\d{2})-(\d{2})$/.exec(value);
    const date = dateOnly
        ? new Date(Number(dateOnly[1]), Number(dateOnly[2]) - 1, Number(dateOnly[3]))
        : new Date(value);
    return Number.isNaN(date.getTime()) ? null : date;
}

function formatHassDate(value: string, format: 'date' | 'date-short' | 'datetime') {
    const date = parseHassDate(value);
    if (!date) return value;
    const currentLocale = getDisplayLocale();
    if (format === 'date-short') return new Intl.DateTimeFormat(currentLocale, { month: 'short', day: 'numeric' }).format(date);
    if (format === 'date') return new Intl.DateTimeFormat(currentLocale, { year: 'numeric', month: 'short', day: 'numeric' }).format(date);
    return new Intl.DateTimeFormat(currentLocale, { year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' }).format(date);
}

function formatRelativeTime(value: string) {
    const date = parseHassDate(value);
    if (!date) return null;
    const seconds = (date.getTime() - Date.now()) / 1000;
    const absoluteSeconds = Math.abs(seconds);
    const [amount, unit] = absoluteSeconds < 60 ? [seconds, 'second'] as const
        : absoluteSeconds < 3600 ? [seconds / 60, 'minute'] as const
        : absoluteSeconds < 86400 ? [seconds / 3600, 'hour'] as const
        : absoluteSeconds < 604800 ? [seconds / 86400, 'day'] as const
        : absoluteSeconds < 2592000 ? [seconds / 604800, 'week'] as const
        : absoluteSeconds < 31536000 ? [seconds / 2592000, 'month'] as const
        : [seconds / 31536000, 'year'] as const;
    return new Intl.RelativeTimeFormat(getDisplayLocale(), { numeric: 'auto' }).format(Math.round(amount), unit);
}

export function getHassEntityDisplayState(
    entityId: string,
    stateObject: IHassStateEntry,
    variant: THassEntityDisplayMode = 'entity',
    displayName?: string,
    displayPrecision?: number,
) {
    const state = stateObject.state.trim();
    const attributes = stateObject.attributes;
    const normalizedState = state.toLowerCase();
    if (normalizedState === 'unavailable') return translateHassState('unavailable');
    const config = getHassEntitySupportConfig(entityId)?.stateDisplay;
    const deviceClass = getAttributeString(attributes, 'device_class');
    const classConfig = config?.byDeviceClass?.[deviceClass];
    const format = classConfig?.[variant] ?? (variant === 'badge' ? config?.badge : undefined) ?? config?.entity;
    if (format === 'name') return displayName || getAttributeString(attributes, 'friendly_name') || entityId;
    if (normalizedState === 'unknown' || !normalizedState) return translateHassState('unknown');

    switch (format) {
        case 'toggle':
            return normalizedState === 'on' ? translateHassState('on') : normalizedState === 'off' ? translateHassState('off') : prettifyState(state);
        case 'light': {
            if (normalizedState !== 'on') return normalizedState === 'off' ? translateHassState('off') : prettifyState(state);
            const brightness = getAttributeNumber(attributes, 'brightness');
            return brightness !== null && brightness >= 0
                ? `${Math.round(brightness / 255 * 100)}%`
                : translateHassState('on');
        }
        case 'light-badge': {
            if (normalizedState !== 'on') return normalizedState === 'off' ? translateHassState('off') : prettifyState(state);
            if (!supportsLightBrightness(attributes)) return translateHassState('on');
            const brightness = getAttributeNumber(attributes, 'brightness');
            return brightness !== null ? `${Math.round(Math.max(0, Math.min(255, brightness)) / 255 * 100)}%` : '--%';
        }
        case 'cover': {
            const position = getAttributeNumber(attributes, 'current_position');
            if (['open', 'opening', 'closing'].includes(normalizedState) && position !== null) {
                return `${prettifyState(state)} · ${Math.round(position)}%`;
            }
            return prettifyState(state);
        }
        case 'cover-position': {
            const position = getAttributeNumber(attributes, 'current_position');
            return ['open', 'opening', 'closing'].includes(normalizedState) && position !== null
                ? `${Math.round(position)}%`
                : prettifyState(state);
        }
        case 'cover-badge': {
            if (normalizedState === 'closed') return translateHassState('closed');
            if (normalizedState === 'opening') return translateHassState('opening');
            if (normalizedState === 'closing') return translateHassState('closing');
            if (normalizedState !== 'open') return prettifyState(state);
            if (!supportsCoverPosition(attributes)) return translateHassState('open');
            const position = getAttributeNumber(attributes, 'current_position');
            return position !== null ? `${Math.round(Math.max(0, Math.min(100, position)))}%` : '--%';
        }
        case 'sensor':
            return formatSensorState(state, getAttributeString(attributes, 'unit_of_measurement'), displayPrecision);
        case 'enum':
            return prettifyState(state);
        case 'date':
        case 'date-short':
        case 'datetime':
            return formatHassDate(state, format);
        case 'binary-sensor': {
            if (normalizedState === 'on') return translateHassState(classConfig?.on ?? 'detected');
            if (normalizedState === 'off') return translateHassState(classConfig?.off ?? 'clear');
            return prettifyState(state);
        }
        case 'climate': {
            if (normalizedState === 'off') return translateHassState('off');
            const temperature = getAttributeString(attributes, 'current_temperature');
            const unit = getAttributeString(attributes, 'temperature_unit') || getAttributeString(attributes, 'unit_of_measurement');
            return temperature ? `${prettifyClimateState(state)} ${formatUnit(temperature, unit)}` : prettifyClimateState(state);
        }
        case 'climate-temperature': {
            if (normalizedState === 'off') return translateHassState('off');
            const temperature = getAttributeString(attributes, 'current_temperature');
            const unit = getAttributeString(attributes, 'temperature_unit') || getAttributeString(attributes, 'unit_of_measurement');
            return temperature ? formatUnit(temperature, unit) : prettifyClimateState(state);
        }
        case 'climate-badge': {
            const temperature = getAttributeString(attributes, 'current_temperature') || '--';
            const unit = getAttributeString(attributes, 'temperature_unit') || getAttributeString(attributes, 'unit_of_measurement');
            return `${prettifyClimateState(state)}·${formatUnit(temperature, unit)}`;
        }
        case 'relative-time':
            return formatRelativeTime(state) ?? prettifyState(state);
        case 'event':
            return formatRelativeTime(state) ?? prettifyState(getAttributeString(attributes, 'event_type') || state);
        case 'fan': {
            if (normalizedState !== 'on') return normalizedState === 'off' ? translateHassState('off') : prettifyState(state);
            if (!supportsFanSpeed(attributes)) return translateHassState('on');
            const percentage = getAttributeNumber(attributes, 'percentage');
            return percentage !== null ? `${Math.round(percentage)}%` : '--%';
        }
        case 'fan-badge': {
            if (normalizedState === 'off') return translateHassState('off');
            if (normalizedState !== 'on') return prettifyState(state);
            if (!supportsFanSpeed(attributes)) return translateHassState('on');
            const percentage = getAttributeNumber(attributes, 'percentage');
            return percentage !== null ? `${Math.round(Math.max(0, Math.min(100, percentage)))}%` : '--%';
        }
        default:
            return prettifyState(state);
    }
}

function prettifyState(state: string) {
    const translatedKey = STATE_TRANSLATION_KEYS[state.toLowerCase()];
    if (translatedKey) return translateHassState(translatedKey);
    return state.replace(/_/g, ' ').replace(/\b\w/g, (letter: string) => letter.toUpperCase());
}

function prettifyClimateState(state: string) {
    const translatedKey = CLIMATE_MODE_TRANSLATION_KEYS[state.toLowerCase()];
    return translatedKey ? translateHassState(translatedKey) : prettifyState(state);
}

const BINARY_SENSOR_ALERT_CLASSES = new Set([
    'battery', 'carbon_monoxide', 'co', 'gas', 'glass_break', 'heat', 'lock',
    'moisture', 'problem', 'safety', 'smoke', 'sound', 'tamper',
]);

function getEntityTone(domain: string, state: string, fallback: IHassEntityPreviewConfig['tone'], deviceClass?: string): IHassEntityPreviewConfig['tone'] {
    const normalizedState = state.toLowerCase();
    if (normalizedState === 'unavailable' || normalizedState === 'unknown' || !normalizedState) return 'ha-inactive';
    if (domain === 'sensor') {
        if (deviceClass === 'battery' && state.trim()) {
            const batteryLevel = Number(state);
            if (Number.isFinite(batteryLevel)) return batteryLevel >= 70 ? 'ha-battery-high' : batteryLevel >= 30 ? 'ha-battery-medium' : 'ha-battery-low';
        }
        return 'ha-sensor';
    }
    if (domain === 'binary_sensor') {
        if (normalizedState !== 'on') return 'ha-inactive';
        return deviceClass && BINARY_SENSOR_ALERT_CLASSES.has(deviceClass) ? 'danger' : 'warm';
    }
    if (domain === 'switch' || domain === 'light') return normalizedState === 'off' ? 'ha-inactive' : 'warm';
    if (domain === 'cover') return normalizedState === 'closed' ? 'ha-inactive' : 'ha-cover';
    if (domain === 'fan') return normalizedState === 'off' ? 'ha-inactive' : 'ha-fan';
    if (domain === 'siren') return normalizedState === 'off' ? 'ha-inactive' : 'danger';
    if (domain === 'climate') {
        if (normalizedState === 'off') return 'ha-inactive';
        switch (normalizedState) {
            case 'auto': return 'positive';
            case 'cool': return 'blue';
            case 'dry': return 'ha-orange';
            case 'fan_only': return 'ha-fan';
            case 'heat': return 'ha-deep-orange';
            default: return 'warm';
        }
    }
    if (['scene', 'button', 'event'].includes(domain)) return 'ha-sensor';
    if (normalizedState === 'off') return 'ha-inactive';
    return fallback;
}

function getHassLightRgbColor(rgbColor: unknown): [number, number, number] | null {
    if (!Array.isArray(rgbColor) || rgbColor.length < 3) return null;
    const rgb = rgbColor.slice(0, 3).map(Number);
    if (!rgb.every((value) => Number.isFinite(value) && value >= 0 && value <= 255)) return null;

    const [red, green, blue] = rgb;
    let value = Math.max(red, green, blue);
    const chroma = value - Math.min(red, green, blue);
    let saturation = value ? chroma / value : 0;
    if (saturation >= 0.4) return [red, green, blue];

    // HA increases the contrast of pale light colors on tile cards and badges.
    const hueSegment = chroma === 0 ? 0
        : value === red ? (green - blue) / chroma
            : value === green ? 2 + (blue - red) / chroma
                : 4 + (red - green) / chroma;
    const hue = (hueSegment + 6) % 6;
    if (saturation < 0.1) value = 225;
    else saturation = 0.4;
    const channel = (offset: number) => {
        const position = (offset + hue) % 6;
        return Math.round(value - value * saturation * Math.max(Math.min(position, 4 - position, 1), 0));
    };
    return [channel(5), channel(3), channel(1)];
}

function getRegistryData(registry?: IHassEntityRegistryData) {
    return registry ?? get(haRegistryStore);
}

function findEntityState(data: IHassEntityRegistryData, entityId: string) {
    return data.statesById ? data.statesById.get(entityId) : data.states.find((item) => item.entity_id === entityId);
}

export function getHassEntityDomain(entityId: string) {
    return entityId.split('.', 1)[0] ?? '';
}

export function getHassEntitySupportConfig(entityId: string): IHassEntitySupportConfig | undefined {
    return HASS_ENTITY_SUPPORT_CONFIG[getHassEntityDomain(entityId)];
}

export function getHassEntityDisplayName(entityId: string, registry?: IHassEntityRegistryData) {
    const data = getRegistryData(registry);
    const friendlyName = findEntityState(data, entityId)?.attributes?.friendly_name;
    return typeof friendlyName === 'string' && friendlyName.trim() ? friendlyName.trim() : entityId;
}

export function getHassEntityContext(entityId: string, registry?: IHassEntityRegistryData) {
    const data = getRegistryData(registry);
    const entity = data.entities.find((item) => (item.ei ?? item.entity_id) === entityId);
    const deviceId = entity?.di ?? entity?.device_id;
    const device = deviceId ? data.devices.find((item) => item.id === deviceId) : undefined;
    const areaId = entity?.ai ?? entity?.area_id ?? device?.area_id;
    const area = areaId ? data.areas.find((item) => item.area_id === areaId) : undefined;
    return [area?.name, device?.name_by_user ?? device?.name].filter(Boolean).join(' › ') || entityId;
}

export function getHassEntityCapabilities(entityId: string, registry?: IHassEntityRegistryData) {
    const data = getRegistryData(registry);
    const stateObject = findEntityState(data, entityId);
    const deviceClass = typeof stateObject?.attributes?.device_class === 'string' ? stateObject.attributes.device_class : '';
    const config = getHassEntitySupportConfig(entityId);
    const interactions = config?.interactions ?? {
        tap: { supported: true, options: ['more-info', 'toggle', 'nothing'] as const, default: 'more-info' as const },
        hold: { supported: true, options: ['more-info', 'toggle', 'nothing'] as const, default: 'nothing' as const },
        iconTap: { supported: true, options: ['more-info', 'toggle', 'nothing'] as const, default: 'nothing' as const },
    };
    return {
        config,
        deviceClass,
        supported: isHassEntitySupported(entityId, deviceClass),
        interactions,
        defaultActions: {
            tap: interactions.tap.default,
            hold: interactions.hold.default,
            iconTap: interactions.iconTap.default,
        },
    };
}

export function normalizeHassEntityAction(
    action: THassActionSelection,
    entityId: string,
    registry?: IHassEntityRegistryData,
    interaction: 'tap' | 'hold' | 'iconTap' = 'iconTap',
    mode: 'entity' | 'badge' = 'entity',
): THassActionSelection {
    if (action === 'default') return action;
    const capabilities = getHassEntityCapabilities(entityId, registry);
    const interactionConfig = mode === 'badge' && interaction !== 'iconTap'
        ? capabilities.config?.badgeInteractions?.[interaction] ?? capabilities.interactions[interaction]
        : capabilities.interactions[interaction];
    return interactionConfig.options.includes(action) ? action : 'default';
}

export function getHassEntityPresentation(
    entityId: string,
    registry?: IHassEntityRegistryData,
    options: IHassEntityPresentationOptions = {},
) {
    const data = getRegistryData(registry);
    const stateObject = findEntityState(data, entityId);
    const registryEntity = data.entities.find((item) => (item.ei ?? item.entity_id) === entityId);
    const capabilities = getHassEntityCapabilities(entityId, data);
    const preview = capabilities.config?.previewByDeviceClass?.[capabilities.deviceClass] ?? capabilities.config?.preview ?? FALLBACK_PREVIEW;
    const actualState = options.state ?? stateObject?.state;
    const isUnavailable = actualState?.toLowerCase() === 'unavailable' || !stateObject;
    const rawState = actualState ?? 'unknown';
    const iconState = isUnavailable ? 'unavailable' : rawState;
    const entityDomain = getHassEntityDomain(entityId);
    const displayName = getHassEntityDisplayName(entityId, data);
    const variant = options.variant ?? 'entity';
    const displayStateObject = stateObject && options.state !== undefined ? { ...stateObject, state: options.state } : stateObject;
    const hvacAction = stateObject?.attributes?.hvac_action;
    const climateActionIndicator = variant === 'entity' && entityDomain === 'climate' && !['off', 'unknown', 'unavailable'].includes(rawState.toLowerCase())
        && typeof hvacAction === 'string'
        ? CLIMATE_ACTION_INDICATORS[hvacAction.toLowerCase() as keyof typeof CLIMATE_ACTION_INDICATORS] ?? null
        : null;
    const actualPreview = {
        ...preview,
        state: rawState,
        label: isUnavailable || !displayStateObject ? translateHassState('unavailable') : getHassEntityDisplayState(entityId, displayStateObject, variant, displayName, registryEntity?.dp),
        tone: isUnavailable ? 'neutral' as const : getEntityTone(entityDomain, rawState, preview.tone, capabilities.deviceClass),
    };
    const configuredIcon = options.icon ?? registryEntity?.ic ?? registryEntity?.icon ?? stateObject?.attributes?.icon;
    let iconStyle: string | undefined;
    if (capabilities.config?.useRgbColor && !isUnavailable) {
        const values = getHassLightRgbColor(stateObject?.attributes?.rgb_color);
        if (values) iconStyle = `--entity-icon-color: rgb(${values.join(', ')}); --entity-icon-background: rgba(${values.join(', ')}, 0.2);`;
    }
    return {
        ...capabilities,
        displayName,
        context: getHassEntityContext(entityId, data),
        displayState: actualPreview.label,
        badgeUsesName: capabilities.config?.stateDisplay.badge === 'name',
        icon: getHassEntityMdiIcon(entityId, configuredIcon, capabilities.deviceClass, iconState),
        climateActionIndicator,
        iconState,
        iconStyle,
        isUnavailable,
        preview: actualPreview,
        stateObject,
    };
}
