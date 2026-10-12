import type { RouteDefinition } from 'svelte-spa-router';
import { ROUTES_PATH } from '../constant/route';
import Login from '@/pages/login/index.svelte';
import ScreenConfiguration from '../pages/screen';
import HomeAssistant from '@/pages/home-assistant';
import AddHomeAssistant from '@/pages/home-assistant/AddHomeAssistant.svelte';
import EditHomeAssistant from '@/pages/home-assistant/EditHomeAssistant.svelte';
import HomeAssistantOAuthCallback from '@/pages/home-assistant/HomeAssistantOAuthCallback.svelte';
import Network from '@/pages/network';
import ApSettings from '@/pages/network/ApSettings.svelte';
import ApSsidSettings from '@/pages/network/ApSsidSettings.svelte';
import ApPasswordSettings from '@/pages/network/ApPasswordSettings.svelte';
import ApIpv4Settings from '@/pages/network/ApIpv4Settings.svelte';
import ApChannelSettings from '@/pages/network/ApChannelSettings.svelte';
import WlanSettings from '@/pages/network/WlanSettings.svelte';
import SsidSettings from '@/pages/network/SsidSettings.svelte';
import IpSettings from '@/pages/network/IpSettings.svelte';
import IpFieldSettings from '@/pages/network/IpFieldSettings.svelte';
import Settings from '@/pages/settings';
import DeviceName from '@/pages/settings/DeviceName.svelte';
import MdnsSettings from '@/pages/settings/MdnsSettings.svelte';
import TimeSettings from '@/pages/settings/TimeSettings.svelte';
import TimeZone from '@/pages/settings/TimeZone.svelte';
import NtpServerSettings from '@/pages/settings/NtpServerSettings.svelte';
import LanguageSettings from '@/pages/settings/LanguageSettings.svelte';
import DisplaySettings from '@/pages/settings/DisplaySettings.svelte';
import ScreenLockSettings from '@/pages/settings/ScreenLockSettings.svelte';
import NightMode from '@/pages/settings/NightMode.svelte';
import DarkMode from '@/pages/settings/DarkMode.svelte';
import DebugSettings from '@/pages/settings/DebugSettings.svelte';
import FeedbackSettings from '@/pages/settings/FeedbackSettings.svelte';
import NotFound from '@/pages/not-found';
import WifiSetup from '@/pages/setup/WifiSetup.svelte';

export const publicRoutes: RouteDefinition = {
    [ROUTES_PATH.LOGIN]: Login,
    [ROUTES_PATH.WIFI_SETUP]: WifiSetup,
    [ROUTES_PATH.NOT_FOUND]: Login,
};

export const protectedRoutes: RouteDefinition = {
    [ROUTES_PATH.ROOT]: ScreenConfiguration,
    [ROUTES_PATH.WIFI_SETUP]: WifiSetup,
    [ROUTES_PATH.HOME_ASSISTANT]: HomeAssistant,
    [ROUTES_PATH.HOME_ASSISTANT_ADD]: AddHomeAssistant,
    [ROUTES_PATH.HOME_ASSISTANT_EDIT]: EditHomeAssistant,
    [ROUTES_PATH.HOME_ASSISTANT_OAUTH_CALLBACK]: HomeAssistantOAuthCallback,
    [ROUTES_PATH.NETWORK]: Network,
    [ROUTES_PATH.NETWORK_AP]: ApSettings,
    [ROUTES_PATH.NETWORK_AP_SSID]: ApSsidSettings,
    [ROUTES_PATH.NETWORK_AP_PASSWORD]: ApPasswordSettings,
    [ROUTES_PATH.NETWORK_AP_IPV4]: ApIpv4Settings,
    [ROUTES_PATH.NETWORK_AP_CHANNEL]: ApChannelSettings,
    [ROUTES_PATH.NETWORK_WLAN]: WlanSettings,
    [ROUTES_PATH.NETWORK_WLAN_SSID]: SsidSettings,
    [ROUTES_PATH.NETWORK_WLAN_IP]: IpSettings,
    [ROUTES_PATH.NETWORK_WLAN_IP_FIELD]: IpFieldSettings,
    [ROUTES_PATH.SETTINGS]: Settings,
    [ROUTES_PATH.SETTINGS_DEVICE_NAME]: DeviceName,
    [ROUTES_PATH.SETTINGS_MDNS]: MdnsSettings,
    [ROUTES_PATH.SETTINGS_TIME]: TimeSettings,
    [ROUTES_PATH.SETTINGS_TIME_ZONE]: TimeZone,
    [ROUTES_PATH.SETTINGS_TIME_NTP]: NtpServerSettings,
    [ROUTES_PATH.SETTINGS_LANGUAGE]: LanguageSettings,
    [ROUTES_PATH.SETTINGS_DISPLAY]: DisplaySettings,
    [ROUTES_PATH.SETTINGS_SCREEN_LOCK]: ScreenLockSettings,
    [ROUTES_PATH.SETTINGS_NIGHT_MODE]: NightMode,
    [ROUTES_PATH.SETTINGS_DARK_MODE]: DarkMode,
    [ROUTES_PATH.SETTINGS_DEBUG]: DebugSettings,
    [ROUTES_PATH.SETTINGS_FEEDBACK]: FeedbackSettings,
    [ROUTES_PATH.NOT_FOUND]: NotFound,
};

export const routes: RouteDefinition = {
    ...publicRoutes,
    ...protectedRoutes,
};
