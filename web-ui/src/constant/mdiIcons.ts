import type { SvgConfig } from './svgMap';
import {
    mdiAccount,
    mdiAlertCircle,
    mdiArrowCollapseHorizontal,
    mdiArrowDownBox,
    mdiArrowRight,
    mdiArrowSplitVertical,
    mdiArrowUpBox,
    mdiBattery,
    mdiBatteryCharging,
    mdiBatteryOutline,
    mdiBrightness5,
    mdiBrightness7,
    mdiBullhorn,
    mdiBlindsHorizontal,
    mdiBlindsHorizontalClosed,
    mdiCalendar,
    mdiCalendarAlert,
    mdiCheckboxMarkedCircle,
    mdiCheckCircle,
    mdiCheckNetworkOutline,
    mdiClose,
    mdiCloseNetworkOutline,
    mdiCropPortrait,
    mdiCurtains,
    mdiCurtainsClosed,

    mdiDevices,
    mdiDoor,
    mdiDoorClosed,
    mdiDoorOpen,
    mdiFire,
    mdiFan,

    mdiFormatListChecks,
    mdiFormatListBulletedSquare,
    mdiForumOutline,
    mdiGarage,
    mdiGarageOpen,
    mdiGate,
    mdiGateOpen,
    mdiGestureTapButton,
    mdiHelpCircle,
    mdiHome,
    mdiHomeAssistant,
    mdiHomeOutline,
    mdiLayers,
    mdiLightbulb,
    mdiLightbulbOff,
    mdiLock,
    mdiLockOpen,
    mdiMagnify,
    mdiMapMarkerRadius,
    mdiMessageTextOutline,
    mdiMotionSensor,
    mdiMotionSensorOff,
    mdiMusicNote,
    mdiMusicNoteOff,
    mdiOctagon,
    mdiPackage,
    mdiPackageUp,
    mdiPalette,
    mdiPlay,
    mdiPowerPlug,
    mdiPowerPlugOff,
    mdiPuzzle,
    mdiRadioboxBlank,
    mdiRollerShade,
    mdiRollerShadeClosed,
    mdiRobot,
    mdiShape,
    mdiSmokeDetector,
    mdiSmokeDetectorAlert,
    mdiSmokeDetectorVariant,
    mdiSmokeDetectorVariantAlert,
    mdiSnowflake,
    mdiSquare,
    mdiSquareOutline,
    mdiStop,
    mdiStorefrontOutline,
    mdiThermometer,
    mdiThermostat,
    mdiToggleSwitch,
    mdiToggleSwitchVariant,
    mdiToggleSwitchVariantOff,
    mdiVibrate,
    mdiViewGrid,
    mdiWater,
    mdiWaterOff,
    mdiWaterPercent,
    mdiWhiteBalanceSunny,
    mdiWindowClosed,
    mdiWindowOpen,
    mdiWindowShutter,
    mdiWindowShutterOpen,
    mdiCircle,
    mdiCircleSlice8,
} from '@mdi/js';

