# ha_entity_store

`ha_entity_store` 是设备端的 Home Assistant Entity 状态数据层。它位于底层
WebSocket 通信组件和 UI/业务组件之间，为设备当前关心的实体维护一份线程安全的本地状态。

组件的核心原则是：

- WebSocket 只负责连接和 Home Assistant 协议通信。
- Entity Store 是实体状态在设备上的唯一事实来源。
- UI 只查询 Store、监听变更，不直接解析 WebSocket JSON。
- 状态获取与服务调用分离；控制命令由单独的 service client 负责。

## 组件边界

本组件负责：

- 注册和释放设备当前使用的 `entity_id`。
- 获取已注册实体的初始状态。
- 接收并处理 `subscribe_entities` 压缩增量事件。
- 缓存实体最新的 `state`、`attributes` 和时间信息。
- 检测有效变化，并通知 UI 或其他业务组件。
- 在断线时保留最后状态并标记为过期。
- 在重连后恢复按需订阅并接收新的初始快照。
- 支持调用方按需重建订阅并刷新状态。
- 提供线程安全的状态和属性查询接口。

本组件不负责：

- 创建、连接或销毁 Home Assistant WebSocket 客户端。
- Wi-Fi 管理、HA 登录授权和 Token 持久化。
- 直接操作 LVGL 控件。
- 调用 Home Assistant service。
- 将实时实体状态持久化到 Flash。

## 公共头文件

- `ha_entity_store.h`：只声明组件对外提供的操作接口。
- `ha_entity_store_type.h`：集中定义公共常量、opaque handle、枚举、配置、快照、事件和回调类型。

通常调用方只需包含 `ha_entity_store.h`，它会自动包含类型头文件。只有单独依赖数据类型、
不调用 Store API 的代码，才需要直接包含 `ha_entity_store_type.h`。

## 总体数据流

```text
Home Assistant
      │
      │ WebSocket API
      ▼
  WebSocket transport
      │ 原始结果和事件
      ▼
 Entity Store 消息队列
      │
      ▼
 Entity Store worker
      ├── 解析消息
      ├── 更新状态缓存
      ├── 检测字段变化
      └── 发布轻量变更通知
      │
      ▼
 UI / 业务组件
```

WebSocket 回调只复制或转换必要的数据并投入队列，不直接更新缓存、调用 LVGL 或执行业务逻辑。
所有缓存写操作由 Store worker 串行完成，以简化并发控制并保持消息顺序。

## 实例模型

`ha_entity_store` 采用**多例 API**，不是进程级单例。实例通过
`ha_entity_store_create()` 创建，并由调用方显式持有 `ha_entity_store_t *`。

这样设计便于：

- 注入 fake transport 进行单元测试。
- 明确资源所有权和生命周期。
- 将来连接多个 Home Assistant 实例。
- 避免全局变量造成的测试和初始化顺序问题。

当前产品只有一个 Home Assistant 连接时，应用层通常只创建一个 Store。建议一个 transport 或
一个 `ha_ws_client_t` 同时只绑定一个活动 Store，否则多个 Store 会建立重复的
`subscribe_entities` 订阅并重复占用内存。

配置中的 `prefer_external_memory=true` 会优先把 Store worker 栈和固定实体表分配到 PSRAM，
降低持续运行的 HA 数据层对内部 RAM 的占用。如果 PSRAM 分配失败，组件会自动回退到内部
RAM，API 和同步行为保持不变。PSRAM worker 通过 `xTaskCreateWithCaps()` 创建，退出时必须使用
与其匹配的 `vTaskDeleteWithCaps()`；普通内部 RAM worker 仍使用 `vTaskDelete()`。

## 生命周期

组件生命周期分为四个操作：

```text
create → start → stop → destroy
           ▲       │
           └───────┘
```

### `ha_entity_store_create()`

创建实例并保存配置，初始化互斥锁、缓存容器和必要的同步对象，但不连接 HA、不订阅事件，
也不启动数据同步。

