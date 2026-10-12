/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "esp_netif.h"
#include "esp_log.h"
#include <inttypes.h>
#include "esp_system.h"
#include "esp_check.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "network_captive_portal.h"

static const char *TAG = "network_captive_portal";

/** @brief DNS service standard port number */
#define DNS_PORT (53)

/** @brief Maximum length of a DNS packet */
#define DNS_MAX_LEN (256)

/** @brief Mask to extract the OPCODE field from DNS flags */
#define OPCODE_MASK (0x7800)

/** @brief Flag indicating a DNS response packet */
#define QR_FLAG (1 << 7)

/** @brief DNS query type A (host address) */
#define QD_TYPE_A (0x0001)

/** @brief Time-to-live for DNS answer in seconds */
#define ANS_TTL_SEC (300)

/** @brief Handle for the DNS server instance */
static dns_server_handle_t dns_server_handle = NULL;

/**
 * @brief DNS packet header structure (packed)
 */
typedef struct __attribute__((__packed__))
{
    /** @brief Identification field */
    uint16_t id;
    /** @brief Flags and codes */
    uint16_t flags;
    /** @brief Number of questions */
    uint16_t qd_count;
    /** @brief Number of answers */
    uint16_t an_count;
    /** @brief Number of authority records */
    uint16_t ns_count;
    /** @brief Number of additional records */
    uint16_t ar_count;
} dns_header_t;

/**
 * @brief DNS question section structure
 */
typedef struct
{
    /** @brief Query type (e.g., A record) */
    uint16_t type;
    /** @brief Query class (usually IN for internet) */
    uint16_t class;
} dns_question_t;

/**
 * @brief DNS answer section structure (packed)
 */
typedef struct __attribute__((__packed__))
{
    /** @brief Pointer offset to domain name */
    uint16_t ptr_offset;
    /** @brief Answer type */
    uint16_t type;
    /** @brief Answer class */
    uint16_t class;
    /** @brief Time-to-live value in seconds */
    uint32_t ttl;
    /** @brief Length of the address */
    uint16_t addr_len;
    /** @brief IP address in network byte order */
    uint32_t ip_addr;
} dns_answer_t;

/**
 * @brief DNS server handle structure
 */
struct dns_server_handle
{
    /** @brief Indicates if the DNS server is started */
    bool started;
    /** @brief Task handle of the DNS server task */
    TaskHandle_t task;
    /** @brief Number of DNS entries */
    int num_of_entries;
    /** @brief Array of DNS entry pairs (hostname-IP mapping) */
    dns_entry_pair_t entry[];
};

/**
 * @brief Parses a raw DNS encoded name into a human-readable string.
 *
 * This function decodes a DNS name from its raw label format (length-prefixed labels)
 * into a dot-separated string. For example, the raw bytes "\3www\6google\3com\0" will
 * be parsed into "www.google.com".
 *
 * @param raw_name Pointer to the raw DNS encoded name buffer.
 * @param parsed_name Buffer where the parsed name string will be stored.
 * @param parsed_name_max_len Maximum length of the parsed_name buffer to avoid overflow.
 *
 * @return Pointer to the byte following the parsed name in the raw buffer on success,
 *         or NULL if the parsed name length exceeds parsed_name_max_len.
 */
static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len)
{
    char *label = raw_name;
    char *name_itr = parsed_name;
    int name_len = 0;
    do {
        int sub_name_len = *label;
        // (len + 1) since we are adding  a '.'
        name_len += (sub_name_len + 1);
        if (name_len > parsed_name_max_len) {
            return NULL;
        }

        // Copy the sub name that follows the the label
        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    } while (*label != 0);

    // Terminate the final string, replacing the last '.'
    parsed_name[name_len - 1] = '\0';
    // Return pointer to first char after the name
    return label + 1;
}

/**
 * @brief Parses a DNS request packet and prepares a DNS reply based on configured DNS entries.
 *
 * This function processes the incoming DNS request in raw format, validates it,
 * extracts queries, and constructs a DNS response. It supports answering A-type
 * queries (IPv4 addresses) based on a list of DNS entries configured in the server handle.
 *
 * @param req Pointer to the raw DNS request buffer.
 * @param req_len Length of the DNS request buffer.
 * @param dns_reply Buffer where the DNS reply will be constructed.
 * @param dns_reply_max_len Maximum size of the dns_reply buffer.
 * @param h DNS server handle containing configured DNS entries to match queries against.
 *
 * @return Length of the constructed DNS reply on success,
 *         0 if the query is not a standard query (no reply generated),
 *         or -1 on error (e.g., buffer overflow, parsing failure).
 */
