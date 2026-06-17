# 图书馆管理系统 - 中英文对照表

## 说明
此文件整理所有界面文本的中英文对照，供后续语言切换功能使用。
格式：`英文 -> 中文`

---

## 主界面 (library_ui.c)

### build_main_screen()
| 英文 | 中文 |
|------|------|
| Welcome To Use Library Management System | 欢迎使用图书馆管理系统 |
| Search, borrow and navigate | 搜索、借阅、导航 |
| Login | 登录 |
| View library layout | 查看图书馆布局 |
| Search book | 搜索图书 |

### build_login_screen()
| 英文 | 中文 |
|------|------|
| Choose login method | 选择登录方式 |
| RFID card | 刷卡登录 |
| QR code | 扫码登录 |
| Password | 密码登录 |
| Back | 返回 |

### build_rfid_stub_screen()
| 英文 | 中文 |
|------|------|
| RFID login | 刷卡登录 |
| Initializing RFID reader... | 正在初始化读卡器... |
| Card not allowed | 此卡未授权 |

### build_password_login_screen()
| 英文 | 中文 |
|------|------|
| Password Login | 密码登录 |
| Account | 账号 |
| Password | 密码 |
| account | 请输入账号 |
| password | 请输入密码 |
| Account or password error | 账号或密码错误 |

### build_mainmenu_screen()
| 英文 | 中文 |
|------|------|
| Main Menu | 主菜单 |
| WiFi: Checking... | WiFi: 检查中... |
| WiFi: Not connected | WiFi: 未连接 |
| Welcome, visitor | 欢迎使用 |
| Welcome, %s | 欢迎, %s |
| Book search | 图书搜索 |
| Borrow/Return | 借还书 |
| Personal | 个人中心 |
| Navigation | 导航导览 |
| Help | 帮助 |
| Logout | 退出登录 |

### build_admin_screen()
| 英文 | 中文 |
|------|------|
| Maintenance | 系统维护 |
| Maintenance Mode | 维护模式 |
| System Self-Check | 系统自检 |
| WiFi Settings | WiFi设置 |
| Back to Menu | 返回菜单 |

### build_selfcheck_screen()
| 英文 | 中文 |
|------|------|
| System Self-Check | 系统自检 |
| RFID Module | RFID模块 |
| Touchscreen | 触摸屏 |
| WiFi Module | WiFi模块 |
| IR Sensor | 红外传感器 |
| Battery | 电池 |
| Back | 返回 |
| Testing... | 检测中... |
| OK | 正常 |
| Error | 异常 |
| No memory | 内存不足 |
| Task create fail | 任务创建失败 |

### build_personal_screen()
| 英文 | 中文 |
|------|------|
| Personal Center | 个人中心 |
| Name: -- | 姓名: -- |
| Name: %s | 姓名: %s |
| ID: -- | 账号: -- |
| ID: %s | 账号: %s |
| Current: | 当前: |
| History: | 历史: |
| Credit: | 信用: |
| Current borrowed | 当前借阅 |
| History borrowed | 历史借阅 |

### build_nav_guide_screen()
| 英文 | 中文 |
|------|------|
| Navigation | 导航导览 |
| View library layout | 查看图书馆布局 |
| Search book location | 搜索图书位置 |

### build_help_screen()
| 英文 | 中文 |
|------|------|
| Help | 帮助 |

---

## 借还书界面 (ui_borrow.c)

### build_borrow_screen()
| 英文 | 中文 |
|------|------|
| Borrow / Return | 借还书 |
| Select Borrow or Return, then scan the book | 请选择借书或还书，然后扫描图书 |
| Borrow | 借书 |
| Return | 还书 |
| Back | 返回 |

### build_borrow_only_screen()
| 英文 | 中文 |
|------|------|
| Borrow | 借书 |
| Please scan the book | 请扫描图书 |
| Debug: waiting... | 调试: 等待中... |

### build_return_only_screen()
| 英文 | 中文 |
|------|------|
| Return | 还书 |
| Please scan book to return | 请扫描图书归还 |
| IR: -- | 红外: -- |

