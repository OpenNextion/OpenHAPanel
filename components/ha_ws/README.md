# ha_ws

`ha_ws` 是一个用于 ESP-IDF 的 Home Assistant WebSocket API 客户端组件。它负责：

- 建立并维护到 Home Assistant `/api/websocket` 端点的连接
- 使用长期访问令牌完成认证
- 发送请求并按消息 ID 分发响应
- 订阅、取消订阅 Home Assistant 事件
- 在启用自动重连时恢复已有事件订阅
- 组装分片 WebSocket 消息，并按需启用合并消息功能
- 通过日志报告每个完整 JSON payload 的大小

## 要求

- ESP-IDF 5.x（当前工程使用 ESP-IDF 5.5.5）
- 可访问 Home Assistant 实例的网络连接
- Home Assistant 长期访问令牌
- `espressif/esp_websocket_client >= 1.0.0`

组件清单已在 `idf_component.yml` 中声明 `esp_websocket_client` 依赖。将整个
`ha_ws` 目录放入工程的 `components` 目录后，ESP-IDF Component Manager 会在配置或
构建工程时解析该依赖。

使用该组件的 component 可在 `CMakeLists.txt` 中声明依赖：

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ha_ws
)
```

## 快速开始

应用应先完成网络初始化，再创建并连接客户端。认证过程由组件自动完成；业务命令应
在收到 `HA_WS_EVENT_AUTHENTICATED` 后发送。

```c
#include "cJSON.h"
#include "esp_log.h"
#include "ha_ws.h"

static const char *TAG = "ha_example";
static ha_ws_client_t *s_client;

static void result_cb(ha_ws_client_t *client,
                      int id,
                      esp_err_t err,
                      const cJSON *result,
                      const cJSON *error,
                      void *user_ctx)
{
    (void)client;
    (void)user_ctx;

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Command %d failed: %s", id, esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Command %d completed", id);
    /* Read result here. It is only valid for the duration of this callback. */
}

static void connection_cb(ha_ws_client_t *client,
                          ha_ws_event_t event,
                          void *user_ctx)
{
    (void)user_ctx;

    switch (event) {
    case HA_WS_EVENT_AUTHENTICATED:
        ESP_LOGI(TAG, "Authenticated with Home Assistant %s",
                 ha_ws_get_ha_version(client));
        ha_ws_get_states(client, result_cb, NULL, NULL);
        break;
    case HA_WS_EVENT_AUTH_INVALID:
        ESP_LOGE(TAG, "Home Assistant rejected the access token");
        break;
    case HA_WS_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected");
        break;
    default:
        break;
    }
}

