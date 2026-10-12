#include <esp_log.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>

#include "esp_timer.h"
#include "network.h"
#include "webserver.h"

#define TAG "webserver_client_node"
#define AUTH_FAILURE_LIMIT 5
#define AUTH_LOCKOUT_US (30LL * 1000LL * 1000LL)

SemaphoreHandle_t webserver_clients_mutex = NULL;
webserver_client_node_t *webserver_node_clients;
static int client_count = 0;

#define LOCK()                                                                                                         \
    do {                                                                                                               \
        if (xSemaphoreTake(webserver_clients_mutex, portMAX_DELAY) != pdTRUE) {                                        \
            ESP_LOGE(TAG, "Failed to take mutex");                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)
#define UNLOCK() xSemaphoreGive(webserver_clients_mutex)

/**
 * @brief Initialize the client list and create the mutex for thread safety.
 *
 * This function initializes the global client list pointer and client count,
 * and creates a mutex to protect the client list during concurrent access.
 * It must be called before any client management operations.
 * If the mutex already exists, an error message is logged and the function returns.
 *
 */
void webserver_client_list_init(void)
{
    webserver_node_clients = NULL;
    client_count = 0;

    if (webserver_clients_mutex != NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex already created!");
        return;
    }
    webserver_clients_mutex = xSemaphoreCreateMutex();
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create webserver_clients_mutex!");
    }
}

/**
 * @brief Add a new client node to the client list.
 *
 * Allocates memory for a new client node, initializes it with the given socket
 * and mode flag, then inserts it at the head of the linked list.
 * The operation is thread-safe and protected by a mutex.
 *
 * @param sock_fd Socket file descriptor for the client connection
 * @param is_ap Boolean flag indicating if client is connected in AP mode
 *
 * @return true if the client was successfully added, false if the list is full
 *         or memory allocation failed.
 */
bool webserver_client_add(int sock_fd, bool is_ap)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    if (client_count < MAX_OPEN_SOCKETS) {
        webserver_client_node_t *new_node = calloc(1, sizeof(webserver_client_node_t));
        if (!new_node)
            return false;

        new_node->sock_fd = sock_fd;
        new_node->is_ap = is_ap;
        new_node->auth = is_ap;
        new_node->auth_source = is_ap ? WEB_AUTH_SOURCE_AP : WEB_AUTH_SOURCE_NONE;
        LOCK();
        new_node->next = webserver_node_clients;
        webserver_node_clients = new_node;
        client_count++;
        UNLOCK();

        ESP_LOGI(TAG, "Client added: fd=%d, count=%d", sock_fd, client_count);
        network_ap_client_connection_changed();
        return true;
    }
    return false;
}

/**
 * @brief Remove a client node from the list by its socket file descriptor.
 *
 * Searches the linked list for a client node matching the given socket fd.
 * If found, removes it from the list, frees the memory, and decrements the count.
 * The operation is thread-safe via mutex protection.
 *
 * If not found, closes the socket fd and returns false.
 *
 * @param sock_fd Socket file descriptor identifying the client to remove.
 *
 * @return true if the client was found and removed; false otherwise.
 */
bool webserver_client_remove(int sock_fd)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    LOCK();
    webserver_client_node_t **ptr = &webserver_node_clients;
    while (*ptr) {
        if ((*ptr)->sock_fd == sock_fd) {
            webserver_client_node_t *temp = *ptr;
            *ptr = (*ptr)->next;
            free(temp);
            client_count--;
            UNLOCK();

            ESP_LOGI(TAG, "Client removed: fd=%d, count=%d", sock_fd, client_count);
            network_ap_client_connection_changed();
            return true;
        }
        ptr = &((*ptr)->next);
    }
    UNLOCK();
    return false;
}

/**
 * @brief Find and return a pointer to a client node by socket fd.
 *
 * Searches the client list for a node with the given socket fd.
 * Returns a direct pointer to the node if found; otherwise returns NULL.
 *
 * @note The returned pointer is to the internal list node. The caller should not
 *       hold or modify it for long periods without proper synchronization.
 *
 * @param sock_fd Socket file descriptor to search for.
 *
 * @return Pointer to the client node if found, NULL if not found or error.
 */
