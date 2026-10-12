export const ERROR_CODE = {
    INPUT_BYTE_SIZE_EXCEEDED: -4,

    /** service side unknown error */
    SERVICE_UNKNOWN_ERROR: -1,

    /** Response success */
    SUCCESS: 0,

    /** Device password not initialized */
    UNINITIALIZED_PASSWORD: 1,

    /** Websocket authentication failed */
    AUTHENTICATION_FAILED: 2,

    /** Websocket connection timeout */
    AUTHENTICATION_TIMEOUT: 3,

    /** The number of websocket connections exceeds the threshold */
    CONNECTION_EXCEEDS: 4,

    /** The current mode does not support */
    MODE_NOT_SUPPORT: 5,

    /** Not a backup file for this device */
    BACKUP_FILE_ERROR: 300,

    /** Common network configuration errors */
    NETWORK_ERROR: 400,

    /** Wi-Fi authentication or key negotiation failed */
    WIFI_AUTH_FAILED: 420,

    /** Wi-Fi association, discovery, DHCP, or connection failed */
    WIFI_CONNECT_FAILED: 421,

    /** Wi-Fi/Channel health scanning is in progress */
    SCAN_IN_PROGRESS: 401,

    /** Conflict occurs when manually setting the IP address */
    IP_CONFLICT: 402,

    /** An mdns domain name conflict occurs  */
    MDNS_DOMAIN_CONFLICT: 410,

    /** Other web pages are resetting passwords! */
    CHANGE_PASSWORD_IN_PROGRESS: 901,

    /** Ota in progress, Please do not repeat the operation */
    OTA_IN_PROGRESS: 904,

    /** Detecting time zone */
    TIMEZONE_DETECTING: 905,

    /** Network anomaly causes time zone detection failure */
    TIMEZONE_DETECTION_ERROR: 906,

    /** ip white list exist */
    IP_WHITE_LIST_EXIST: 907,

    SESSION_INVALID: 910,
    SESSION_EXPIRED: 911,
    TIME_UNAVAILABLE: 912,
    TOO_MANY_ATTEMPTS: 913,
    SESSION_STORAGE_FAILED: 914,
    SESSION_LIMIT: 915,
};

export const EWsErrorCode = {
    SUCCESS: 0,
    INIT_CONNECT_ERROR: 601,
    INIT_CONNECT_TIMEOUT: 602,
    WS_INSTANCE_NOT_EXIST: 603,
};