### 运行时状态信息
| 英文 | 中文 |
|------|------|
| User not logged in | 用户未登录 |
| Please login first | 请先登录 |
| borrow ok: %s | 借书成功: %s |
| borrow ok (server) | 借书成功(服务器) |
| borrow failed | 借书失败 |
| Book not found | 图书未找到 |
| Book already borrowed | 图书已被借出 |
| Unknown book (UID: %s) | 未知图书 (UID: %s) |
| borrow success: %s | 借书成功: %s |
| return ok (server) | 还书成功(服务器) |
| return failed | 还书失败 |
| Book not found | 图书未找到 |
| Book not borrowed | 图书未借出 |
| return success: %s | 还书成功: %s |
| Return failed: %s | 还书失败: %s |
| Return success (UID: %s) | 还书成功 (UID: %s) |
| user didn't login | 用户未登录 |
| IR: [BLOCKED] Place book in box | 红外: [阻挡] 放入图书 |
| IR: [CLEAR] Waiting for book... | 红外: [空闲] 等待图书... |
| IR: detected but DB failed | 红外检测到但数据库更新失败 |
| IR: [OK] Saved to server | 红外: [成功] 已保存到服务器 |
| Please scan book to borrow | 请扫描图书借书 |
| Please scan book to return | 请扫描图书归还 |
| Invalid UID | 无效的UID |
| Checking book... | 正在检查图书... |
| User not found | 用户未找到 |
| Book not borrowed by you | 图书不是您借的 |
| UID %s - Please put book into return box | UID %s - 请将图书放入还书箱 |
| [IR] Detected! Processing... | [红外] 检测到! 处理中... |

---

## WiFi设置界面 (ui_wifi.c)

| 英文 | 中文 |
|------|------|
| WiFi Settings | WiFi设置 |
| Status: (not connected) | 状态: (未连接) |
| Status: Connected to %s | 状态: 已连接到 %s |
| Status: Not connected | 状态: 未连接 |
| Scanning... | 扫描中... |
| Scan | 扫描 |
| SSID: -- | 网络名: -- |
| SSID: %s | 网络名: %s |
| Password: | 密码: |
| password | 请输入密码 |
| Connected, config saved. | 已连接，已保存配置 |
| Connection failed. | 连接失败 |
| Connect | 连接 |
| Cancel | 取消 |
| Hide KB | 隐藏键盘 |

---

## 楼层选择界面 (ui_map.c)

| 英文 | 中文 |
|------|------|
| Select floor | 选择楼层 |
| 1F - Floor 1 | 1F - 一楼 |
| 2F - Floor 2 | 2F - 二楼 |
| 3F - Floor 3 | 3F - 三楼 |
| 4F - Floor 4 | 4F - 四楼 |
| Back | 返回 |
| [Floor map image] | [楼层图] |
| Image not loaded | 图片未加载 |
| Add floor images to display | 请添加楼层图片 |

---

## 图书搜索界面 (ui_search.c)

| 英文 | 中文 |
|------|------|
| Book Search | 图书搜索 |
| Keyword Search | 关键词搜索 |
| Category Search | 分类搜索 |
| Back | 返回 |
| Under Development | 正在开发中 |
| Please wait for further updates. | 请等待后续更新 |

---

## 扫码登录界面 (ui_qr_login.c)

| 英文 | 中文 |
|------|------|
| QR code login | 扫码登录 |
| Waiting... | 等待中... |
| Connecting... | 连接中... |
| Connecting to server... | 正在连接服务器... |
| Scan QR code | 请扫码 |
| Waiting for scan... | 等待扫码... |
| Token expired | Token已过期 |
| HTTP start failed | HTTP启动失败 |
| register_token fail | Token注册失败 |
| Error: Empty token | 错误: 空Token |
| Login successful! | 登录成功! |
| Poll task running | 轮询任务运行中 |
| Task create failed | 任务创建失败 |
| Back | 返回 |

---

## RFID模块 (ui_rfid.c)

| 英文 | 中文 |
|------|------|
| Checking local DB... | 正在检查本地数据库... |
| DB: %d readers | 数据库: %d 位读者 |
| Found: %s | 找到: %s |
| Not in local DB, trying server... | 本地数据库未找到，正在尝试服务器... |
| Querying server... | 正在查询服务器... |
| Server OK: %s | 服务器验证成功: %s |
| Not found: %s | 未找到: %s |

---

## 实现建议

### 方案A: 简单切换（仅按钮文本切换）
- 添加 `g_use_chinese` 全局标志
- 修改按钮创建函数，支持双语文本
- 点击切换标志并重新加载界面

### 方案B: 完整国际化（推荐）
1. 创建 `i18n.h` / `i18n.c` 国际化模块
2. 定义所有文本的结构体数组
3. 提供 `_(key)` 宏或函数获取当前语言文本
4. 所有界面文本使用国际化函数

### 字体要求
- 中文显示需要加载支持中文的 LVGL 字体
- 可使用 `lv_font_montserrat_chinese_16` 或其他中文字体
