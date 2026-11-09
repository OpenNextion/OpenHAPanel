import type { THassScreenAction } from '@/types/screenConfig';

export type THassEntityPreviewTone =
    | 'warm' | 'positive' | 'danger' | 'blue' | 'cyan' | 'neutral'
    | 'ha-inactive' | 'ha-cover' | 'ha-fan' | 'ha-sensor'
    | 'ha-battery-high' | 'ha-battery-medium' | 'ha-battery-low'
    | 'ha-orange' | 'ha-deep-orange';

export interface IHassEntityPreviewConfig {
    tone: THassEntityPreviewTone;
}

export interface IHassEntityInteractionConfig {
    supported: boolean;
    options: readonly THassScreenAction[];
    default: THassScreenAction;
}

export interface IHassEntityInteractionsConfig {
    tap: IHassEntityInteractionConfig;
    hold: IHassEntityInteractionConfig;
    iconTap: IHassEntityInteractionConfig;
}

export type THassEntityDisplayMode = 'badge' | 'entity';
export type THassEntityStateFormat =
    | 'toggle' | 'light' | 'light-badge' | 'cover' | 'cover-position' | 'cover-badge' | 'sensor' | 'binary-sensor'
    | 'climate' | 'climate-temperature' | 'climate-badge' | 'relative-time' | 'event' | 'fan' | 'fan-badge'
    | 'raw' | 'date-short' | 'enum' | 'name';

export interface IHassEntityDeviceClassDisplayConfig {
    entity?: THassEntityStateFormat;
    badge?: THassEntityStateFormat;
    on?: string;
    off?: string;
}

export interface IHassEntityStateDisplayConfig {
    entity: THassEntityStateFormat;
    badge?: THassEntityStateFormat;
    byDeviceClass?: Readonly<Record<string, IHassEntityDeviceClassDisplayConfig>>;
}

export interface IHassEntitySupportConfig {
    deviceClasses?: readonly string[];
    previewSupported?: boolean;
    interactions: IHassEntityInteractionsConfig;
    badgeInteractions?: Pick<IHassEntityInteractionsConfig, 'tap' | 'hold'>;
    stateDisplay: IHassEntityStateDisplayConfig;
    preview: IHassEntityPreviewConfig;
    previewByDeviceClass?: Readonly<Record<string, IHassEntityPreviewConfig>>;
    useRgbColor?: boolean;
}

/** Entity capabilities currently supported by the screen renderer. */
const ALL_INTERACTION_ACTIONS: readonly THassScreenAction[] = ['more-info', 'toggle', 'nothing'];
const READ_ONLY_INTERACTION_ACTIONS: readonly THassScreenAction[] = ['more-info', 'nothing'];
const MORE_INFO_DEFAULT = { supported: true, options: ALL_INTERACTION_ACTIONS, default: 'more-info' } as const;
const MORE_INFO_WITHOUT_TOGGLE = { supported: true, options: READ_ONLY_INTERACTION_ACTIONS, default: 'more-info' } as const;
const NOTHING_DEFAULT = { supported: true, options: ALL_INTERACTION_ACTIONS, default: 'nothing' } as const;
const NOTHING_WITHOUT_TOGGLE = { supported: true, options: READ_ONLY_INTERACTION_ACTIONS, default: 'nothing' } as const;
const ICON_TOGGLE_DEFAULT = { supported: true, options: ALL_INTERACTION_ACTIONS, default: 'toggle' } as const;
const BADGE_TOGGLE_TAP = { supported: true, options: ALL_INTERACTION_ACTIONS, default: 'toggle' } as const;
const BADGE_MORE_INFO_HOLD = { supported: true, options: ALL_INTERACTION_ACTIONS, default: 'more-info' } as const;
const BADGE_SENSOR_TAP = { supported: true, options: READ_ONLY_INTERACTION_ACTIONS, default: 'nothing' } as const;
const BADGE_SENSOR_HOLD = { supported: true, options: READ_ONLY_INTERACTION_ACTIONS, default: 'more-info' } as const;
const BADGE_EVENT_TAP = { supported: true, options: READ_ONLY_INTERACTION_ACTIONS, default: 'more-info' } as const;
const BADGE_EVENT_HOLD = { supported: true, options: READ_ONLY_INTERACTION_ACTIONS, default: 'nothing' } as const;
const UNSUPPORTED_HOLD = { supported: false, options: [], default: 'nothing' } as const;