const mdiPaths: Record<string, string> = {
    'mdi:layers': mdiLayers,
    'mdi:door': mdiDoor,
    'mdi:devices': mdiDevices,
    'mdi:shape': mdiShape,
    'mdi:puzzle': mdiPuzzle,
    'mdi:robot': mdiRobot,
    'mdi:calendar': mdiCalendar,
    'mdi:close': mdiClose,
    'mdi:forum-outline': mdiForumOutline,
    'mdi:account': mdiAccount,
    'mdi:alert-circle': mdiAlertCircle,
    'mdi:arrow-collapse-horizontal': mdiArrowCollapseHorizontal,
    'mdi:arrow-down-box': mdiArrowDownBox,
    'mdi:arrow-right': mdiArrowRight,
    'mdi:arrow-split-vertical': mdiArrowSplitVertical,
    'mdi:arrow-up-box': mdiArrowUpBox,
    'mdi:battery': mdiBattery,
    'mdi:battery-charging': mdiBatteryCharging,
    'mdi:battery-outline': mdiBatteryOutline,
    'mdi:brightness-5': mdiBrightness5,
    'mdi:brightness-7': mdiBrightness7,
    'mdi:bullhorn': mdiBullhorn,
    'mdi:blinds-horizontal': mdiBlindsHorizontal,
    'mdi:blinds-horizontal-closed': mdiBlindsHorizontalClosed,
    'mdi:checkbox-marked-circle': mdiCheckboxMarkedCircle,
    'mdi:check-circle': mdiCheckCircle,
    'mdi:check-network-outline': mdiCheckNetworkOutline,
    'mdi:close-network-outline': mdiCloseNetworkOutline,
    'mdi:crop-portrait': mdiCropPortrait,
    'mdi:curtains': mdiCurtains,
    'mdi:curtains-closed': mdiCurtainsClosed,
    'mdi:calendar-alert': mdiCalendarAlert,
    'mdi:door-closed': mdiDoorClosed,
    'mdi:door-open': mdiDoorOpen,
    'mdi:fire': mdiFire,
    'mdi:fan': mdiFan,
    'mdi:garage': mdiGarage,
    'mdi:garage-open': mdiGarageOpen,
    'mdi:gate': mdiGate,
    'mdi:gate-open': mdiGateOpen,
    'mdi:home': mdiHome,
    'mdi:home-outline': mdiHomeOutline,
    'mdi:lightbulb': mdiLightbulb,
    'mdi:lightbulb-off': mdiLightbulbOff,
    'mdi:lock': mdiLock,
    'mdi:lock-open': mdiLockOpen,
    'mdi:motion-sensor': mdiMotionSensor,
    'mdi:motion-sensor-off': mdiMotionSensorOff,
    'mdi:music-note': mdiMusicNote,
    'mdi:music-note-off': mdiMusicNoteOff,
    'mdi:octagon': mdiOctagon,
    'mdi:package': mdiPackage,
    'mdi:package-up': mdiPackageUp,
    'mdi:play': mdiPlay,
    'mdi:power-plug': mdiPowerPlug,
    'mdi:power-plug-off': mdiPowerPlugOff,
    'mdi:radiobox-blank': mdiRadioboxBlank,
    'mdi:roller-shade': mdiRollerShade,
    'mdi:roller-shade-closed': mdiRollerShadeClosed,
    'mdi:smoke-detector': mdiSmokeDetector,
    'mdi:smoke-detector-alert': mdiSmokeDetectorAlert,
    'mdi:smoke-detector-variant': mdiSmokeDetectorVariant,
    'mdi:smoke-detector-variant-alert': mdiSmokeDetectorVariantAlert,
    'mdi:snowflake': mdiSnowflake,
    'mdi:square': mdiSquare,
    'mdi:square-outline': mdiSquareOutline,
    'mdi:stop': mdiStop,
    'mdi:storefront-outline': mdiStorefrontOutline,
    'mdi:thermometer': mdiThermometer,
    'mdi:thermostat': mdiThermostat,
    'mdi:toggle-switch-variant': mdiToggleSwitchVariant,
    'mdi:toggle-switch-variant-off': mdiToggleSwitchVariantOff,
    'mdi:vibrate': mdiVibrate,
    'mdi:water': mdiWater,
    'mdi:water-off': mdiWaterOff,
    'mdi:water-percent': mdiWaterPercent,
    'mdi:window-closed': mdiWindowClosed,
    'mdi:window-open': mdiWindowOpen,
    'mdi:window-shutter': mdiWindowShutter,
    'mdi:window-shutter-open': mdiWindowShutterOpen,
    'mdi:circle': mdiCircle,
    'mdi:circle-slice-8': mdiCircleSlice8,
    'mdi:palette': mdiPalette,
    'mdi:white-balance-sunny': mdiWhiteBalanceSunny,
    'mdi:format-list-checks': mdiFormatListChecks,
    'mdi:format-list-bulleted-square': mdiFormatListBulletedSquare,
    'mdi:gesture-tap-button': mdiGestureTapButton,
    'mdi:message-text-outline': mdiMessageTextOutline,
    'mdi:map-marker-radius': mdiMapMarkerRadius,
    //'mdi:lightbulb': mdiLightbulb,
    'mdi:toggle-switch': mdiToggleSwitch,
    //'mdi:thermometer': mdiThermometer,
    //'mdi:circle-half-full': mdiCircleHalfFull,
    //'mdi:thermostat': mdiThermostat,
    //'mdi:window-shutter': mdiWindowShutter,
    'mdi:home-assistant': mdiHomeAssistant,
    'mdi:help-circle': mdiHelpCircle,
    //'mdi:power-socket-us': mdiPowerSocketUs,
    //'mdi:fan': mdiFan,
    //'mdi:water': mdiWater,
    //'mdi:lock': mdiLock,
    //'mdi:camera': mdiCamera,
    //'mdi:speaker': mdiSpeaker,
    //'mdi:robot-vacuum': mdiRobotVacuum,
    //'mdi:weather-partly-cloudy': mdiWeatherPartlyCloudy,
    //'mdi:gauge': mdiGauge,
    //'mdi:eye': mdiEye,
    //'mdi:account': mdiAccount,
    'mdi:view-grid': mdiViewGrid,
    //'mdi:inbox': mdiInbox,
    'mdi:magnify': mdiMagnify,
};