webserver_client_node_t *webserver_client_find(int sock_fd)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return NULL;
    }
    LOCK();
    webserver_client_node_t *current = webserver_node_clients;
    while (current) {
        if (current->sock_fd == sock_fd) {
            UNLOCK();
            return current;
        }
        current = current->next;
    }
    UNLOCK();
    return NULL;
}

/**
 * @brief Find a client node by socket fd and copy its data into provided struct.
 *
 * Similar to webserver_client_find(), but instead of returning a pointer,
 * copies the client's data into the user-supplied struct for safe access.
 *
 * @param sock_fd Socket file descriptor to find.
 * @param out Pointer to a webserver_client_node_t struct to receive the copied data.
 *
 * @return true if found and copied successfully, false if not found or on error.
 */
bool webserver_client_find_copy(int sock_fd, webserver_client_node_t *out)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    if (!out)
        return false;
    LOCK();
    webserver_client_node_t *current = webserver_node_clients;
    while (current) {
        if (current->sock_fd == sock_fd) {
            memcpy(out, current, sizeof(webserver_client_node_t));
            UNLOCK();
            return true;
        }
        current = current->next;
    }
    UNLOCK();
    return false;
}

/**
 * @brief Update the 'nonce' field of the client identified by socket fd.
 *
 * Searches for the client node matching the given sock_fd, then copies the
 * provided nonce string into the node's fixed-size nonce field.
 * The string is null-terminated to avoid buffer overflows.
 *
 * @param sock_fd Socket file descriptor identifying the client to update.
 * @param nonce Null-terminated string to copy into the nonce field.
 *
 * @return true if the client was found and updated, false otherwise.
 */
bool webserver_client_update_nonce(int sock_fd, const char *nonce)
{
    if (webserver_clients_mutex == NULL || nonce == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    LOCK();
    webserver_client_node_t *node = webserver_node_clients;
    while (node) {
        if (node->sock_fd == sock_fd) {
            strlcpy(node->nonce, nonce, sizeof(node->nonce));
            UNLOCK();
            ESP_LOGI(TAG, "Client updated: fd=%d", sock_fd);
            return true;
        }
        node = node->next;
    }
    UNLOCK();
    return false;
}

/**
 * @brief Update the 'auth' field of the client identified by socket fd.
 *
 * Searches for the client node matching the given sock_fd, then copies the
 * provided auth bool into the node's auth field.
 *
 * @param sock_fd Socket file descriptor identifying the client to update.
 * @param auth auth bool.
 *
 * @return true if the client was found and updated, false otherwise.
 */
bool webserver_client_update_auth(int sock_fd, bool auth)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    LOCK();
    webserver_client_node_t *node = webserver_node_clients;
    while (node) {
        if (node->sock_fd == sock_fd) {
            node->auth = auth;
            UNLOCK();
            ESP_LOGI(TAG, "Client updated: fd=%d", sock_fd);
            return true;
        }
        node = node->next;
    }
    UNLOCK();
    return false;
}

bool webserver_client_set_auth_context(int sock_fd,
                                       bool auth,
                                       web_auth_source_t source,
                                       const char *session_id)
{
    if (webserver_clients_mutex == NULL) return false;
    LOCK();
    webserver_client_node_t *node = webserver_node_clients;
    while (node) {
        if (node->sock_fd == sock_fd) {
            node->auth = auth;
            node->auth_source = auth ? source : WEB_AUTH_SOURCE_NONE;
            node->session_id[0] = '\0';
            if (auth && session_id) {
                strlcpy(node->session_id, session_id, sizeof(node->session_id));
            }
            if (auth) {
                node->auth_failures = 0;
                node->auth_locked_until_us = 0;
            }
            UNLOCK();
            return true;
        }
        node = node->next;
    }
    UNLOCK();
    return false;
}

bool webserver_client_auth_attempt_allowed(int sock_fd)
{
    if (webserver_clients_mutex == NULL) return false;
    int64_t now = esp_timer_get_time();
    LOCK();
    webserver_client_node_t *node = webserver_node_clients;
    while (node) {
        if (node->sock_fd == sock_fd) {
            if (node->auth_locked_until_us > 0 && now >= node->auth_locked_until_us) {
                node->auth_failures = 0;
                node->auth_locked_until_us = 0;
            }
            bool allowed = node->auth_locked_until_us == 0;
            UNLOCK();
            return allowed;
        }
        node = node->next;
    }
    UNLOCK();
    return false;
}