static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len,
                             dns_server_handle_t h)
{
    if (req_len > dns_reply_max_len) {
        return -1;
    }

    // Prepare the reply
    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    // Endianess of NW packet different from chip
    dns_header_t *header = (dns_header_t *)dns_reply;

    // Not a standard query
    if ((header->flags & OPCODE_MASK) != 0) {
        return 0;
    }

    // Set question response flag
    header->flags |= QR_FLAG;

    uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len) {
        return -1;
    }

    // Pointer to current answer and question
    char *cur_ans_ptr = dns_reply + req_len;
    char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    // Respond to all questions based on configured rules
    for (int qd_i = 0; qd_i < qd_count; qd_i++) {
        char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL) {
            ESP_LOGE(TAG, "Failed to parse DNS question: %s", cur_qd_ptr);
            return -1;
        }

        dns_question_t *question = (dns_question_t *)(name_end_ptr);
        uint16_t qd_type = ntohs(question->type);
        uint16_t qd_class = ntohs(question->class);

        if (qd_type == QD_TYPE_A) {
            esp_ip4_addr_t ip = {.addr = IPADDR_ANY};
            // Check the configured rules to decide whether to answer this question or not
            for (int i = 0; i < h->num_of_entries; ++i) {
                // check if the name either corresponds to the entry, or if we should answer to all queries ("*")
                if (strcmp(h->entry[i].name, "*") == 0 || strcmp(h->entry[i].name, name) == 0) {
                    if (h->entry[i].if_key) {
                        esp_netif_ip_info_t ip_info;
                        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey(h->entry[i].if_key), &ip_info);
                        ip.addr = ip_info.ip.addr;
                        break;
                    }
                    else if (h->entry->ip.addr != IPADDR_ANY) {
                        ip.addr = h->entry[i].ip.addr;
                        break;
                    }
                }
            }
            if (ip.addr == IPADDR_ANY) {
                // no rule applies, continue with another question
                continue;
            }
            dns_answer_t *answer = (dns_answer_t *)cur_ans_ptr;

            answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = htons(qd_class);
            answer->ttl = htonl(ANS_TTL_SEC);

            answer->addr_len = htons(sizeof(ip.addr));
            answer->ip_addr = ip.addr;
        }
    }
    return reply_len;
}

/**
 * @brief Task function that runs the DNS server.
 *
 * This function creates a UDP socket bound to the DNS port (53) and listens for incoming DNS queries.
 * For each received DNS request, it processes the query, generates a DNS response using the configured
 * DNS entries, and sends the reply back to the requester.
 *
 * The task runs in a loop until the DNS server handle's started flag is cleared.
 * Handles both IPv4 and IPv6 client addresses.
 *
 * @param pvParameters Pointer to the DNS server handle (dns_server_handle_t).
 */
static void dns_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    dns_server_handle_t handle = pvParameters;

    while (handle->started) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(DNS_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket bound, port %d", DNS_PORT);

        while (handle->started) {
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                close(sock);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                }
                else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                // Null-terminate whatever we received and treat like a string...
                rx_buffer[len] = 0;
                char reply[DNS_MAX_LEN];
                int reply_len = parse_dns_request(rx_buffer, len, reply, DNS_MAX_LEN, handle);

                if (reply_len <= 0) {
                    ESP_LOGE(TAG, "Failed to prepare a DNS reply");
                }
                else {
                    int err = sendto(sock, reply, reply_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                        break;
                    }
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Set ups and starts a simple DNS server that will respond to all A queries (IPv4)
 * based on configured rules, pairs of name and either IPv4 address or a netif ID (to respond by it's IPv4 add)
 *
 * @param config Configuration structure listing the pairs of (name, IP/netif-id)
 * @return dns_server's handle on success, NULL on failure
 */
static dns_server_handle_t start_dns_server(dns_server_config_t *config)
{
    dns_server_handle_t handle =
        calloc(1, sizeof(struct dns_server_handle) + config->num_of_entries * sizeof(dns_entry_pair_t));
    ESP_RETURN_ON_FALSE(handle, NULL, TAG, "Failed to allocate dns server handle");

    handle->started = true;
    handle->num_of_entries = config->num_of_entries;
    memcpy(handle->entry, config->item, config->num_of_entries * sizeof(dns_entry_pair_t));

    xTaskCreatePinnedToCore(dns_server_task, "dns_server", 3 * 1024, handle, 5, &handle->task, 1);
    return handle;
}

/**
 * @brief Initialize the captive portal DNS server.
 *
 * This function configures and starts a DNS server that responds to all DNS A queries
 * with a wildcard entry ("*"), effectively redirecting all DNS requests to the device
 * running the WiFi Access Point identified by "WIFI_AP_DEF".
 *
 * Typically used to implement captive portal behavior on the softAP interface.
 */
void captive_portal_init()
{
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    dns_server_handle = start_dns_server(&config);
}