/** match ipv4 address string */
export const REG_EXP_IPV4 = /^(25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)){3}$/;

/** match a valid domain name with at least two labels */
export const REG_EXP_DOMAIN = /^(?=.{1,253}$)(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+(?=[a-zA-Z0-9-]{1,63}$)(?=[a-zA-Z0-9-]*[a-zA-Z])[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$/;

/** match an integer TCP/UDP port from 1 to 65535 */
export const REG_EXP_PORT = /^(?:[1-9]\d{0,3}|[1-5]\d{4}|6[0-4]\d{3}|65[0-4]\d{2}|655[0-2]\d|6553[0-5])$/;

/** match a valid IPv4 subnet mask with contiguous one bits */
export const REG_EXP_IPV4_NETMASK = /^(?:(?:255\.){3}(?:255|254|252|248|240|224|192|128|0)|(?:255\.){2}(?:254|252|248|240|224|192|128|0)\.0|255\.(?:254|252|248|240|224|192|128|0)(?:\.0){2}|(?:254|252|248|240|224|192|128|0)(?:\.0){3})$/;

/** password must contain at least one letter and one number */
export const REG_EXP_PASSWORD_LETTER_AND_NUMBER = /^(?=.*[A-Za-z])(?=.*\d).*$/;

/** match the mdns domain name to the format required */
export const REG_EXP_MDNS_DOMAIN_FORMAT = /^[a-zA-Z0-9][a-zA-Z0-9-]{0,61}[a-zA-Z0-9]+\.local$/;

/** match Chinese Han characters */
export const REG_EXP_CHINESE_CHARACTERS = /\p{Script=Han}/gu;
