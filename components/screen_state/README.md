# screen_state

设备全屏页面状态机。组件不依赖 LVGL，采用“请求、校验、确认”三阶段切换；具体页面及准入条件由 UI 层管理。

## 页面枚举

| 枚举 | 页面 |
| --- | --- |
| `SCREEN_PAGE_AP_PROVISIONING` | AP 配网引导页 |
| `SCREEN_PAGE_HA_PROVISIONING` | Home Assistant 配网/授权引导页 |
| `SCREEN_PAGE_FORGOT_PASSWORD` | 忘记密码，等待双击设备按键确认页 |
| `SCREEN_PAGE_RESET_PASSWORD` | 双击确认后，等待浏览器设置新密码页 |
| `SCREEN_PAGE_SPLIT_VIEW` | 展示分屏配置；当前分屏无 badge/entity 时显示默认空态 |
| `SCREEN_PAGE_RESTARTING` | 设备重启页 |

## API

```c
/* Register the transition-request listener before initialization. */
ESP_ERROR_CHECK(screen_ui_init());
ESP_ERROR_CHECK(screen_state_init(SCREEN_PAGE_AP_PROVISIONING));

screen_page_t current = screen_state_get_current();
ESP_ERROR_CHECK(screen_state_request_transition(SCREEN_PAGE_HA_PROVISIONING));
```

初始化时当前页面为内部状态 `SCREEN_PAGE_NONE`，并立即请求进入 `initial_page`。
因此首个页面同样必须经过 UI 仲裁，而不是在初始化时被直接视为已经显示。

调用 `screen_state_request_transition()` 后，当前页面不会立刻变化。组件首先发布
`SCREEN_STATE_EVENT_TRANSITION_REQUESTED`：

```c
typedef struct {
    screen_page_t current;
    screen_page_t requested;
    uint32_t generation;
} screen_state_transition_request_t;
```

`screen_ui.c` 监听该事件并检查进入条件。条件成立时调用
`screen_state_commit_transition()`，条件不成立时调用
`screen_state_reject_transition()`。因此被拒绝的请求不会改变
`screen_state_get_current()` 的返回值。

配置重新加载通过 `screen_state_request_refresh()` 请求当前页面重新进入，仍然由同一个
`screen_ui_state_event_handler()` 完成仲裁和渲染；配置加载函数不会直接切换页面。
刷新不会发布 `SCREEN_STATE_EVENT_CHANGED`，因为当前页面枚举并未发生变化。

确认切换后会发布 `SCREEN_STATE_EVENT_CHANGED`：

```c
typedef struct {
    screen_page_t previous;
    screen_page_t current;
} screen_state_changed_event_t;
```

`SCREEN_PAGE_RESTARTING` 是终态。该状态一旦进入，或者已存在等待处理的重启请求，
在设备真正重启前都会拒绝切换至其他页面。

当前 AP 配网、HA 配网和默认首页使用居中标签作为简单页面；分屏页面使用现有
Tileview 渲染器。设备重启页仅保留路由入口，尚未实现具体界面。

应用层当前采用以下优先级计算目标页面：

1. 忘记密码流程正在等待实体按键双击：忘记密码确认页。
2. 实体按键双击确认成功：设置新密码页。
3. 未保存 Wi-Fi 配置且当前没有以太网连接：AP 配网引导页。
4. 网络已配置但 HA 未授权：HA 配网/授权引导页。
5. HA 已配置但没有分屏配置：默认首页。
6. 已存在分屏配置：分屏 Tileview。
7. 收到重启或恢复出厂命令：重启页，并保持到设备重启。

忘记密码流程激活时，`SCREEN_PAGE_FORGOT_PASSWORD` 或
`SCREEN_PAGE_RESET_PASSWORD` 会拒绝 AP 配网、HA 配网、默认首页和分屏页的
较低优先级切换请求。`SCREEN_PAGE_RESTARTING` 仍是最高优先级终态。