export const HASS_ENTITY_SUPPORT_CONFIG: Readonly<Record<string, IHassEntitySupportConfig>> = Object.freeze({
    switch: {
        deviceClasses: ['switch', 'outlet'],
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: {
            entity: 'toggle',
            byDeviceClass: {
                switch: { entity: 'toggle', badge: 'toggle' },
                outlet: { entity: 'toggle', badge: 'toggle' },
            },
        },
        preview: { tone: 'positive' },
    },
    light: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        useRgbColor: true,
        stateDisplay: { entity: 'light', badge: 'light-badge' },
        preview: { tone: 'warm' },
    },
    cover: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: { entity: 'cover', badge: 'cover-badge' },
        preview: { tone: 'neutral' },
    },
    scene: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: { entity: 'relative-time', badge: 'name' },
        preview: { tone: 'blue' },
    },
    button: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: { entity: 'relative-time', badge: 'name' },
        preview: { tone: 'blue' },
    },
    sensor: {
        deviceClasses: [
            'absolute_humidity',
            'apparent_power',
            'aqi',
            'area',
            'atmospheric_pressure',
            'battery',
            'blood_glucose_concentration',
            'carbon_dioxide',
            'carbon_monoxide',
            'co2',
            'co',
            'conductivity',
            'current',
            'data_rate',
            'data_size',
            'date',
            'distance',
            'duration',
            'energy',
            'energy_distance',
            'energy_storage',
            'enum',
            'frequency',
            'gas',
            'humidity',
            'illuminance',
            'irradiance',
            'moisture',
            'monetary',
            'nitrogen_dioxide',
            'nitrogen_monoxide',
            'nitrous_oxide',
            'ozone',
            'ph',
            'pm1',
            'pm25',
            'pm4',
            'pm10',
            'power',
            'power_factor',
            'precipitation',
            'precipitation_intensity',
            'pressure',
            'radon',
            'reactive_energy',
            'reactive_power',
            'signal_strength',
            'sound_pressure',
            'speed',
            'sulphur_dioxide',
            'temperature',
            'temperature_delta',
            'timestamp',
            'uptime',
            'volatile_organic_compounds',
            'volatile_organic_compounds_parts',
            'voltage',
            'volume',
            'volume_flow_rate',
            'volume_storage',
            'water',
            'weight',
            'wind_direction',
            'wind_speed',
        ],
        interactions: { tap: MORE_INFO_WITHOUT_TOGGLE, hold: NOTHING_WITHOUT_TOGGLE, iconTap: NOTHING_WITHOUT_TOGGLE },
        badgeInteractions: { tap: BADGE_SENSOR_TAP, hold: BADGE_SENSOR_HOLD },
        stateDisplay: {
            entity: 'sensor',
            badge: 'sensor',
            byDeviceClass: {
                date: { entity: 'raw', badge: 'date-short' },
                enum: { entity: 'enum' },
                timestamp: { entity: 'relative-time', badge: 'date-short' },
                uptime: { entity: 'relative-time' },
            },
        },
        preview: { tone: 'cyan' },
        previewByDeviceClass: { humidity: { tone: 'cyan' } },
    },
    binary_sensor: {
        interactions: { tap: MORE_INFO_WITHOUT_TOGGLE, hold: NOTHING_WITHOUT_TOGGLE, iconTap: NOTHING_WITHOUT_TOGGLE },
        badgeInteractions: { tap: BADGE_SENSOR_TAP, hold: BADGE_SENSOR_HOLD },
        stateDisplay: {
            entity: 'binary-sensor',
            byDeviceClass: {
                battery: { on: 'low', off: 'normal' },
                battery_charging: { on: 'charging', off: 'notCharging' },
                co: { on: 'detected', off: 'clear' },
                carbon_monoxide: { on: 'detected', off: 'clear' },
                cold: { on: 'cold', off: 'normal' },
                connectivity: { on: 'connected', off: 'disconnected' },
                door: { on: 'open', off: 'closed' },
                garage_door: { on: 'open', off: 'closed' },
                gas: { on: 'detected', off: 'clear' },
                glass_break: { on: 'glassBreakDetached', off: 'clear' },
                heat: { on: 'hot', off: 'normal' },
                light: { on: 'lightDetected', off: 'noLight' },
                lock: { on: 'unlocked', off: 'locked' },
                moisture: { on: 'wet', off: 'dry' },
                motion: { on: 'detected', off: 'clear' },
                moving: { on: 'moving', off: 'stopped' },
                occupancy: { on: 'detected', off: 'clear' },
                opening: { on: 'open', off: 'closed' },
                plug: { on: 'pluggedIn', off: 'unplugged' },
                power: { on: 'on', off: 'off' },
                presence: { on: 'home', off: 'away' },
                problem: { on: 'problem', off: 'ok' },
                running: { on: 'running', off: 'notRunning' },
                safety: { on: 'unsafe', off: 'safe' },
                smoke: { on: 'detected', off: 'clear' },
                sound: { on: 'detected', off: 'clear' },
                tamper: { on: 'tamperingDetected', off: 'clear' },
                update: { on: 'updateAvailable', off: 'upToDate' },
                vibration: { on: 'detected', off: 'clear' },
                window: { on: 'open', off: 'closed' },
            },
        },
        preview: { tone: 'danger' },
    },
    climate: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: { entity: 'climate', badge: 'climate-badge' },
        preview: { tone: 'cyan' },
    },
    event: {
        interactions: { tap: MORE_INFO_WITHOUT_TOGGLE, hold: NOTHING_WITHOUT_TOGGLE, iconTap: NOTHING_WITHOUT_TOGGLE },
        badgeInteractions: { tap: BADGE_EVENT_TAP, hold: BADGE_EVENT_HOLD },
        stateDisplay: { entity: 'event', badge: 'name' },
        preview: { tone: 'blue' },
    },
    siren: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: { entity: 'toggle' },
        preview: { tone: 'danger' },
    },
    fan: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        stateDisplay: { entity: 'fan', badge: 'fan-badge' },
        preview: { tone: 'blue' },
    },
});

function matchesSupportConfig(entityId: string, deviceClass?: unknown) {
    const domain = entityId.split('.', 1)[0];
    const config = HASS_ENTITY_SUPPORT_CONFIG[domain];
    if (!config) return false;
    if (!config.deviceClasses?.length) return true;
    // Older switch entities may not expose a device class.
    if (domain === 'switch' && (deviceClass === undefined || deviceClass === null || deviceClass === '')) return true;
    return typeof deviceClass === 'string' && config.deviceClasses.includes(deviceClass);
}

export function isHassEntityVisibleInPicker(entityId: string, deviceClass?: unknown) {
    return matchesSupportConfig(entityId, deviceClass);
}

export function isHassEntitySupported(entityId: string, deviceClass?: unknown) {
    const domain = entityId.split('.', 1)[0];
    const config = HASS_ENTITY_SUPPORT_CONFIG[domain];
    return Boolean(config && config.previewSupported !== false && matchesSupportConfig(entityId, deviceClass));
}