创建后的状态为 `HA_ENTITY_STORE_STOPPED`。

### `ha_entity_store_start()`

启动 Store 的运行期功能：

1. 启动 Store worker task，并允许 transport 回调向内部队列投递消息。
2. 注册或启用 transport 的连接状态和消息监听。
3. 如果 HA 尚未认证，进入 `OFFLINE` 并等待认证事件。
4. 如果 HA 已经认证，开始启动同步流程：
   - 使用 `subscribe_entities` 订阅当前已经注册的实体；
   - 等待订阅成功及其初始状态快照；
   - 合并后续压缩状态增量；
   - 同步完成后进入 `READY`。

`start()` 不创建或连接 WebSocket，也不阻塞等待 HA 上线。它只保证 Store 已经具备接收事件和
执行同步的能力。重复调用 `start()` 是幂等操作，已经启动时返回 `ESP_OK`。

调用成功只代表 worker 已启动，不代表实体数据已经可用；调用方应检查 Store 状态或监听
`READY` 事件。

### `ha_entity_store_stop()`

停止 Store 的运行期功能：

1. 禁止新的 transport 消息进入队列。
2. 从 transport 注销监听，避免停止后仍收到回调。
3. 在连接可用时取消 `subscribe_entities` 订阅。
4. 通知 worker 退出并等待其结束。
5. 清理尚未处理的临时消息。
6. 将 Store 状态切换为 `STOPPED`。

`stop()` 不销毁 Store，不释放调用方已经 acquire 的实体，也不销毁外部 WebSocket 客户端。
现有缓存可以继续用于显示最后状态，但其同步状态会被标记为 `STALE`。之后再次调用
`start()` 会保留实体注册关系，并重新订阅和全量同步。

`stop()` 是同步操作：返回后保证不会再触发该 Store 的业务监听回调。重复调用 `stop()` 是
幂等操作，已经停止时返回 `ESP_OK`。

需要注意：如果 `stop()` 是从 Store 自己的 worker 回调中调用，实现不能直接等待当前 worker
退出。第一版可以明确禁止这种调用并返回 `ESP_ERR_INVALID_STATE`；业务层应把停止请求转交给
应用任务执行。

### `ha_entity_store_destroy()`

销毁实例。如果 Store 仍在运行，内部先执行等价于 `stop()` 的收尾工作，然后释放队列、锁、
实体缓存和实例内存。

销毁后所有 Store 指针和 entity handle 都不再有效。调用方应先停止使用并释放 entity handle，
再销毁 Store。如果仍有 entity handle 或 listener handle 未释放，`destroy()` 返回
`ESP_ERR_INVALID_STATE`，不会销毁实例。

## Store 状态机

```c
typedef enum {
    HA_ENTITY_STORE_STOPPED,
    HA_ENTITY_STORE_OFFLINE,
    HA_ENTITY_STORE_SUBSCRIBING,
    HA_ENTITY_STORE_SYNCING,
    HA_ENTITY_STORE_READY,
    HA_ENTITY_STORE_DEGRADED,
} ha_entity_store_status_t;
```

典型状态转换：

```text
STOPPED
   │ start
   ▼
OFFLINE
   │ HA authenticated
   ▼
SYNCING
   │ initial entity snapshot received
   ▼
READY
   │ disconnected
   ▼
OFFLINE
```

`DEGRADED` 表示组件仍能提供部分数据，但发生了可能造成状态不完整的问题，例如初始同步失败、
队列溢出或连续解析错误。

## 订阅与初始同步

Store 使用 Home Assistant 的 `subscribe_entities`，只请求当前已经 acquire 的实体。
该订阅会先返回这些实体的压缩初始快照，之后继续发送压缩增量更新。

同步必须遵循以下顺序：

1. 收集并去重当前已经 acquire 的实体 ID。
2. 建立带 `entity_ids` 过滤条件的 `subscribe_entities` 订阅。
3. 等待订阅成功响应和初始快照。
4. 按 WebSocket 消息到达顺序处理初始快照和后续增量事件。

