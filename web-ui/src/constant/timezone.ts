export const TIMEZONE_INFO: {
    [key: string]: {
        tzName: string;
        utcOffset: number;
        dstOffset?: number;
        dstStartRule?: string;
        dstEndRule?: string;
    };
} = {
    'Africa/Abidjan': {
        tzName: 'Africa/Abidjan',
        utcOffset: 0,
    },
    'Africa/Algiers': {
        tzName: 'Africa/Algiers',
        utcOffset: 60,
    },
    'Africa/El_Aaiun': {
        tzName: 'Africa/El_Aaiun',
        utcOffset: 60,
    },
    'Africa/Tripoli': {
        tzName: 'Africa/Tripoli',
        utcOffset: 120,
    },
    'Africa/Maputo': {
        tzName: 'Africa/Maputo',
        utcOffset: 120,
    },
    'Africa/Cairo': {
        tzName: 'Africa/Cairo',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 6L 4 *',
        dstEndRule: '0 0 0 ? 5L 10 *',
    },
    'Africa/Monrovia': {
        tzName: 'Africa/Monrovia',
        utcOffset: 0,
    },
    'Africa/Nairobi': {
        tzName: 'Africa/Nairobi',
        utcOffset: 180,
    },
    'Africa/Khartoum': {
        tzName: 'Africa/Khartoum',
        utcOffset: 120,
    },
    'Africa/Windhoek': {
        tzName: 'Africa/Windhoek',
        utcOffset: 120,
    },
    'Africa/Tunis': {
        tzName: 'Africa/Tunis',
        utcOffset: 60,
    },
    'Africa/Johannesburg': {
        tzName: 'Africa/Johannesburg',
        utcOffset: 120,
    },
    'Africa/Casablanca': {
        tzName: 'Africa/Casablanca',
        utcOffset: 60,
    },
    'Africa/Ndjamena': {
        tzName: 'Africa/Ndjamena',
        utcOffset: 60,
    },
    'Indian/Mauritius': {
        tzName: 'Indian/Mauritius',
        utcOffset: 240,
    },
    'Atlantic/Cape_Verde': {
        tzName: 'Atlantic/Cape_Verde',
        utcOffset: -60,
    },
    'Africa/Bissau': {
        tzName: 'Africa/Bissau',
        utcOffset: 0,
    },
    'Africa/Sao_Tome': {
        tzName: 'Africa/Sao_Tome',
        utcOffset: 0,
    },
    'Africa/Juba': {
        tzName: 'Africa/Juba',
        utcOffset: 120,
    },
    'Africa/Lagos': {
        tzName: 'Africa/Lagos',
        utcOffset: 60,
    },
    'Antarctica/Troll': {
        tzName: 'Antarctica/Troll',
        utcOffset: 0,
        dstOffset: 120,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Antarctica/Davis': {
        tzName: 'Antarctica/Davis',
        utcOffset: 420,
    },
    'Antarctica/Vostok': {
        tzName: 'Antarctica/Vostok',
        utcOffset: 300,
    },
    'Antarctica/Mawson': {
        tzName: 'Antarctica/Mawson',
        utcOffset: 300,
    },
    'Antarctica/Casey': {
        tzName: 'Antarctica/Casey',
        utcOffset: 480,
    },
    'Antarctica/Rothera': {
        tzName: 'Antarctica/Rothera',
        utcOffset: -180,
    },
    'Asia/Baghdad': {
        tzName: 'Asia/Baghdad',
        utcOffset: 180,
    },
    'Asia/Pontianak': {
        tzName: 'Asia/Pontianak',
        utcOffset: 420,
    },
    'Asia/Ulaanbaatar': {
        tzName: 'Asia/Ulaanbaatar',
        utcOffset: 480,
    },
    'Asia/Aqtau': {
        tzName: 'Asia/Aqtau',
        utcOffset: 300,
    },
    'Asia/Almaty': {
        tzName: 'Asia/Almaty',
        utcOffset: 300,
    },
    'Asia/Qyzylorda': {
        tzName: 'Asia/Qyzylorda',
        utcOffset: 300,
    },
    'Asia/Urumqi': {
        tzName: 'Asia/Urumqi',
        utcOffset: 360,
    },
    'Asia/Dushanbe': {
        tzName: 'Asia/Dushanbe',
        utcOffset: 300,
    },
    'Asia/Amman': {
        tzName: 'Asia/Amman',
        utcOffset: 180,
    },
    'Asia/Yerevan': {
        tzName: 'Asia/Yerevan',
        utcOffset: 240,
    },
    'Asia/Beirut': {
        tzName: 'Asia/Beirut',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1L 3 *',
        dstEndRule: '0 0 0 ? 1L 10 *',
    },
    'Asia/Singapore': {
        tzName: 'Asia/Singapore',
        utcOffset: 480,
    },
    'Asia/Atyrau': {
        tzName: 'Asia/Atyrau',
        utcOffset: 300,
    },
    'Asia/Nicosia': {
        tzName: 'Asia/Nicosia',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Asia/Hong_Kong': {
        tzName: 'Asia/Hong_Kong',
        utcOffset: 480,
    },
    'Indian/Maldives': {
        tzName: 'Indian/Maldives',
        utcOffset: 300,
    },
    'Asia/Tokyo': {
        tzName: 'Asia/Tokyo',
        utcOffset: 540,
    },
    'Asia/Tehran': {
        tzName: 'Asia/Tehran',
        utcOffset: 210,
    },
    'Asia/Macau': {
        tzName: 'Asia/Macau',
        utcOffset: 480,
    },
    'Asia/Taipei': {
        tzName: 'Asia/Taipei',
        utcOffset: 480,
    },
    'Asia/Hovd': {
        tzName: 'Asia/Hovd',
        utcOffset: 420,
    },
    'Asia/Makassar': {
        tzName: 'Asia/Makassar',
        utcOffset: 480,
    },
    'Asia/Jerusalem': {
        tzName: 'Asia/Jerusalem',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 6LF 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Asia/Dubai': {
        tzName: 'Asia/Dubai',
        utcOffset: 240,
    },
    'Asia/Bishkek': {
        tzName: 'Asia/Bishkek',
        utcOffset: 360,
    },
    'Asia/Kabul': {
        tzName: 'Asia/Kabul',
        utcOffset: 270,
    },
    'Asia/Aqtobe': {
        tzName: 'Asia/Aqtobe',
        utcOffset: 300,
    },
    'Asia/Bangkok': {
        tzName: 'Asia/Bangkok',
        utcOffset: 420,
    },
    'Asia/Dhaka': {
        tzName: 'Asia/Dhaka',
        utcOffset: 360,
    },
    'Indian/Chagos': {
        tzName: 'Indian/Chagos',
        utcOffset: 360,
    },
    'Asia/Colombo': {
        tzName: 'Asia/Colombo',
        utcOffset: 330,
    },
    'Asia/Qatar': {
        tzName: 'Asia/Qatar',
        utcOffset: 180,
    },
    'Asia/Kathmandu': {
        tzName: 'Asia/Kathmandu',
        utcOffset: 345,
    },
    'Asia/Jayapura': {
        tzName: 'Asia/Jayapura',
        utcOffset: 540,
    },
    'Asia/Qostanay': {
        tzName: 'Asia/Qostanay',
        utcOffset: 300,
    },
    'Asia/Dili': {
        tzName: 'Asia/Dili',
        utcOffset: 540,
    },
    'Asia/Hebron': {
        tzName: 'Asia/Hebron',
        utcOffset: 120,
    },
    'Asia/Ho_Chi_Minh': {
        tzName: 'Asia/Ho_Chi_Minh',
        utcOffset: 420,
    },
    'Asia/Kolkata': {
        tzName: 'Asia/Kolkata',
        utcOffset: 330,
    },
    'Asia/Damascus': {
        tzName: 'Asia/Damascus',
        utcOffset: 180,
    },
    'Asia/Riyadh': {
        tzName: 'Asia/Riyadh',
        utcOffset: 180,
    },
    'Asia/Kuching': {
        tzName: 'Asia/Kuching',
        utcOffset: 480,
    },
    'Asia/Manila': {
        tzName: 'Asia/Manila',
        utcOffset: 480,
    },
    'Asia/Yangon': {
        tzName: 'Asia/Yangon',
        utcOffset: 390,
    },
    'Asia/Gaza': {
        tzName: 'Asia/Gaza',
        utcOffset: 120,
    },
    'Asia/Baku': {
        tzName: 'Asia/Baku',
        utcOffset: 240,
    },
    'Asia/Oral': {
        tzName: 'Asia/Oral',
        utcOffset: 300,
    },
    'Asia/Karachi': {
        tzName: 'Asia/Karachi',
        utcOffset: 300,
    },
    'Asia/Pyongyang': {
        tzName: 'Asia/Pyongyang',
        utcOffset: 540,
    },
    'Asia/Seoul': {
        tzName: 'Asia/Seoul',
        utcOffset: 540,
    },
    'Asia/Jakarta': {
        tzName: 'Asia/Jakarta',
        utcOffset: 420,
    },
    'Asia/Thimphu': {
        tzName: 'Asia/Thimphu',
        utcOffset: 360,
    },
    'Asia/Samarkand': {
        tzName: 'Asia/Samarkand',
        utcOffset: 300,
    },
    'Asia/Ashgabat': {
        tzName: 'Asia/Ashgabat',
        utcOffset: 300,
    },
    'Asia/Famagusta': {
        tzName: 'Asia/Famagusta',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Asia/Shanghai': {
        tzName: 'Asia/Shanghai',
        utcOffset: 480,
    },
    'Asia/Tashkent': {
        tzName: 'Asia/Tashkent',
        utcOffset: 300,
    },
    'Asia/Tbilisi': {
        tzName: 'Asia/Tbilisi',
        utcOffset: 240,
    },
    'Pacific/Tongatapu': {
        tzName: 'Pacific/Tongatapu',
        utcOffset: 780,
    },
    'Pacific/Apia': {
        tzName: 'Pacific/Apia',
        utcOffset: 780,
    },
    'Antarctica/Macquarie': {
        tzName: 'Antarctica/Macquarie',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/Darwin': {
        tzName: 'Australia/Darwin',
        utcOffset: 570,
    },
    'Pacific/Auckland': {
        tzName: 'Pacific/Auckland',
        utcOffset: 720,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 9 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Pacific/Kwajalein': {
        tzName: 'Pacific/Kwajalein',
        utcOffset: 720,
    },
    'Pacific/Efate': {
        tzName: 'Pacific/Efate',
        utcOffset: 660,
    },
    'Pacific/Port_Moresby': {
        tzName: 'Pacific/Port_Moresby',
        utcOffset: 600,
    },
    'Pacific/Noumea': {
        tzName: 'Pacific/Noumea',
        utcOffset: 660,
    },
    'Pacific/Kosrae': {
        tzName: 'Pacific/Kosrae',
        utcOffset: 660,
    },
    'Pacific/Marquesas': {
        tzName: 'Pacific/Marquesas',
        utcOffset: -510,
    },
    'Pacific/Kiritimati': {
        tzName: 'Pacific/Kiritimati',
        utcOffset: 840,
    },
    'Pacific/Guam': {
        tzName: 'Pacific/Guam',
        utcOffset: 600,
    },
    'Pacific/Fiji': {
        tzName: 'Pacific/Fiji',
        utcOffset: 720,
    },
    'Pacific/Nauru': {
        tzName: 'Pacific/Nauru',
        utcOffset: 720,
    },
    'Pacific/Tahiti': {
        tzName: 'Pacific/Tahiti',
        utcOffset: -600,
    },
    'Pacific/Chatham': {
        tzName: 'Pacific/Chatham',
        utcOffset: 765,
        dstOffset: 60,
        dstStartRule: '0 45 2 ? 1L 9 *',
        dstEndRule: '0 45 3 ? 1#1 4 *',
    },
    'Australia/Lindeman': {
        tzName: 'Australia/Lindeman',
        utcOffset: 600,
    },
    'Pacific/Tarawa': {
        tzName: 'Pacific/Tarawa',
        utcOffset: 720,
    },
    'Australia/Perth': {
        tzName: 'Australia/Perth',
        utcOffset: 480,
    },
    'Pacific/Fakaofo': {
        tzName: 'Pacific/Fakaofo',
        utcOffset: 780,
    },
    'Pacific/Norfolk': {
        tzName: 'Pacific/Norfolk',
        utcOffset: 660,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Pacific/Kanton': {
        tzName: 'Pacific/Kanton',
        utcOffset: 780,
    },
    'Pacific/Pago_Pago': {
        tzName: 'Pacific/Pago_Pago',
        utcOffset: -660,
    },
    'Australia/Lord_Howe': {
        tzName: 'Australia/Lord_Howe',
        utcOffset: 630,
        dstOffset: 30,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 2 ? 1#1 4 *',
    },
    'Australia/Broken_Hill': {
        tzName: 'Australia/Broken_Hill',
        utcOffset: 570,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Pacific/Niue': {
        tzName: 'Pacific/Niue',
        utcOffset: -660,
    },
    'Australia/Eucla': {
        tzName: 'Australia/Eucla',
        utcOffset: 525,
    },
    'Pacific/Palau': {
        tzName: 'Pacific/Palau',
        utcOffset: 540,
    },
    'Pacific/Guadalcanal': {
        tzName: 'Pacific/Guadalcanal',
        utcOffset: 660,
    },
    'Australia/Sydney': {
        tzName: 'Australia/Sydney',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/Hobart': {
        tzName: 'Australia/Hobart',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Pacific/Rarotonga': {
        tzName: 'Pacific/Rarotonga',
        utcOffset: -600,
    },
    'Australia/Brisbane': {
        tzName: 'Australia/Brisbane',
        utcOffset: 600,
    },
    'Australia/Melbourne': {
        tzName: 'Australia/Melbourne',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Pacific/Bougainville': {
        tzName: 'Pacific/Bougainville',
        utcOffset: 660,
    },
    'Pacific/Pitcairn': {
        tzName: 'Pacific/Pitcairn',
        utcOffset: -480,
    },
    'Australia/Adelaide': {
        tzName: 'Australia/Adelaide',
        utcOffset: 570,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Pacific/Gambier': {
        tzName: 'Pacific/Gambier',
        utcOffset: -540,
    },
    'Asia/Irkutsk': {
        tzName: 'Asia/Irkutsk',
        utcOffset: 480,
    },
    'Asia/Srednekolymsk': {
        tzName: 'Asia/Srednekolymsk',
        utcOffset: 660,
    },
    'America/Scoresbysund': {
        tzName: 'America/Scoresbysund',
        utcOffset: -120,
        dstOffset: 60,
        dstStartRule: '0 0 23 ? 1L 3 *',
        dstEndRule: '0 0 0 ? 1L 10 *',
    },
    'Europe/Kirov': {
        tzName: 'Europe/Kirov',
        utcOffset: 180,
    },
    'Europe/Bucharest': {
        tzName: 'Europe/Bucharest',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Asia/Kamchatka': {
        tzName: 'Asia/Kamchatka',
        utcOffset: 720,
    },
    'Europe/Prague': {
        tzName: 'Europe/Prague',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Riga': {
        tzName: 'Europe/Riga',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Africa/Ceuta': {
        tzName: 'Africa/Ceuta',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Berlin': {
        tzName: 'Europe/Berlin',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'America/Danmarkshavn': {
        tzName: 'America/Danmarkshavn',
        utcOffset: 0,
    },
    'Asia/Novosibirsk': {
        tzName: 'Asia/Novosibirsk',
        utcOffset: 420,
    },
    'Europe/Andorra': {
        tzName: 'Europe/Andorra',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Vilnius': {
        tzName: 'Europe/Vilnius',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Asia/Sakhalin': {
        tzName: 'Asia/Sakhalin',
        utcOffset: 660,
    },
    'Europe/London': {
        tzName: 'Europe/London',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Rome': {
        tzName: 'Europe/Rome',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Minsk': {
        tzName: 'Europe/Minsk',
        utcOffset: 180,
    },
    'Europe/Ulyanovsk': {
        tzName: 'Europe/Ulyanovsk',
        utcOffset: 240,
    },
    'Europe/Moscow': {
        tzName: 'Europe/Moscow',
        utcOffset: 180,
    },
    'Europe/Kaliningrad': {
        tzName: 'Europe/Kaliningrad',
        utcOffset: 120,
    },
    'Europe/Madrid': {
        tzName: 'Europe/Madrid',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Belgrade': {
        tzName: 'Europe/Belgrade',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Lisbon': {
        tzName: 'Europe/Lisbon',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Asia/Vladivostok': {
        tzName: 'Asia/Vladivostok',
        utcOffset: 600,
    },
    'Asia/Magadan': {
        tzName: 'Asia/Magadan',
        utcOffset: 660,
    },
    'Asia/Chita': {
        tzName: 'Asia/Chita',
        utcOffset: 540,
    },
    'Asia/Barnaul': {
        tzName: 'Asia/Barnaul',
        utcOffset: 420,
    },
    'Atlantic/Azores': {
        tzName: 'Atlantic/Azores',
        utcOffset: -60,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1L 3 *',
        dstEndRule: '0 0 1 ? 1L 10 *',
    },
    'America/Thule': {
        tzName: 'America/Thule',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Europe/Astrakhan': {
        tzName: 'Europe/Astrakhan',
        utcOffset: 240,
    },
    'Atlantic/Madeira': {
        tzName: 'Atlantic/Madeira',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Gibraltar': {
        tzName: 'Europe/Gibraltar',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Sofia': {
        tzName: 'Europe/Sofia',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Kyiv': {
        tzName: 'Europe/Kyiv',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Istanbul': {
        tzName: 'Europe/Istanbul',
        utcOffset: 180,
    },
    'Atlantic/Canary': {
        tzName: 'Atlantic/Canary',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Asia/Krasnoyarsk': {
        tzName: 'Asia/Krasnoyarsk',
        utcOffset: 420,
    },
    'Asia/Anadyr': {
        tzName: 'Asia/Anadyr',
        utcOffset: 720,
    },
    'Europe/Samara': {
        tzName: 'Europe/Samara',
        utcOffset: 240,
    },
    'America/Nuuk': {
        tzName: 'America/Nuuk',
        utcOffset: -120,
        dstOffset: 60,
        dstStartRule: '0 0 23 ? 1L 3 *',
        dstEndRule: '0 0 0 ? 1L 10 *',
    },
    'Europe/Simferopol': {
        tzName: 'Europe/Simferopol',
        utcOffset: 180,
    },
    'Europe/Warsaw': {
        tzName: 'Europe/Warsaw',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Helsinki': {
        tzName: 'Europe/Helsinki',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Budapest': {
        tzName: 'Europe/Budapest',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Asia/Ust-Nera': {
        tzName: 'Asia/Ust-Nera',
        utcOffset: 600,
    },
    'Europe/Malta': {
        tzName: 'Europe/Malta',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Tirane': {
        tzName: 'Europe/Tirane',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Tallinn': {
        tzName: 'Europe/Tallinn',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Asia/Novokuznetsk': {
        tzName: 'Asia/Novokuznetsk',
        utcOffset: 420,
    },
    'Europe/Volgograd': {
        tzName: 'Europe/Volgograd',
        utcOffset: 180,
    },
    'Europe/Brussels': {
        tzName: 'Europe/Brussels',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Asia/Tomsk': {
        tzName: 'Asia/Tomsk',
        utcOffset: 420,
    },
    'Asia/Yakutsk': {
        tzName: 'Asia/Yakutsk',
        utcOffset: 540,
    },
    'Europe/Saratov': {
        tzName: 'Europe/Saratov',
        utcOffset: 240,
    },
    'Europe/Dublin': {
        tzName: 'Europe/Dublin',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Paris': {
        tzName: 'Europe/Paris',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Atlantic/Faroe': {
        tzName: 'Atlantic/Faroe',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Asia/Omsk': {
        tzName: 'Asia/Omsk',
        utcOffset: 360,
    },
    'Asia/Yekaterinburg': {
        tzName: 'Asia/Yekaterinburg',
        utcOffset: 300,
    },
    'Europe/Zurich': {
        tzName: 'Europe/Zurich',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Vienna': {
        tzName: 'Europe/Vienna',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Athens': {
        tzName: 'Europe/Athens',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Chisinau': {
        tzName: 'Europe/Chisinau',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Asia/Khandyga': {
        tzName: 'Asia/Khandyga',
        utcOffset: 540,
    },
    'America/New_York': {
        tzName: 'America/New_York',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Grand_Turk': {
        tzName: 'America/Grand_Turk',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Ojinaga': {
        tzName: 'America/Ojinaga',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Matamoros': {
        tzName: 'America/Matamoros',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Vancouver': {
        tzName: 'America/Vancouver',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Atlantic/Bermuda': {
        tzName: 'Atlantic/Bermuda',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/North_Dakota/Center': {
        tzName: 'America/North_Dakota/Center',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Regina': {
        tzName: 'America/Regina',
        utcOffset: -360,
    },
    'America/St_Johns': {
        tzName: 'America/St_Johns',
        utcOffset: -150,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Toronto': {
        tzName: 'America/Toronto',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Menominee': {
        tzName: 'America/Menominee',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Cancun': {
        tzName: 'America/Cancun',
        utcOffset: -300,
    },
    'America/Inuvik': {
        tzName: 'America/Inuvik',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Swift_Current': {
        tzName: 'America/Swift_Current',
        utcOffset: -360,
    },
    'America/Indiana/Knox': {
        tzName: 'America/Indiana/Knox',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Denver': {
        tzName: 'America/Denver',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Santo_Domingo': {
        tzName: 'America/Santo_Domingo',
        utcOffset: -240,
    },
    'America/Glace_Bay': {
        tzName: 'America/Glace_Bay',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Indiana/Marengo': {
        tzName: 'America/Indiana/Marengo',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Martinique': {
        tzName: 'America/Martinique',
        utcOffset: -240,
    },
    'America/Nome': {
        tzName: 'America/Nome',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Barbados': {
        tzName: 'America/Barbados',
        utcOffset: -240,
    },
    'America/Indiana/Vincennes': {
        tzName: 'America/Indiana/Vincennes',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Dawson_Creek': {
        tzName: 'America/Dawson_Creek',
        utcOffset: -420,
    },
    'America/Kentucky/Louisville': {
        tzName: 'America/Kentucky/Louisville',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/El_Salvador': {
        tzName: 'America/El_Salvador',
        utcOffset: -360,
    },
    'America/Chihuahua': {
        tzName: 'America/Chihuahua',
        utcOffset: -360,
    },
    'America/Moncton': {
        tzName: 'America/Moncton',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Phoenix': {
        tzName: 'America/Phoenix',
        utcOffset: -420,
    },
    'America/Tegucigalpa': {
        tzName: 'America/Tegucigalpa',
        utcOffset: -360,
    },
    'America/Dawson': {
        tzName: 'America/Dawson',
        utcOffset: -420,
    },
    'America/Boise': {
        tzName: 'America/Boise',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/North_Dakota/Beulah': {
        tzName: 'America/North_Dakota/Beulah',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Halifax': {
        tzName: 'America/Halifax',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Miquelon': {
        tzName: 'America/Miquelon',
        utcOffset: -180,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Indiana/Tell_City': {
        tzName: 'America/Indiana/Tell_City',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Belize': {
        tzName: 'America/Belize',
        utcOffset: -360,
    },
    'America/Iqaluit': {
        tzName: 'America/Iqaluit',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Metlakatla': {
        tzName: 'America/Metlakatla',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Costa_Rica': {
        tzName: 'America/Costa_Rica',
        utcOffset: -360,
    },
    'America/Rankin_Inlet': {
        tzName: 'America/Rankin_Inlet',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Indiana/Vevay': {
        tzName: 'America/Indiana/Vevay',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Mexico_City': {
        tzName: 'America/Mexico_City',
        utcOffset: -360,
    },
    'America/Cambridge_Bay': {
        tzName: 'America/Cambridge_Bay',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Winnipeg': {
        tzName: 'America/Winnipeg',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Detroit': {
        tzName: 'America/Detroit',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Indiana/Petersburg': {
        tzName: 'America/Indiana/Petersburg',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Indiana/Indianapolis': {
        tzName: 'America/Indiana/Indianapolis',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Chicago': {
        tzName: 'America/Chicago',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Indiana/Winamac': {
        tzName: 'America/Indiana/Winamac',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Goose_Bay': {
        tzName: 'America/Goose_Bay',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Resolute': {
        tzName: 'America/Resolute',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Fort_Nelson': {
        tzName: 'America/Fort_Nelson',
        utcOffset: -420,
    },
    'America/Yakutat': {
        tzName: 'America/Yakutat',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Mazatlan': {
        tzName: 'America/Mazatlan',
        utcOffset: -420,
    },
    'America/Ciudad_Juarez': {
        tzName: 'America/Ciudad_Juarez',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Los_Angeles': {
        tzName: 'America/Los_Angeles',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Tijuana': {
        tzName: 'America/Tijuana',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/North_Dakota/New_Salem': {
        tzName: 'America/North_Dakota/New_Salem',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Panama': {
        tzName: 'America/Panama',
        utcOffset: -300,
    },
    'America/Port-au-Prince': {
        tzName: 'America/Port-au-Prince',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Anchorage': {
        tzName: 'America/Anchorage',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Puerto_Rico': {
        tzName: 'America/Puerto_Rico',
        utcOffset: -240,
    },
    'America/Havana': {
        tzName: 'America/Havana',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#2 3 *',
        dstEndRule: '0 0 1 ? 1#1 11 *',
    },
    'America/Bahia_Banderas': {
        tzName: 'America/Bahia_Banderas',
        utcOffset: -360,
    },
    'America/Whitehorse': {
        tzName: 'America/Whitehorse',
        utcOffset: -420,
    },
    'America/Guatemala': {
        tzName: 'America/Guatemala',
        utcOffset: -360,
    },
    'America/Merida': {
        tzName: 'America/Merida',
        utcOffset: -360,
    },
    'America/Edmonton': {
        tzName: 'America/Edmonton',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Monterrey': {
        tzName: 'America/Monterrey',
        utcOffset: -360,
    },
    'America/Kentucky/Monticello': {
        tzName: 'America/Kentucky/Monticello',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Adak': {
        tzName: 'America/Adak',
        utcOffset: -600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Hermosillo': {
        tzName: 'America/Hermosillo',
        utcOffset: -420,
    },
    'America/Juneau': {
        tzName: 'America/Juneau',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Pacific/Honolulu': {
        tzName: 'Pacific/Honolulu',
        utcOffset: -600,
    },
    'America/Sitka': {
        tzName: 'America/Sitka',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Managua': {
        tzName: 'America/Managua',
        utcOffset: -360,
    },
    'America/Jamaica': {
        tzName: 'America/Jamaica',
        utcOffset: -300,
    },
    'America/Manaus': {
        tzName: 'America/Manaus',
        utcOffset: -240,
    },
    'America/Argentina/Jujuy': {
        tzName: 'America/Argentina/Jujuy',
        utcOffset: -180,
    },
    'America/Bogota': {
        tzName: 'America/Bogota',
        utcOffset: -300,
    },
    'America/Argentina/La_Rioja': {
        tzName: 'America/Argentina/La_Rioja',
        utcOffset: -180,
    },
    'Atlantic/South_Georgia': {
        tzName: 'Atlantic/South_Georgia',
        utcOffset: -120,
    },
    'America/Santiago': {
        tzName: 'America/Santiago',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#1 9 *',
        dstEndRule: '0 0 0 ? 1#1 4 *',
    },
    'America/Sao_Paulo': {
        tzName: 'America/Sao_Paulo',
        utcOffset: -180,
    },
    'America/Argentina/Ushuaia': {
        tzName: 'America/Argentina/Ushuaia',
        utcOffset: -180,
    },
    'America/Porto_Velho': {
        tzName: 'America/Porto_Velho',
        utcOffset: -240,
    },
    'America/Lima': {
        tzName: 'America/Lima',
        utcOffset: -300,
    },
    'America/Argentina/San_Luis': {
        tzName: 'America/Argentina/San_Luis',
        utcOffset: -180,
    },
    'America/Guayaquil': {
        tzName: 'America/Guayaquil',
        utcOffset: -300,
    },
    'America/Belem': {
        tzName: 'America/Belem',
        utcOffset: -180,
    },
    'Antarctica/Palmer': {
        tzName: 'Antarctica/Palmer',
        utcOffset: -180,
    },
    'America/Maceio': {
        tzName: 'America/Maceio',
        utcOffset: -180,
    },
    'America/Bahia': {
        tzName: 'America/Bahia',
        utcOffset: -180,
    },
    'America/Argentina/Tucuman': {
        tzName: 'America/Argentina/Tucuman',
        utcOffset: -180,
    },
    'America/Caracas': {
        tzName: 'America/Caracas',
        utcOffset: -240,
    },
    'America/Paramaribo': {
        tzName: 'America/Paramaribo',
        utcOffset: -180,
    },
    'Pacific/Easter': {
        tzName: 'Pacific/Easter',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#1 9 *',
        dstEndRule: '0 0 0 ? 1#1 4 *',
    },
    'America/Rio_Branco': {
        tzName: 'America/Rio_Branco',
        utcOffset: -300,
    },
    'America/Argentina/San_Juan': {
        tzName: 'America/Argentina/San_Juan',
        utcOffset: -180,
    },
    'America/Fortaleza': {
        tzName: 'America/Fortaleza',
        utcOffset: -180,
    },
    'America/Argentina/Cordoba': {
        tzName: 'America/Argentina/Cordoba',
        utcOffset: -180,
    },
    'America/Punta_Arenas': {
        tzName: 'America/Punta_Arenas',
        utcOffset: -180,
    },
    'America/Guyana': {
        tzName: 'America/Guyana',
        utcOffset: -240,
    },
    'America/Noronha': {
        tzName: 'America/Noronha',
        utcOffset: -120,
    },
    'Pacific/Galapagos': {
        tzName: 'Pacific/Galapagos',
        utcOffset: -360,
    },
    'America/Argentina/Catamarca': {
        tzName: 'America/Argentina/Catamarca',
        utcOffset: -180,
    },
    'America/La_Paz': {
        tzName: 'America/La_Paz',
        utcOffset: -240,
    },
    'America/Argentina/Buenos_Aires': {
        tzName: 'America/Argentina/Buenos_Aires',
        utcOffset: -180,
    },
    'America/Argentina/Salta': {
        tzName: 'America/Argentina/Salta',
        utcOffset: -180,
    },
    'America/Boa_Vista': {
        tzName: 'America/Boa_Vista',
        utcOffset: -240,
    },
    'America/Eirunepe': {
        tzName: 'America/Eirunepe',
        utcOffset: -300,
    },
    'America/Argentina/Rio_Gallegos': {
        tzName: 'America/Argentina/Rio_Gallegos',
        utcOffset: -180,
    },
    'America/Asuncion': {
        tzName: 'America/Asuncion',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#1 10 *',
        dstEndRule: '0 0 0 ? 1#4 3 *',
    },
    'America/Campo_Grande': {
        tzName: 'America/Campo_Grande',
        utcOffset: -240,
    },
    'America/Cayenne': {
        tzName: 'America/Cayenne',
        utcOffset: -180,
    },
    'America/Montevideo': {
        tzName: 'America/Montevideo',
        utcOffset: -180,
    },
    'America/Argentina/Mendoza': {
        tzName: 'America/Argentina/Mendoza',
        utcOffset: -180,
    },
    'Atlantic/Stanley': {
        tzName: 'Atlantic/Stanley',
        utcOffset: -180,
    },
    'America/Recife': {
        tzName: 'America/Recife',
        utcOffset: -180,
    },
    'America/Cuiaba': {
        tzName: 'America/Cuiaba',
        utcOffset: -240,
    },
    'America/Araguaina': {
        tzName: 'America/Araguaina',
        utcOffset: -180,
    },
    'America/Santarem': {
        tzName: 'America/Santarem',
        utcOffset: -180,
    },
    'Australia/ACT': {
        tzName: 'Australia/ACT',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/LHI': {
        tzName: 'Australia/LHI',
        utcOffset: 630,
        dstOffset: 30,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 2 ? 1#1 4 *',
    },
    'Australia/NSW': {
        tzName: 'Australia/NSW',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/North': {
        tzName: 'Australia/North',
        utcOffset: 570,
    },
    'Australia/Queensland': {
        tzName: 'Australia/Queensland',
        utcOffset: 600,
    },
    'Australia/South': {
        tzName: 'Australia/South',
        utcOffset: 570,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/Tasmania': {
        tzName: 'Australia/Tasmania',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/Victoria': {
        tzName: 'Australia/Victoria',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/West': {
        tzName: 'Australia/West',
        utcOffset: 480,
    },
    'Australia/Yancowinna': {
        tzName: 'Australia/Yancowinna',
        utcOffset: 570,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Brazil/Acre': {
        tzName: 'Brazil/Acre',
        utcOffset: -300,
    },
    'Brazil/DeNoronha': {
        tzName: 'Brazil/DeNoronha',
        utcOffset: -120,
    },
    'Brazil/East': {
        tzName: 'Brazil/East',
        utcOffset: -180,
    },
    'Brazil/West': {
        tzName: 'Brazil/West',
        utcOffset: -240,
    },
    CET: {
        tzName: 'CET',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    CST6CDT: {
        tzName: 'CST6CDT',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Atlantic': {
        tzName: 'Canada/Atlantic',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Central': {
        tzName: 'Canada/Central',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Eastern': {
        tzName: 'Canada/Eastern',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Mountain': {
        tzName: 'Canada/Mountain',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Newfoundland': {
        tzName: 'Canada/Newfoundland',
        utcOffset: -150,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Pacific': {
        tzName: 'Canada/Pacific',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Canada/Saskatchewan': {
        tzName: 'Canada/Saskatchewan',
        utcOffset: -360,
    },
    'Canada/Yukon': {
        tzName: 'Canada/Yukon',
        utcOffset: -420,
    },
    'Chile/Continental': {
        tzName: 'Chile/Continental',
        utcOffset: -240,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#1 9 *',
        dstEndRule: '0 0 0 ? 1#1 4 *',
    },
    'Chile/EasterIsland': {
        tzName: 'Chile/EasterIsland',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#1 9 *',
        dstEndRule: '0 0 0 ? 1#1 4 *',
    },
    Cuba: {
        tzName: 'Cuba',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 1#2 3 *',
        dstEndRule: '0 0 1 ? 1#1 11 *',
    },
    EET: {
        tzName: 'EET',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    EST: {
        tzName: 'EST',
        utcOffset: -300,
    },
    EST5EDT: {
        tzName: 'EST5EDT',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    Egypt: {
        tzName: 'Egypt',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 0 ? 6L 4 *',
        dstEndRule: '0 0 0 ? 5L 10 *',
    },
    Eire: {
        tzName: 'Eire',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    GB: {
        tzName: 'GB',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'GB-Eire': {
        tzName: 'GB-Eire',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    Hongkong: {
        tzName: 'Hongkong',
        utcOffset: 480,
    },
    Iceland: {
        tzName: 'Iceland',
        utcOffset: 0,
    },
    Iran: {
        tzName: 'Iran',
        utcOffset: 210,
    },
    Israel: {
        tzName: 'Israel',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 6LF 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    Jamaica: {
        tzName: 'Jamaica',
        utcOffset: -300,
    },
    Japan: {
        tzName: 'Japan',
        utcOffset: 540,
    },
    Kwajalein: {
        tzName: 'Kwajalein',
        utcOffset: 720,
    },
    Libya: {
        tzName: 'Libya',
        utcOffset: 120,
    },
    MET: {
        tzName: 'MET',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    MST: {
        tzName: 'MST',
        utcOffset: -420,
    },
    MST7MDT: {
        tzName: 'MST7MDT',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Mexico/BajaNorte': {
        tzName: 'Mexico/BajaNorte',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Mexico/BajaSur': {
        tzName: 'Mexico/BajaSur',
        utcOffset: -420,
    },
    'Mexico/General': {
        tzName: 'Mexico/General',
        utcOffset: -360,
    },
    NZ: {
        tzName: 'NZ',
        utcOffset: 720,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 9 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'NZ-CHAT': {
        tzName: 'NZ-CHAT',
        utcOffset: 765,
        dstOffset: 60,
        dstStartRule: '0 45 2 ? 1L 9 *',
        dstEndRule: '0 45 3 ? 1#1 4 *',
    },
    Navajo: {
        tzName: 'Navajo',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    PRC: {
        tzName: 'PRC',
        utcOffset: 480,
    },
    Poland: {
        tzName: 'Poland',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    Portugal: {
        tzName: 'Portugal',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    ROC: {
        tzName: 'ROC',
        utcOffset: 480,
    },
    ROK: {
        tzName: 'ROK',
        utcOffset: 540,
    },
    Singapore: {
        tzName: 'Singapore',
        utcOffset: 480,
    },
    Turkey: {
        tzName: 'Turkey',
        utcOffset: 180,
    },
    'US/Alaska': {
        tzName: 'US/Alaska',
        utcOffset: -540,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Aleutian': {
        tzName: 'US/Aleutian',
        utcOffset: -600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Arizona': {
        tzName: 'US/Arizona',
        utcOffset: -420,
    },
    'US/Central': {
        tzName: 'US/Central',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/East-Indiana': {
        tzName: 'US/East-Indiana',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Eastern': {
        tzName: 'US/Eastern',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Hawaii': {
        tzName: 'US/Hawaii',
        utcOffset: -600,
    },
    'US/Indiana-Starke': {
        tzName: 'US/Indiana-Starke',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Michigan': {
        tzName: 'US/Michigan',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Mountain': {
        tzName: 'US/Mountain',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Pacific': {
        tzName: 'US/Pacific',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'US/Samoa': {
        tzName: 'US/Samoa',
        utcOffset: -660,
    },
    'W-SU': {
        tzName: 'W-SU',
        utcOffset: 180,
    },
    'America/Buenos_Aires': {
        tzName: 'America/Buenos_Aires',
        utcOffset: -180,
    },
    'America/Catamarca': {
        tzName: 'America/Catamarca',
        utcOffset: -180,
    },
    'America/Cordoba': {
        tzName: 'America/Cordoba',
        utcOffset: -180,
    },
    'America/Indianapolis': {
        tzName: 'America/Indianapolis',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Jujuy': {
        tzName: 'America/Jujuy',
        utcOffset: -180,
    },
    'America/Knox_IN': {
        tzName: 'America/Knox_IN',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Louisville': {
        tzName: 'America/Louisville',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Mendoza': {
        tzName: 'America/Mendoza',
        utcOffset: -180,
    },
    'America/Virgin': {
        tzName: 'America/Virgin',
        utcOffset: -240,
    },
    'Pacific/Samoa': {
        tzName: 'Pacific/Samoa',
        utcOffset: -660,
    },
    'Africa/Accra': {
        tzName: 'Africa/Accra',
        utcOffset: 0,
    },
    'Africa/Addis_Ababa': {
        tzName: 'Africa/Addis_Ababa',
        utcOffset: 180,
    },
    'Africa/Asmara': {
        tzName: 'Africa/Asmara',
        utcOffset: 180,
    },
    'Africa/Bamako': {
        tzName: 'Africa/Bamako',
        utcOffset: 0,
    },
    'Africa/Bangui': {
        tzName: 'Africa/Bangui',
        utcOffset: 60,
    },
    'Africa/Banjul': {
        tzName: 'Africa/Banjul',
        utcOffset: 0,
    },
    'Africa/Blantyre': {
        tzName: 'Africa/Blantyre',
        utcOffset: 120,
    },
    'Africa/Brazzaville': {
        tzName: 'Africa/Brazzaville',
        utcOffset: 60,
    },
    'Africa/Bujumbura': {
        tzName: 'Africa/Bujumbura',
        utcOffset: 120,
    },
    'Africa/Conakry': {
        tzName: 'Africa/Conakry',
        utcOffset: 0,
    },
    'Africa/Dakar': {
        tzName: 'Africa/Dakar',
        utcOffset: 0,
    },
    'Africa/Dar_es_Salaam': {
        tzName: 'Africa/Dar_es_Salaam',
        utcOffset: 180,
    },
    'Africa/Djibouti': {
        tzName: 'Africa/Djibouti',
        utcOffset: 180,
    },
    'Africa/Douala': {
        tzName: 'Africa/Douala',
        utcOffset: 60,
    },
    'Africa/Freetown': {
        tzName: 'Africa/Freetown',
        utcOffset: 0,
    },
    'Africa/Gaborone': {
        tzName: 'Africa/Gaborone',
        utcOffset: 120,
    },
    'Africa/Harare': {
        tzName: 'Africa/Harare',
        utcOffset: 120,
    },
    'Africa/Kampala': {
        tzName: 'Africa/Kampala',
        utcOffset: 180,
    },
    'Africa/Kigali': {
        tzName: 'Africa/Kigali',
        utcOffset: 120,
    },
    'Africa/Kinshasa': {
        tzName: 'Africa/Kinshasa',
        utcOffset: 60,
    },
    'Africa/Libreville': {
        tzName: 'Africa/Libreville',
        utcOffset: 60,
    },
    'Africa/Lome': {
        tzName: 'Africa/Lome',
        utcOffset: 0,
    },
    'Africa/Luanda': {
        tzName: 'Africa/Luanda',
        utcOffset: 60,
    },
    'Africa/Lubumbashi': {
        tzName: 'Africa/Lubumbashi',
        utcOffset: 120,
    },
    'Africa/Lusaka': {
        tzName: 'Africa/Lusaka',
        utcOffset: 120,
    },
    'Africa/Malabo': {
        tzName: 'Africa/Malabo',
        utcOffset: 60,
    },
    'Africa/Maseru': {
        tzName: 'Africa/Maseru',
        utcOffset: 120,
    },
    'Africa/Mbabane': {
        tzName: 'Africa/Mbabane',
        utcOffset: 120,
    },
    'Africa/Mogadishu': {
        tzName: 'Africa/Mogadishu',
        utcOffset: 180,
    },
    'Africa/Niamey': {
        tzName: 'Africa/Niamey',
        utcOffset: 60,
    },
    'Africa/Nouakchott': {
        tzName: 'Africa/Nouakchott',
        utcOffset: 0,
    },
    'Africa/Ouagadougou': {
        tzName: 'Africa/Ouagadougou',
        utcOffset: 0,
    },
    'Africa/Porto-Novo': {
        tzName: 'Africa/Porto-Novo',
        utcOffset: 60,
    },
    'America/Anguilla': {
        tzName: 'America/Anguilla',
        utcOffset: -240,
    },
    'America/Antigua': {
        tzName: 'America/Antigua',
        utcOffset: -240,
    },
    'America/Aruba': {
        tzName: 'America/Aruba',
        utcOffset: -240,
    },
    'America/Atikokan': {
        tzName: 'America/Atikokan',
        utcOffset: -300,
    },
    'America/Blanc-Sablon': {
        tzName: 'America/Blanc-Sablon',
        utcOffset: -240,
    },
    'America/Cayman': {
        tzName: 'America/Cayman',
        utcOffset: -300,
    },
    'America/Creston': {
        tzName: 'America/Creston',
        utcOffset: -420,
    },
    'America/Curacao': {
        tzName: 'America/Curacao',
        utcOffset: -240,
    },
    'America/Dominica': {
        tzName: 'America/Dominica',
        utcOffset: -240,
    },
    'America/Grenada': {
        tzName: 'America/Grenada',
        utcOffset: -240,
    },
    'America/Guadeloupe': {
        tzName: 'America/Guadeloupe',
        utcOffset: -240,
    },
    'America/Kralendijk': {
        tzName: 'America/Kralendijk',
        utcOffset: -240,
    },
    'America/Lower_Princes': {
        tzName: 'America/Lower_Princes',
        utcOffset: -240,
    },
    'America/Marigot': {
        tzName: 'America/Marigot',
        utcOffset: -240,
    },
    'America/Montserrat': {
        tzName: 'America/Montserrat',
        utcOffset: -240,
    },
    'America/Nassau': {
        tzName: 'America/Nassau',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Port_of_Spain': {
        tzName: 'America/Port_of_Spain',
        utcOffset: -240,
    },
    'America/St_Barthelemy': {
        tzName: 'America/St_Barthelemy',
        utcOffset: -240,
    },
    'America/St_Kitts': {
        tzName: 'America/St_Kitts',
        utcOffset: -240,
    },
    'America/St_Lucia': {
        tzName: 'America/St_Lucia',
        utcOffset: -240,
    },
    'America/St_Thomas': {
        tzName: 'America/St_Thomas',
        utcOffset: -240,
    },
    'America/St_Vincent': {
        tzName: 'America/St_Vincent',
        utcOffset: -240,
    },
    'America/Tortola': {
        tzName: 'America/Tortola',
        utcOffset: -240,
    },
    'Antarctica/DumontDUrville': {
        tzName: 'Antarctica/DumontDUrville',
        utcOffset: 600,
    },
    'Antarctica/McMurdo': {
        tzName: 'Antarctica/McMurdo',
        utcOffset: 720,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 9 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Antarctica/Syowa': {
        tzName: 'Antarctica/Syowa',
        utcOffset: 180,
    },
    'Arctic/Longyearbyen': {
        tzName: 'Arctic/Longyearbyen',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Asia/Aden': {
        tzName: 'Asia/Aden',
        utcOffset: 180,
    },
    'Asia/Bahrain': {
        tzName: 'Asia/Bahrain',
        utcOffset: 180,
    },
    'Asia/Brunei': {
        tzName: 'Asia/Brunei',
        utcOffset: 480,
    },
    'Asia/Kuala_Lumpur': {
        tzName: 'Asia/Kuala_Lumpur',
        utcOffset: 480,
    },
    'Asia/Kuwait': {
        tzName: 'Asia/Kuwait',
        utcOffset: 180,
    },
    'Asia/Muscat': {
        tzName: 'Asia/Muscat',
        utcOffset: 240,
    },
    'Asia/Phnom_Penh': {
        tzName: 'Asia/Phnom_Penh',
        utcOffset: 420,
    },
    'Asia/Vientiane': {
        tzName: 'Asia/Vientiane',
        utcOffset: 420,
    },
    'Atlantic/Reykjavik': {
        tzName: 'Atlantic/Reykjavik',
        utcOffset: 0,
    },
    'Atlantic/St_Helena': {
        tzName: 'Atlantic/St_Helena',
        utcOffset: 0,
    },
    'Europe/Amsterdam': {
        tzName: 'Europe/Amsterdam',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Bratislava': {
        tzName: 'Europe/Bratislava',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Busingen': {
        tzName: 'Europe/Busingen',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Copenhagen': {
        tzName: 'Europe/Copenhagen',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Guernsey': {
        tzName: 'Europe/Guernsey',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Isle_of_Man': {
        tzName: 'Europe/Isle_of_Man',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Jersey': {
        tzName: 'Europe/Jersey',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Ljubljana': {
        tzName: 'Europe/Ljubljana',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Luxembourg': {
        tzName: 'Europe/Luxembourg',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Mariehamn': {
        tzName: 'Europe/Mariehamn',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Monaco': {
        tzName: 'Europe/Monaco',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Oslo': {
        tzName: 'Europe/Oslo',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Podgorica': {
        tzName: 'Europe/Podgorica',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/San_Marino': {
        tzName: 'Europe/San_Marino',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Sarajevo': {
        tzName: 'Europe/Sarajevo',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Skopje': {
        tzName: 'Europe/Skopje',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Stockholm': {
        tzName: 'Europe/Stockholm',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Vaduz': {
        tzName: 'Europe/Vaduz',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Vatican': {
        tzName: 'Europe/Vatican',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Zagreb': {
        tzName: 'Europe/Zagreb',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Indian/Antananarivo': {
        tzName: 'Indian/Antananarivo',
        utcOffset: 180,
    },
    'Indian/Christmas': {
        tzName: 'Indian/Christmas',
        utcOffset: 420,
    },
    'Indian/Cocos': {
        tzName: 'Indian/Cocos',
        utcOffset: 390,
    },
    'Indian/Comoro': {
        tzName: 'Indian/Comoro',
        utcOffset: 180,
    },
    'Indian/Kerguelen': {
        tzName: 'Indian/Kerguelen',
        utcOffset: 300,
    },
    'Indian/Mahe': {
        tzName: 'Indian/Mahe',
        utcOffset: 240,
    },
    'Indian/Mayotte': {
        tzName: 'Indian/Mayotte',
        utcOffset: 180,
    },
    'Indian/Reunion': {
        tzName: 'Indian/Reunion',
        utcOffset: 240,
    },
    'Pacific/Chuuk': {
        tzName: 'Pacific/Chuuk',
        utcOffset: 600,
    },
    'Pacific/Funafuti': {
        tzName: 'Pacific/Funafuti',
        utcOffset: 720,
    },
    'Pacific/Majuro': {
        tzName: 'Pacific/Majuro',
        utcOffset: 720,
    },
    'Pacific/Midway': {
        tzName: 'Pacific/Midway',
        utcOffset: -660,
    },
    'Pacific/Pohnpei': {
        tzName: 'Pacific/Pohnpei',
        utcOffset: 660,
    },
    'Pacific/Saipan': {
        tzName: 'Pacific/Saipan',
        utcOffset: 600,
    },
    'Pacific/Wake': {
        tzName: 'Pacific/Wake',
        utcOffset: 720,
    },
    'Pacific/Wallis': {
        tzName: 'Pacific/Wallis',
        utcOffset: 720,
    },
    'Africa/Timbuktu': {
        tzName: 'Africa/Timbuktu',
        utcOffset: 0,
    },
    'America/Argentina/ComodRivadavia': {
        tzName: 'America/Argentina/ComodRivadavia',
        utcOffset: -180,
    },
    'America/Atka': {
        tzName: 'America/Atka',
        utcOffset: -600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Coral_Harbour': {
        tzName: 'America/Coral_Harbour',
        utcOffset: -300,
    },
    'America/Ensenada': {
        tzName: 'America/Ensenada',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Fort_Wayne': {
        tzName: 'America/Fort_Wayne',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Montreal': {
        tzName: 'America/Montreal',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Nipigon': {
        tzName: 'America/Nipigon',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Pangnirtung': {
        tzName: 'America/Pangnirtung',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Porto_Acre': {
        tzName: 'America/Porto_Acre',
        utcOffset: -300,
    },
    'America/Rainy_River': {
        tzName: 'America/Rainy_River',
        utcOffset: -360,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Rosario': {
        tzName: 'America/Rosario',
        utcOffset: -180,
    },
    'America/Santa_Isabel': {
        tzName: 'America/Santa_Isabel',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Shiprock': {
        tzName: 'America/Shiprock',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Thunder_Bay': {
        tzName: 'America/Thunder_Bay',
        utcOffset: -300,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'America/Yellowknife': {
        tzName: 'America/Yellowknife',
        utcOffset: -420,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Antarctica/South_Pole': {
        tzName: 'Antarctica/South_Pole',
        utcOffset: 720,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 9 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Asia/Choibalsan': {
        tzName: 'Asia/Choibalsan',
        utcOffset: 480,
    },
    'Asia/Chongqing': {
        tzName: 'Asia/Chongqing',
        utcOffset: 480,
    },
    'Asia/Harbin': {
        tzName: 'Asia/Harbin',
        utcOffset: 480,
    },
    'Asia/Kashgar': {
        tzName: 'Asia/Kashgar',
        utcOffset: 360,
    },
    'Asia/Tel_Aviv': {
        tzName: 'Asia/Tel_Aviv',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 6LF 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Atlantic/Jan_Mayen': {
        tzName: 'Atlantic/Jan_Mayen',
        utcOffset: 60,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Australia/Canberra': {
        tzName: 'Australia/Canberra',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Australia/Currie': {
        tzName: 'Australia/Currie',
        utcOffset: 600,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#1 10 *',
        dstEndRule: '0 0 3 ? 1#1 4 *',
    },
    'Europe/Belfast': {
        tzName: 'Europe/Belfast',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Tiraspol': {
        tzName: 'Europe/Tiraspol',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1L 3 *',
        dstEndRule: '0 0 3 ? 1L 10 *',
    },
    'Europe/Uzhgorod': {
        tzName: 'Europe/Uzhgorod',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Zaporozhye': {
        tzName: 'Europe/Zaporozhye',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Pacific/Enderbury': {
        tzName: 'Pacific/Enderbury',
        utcOffset: 780,
    },
    'Pacific/Johnston': {
        tzName: 'Pacific/Johnston',
        utcOffset: -600,
    },
    'Pacific/Yap': {
        tzName: 'Pacific/Yap',
        utcOffset: 600,
    },
    WET: {
        tzName: 'WET',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Africa/Asmera': {
        tzName: 'Africa/Asmera',
        utcOffset: 180,
    },
    'America/Godthab': {
        tzName: 'America/Godthab',
        utcOffset: -120,
        dstOffset: 60,
        dstStartRule: '0 0 23 ? 1L 3 *',
        dstEndRule: '0 0 0 ? 1L 10 *',
    },
    'Asia/Ashkhabad': {
        tzName: 'Asia/Ashkhabad',
        utcOffset: 300,
    },
    'Asia/Calcutta': {
        tzName: 'Asia/Calcutta',
        utcOffset: 330,
    },
    'Asia/Chungking': {
        tzName: 'Asia/Chungking',
        utcOffset: 480,
    },
    'Asia/Dacca': {
        tzName: 'Asia/Dacca',
        utcOffset: 360,
    },
    'Asia/Istanbul': {
        tzName: 'Asia/Istanbul',
        utcOffset: 180,
    },
    'Asia/Katmandu': {
        tzName: 'Asia/Katmandu',
        utcOffset: 345,
    },
    'Asia/Macao': {
        tzName: 'Asia/Macao',
        utcOffset: 480,
    },
    'Asia/Rangoon': {
        tzName: 'Asia/Rangoon',
        utcOffset: 390,
    },
    'Asia/Saigon': {
        tzName: 'Asia/Saigon',
        utcOffset: 420,
    },
    'Asia/Thimbu': {
        tzName: 'Asia/Thimbu',
        utcOffset: 360,
    },
    'Asia/Ujung_Pandang': {
        tzName: 'Asia/Ujung_Pandang',
        utcOffset: 480,
    },
    'Asia/Ulan_Bator': {
        tzName: 'Asia/Ulan_Bator',
        utcOffset: 480,
    },
    'Atlantic/Faeroe': {
        tzName: 'Atlantic/Faeroe',
        utcOffset: 0,
        dstOffset: 60,
        dstStartRule: '0 0 1 ? 1L 3 *',
        dstEndRule: '0 0 2 ? 1L 10 *',
    },
    'Europe/Kiev': {
        tzName: 'Europe/Kiev',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    'Europe/Nicosia': {
        tzName: 'Europe/Nicosia',
        utcOffset: 120,
        dstOffset: 60,
        dstStartRule: '0 0 3 ? 1L 3 *',
        dstEndRule: '0 0 4 ? 1L 10 *',
    },
    HST: {
        tzName: 'HST',
        utcOffset: -600,
    },
    PST8PDT: {
        tzName: 'PST8PDT',
        utcOffset: -480,
        dstOffset: 60,
        dstStartRule: '0 0 2 ? 1#2 3 *',
        dstEndRule: '0 0 2 ? 1#1 11 *',
    },
    'Pacific/Ponape': {
        tzName: 'Pacific/Ponape',
        utcOffset: 660,
    },
    'Pacific/Truk': {
        tzName: 'Pacific/Truk',
        utcOffset: 600,
    },
};