const mdiCache = new Map<string, SvgConfig>();

function normalizeMdiName(name: string) {
    return name.trim().toLowerCase();
}

export function hasMdiIcon(name: string) {
    return Boolean(mdiPaths[normalizeMdiName(name)]);
}

export async function loadMdiIcon(name: string): Promise<SvgConfig | undefined> {
    const normalizedName = normalizeMdiName(name);
    const cached = mdiCache.get(normalizedName);
    if (cached) return cached;
    const path = mdiPaths[normalizedName];
    if (!path) return undefined;
    const config: SvgConfig = {
        svgReferenceName: normalizedName,
        html: `<svg aria-hidden="true" viewBox="0 0 24 24" width="1em" height="1em" fill="currentColor"><path d="${path}"></path></svg>`,
        width: '1em',
        height: '1em',
        viewBox: '0 0 24 24',
    };
    mdiCache.set(normalizedName, config);
    return config;
}

const ENTITY_DOMAIN_ICONS: Record<string, string> = {
    light: 'mdi:lightbulb',
    switch: 'mdi:toggle-switch',
    cover: 'mdi:window-closed',
    scene: 'mdi:palette',
    button: 'mdi:gesture-tap-button',
    climate: 'mdi:thermostat',
    event: 'mdi:calendar-alert',
    siren: 'mdi:bullhorn',
    fan: 'mdi:fan',
    //cover: 'mdi:window-shutter',
    //fan: 'mdi:fan',
    //lock: 'mdi:lock',
    //camera: 'mdi:camera',
    //media_player: 'mdi:speaker',
    //vacuum: 'mdi:robot-vacuum',
    //weather: 'mdi:weather-partly-cloudy',
    //person: 'mdi:account',
};

const SENSOR_DEVICE_CLASS_ICONS: Record<string, string> = {
    temperature: 'mdi:thermometer',
    humidity: 'mdi:water-percent',
};

const BINARY_SENSOR_ICONS: Record<string, { off: string; on: string }> = {
    battery: { off: 'mdi:battery', on: 'mdi:battery-outline' },
    battery_charging: { off: 'mdi:battery', on: 'mdi:battery-charging' },
    carbon_monoxide: { off: 'mdi:smoke-detector', on: 'mdi:smoke-detector-alert' },
    cold: { off: 'mdi:thermometer', on: 'mdi:snowflake' },
    connectivity: { off: 'mdi:close-network-outline', on: 'mdi:check-network-outline' },
    door: { off: 'mdi:door-closed', on: 'mdi:door-open' },
    garage_door: { off: 'mdi:garage', on: 'mdi:garage-open' },
    gas: { off: 'mdi:check-circle', on: 'mdi:alert-circle' },
    heat: { off: 'mdi:thermometer', on: 'mdi:fire' },
    light: { off: 'mdi:brightness-5', on: 'mdi:brightness-7' },
    lock: { off: 'mdi:lock', on: 'mdi:lock-open' },
    moisture: { off: 'mdi:water-off', on: 'mdi:water' },
    motion: { off: 'mdi:motion-sensor-off', on: 'mdi:motion-sensor' },
    moving: { off: 'mdi:octagon', on: 'mdi:arrow-right' },
    occupancy: { off: 'mdi:home-outline', on: 'mdi:home' },
    opening: { off: 'mdi:square', on: 'mdi:square-outline' },
    plug: { off: 'mdi:power-plug-off', on: 'mdi:power-plug' },
    power: { off: 'mdi:power-plug-off', on: 'mdi:power-plug' },
    presence: { off: 'mdi:home-outline', on: 'mdi:home' },
    problem: { off: 'mdi:check-circle', on: 'mdi:alert-circle' },
    running: { off: 'mdi:stop', on: 'mdi:play' },
    safety: { off: 'mdi:check-circle', on: 'mdi:alert-circle' },
    smoke: { off: 'mdi:smoke-detector-variant', on: 'mdi:smoke-detector-variant-alert' },
    sound: { off: 'mdi:music-note-off', on: 'mdi:music-note' },
    tamper: { off: 'mdi:check-circle', on: 'mdi:alert-circle' },
    update: { off: 'mdi:package', on: 'mdi:package-up' },
    vibration: { off: 'mdi:crop-portrait', on: 'mdi:vibrate' },
    window: { off: 'mdi:window-closed', on: 'mdi:window-open' },
};