初始状态和实时更新来自同一订阅，可以避免在“读取快照”和“建立订阅”之间发生状态丢失，
也不会接收 Home Assistant 中无关实体的状态。

## 实体注册和引用计数

UI 通过 acquire/release 表达对实体的使用关系：

```c
ha_entity_handle_t *handle = NULL;

ESP_ERROR_CHECK(ha_entity_store_acquire(
    store,
    "light.living_room",
    HA_ENTITY_FIELD_STATE | HA_ENTITY_FIELD_ATTRIBUTES,
    &handle));

/* UI 销毁或不再使用该实体时 */
ha_entity_store_release(handle);
```

同一实体可被多个页面或控件 acquire。Store 为其维护引用计数，只有最后一个使用者 release 后，
才移除该实体的注册关系和缓存。

在 Store 处于 `STOPPED` 或 `OFFLINE` 时也允许注册实体。它们会被记录为待同步，并在下一次进入
同步流程时获取状态。

`acquire()` 本身只修改本地注册关系，不立即重建订阅。这样调用方可以连续注册多个实体，
再通过一次 `ha_entity_store_sync()` 建立一条包含全部目标实体的按需订阅。

示例：

```c
ha_entity_handle_t *light = NULL;
ha_entity_handle_t *temperature = NULL;
ha_entity_handle_t *climate = NULL;

ESP_ERROR_CHECK(ha_entity_store_acquire(
    store, "light.living_room", HA_ENTITY_FIELD_ALL, &light));
ESP_ERROR_CHECK(ha_entity_store_acquire(
    store, "sensor.living_room_temperature", HA_ENTITY_FIELD_ALL, &temperature));
ESP_ERROR_CHECK(ha_entity_store_acquire(
    store, "climate.living_room", HA_ENTITY_FIELD_ALL, &climate));

/* 三个实体只需要建立一次按需订阅。 */
ESP_ERROR_CHECK(ha_entity_store_sync(store));
```

## 主动同步

```c
esp_err_t ha_entity_store_sync(ha_entity_store_t *store);
```

`ha_entity_store_sync()` 请求 Store 使用当前已经 acquire 的实体列表重建
`subscribe_entities` 订阅。订阅建立后，Home Assistant 会立即返回这些实体的初始状态。

这是一个**异步请求接口**。返回 `ESP_OK` 表示同步请求已被接受，不表示服务器结果已经返回。
调用方可以通过 Store 状态和同步事件观察完成结果：

```c
typedef enum {
    HA_ENTITY_STORE_EVENT_STATUS_CHANGED,
    HA_ENTITY_STORE_EVENT_SYNC_STARTED,
    HA_ENTITY_STORE_EVENT_SYNC_SUCCEEDED,
    HA_ENTITY_STORE_EVENT_SYNC_FAILED,
} ha_entity_store_event_type_t;

typedef struct {
    ha_entity_store_event_type_t type;
    esp_err_t result;
    uint32_t sync_generation;
    size_t matched_entities;
    size_t changed_entities;
    ha_entity_store_status_t status;
} ha_entity_store_event_t;

typedef void (*ha_entity_store_event_cb_t)(ha_entity_store_t *store,
                                           const ha_entity_store_event_t *event,
                                           void *user_ctx);
```

一次主动同步的处理过程为：

1. 收集当前已经 acquire 的实体 ID。
2. Store worker 建立过滤后的 `subscribe_entities` 订阅。
3. WebSocket 回调将初始快照和增量更新投入 Store 消息队列。
4. worker 按消息接收顺序处理压缩的 `a`、`c`、`r` 数据。
5. 只更新当前仍然被 acquire 的实体。
6. 快照中不存在的已注册实体标记为 `HA_ENTITY_SYNC_REMOVED`。
7. 发布 `SYNC_SUCCEEDED`；请求失败时发布 `SYNC_FAILED`。

每次实际发出的同步请求都有递增的 `sync_generation`，调用方可用它区分不同轮次。同步过程中
Store 状态临时进入 `SYNCING`，但已有缓存仍可查询。

