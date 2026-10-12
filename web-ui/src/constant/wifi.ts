/**
 * Map RSSI to the three available Wi-Fi signal assets.
 * - Strong: -60 dBm or better
 * - Medium: -70 to -61 dBm
 * - Weak: below -70 dBm
 */
export function getWifiSignalIcon(rssi: number) {
    if (rssi >= -60) return 'wifi_4.svg';
    if (rssi >= -70) return 'wifi_3.svg';
    return 'wifi_2.svg';
}