void webserver_client_record_auth_failure(int sock_fd)
{
    if (webserver_clients_mutex == NULL) return;
    if (xSemaphoreTake(webserver_clients_mutex, portMAX_DELAY) != pdTRUE) return;
    for (webserver_client_node_t *node = webserver_node_clients; node; node = node->next) {
        if (node->sock_fd == sock_fd) {
            if (node->auth_failures < UINT8_MAX) node->auth_failures++;
            if (node->auth_failures >= AUTH_FAILURE_LIMIT) {
                node->auth_locked_until_us = esp_timer_get_time() + AUTH_LOCKOUT_US;
            }
            break;
        }
    }
    xSemaphoreGive(webserver_clients_mutex);
}

void webserver_client_reset_auth_failures(int sock_fd)
{
    if (webserver_clients_mutex == NULL) return;
    if (xSemaphoreTake(webserver_clients_mutex, portMAX_DELAY) != pdTRUE) return;
    for (webserver_client_node_t *node = webserver_node_clients; node; node = node->next) {
        if (node->sock_fd == sock_fd) {
            node->auth_failures = 0;
            node->auth_locked_until_us = 0;
            break;
        }
    }
    xSemaphoreGive(webserver_clients_mutex);
}

/**
 * @brief Check whether a client socket is authenticated.
 *
 * Searches the linked list of webserver clients for the given socket descriptor
 * and returns its authentication status. Thread-safe access is ensured using
 * a mutex.
 *
 * @param sock_fd Socket file descriptor of the client to check.
 * @return true if the client is authenticated, false otherwise.
 */
bool webserver_client_get_auth(int sock_fd)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    LOCK();
    webserver_client_node_t *current = webserver_node_clients;
    while (current) {
        if (current->sock_fd == sock_fd) {
            UNLOCK();
            return current->auth;
        }
        current = current->next;
    }
    UNLOCK();
    return false;
}

/**
 * @brief Clear all webserver_node_clients from the list and free associated memory.
 *
 * Iterates over all client nodes, frees memory, resets client count and list pointer.
 * The operation is thread-safe via mutex protection.
 *
 * @return true on success, false if mutex not initialized.
 */
bool webserver_client_list_clear(void)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return false;
    }
    LOCK();
    webserver_client_node_t *current = webserver_node_clients;
    while (current) {
        webserver_client_node_t *temp = current;
        current = current->next;
        free(temp);
    }
    webserver_node_clients = NULL;
    client_count = 0;
    UNLOCK();

    ESP_LOGI(TAG, "All webserver_node_clients cleared");
    network_ap_client_connection_changed();
    return true;
}

/**
 * @brief Retrieve the current number of connected webserver_node_clients.
 *
 * Safely reads the client_count variable protected by mutex.
 *
 * @return Number of connected webserver_node_clients, or -1 if mutex not initialized.
 */
int webserver_client_get_count(void)
{
    if (webserver_clients_mutex == NULL) {
        ESP_LOGE(TAG, "webserver_clients_mutex not created!");
        return -1;
    }
    LOCK();
    int count = client_count;
    UNLOCK();
    return count;
}

int webserver_client_get_ap_count(void)
{
    if (webserver_clients_mutex == NULL) {
        return -1;
    }
    LOCK();
    int count = 0;
    for (webserver_client_node_t *current = webserver_node_clients; current; current = current->next) {
        if (current->is_ap) {
            count++;
        }
    }
    UNLOCK();
    return count;
}

void webserver_force_free_all_clients(void)
{
    if (webserver_clients_mutex == NULL) {
        return;
    }

    webserver_client_node_t *list = NULL;

    if (xSemaphoreTake(webserver_clients_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        list = webserver_node_clients;
        webserver_node_clients = NULL;
        client_count = 0;
        xSemaphoreGive(webserver_clients_mutex);
    }

    while (list) {
        webserver_client_node_t *next = list->next;

        if (list->sock_fd > 0) {
            shutdown(list->sock_fd, SHUT_RDWR);
        }
        free(list);
        list = next;
    }

    network_ap_client_connection_changed();
}
