import type { THassScreenAction } from '@/types/screenConfig';

export type THassEntityPreviewTone = 'warm' | 'positive' | 'danger' | 'blue' | 'cyan' | 'neutral';

export interface IHassEntityPreviewConfig {
    label: string;
    state: string;
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

export interface IHassEntitySupportConfig {
    deviceClasses?: readonly string[];
    previewSupported?: boolean;
    interactions: IHassEntityInteractionsConfig;
    badgeInteractions?: Pick<IHassEntityInteractionsConfig, 'tap' | 'hold'>;
    preview: IHassEntityPreviewConfig;
    previewByDeviceClass?: Readonly<Record<string, IHassEntityPreviewConfig>>;
    useRgbColor?: boolean;
}

/** Entity capabilities currently supported by the screen renderer. */
const ALL_INTERACTION_ACTIONS: readonly THassScreenAction[] = ['more-info', 'toggle', 'nothing'];
const READ_ONLY_INTERACTION_ACTIONS: readonly THassScreenAction[] = ['more-info', 'nothing'];
const MORE_INFO_DEFAULT = { supported: true, options: ALL_INTERACTION_ACTIONS, default: 'more-info' } as const;
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
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: 'On', state: 'on', tone: 'positive' },
    },
    light: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        useRgbColor: true,
        preview: { label: '20%', state: 'on', tone: 'warm' },
    },
    cover: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: 'Closed', state: 'closed', tone: 'neutral' },
    },
    scene: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: '3 days ago', state: 'unknown', tone: 'blue' },
    },
    button: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: ICON_TOGGLE_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: 'Press', state: 'unknown', tone: 'blue' },
    },
    sensor: {
        deviceClasses: ['temperature', 'humidity'],
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_WITHOUT_TOGGLE, iconTap: NOTHING_WITHOUT_TOGGLE },
        badgeInteractions: { tap: BADGE_SENSOR_TAP, hold: BADGE_SENSOR_HOLD },
        preview: { label: '25°C', state: '25', tone: 'cyan' },
        previewByDeviceClass: { humidity: { label: '65%', state: '65', tone: 'cyan' } },
    },
    binary_sensor: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_WITHOUT_TOGGLE, iconTap: NOTHING_WITHOUT_TOGGLE },
        badgeInteractions: { tap: BADGE_SENSOR_TAP, hold: BADGE_SENSOR_HOLD },
        preview: { label: 'Detected', state: 'on', tone: 'danger' },
    },
    climate: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_DEFAULT, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: '25°C', state: 'heat', tone: 'cyan' },
    },
    event: {
        interactions: { tap: MORE_INFO_DEFAULT, hold: NOTHING_WITHOUT_TOGGLE, iconTap: NOTHING_WITHOUT_TOGGLE },
        badgeInteractions: { tap: BADGE_EVENT_TAP, hold: BADGE_EVENT_HOLD },
        preview: { label: '2 weeks ago', state: 'unknown', tone: 'blue' },
    },
    siren: {
        previewSupported: false,
        interactions: { tap: MORE_INFO_DEFAULT, hold: UNSUPPORTED_HOLD, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: 'Unknown', state: 'unknown', tone: 'neutral' },
    },
    fan: {
        previewSupported: false,
        interactions: { tap: MORE_INFO_DEFAULT, hold: UNSUPPORTED_HOLD, iconTap: NOTHING_DEFAULT },
        badgeInteractions: { tap: BADGE_TOGGLE_TAP, hold: BADGE_MORE_INFO_HOLD },
        preview: { label: 'Unknown', state: 'unknown', tone: 'neutral' },
    },
});

function matchesSupportConfig(entityId: string, deviceClass?: unknown) {
    const domain = entityId.split('.', 1)[0];
    const config = HASS_ENTITY_SUPPORT_CONFIG[domain];
    if (!config) return false;
    if (!config.deviceClasses?.length) return true;
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