调用行为约定：

- Store 处于 `STOPPED` 时调用，返回 `ESP_ERR_INVALID_STATE`。
- 内部命令队列暂时已满时返回 `ESP_ERR_TIMEOUT`，调用方可以稍后重试。
- 当前没有任何已 acquire 实体时，不访问网络，直接发布一次同步成功事件。
- Store 已启动但 HA 尚未认证时，请求被记为 pending 并返回 `ESP_OK`，认证完成后自动执行。
- Store 正在建立事件订阅时，请求被记为 pending，订阅成功后执行。
- 已有一次初始快照正在进行时再次调用，不并行重建订阅，而是合并成最多一次后续同步。
- 合并的后续同步会包含等待期间新 acquire 的实体。
- 同步失败不会清除旧缓存；旧数据保留并标记为 `STALE`，调用方可稍后再次同步。

`start()` 在首次认证成功后会自动进行一次同步，断线重连后也会自动同步。显式
`ha_entity_store_sync()` 主要用于以下场景：

- 批量 acquire 一组新的实体后立即获取状态。
- 页面切换或配置更新后刷新新注册的实体。
- 用户主动下拉刷新。
- 业务层怀疑本地缓存不完整时进行校准。

主动同步会替换 Store 现有的 `subscribe_entities` 订阅，但不会改变实体的引用计数。

## 实体数据模型

HA 的业务状态和本地同步状态是两个不同概念：

```c
typedef enum {
    HA_ENTITY_SYNC_UNKNOWN,
    HA_ENTITY_SYNC_SYNCING,
    HA_ENTITY_SYNC_READY,
    HA_ENTITY_SYNC_STALE,
    HA_ENTITY_SYNC_REMOVED,
} ha_entity_sync_status_t;

typedef struct {
    char *entity_id;
    char *state;
    cJSON *attributes;
    char *last_changed;
    char *last_updated;

    uint32_t revision;
    uint32_t updated_at_ms;
    ha_entity_sync_status_t sync_status;
} ha_entity_record_t;
```

例如 HA 断线时，不把原来的 `state = "on"` 修改成 `unavailable`，而是保留最后真实值并设置：

```text
state       = "on"
sync_status = HA_ENTITY_SYNC_STALE
```

这样 UI 可以显示最后状态，同时通过置灰或离线标志提示数据可能已经过期。

第一版内部可以保存完整的 `attributes` 深拷贝，因为不同 domain 的属性差异很大。组件只缓存
面板实际使用的实体，以限制内存占用。后续可根据内存测试增加属性白名单或紧凑存储策略。

## 断线与重连

断线时：

- 保留每个实体最后一次有效状态。
- 将实体同步状态标记为 `STALE`。
- Store 进入 `OFFLINE`。
- 发布一次 Store 状态事件，避免为大量实体制造重复通知。

重新认证后：

- 恢复过滤后的 `subscribe_entities` 订阅。
- 接收一份新的目标实体初始快照。
- 更新所有已注册实体。
- 将快照中不存在的已注册实体标记为 `REMOVED`。
- 同步成功后进入 `READY`。

重订阅返回的新快照会校准断线期间可能丢失的状态变化。

## 查询接口与数据所有权

查询使用 copy-out，不向调用方暴露内部记录指针：

```c
typedef struct {
    char entity_id[128];
    char state[64];
    char friendly_name[128];
    char unit_of_measurement[32];
    char device_class[48];

    uint32_t revision;
    ha_entity_sync_status_t sync_status;
} ha_entity_snapshot_t;

esp_err_t ha_entity_store_get(ha_entity_store_t *store,
                              const char *entity_id,
                              ha_entity_snapshot_t *out);
```

动态属性通过类型化接口读取：

