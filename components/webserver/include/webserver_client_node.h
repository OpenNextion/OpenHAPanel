#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_CLIENT_NODE_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_CLIENT_NODE_H

#include <stdbool.h>
#include <stdint.h>

#define WEB_AUTH_NONCE_HEX_LEN 64
#define WEB_AUTH_SESSION_ID_HEX_LEN 32

typedef enum {
    WEB_AUTH_SOURCE_NONE = 0,
    WEB_AUTH_SOURCE_AP,
    WEB_AUTH_SOURCE_PASSWORD,
    WEB_AUTH_SOURCE_SESSION,
    WEB_AUTH_SOURCE_PROVISIONING,
} web_auth_source_t;

typedef struct client_node
{
    int sock_fd;
    bool is_ap;
    bool auth;
    web_auth_source_t auth_source;
    char nonce[WEB_AUTH_NONCE_HEX_LEN + 1];
    char session_id[WEB_AUTH_SESSION_ID_HEX_LEN + 1];
    uint8_t auth_failures;
    int64_t auth_locked_until_us;
    struct client_node *next;
} webserver_client_node_t;

/**
 * @brief Initialize the client list and create mutex for thread safety.
 *
 * Must be called before any other client list operations.
 */
void webserver_client_list_init(void);

/**
 * @brief Add a new client node to the client list.
 *
 * @param sock_fd Socket file descriptor for the client connection.
 * @param is_ap Indicates whether the client is connected in AP mode.
 * @return true if the client was added successfully, false otherwise.
 */
bool webserver_client_add(int sock_fd, bool is_ap);

/**
 * @brief Remove a client node from the list by socket file descriptor.
 *
 * @param sock_fd Socket file descriptor identifying the client to remove.
 * @return true if the client was found and removed; false otherwise.
 */
bool webserver_client_remove(int sock_fd);

/**
 * @brief Find a client node by socket file descriptor.
 *
 * @param sock_fd Socket file descriptor to search for.
 * @return Pointer to the client node if found, NULL otherwise.
 * @note The returned pointer is internal to the list; use with caution.
 */
webserver_client_node_t *webserver_client_find(int sock_fd);

/**
 * @brief Find a client node by socket fd and copy its data to user-provided struct.
 *
 * @param sock_fd Socket file descriptor to find.
 * @param out Pointer to struct to receive client data copy.
 * @return true if found and copied; false otherwise.
 */
bool webserver_client_find_copy(int sock_fd, webserver_client_node_t *out);

/**
 * @brief Update the nonce field of a client node identified by socket fd.
 *
 * @param sock_fd Socket file descriptor identifying the client.
 * @param nonce Null-terminated string to copy to the client's nonce field.
 * @return true if updated successfully; false otherwise.
 */
bool webserver_client_update_nonce(int sock_fd, const char *nonce);

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
bool webserver_client_update_auth(int sock_fd, bool auth);

bool webserver_client_set_auth_context(int sock_fd,
                                       bool auth,
                                       web_auth_source_t source,
                                       const char *session_id);

bool webserver_client_auth_attempt_allowed(int sock_fd);
void webserver_client_record_auth_failure(int sock_fd);
void webserver_client_reset_auth_failures(int sock_fd);

/**
 * @brief Check whether a client socket is authenticated.
 *
 * This function searches the linked list of connected webserver clients
 * and checks if the client associated with the given socket file descriptor
 * has been authenticated.
 *
 * The function acquires a mutex to ensure thread-safe access to the client list.
 *
 * @param sock_fd The socket file descriptor of the client.
 * @return true if the client is authenticated, false otherwise.
 */
bool webserver_client_get_auth(int sock_fd);

/**
 * @brief Clear all webserver_node_clients from the list and free allocated memory.
 *
 * @return true on success; false if mutex is not initialized.
 */
bool webserver_client_list_clear(void);

/**
 * @brief Get the current number of connected webserver_node_clients.
 *
 * @return Number of webserver_node_clients; -1 if mutex is not initialized.
 */
int webserver_client_get_count(void);

/** Return the number of active WebSocket clients connected through the AP subnet. */
int webserver_client_get_ap_count(void);

void webserver_force_free_all_clients(void);

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_CLIENT_NODE_H

#ifdef __cplusplus
}
#endif