void start_home_assistant_client(void)
{
    const ha_ws_config_t config = {
        .hass_url = "http://homeassistant.local:8123",
        .access_token = "YOUR_LONG_LIVED_ACCESS_TOKEN",
        .auto_reconnect = true,
        .coalesce_messages = false,
        .connection_cb = connection_cb,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(ha_ws_client_init(&config, &s_client));
    ESP_ERROR_CHECK(ha_ws_connect(s_client));
}
```

`hass_url` 可使用 `http://`、`https://`、`ws://` 或 `wss://`。组件会将 HTTP(S)
地址转换为 WebSocket 地址，并在需要时追加 `/api/websocket`。

`coalesce_messages` 默认为关闭。设为 `true` 后，组件会在每次认证成功时向 Home
Assistant 声明支持 `coalesce_messages`，并解析服务端合并返回的 JSON 消息数组。

组件用于组装 HA WebSocket 分片消息的接收缓存会优先从 PSRAM 分配，失败时自动回退到内部
RAM。底层 `esp_websocket_client` 任务栈仍保留在内部 RAM，以维持 TLS 和网络任务的稳定性。

## 调用服务

`service_data` 和 `target` 可以为 `NULL`。组件会深拷贝传入的 JSON，因此函数返回后
调用方即可释放原始对象。

```c
cJSON *target = cJSON_CreateObject();
cJSON_AddStringToObject(target, "entity_id", "light.living_room");

esp_err_t err = ha_ws_call_service(s_client,
                                   "light",
                                   "turn_on",
                                   NULL,
                                   target,
                                   false,
                                   result_cb,
                                   NULL,
                                   NULL);
cJSON_Delete(target);
```

## 订阅事件

传入 `NULL` 作为 `event_type` 可订阅所有事件；指定事件类型则只接收对应事件。

```c
static int s_subscription_id;

static void event_cb(ha_ws_client_t *client,
                     int subscription_id,
                     const cJSON *event,
                     void *user_ctx)
{
    (void)client;
    (void)user_ctx;
    s_subscription_id = subscription_id;

    const cJSON *event_type =
        cJSON_GetObjectItemCaseSensitive(event, "event_type");
    if (cJSON_IsString(event_type)) {
        ESP_LOGI(TAG, "Event: %s", event_type->valuestring);
    }
}

esp_err_t err = ha_ws_subscribe_events(s_client,
                                       "state_changed",
                                       event_cb,
                                       result_cb,
                                       NULL,
                                       &s_subscription_id);
```

取消订阅：

```c
ha_ws_unsubscribe_events(s_client,
                         s_subscription_id,
                         result_cb,
                         NULL,
                         NULL);
```

启用自动重连时，组件会在重新认证后恢复已登记的订阅。重新订阅会产生新的消息 ID，
因此应用若需要稍后取消订阅，应保存 `event_cb` 参数中的最新 `subscription_id`。

## 自定义命令

对于未封装的 Home Assistant WebSocket 命令，可直接调用 `ha_ws_send_command()`。
调用方负责创建和释放消息对象；组件会在发送前向对象中加入 `id` 字段。

```c
cJSON *message = cJSON_CreateObject();
cJSON_AddStringToObject(message, "type", "config/entity_registry/list");

esp_err_t err = ha_ws_send_command(s_client,
                                   message,
                                   result_cb,
                                   NULL,
                                   NULL);
cJSON_Delete(message);
```

## API 概览

| API | 用途 |
| --- | --- |
| `ha_ws_client_init` | 创建客户端并复制配置中的 URL 和令牌 |
| `ha_ws_connect` | 启动 WebSocket 客户端 |
| `ha_ws_close` | 停止 WebSocket 客户端 |
| `ha_ws_destroy` | 释放客户端及其持有的资源 |
| `ha_ws_is_authenticated` | 查询当前认证状态 |
| `ha_ws_get_ha_version` | 获取最近一次认证返回的 Home Assistant 版本 |
| `ha_ws_send_command` | 发送自定义 WebSocket API 命令 |
| `ha_ws_get_states` | 获取全部实体状态 |
| `ha_ws_get_config` | 获取 Home Assistant 配置 |
| `ha_ws_get_services` | 获取可用服务 |
| `ha_ws_get_user` | 获取当前认证用户 |
| `ha_ws_ping` | 发送应用层 ping |
| `ha_ws_call_service` | 调用 Home Assistant 服务 |
| `ha_ws_subscribe_events` | 订阅事件 |
| `ha_ws_subscribe_entities` | 按实体 ID 订阅初始状态和压缩增量更新 |
| `ha_ws_unsubscribe_events` | 取消事件订阅 |

`ha_ws` 在组件内部维护一个进程级连接槽。同一时刻只允许一个客户端进入已启动状态；如果已有客户端占用连接槽，另一个客户端调用 `ha_ws_connect()` 时会等待，直到前者调用 `ha_ws_close()` 或 `ha_ws_destroy()` 释放连接槽。可以创建多个客户端实例，但不会同时建立多条 Home Assistant WebSocket 连接。

## 连接事件

`ha_ws_connection_cb_t` 可能收到以下事件：

| 事件 | 含义 |
| --- | --- |
| `HA_WS_EVENT_CONNECTED` | WebSocket 传输层已连接，尚未完成认证 |
| `HA_WS_EVENT_AUTH_REQUIRED` | Home Assistant 已请求认证，组件将自动发送令牌 |
| `HA_WS_EVENT_AUTHENTICATED` | 认证成功，可以发送业务命令 |
| `HA_WS_EVENT_AUTH_INVALID` | 令牌被拒绝，同时会停用自动重连 |
| `HA_WS_EVENT_DISCONNECTED` | WebSocket 已断开 |
| `HA_WS_EVENT_ERROR` | 底层 WebSocket 客户端报告错误 |

## 配置项

可通过 `idf.py menuconfig` 中的 **Home Assistant WebSocket SDK** 菜单配置：

| Kconfig 选项 | 默认值 | 说明 |
| --- | ---: | --- |
| `CONFIG_HA_WS_MAX_PENDING` | `8` | 同时等待响应的命令数量上限 |
| `CONFIG_HA_WS_MAX_SUBSCRIPTIONS` | `8` | 活跃事件订阅数量上限 |
| `CONFIG_HA_WS_RX_BUFFER_SIZE` | `262144` | 单条完整 WebSocket JSON 消息的最大字节数 |
| `CONFIG_HA_WS_TX_TIMEOUT_MS` | `5000` | WebSocket 文本发送超时时间，单位为毫秒 |

`get_states` 和 `get_services` 的响应可能较大。如果日志出现丢弃超大 payload 的提示，
应提高 `CONFIG_HA_WS_RX_BUFFER_SIZE`，同时确认设备有足够的可用内存。

## 接收数据大小日志

每收到一条完整的 Home Assistant WebSocket JSON payload，组件都会输出 INFO 日志：

```text
I (12345) ha_ws: Received HA JSON payload: 18432 bytes
```

这里记录的是 WebSocket payload 的原始 JSON 字节数，不包括结尾的 `\0`。如果底层
将一条消息分片交付，组件会在全部分片组装完成后只记录一次。启用
`coalesce_messages` 时，一个 payload 可能是包含多条 HA 协议消息的 JSON 数组，因此
日志显示的是整个数组的大小，而不是其中每个元素的大小。

## 返回值和数据生命周期

- API 返回 `ESP_OK` 表示命令已成功提交，不表示 Home Assistant 已成功执行命令；最终
  结果通过 `ha_ws_result_cb_t` 返回。
- Home Assistant 返回失败结果时，回调中的 `err` 为 `ESP_FAIL`，`error` 指向服务端的
  错误对象。
- `result`、`error` 和事件回调中的 `event` 均由组件管理，仅在当前回调执行期间有效。
  如需异步处理，应用必须使用 `cJSON_Duplicate(..., true)` 创建副本。
- 回调运行在 WebSocket 事件处理上下文中。避免在回调内长时间阻塞；需要执行耗时工作
  时，应复制所需数据并交给其他任务处理。
- `id_out`、`subscription_id_out` 和各类回调均可按 API 声明选择性传入 `NULL`。

组件自定义错误码包括：

| 错误码 | 含义 |
| --- | --- |
| `HA_WS_ERR_NOT_AUTHENTICATED` | 客户端尚未完成认证 |
| `HA_WS_ERR_NO_FREE_SLOT` | 待处理命令或订阅槽位已满 |
| `HA_WS_ERR_INVALID_RESPONSE` | 无效响应（当前版本保留） |
| `HA_WS_ERR_AUTH_INVALID` | 认证失败（当前版本保留） |

## 关闭与释放

不再使用客户端时，先停止连接，再释放资源：

```c
ESP_ERROR_CHECK(ha_ws_close(s_client));
ESP_ERROR_CHECK(ha_ws_destroy(s_client));
s_client = NULL;
```

完整工程用法可参考本仓库的 `examples/main.c`。