```c
esp_err_t ha_entity_store_get_attr_string(ha_entity_store_t *store,
                                          const char *entity_id,
                                          const char *attribute,
                                          char *buffer,
                                          size_t buffer_size);

esp_err_t ha_entity_store_get_attr_number(ha_entity_store_t *store,
                                          const char *entity_id,
                                          const char *attribute,
                                          double *out);

esp_err_t ha_entity_store_get_attr_bool(ha_entity_store_t *store,
                                        const char *entity_id,
                                        const char *attribute,
                                        bool *out);
```

UI 不持有 Store 内部字符串或 `cJSON *`，因此锁释放后不会出现悬空指针。

## 变更检测和通知

```c
typedef enum {
    HA_ENTITY_CHANGE_STATE        = 1 << 0,
    HA_ENTITY_CHANGE_ATTRIBUTES   = 1 << 1,
    HA_ENTITY_CHANGE_AVAILABILITY = 1 << 2,
    HA_ENTITY_CHANGE_ADDED        = 1 << 3,
    HA_ENTITY_CHANGE_REMOVED      = 1 << 4,
} ha_entity_change_mask_t;

typedef struct {
    char entity_id[128];
    uint32_t revision;
    uint32_t changes;
} ha_entity_changed_event_t;
```

只有内容实际变化时才递增 `revision` 并发布通知，避免 HA 重复事件导致 UI 无意义重绘。
通知只携带实体 ID、revision 和变化掩码；监听者收到通知后通过查询接口获取最新快照。

Store 的监听回调不应直接操作 LVGL。UI 层应把通知转发到 LVGL task，或者使用 LVGL 提供的异步
调度机制。

## 公开 API

```c
typedef struct ha_entity_store ha_entity_store_t;
typedef struct ha_entity_handle ha_entity_handle_t;
typedef struct ha_entity_listener_handle ha_entity_listener_handle_t;

typedef struct {
    ha_ws_client_t *ws_client;
    size_t max_entities;
    size_t event_queue_size;
    uint32_t worker_stack_size;
    int worker_priority;
    bool prefer_external_memory;
} ha_entity_store_config_t;

esp_err_t ha_entity_store_create(const ha_entity_store_config_t *config,
                                 ha_entity_store_t **out_store);

esp_err_t ha_entity_store_start(ha_entity_store_t *store);
esp_err_t ha_entity_store_stop(ha_entity_store_t *store);
esp_err_t ha_entity_store_destroy(ha_entity_store_t *store);

/* 异步重建按需实体订阅，并刷新已 acquire 实体的快照。 */
esp_err_t ha_entity_store_sync(ha_entity_store_t *store);

esp_err_t ha_entity_store_acquire(ha_entity_store_t *store,
                                  const char *entity_id,
                                  uint32_t requested_fields,
                                  ha_entity_handle_t **out_handle);

void ha_entity_store_release(ha_entity_handle_t *handle);

esp_err_t ha_entity_store_get(ha_entity_store_t *store,
                              const char *entity_id,
                              ha_entity_snapshot_t *out);

ha_entity_store_status_t ha_entity_store_get_status(ha_entity_store_t *store);

esp_err_t ha_entity_store_add_event_listener(
    ha_entity_store_t *store,
    ha_entity_store_event_cb_t callback,
    void *user_ctx,
    ha_entity_listener_handle_t **out_handle);

esp_err_t ha_entity_store_add_listener(ha_entity_store_t *store,
                                       ha_entity_store_listener_t callback,
                                       void *user_ctx,
                                       ha_entity_listener_handle_t **out_handle);

void ha_entity_store_remove_listener(ha_entity_listener_handle_t *handle);
```

## 实现与验证顺序

1. 创建实例、生命周期和 Store 状态机。
2. 实现 worker task、消息队列和单写者缓存。
3. 实现实体 acquire/release 和引用计数。
4. 接入按需 `subscribe_entities` 订阅及压缩增量解析。
5. 实现订阅初始快照和显式 `ha_entity_store_sync()`。
6. 实现线程安全 copy-out 查询和属性 getter。
7. 实现 change mask、revision 和监听器。
8. 实现断线标记、重连全量同步和异常恢复。
9. 使用 fake transport 测试乱序、断线、删除、队列溢出和错误 JSON。