const COVER_ICONS: Record<string, { default: string; closed: string; closing?: string; opening?: string }> = {
    awning: { default: 'mdi:storefront-outline', closed: 'mdi:storefront-outline' },
    blind: { default: 'mdi:blinds-horizontal', closed: 'mdi:blinds-horizontal-closed', closing: 'mdi:arrow-down-box', opening: 'mdi:arrow-up-box' },
    curtain: { default: 'mdi:curtains', closed: 'mdi:curtains-closed', closing: 'mdi:arrow-collapse-horizontal', opening: 'mdi:arrow-split-vertical' },
    damper: { default: 'mdi:circle', closed: 'mdi:circle-slice-8' },
    door: { default: 'mdi:door-open', closed: 'mdi:door-closed' },
    garage: { default: 'mdi:garage-open', closed: 'mdi:garage', closing: 'mdi:arrow-down-box', opening: 'mdi:arrow-up-box' },
    gate: { default: 'mdi:gate-open', closed: 'mdi:gate', closing: 'mdi:arrow-right', opening: 'mdi:arrow-right' },
    shade: { default: 'mdi:roller-shade', closed: 'mdi:roller-shade-closed', closing: 'mdi:arrow-down-box', opening: 'mdi:arrow-up-box' },
    shutter: { default: 'mdi:window-shutter-open', closed: 'mdi:window-shutter', closing: 'mdi:arrow-down-box', opening: 'mdi:arrow-up-box' },
    window: { default: 'mdi:window-open', closed: 'mdi:window-closed', closing: 'mdi:arrow-down-box', opening: 'mdi:arrow-up-box' },
};

export function getHassEntityMdiIcon(entityId: string, configuredIcon?: string | null, deviceClass?: unknown, state?: string) {
    if (configuredIcon && hasMdiIcon(configuredIcon)) return normalizeMdiName(configuredIcon);
    const domain = entityId.split('.')[0] ?? '';
    if (domain === 'sensor' && typeof deviceClass === 'string') return SENSOR_DEVICE_CLASS_ICONS[deviceClass] ?? 'mdi:home-assistant';
    if (domain === 'binary_sensor') {
        const icons = typeof deviceClass === 'string' ? BINARY_SENSOR_ICONS[deviceClass] : undefined;
        if (icons) return state === 'on' ? icons.on : icons.off;
        return state === 'on' ? 'mdi:checkbox-marked-circle' : 'mdi:radiobox-blank';
    }
    if (domain === 'light') return state === 'off' ? 'mdi:lightbulb-off' : 'mdi:lightbulb';
    if (domain === 'switch') {
        if (deviceClass === 'outlet') return state === 'off' ? 'mdi:power-plug-off' : 'mdi:power-plug';
        return state === 'off' ? 'mdi:toggle-switch-variant-off' : 'mdi:toggle-switch-variant';
    }
    if (domain === 'cover') {
        const icons = typeof deviceClass === 'string' ? COVER_ICONS[deviceClass] : undefined;
        if (!icons) return state === 'closed' ? 'mdi:window-closed' : state === 'closing' ? 'mdi:arrow-down-box' : state === 'opening' ? 'mdi:arrow-up-box' : 'mdi:window-open';
        if (state === 'closed') return icons.closed;
        if (state === 'closing' && icons.closing) return icons.closing;
        if (state === 'opening' && icons.opening) return icons.opening;
        return icons.default;
    }
    return ENTITY_DOMAIN_ICONS[domain] ?? 'mdi:home-assistant';
}
