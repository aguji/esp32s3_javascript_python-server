# 智能图书馆导览机器人 - 项目总结报告

> 本项目为毕业设计实现，命名为「智能图书馆导览机器人」，基于 Flask + MySQL 构建后端服务，前端采用 HTML5 + CSS3 + Vanilla JavaScript 实现 SPA（单页面应用）架构，支持 RFID 刷卡、扫码登录、自助借还书、图书检索、图书馆导航、个人中心及管理后台等功能。

---

## 一、项目概述

### 1.1 项目背景与目标

本项目旨在为图书馆设计一套智能化、自助化的读者服务系统。系统部署于图书馆自助终端设备上（ESP32 嵌入式设备 + 触摸屏），读者可通过 RFID 刷卡、扫码或账号密码三种方式登录系统，完成图书借还、检索、个人信息查询等操作；同时配备管理后台，供图书馆管理员进行读者管理、图书管理、代录借还及密码重置等日常运维工作。

### 1.2 技术栈

| 层级 | 技术选型 | 说明 |
|------|---------|------|
| **后端** | Python 3 + Flask | 轻量级 Web 框架，提供 RESTful 接口 |
| **数据库** | MySQL（pymysql 驱动） | 存储读者、图书、借阅记录数据 |
| **前端** | HTML5 + CSS3 + Vanilla JS | 无框架，依赖 Pico CSS + Font Awesome |
| **图标** | Font Awesome 6.4.0 | CDN 引入 |
| **通信** | Fetch API | 前端与后端通过 JSON 进行数据交互 |
| **跨域** | Flask @after_request CORS | 支持终端与 PC 不同端口访问 |

### 1.3 项目文件结构

```
graduate_website/
├── server.py                      # Flask 后端服务器（全部 API 路由）
│
├── index.html                     # 主入口页面（SPA 根页面）
│
├── js/                           # JavaScript 核心模块
│   ├── app-state.js              # 全局状态 + 模拟数据（离线回退）
│   ├── api.js                    # 所有后端 API 调用封装
│   ├── auth.js                   # 用户认证（登录/登出/会话）
│   ├── search.js                 # 图书检索逻辑
│   ├── borrow.js                 # 自助借书流程
│   ├── return.js                 # 自助还书流程
│   ├── profile.js                # 个人中心
│   ├── navigation.js             # 图书馆导航地图
│   ├── help.js                  # 帮助页面逻辑
│   ├── admin.js                 # 管理后台逻辑
│   ├── utils.js                 # 工具函数（权限判断/HTML转义/URL处理）
│   ├── notification.js          # 顶部通知提示系统
│   ├── system.js               # 系统状态栏（时间/WiFi/电池）
│   ├── page-manager.js          # SPA 页面切换
│   ├── main.js                  # 全局初始化 + 无操作超时登出
│   └── features.js              # 功能入口逻辑
│
├── css/                          # 样式文件
│   ├── base.css                  # 全局变量、布局基础
│   ├── welcome.css               # 欢迎页样式
│   ├── menu.css                 # 主菜单样式
│   ├── search.css               # 搜索页样式
│   ├── borrow.css               # 借书页样式
│   ├── return.css              # 还书页样式
│   ├── profile.css             # 个人中心样式
│   ├── navigation.css           # 导航页样式
│   ├── help.css                # 帮助页样式
│   ├── login.css               # 登录页样式
│   ├── admin.css              # 管理后台样式
│   └── self-service.css        # 自助借还说明页样式
│
├── pages/
│   ├── admin/
│   │   ├── admin.html           # 管理后台主页
│   │   ├── admin-menu.html     # 管理员入口页
│   │   └── help.html           # 管理员帮助页
│   └── user/
│       ├── main-menu.html        # 用户主菜单
│       ├── login.html            # 登录方式选择
│       ├── login-rfid.html      # RFID 刷卡登录
│       ├── login-qrcode.html   # 扫码登录
│       ├── password-login.html  # 账号密码登录
│       ├── self-service.html   # 自助借还说明
│       ├── borrow.html         # 自助借书
│       ├── return.html         # 自助还书
│       ├── profile.html         # 个人中心
│       ├── search.html         # 图书检索
│       ├── navigation.html     # 图书馆导航
│       └── help.html           # 用户帮助页
│
└── docs/
    ├── 项目解读.md
    ├── 管理员端功能说明（论文用）.md
    ├── 用户端功能说明（论文用）.md
    └── 扫码登录对接说明.md
```

---

## 二、数据库设计

数据库名为 `gradata`，共 3 张表，无单独的统计表（统计均为实时聚合查询）。

### 2.1 tb_reader（读者表）

存储读者账户信息。`rfid_uid` 字段用于 ESP32 刷卡识别，`is_admin` 区分普通读者与管理员。

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INT | 主键，自增 |
| card_number | VARCHAR(50) | 借书证号，唯一索引 |
| name | VARCHAR(100) | 读者姓名 |
| password | VARCHAR(100) | 登录密码（默认 123456） |
| phone | VARCHAR(20) | 电话号码 |
| is_admin | TINYINT | 是否管理员（0=读者，1=管理员） |
| rfid_uid | VARCHAR(100) | 校园卡 RFID UID（大小写不敏感匹配） |
| created_at | DATETIME | 注册时间 |

### 2.2 tb_book（图书表）

存储图书书目信息。`rfid_uid` 为每本书粘贴的 RFID 标签 UID，用于自助借还识别。

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INT | 主键，自增 |
| isbn | VARCHAR(50) | ISBN 号 |
| title | VARCHAR(200) | 书名 |
| author | VARCHAR(100) | 作者 |
| publisher | VARCHAR(100) | 出版社 |
| location | VARCHAR(100) | 馆藏位置（如"三楼A区书架3-2"） |
| category | VARCHAR(50) | 分类（computer / literature / science / history / art / economics） |
| rfid_uid | VARCHAR(100) | 图书 RFID 标签 UID（唯一索引，大小写不敏感） |
| status | TINYINT | 在架状态（0=可借阅，1=已借出） |

### 2.3 tb_borrow（借阅记录表）

每发生一次借还操作产生一条记录。`status=0` 且 `return_time IS NULL` 表示当前在借。

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INT | 主键，自增 |
| reader_id | INT | 读者 ID（关联 tb_reader.id） |
| book_id | INT | 图书 ID（关联 tb_book.id） |
| borrow_time | DATETIME | 借出时间 |
| due_time | DATETIME | 应还时间（borrow_time + 30天） |
| return_time | DATETIME | 实际归还时间（NULL 表示未还） |
| status | TINYINT | 状态（0=借阅中，1=已归还） |

---

## 三、后端接口设计（server.py）

### 3.1 接口总览

Flask 运行在 `0.0.0.0:5000`，所有接口返回 JSON（登录页除外）。已配置全局 CORS 头，支持跨域访问。

#### 健康检查与系统

| 路由 | 方法 | 说明 |
|------|------|------|
| `/health` | GET | 健康检查：返回服务器状态 + 数据库连接状态 |
| `/ping` | GET | 简单 ping，返回 "ok" |

#### 扫码登录（Token 机制）

| 路由 | 方法 | 说明 |
|------|------|------|
| `/request_token` | GET | 生成随机 token，返回给前端展示二维码 |
| `/register_token` | GET/POST | ESP32/前端注册 token（标记为 pending） |
| `/check_token` | GET | 查询 token 状态（pending / success / invalid） |
| `/login` | GET/POST | 二维码页面 GET 返回 HTML 表单；POST 验证账号密码，成功则 token 状态置为 success |

#### 读者认证

| 路由 | 方法 | 说明 |
|------|------|------|
| `/rfid_login` | GET | ESP32 刷卡：根据 rfid_uid 查询读者，返回 card_number + name + is_admin |
| `/verify_password` | POST | 账号密码登录验证（JSON：username, password） |

#### 图书检索

| 路由 | 方法 | 说明 |
|------|------|------|
| `/search` | GET | 模糊搜索（书名/作者/ISBN），最多返回 50 条 |
| `/categories` | GET | 获取所有图书分类列表 |
| `/book/<book_id>` | GET | 获取单本图书详细信息 |
| `/search_by_category` | GET | 按分类精确匹配搜索 |

#### 借还书

| 路由 | 方法 | 说明 |
|------|------|------|
| `/borrow` | POST | 借书：reader_card + book_uid → 插入 tb_borrow，tb_book.status=1。检查图书在架、借阅记录无冲突 |
| `/return_book` | POST | 还书：reader_card + book_uid → 更新 tb_borrow.return_time=NOW()，tb_book.status=0 |

#### 个人中心

| 路由 | 方法 | 说明 |
|------|------|------|
| `/reader_profile` | GET | 获取读者资料（name, card_number, phone, is_admin, created_at） |
| `/reader_borrows` | GET | 获取当前借阅列表（status=0 且 return_time IS NULL），含 title/isbn/book_uid |
| `/reader_history` | GET | 获取借阅历史（最近 50 条），含归还时间与状态 |

#### 管理后台

| 路由 | 方法 | 说明 |
|------|------|------|
| `/admin/stats` | GET | 统计：读者总数、图书总数、当前有借阅的读者人数、累计借阅笔数 |
| `/admin/readers` | GET | 读者列表，支持 `?q=` 模糊搜索（卡号/姓名），返回当前借阅数 + 历史借阅数 |
| `/admin/books` | GET | 图书列表，`?available=1` 仅在架可借，`?q=` 模糊搜索（书名/ISBN/作者） |
| `/admin/search_books` | GET | 图书模糊搜索并附带当前借阅人信息（card_number, name, borrow_time） |
| `/admin/reset_reader_password` | POST | 重置读者密码（JSON：card_number, new_password），默认重置为 123456 |

### 3.2 借还书业务规则

**借书条件：**
1. 读者存在于 `tb_reader`（按 card_number 精确匹配）
2. 图书存在于 `tb_book`（按 rfid_uid 大小写不敏感匹配）
3. 图书 `status = 0`（在架可借）
4. 该图书当前无未归还的借阅记录

**还书条件：**
1. 读者与图书均存在
2. 存在一条 `status=0` 且 `return_time IS NULL` 且 `reader_id` 与 `book_id` 均匹配的借阅记录

**RFID 匹配策略：** 所有涉及 RFID 的查询均使用 `UPPER(TRIM(rfid_uid)) = UPPER(%s)`，大小写不敏感，去除首尾空格。

---

## 四、前端架构设计

### 4.1 SPA 单页面应用

项目以 `index.html` 为唯一入口，通过 JavaScript 动态显示/隐藏页面 sections，模拟页面跳转效果。各功能页面的 HTML 结构集中在 index.html 中，每个 section 对应一个功能页面。

### 4.2 全局状态管理（app-state.js）

`AppState` 对象管理所有前端状态：

- `currentUser`：当前登录用户对象（id, name, role, is_admin 等）
- `isLoggedIn`：登录状态
- `mockBooks` / `mockUsers`：离线模拟数据，API 不可用时自动回退
- `wifiConnected` / `batteryLevel`：系统状态
- `language` / `settings`：用户偏好设置

登录成功后，用户信息持久化到 `localStorage`，页面刷新后自动恢复登录态。

### 4.3 API 通信层（api.js）

所有后端请求经由 `apiGet()` 和 `apiPost()` 封装，统一处理：
- 请求超时处理
- 服务器不可用时自动切换到本地 mock 数据
- 错误提示

### 4.4 功能模块

| 模块 | 职责 |
|------|------|
| `auth.js` | 登录（RFID/扫码/密码）、登出、会话管理 |
| `search.js` | 图书检索（关键词 + 分类筛选）、结果渲染 |
| `borrow.js` | 自助借书三步流程（刷卡→扫描→确认） |
| `return.js` | 自助还书三步流程（扫描→放入→完成） |
| `profile.js` | 个人中心（资料展示 / 当前借阅 / 历史统计） |
| `navigation.js` | 图书馆导览地图（楼层切换 1-4F） |
| `admin.js` | 管理后台（统计 / 用户列表 / 图书列表 / 代录借还 / 重置密码） |
| `help.js` | 帮助页面（使用指南 / 联系信息 / 系统设置） |

### 4.5 页面流程图

```
[欢迎页 / index.html]
        │
        ▼
   [登录选择] ─RFID刷卡──▶ /login-rfid.html
        │
        ├──▶ /login-qrcode.html ──▶ [手机扫码] ──▶ /login (POST) ──┐
        │                                                              │
        └──▶ /password-login.html ──▶ /verify_password (POST) ───────┘
                                                                          │
                                                                          ▼
                                                         [用户主菜单 main-menu.html]
                                                         ┌──────┬───────┬───────┬───────┐
                                                         ▼      ▼       ▼       ▼       ▼
                                                    图书检索 借书    还书    个人中心  导航/帮助
                                                   /search /borrow  /return /profile /navigation
```

---

## 五、核心功能详细说明

### 5.1 三种登录方式

**（1）RFID 刷卡登录**
- ESP32 读取校园卡 UID → 调用 `/rfid_login?uid=xxx`
- 后端按 `UPPER(TRIM(rfid_uid))` 匹配 `tb_reader`
- 返回 reader 信息，前端自动完成登录

**（2）扫码登录**
- 前端调用 `/request_token` 获取随机 token，生成二维码
- ESP32 调用 `/register_token?token=xxx` 注册该 token（处于 pending 状态）
- 读者手机扫码 → 打开 `/login?token=xxx` 页面 → 输入账号密码 POST 提交
- 成功后 token 状态置为 success，ESP32 轮询 `/check_token` 检测到后完成登录

**（3）账号密码登录**
- 表单提交 → 调用 `/verify_password`（JSON POST）
- 后端按 card_number + password 查询 tb_reader
- 返回用户信息，前端完成登录

### 5.2 自助借还书流程

**借书（borrow.html）三步：**
1. **刷卡识别**：读者刷校园卡 → `/rfid_login` 确认身份
2. **扫描书籍**：扫描图书 RFID → `/search` 确认图书可借
3. **确认借阅**：调用 `/borrow` → 插入借阅记录，更新图书状态

**还书（return.html）三步：**
1. **扫描书籍**：扫描图书 RFID → `/search` 确认图书在馆
2. **放入还书口**：设备提示放书
3. **完成还书**：调用 `/return_book` → 更新借阅记录，图书状态恢复可借

### 5.3 管理后台（admin.html）

- **统计卡片**：实时查询 MySQL 聚合计算（无单独统计表）
  - 用户总数：`COUNT(*) FROM tb_reader`
  - 图书总数：`COUNT(*) FROM tb_book`
  - 当前借阅人数：`COUNT(DISTINCT reader_id) FROM tb_borrow WHERE status=0 AND return_time IS NULL`
  - 累计借阅：`COUNT(*) FROM tb_borrow`
- **用户列表**：调用 `/admin/readers`，点击行跳转检索该用户
- **图书列表**：调用 `/admin/books`，展示所有书目状态
- **按用户检索**：调用 `/admin/readers?q=`，显示该读者详情、当前借阅、借阅历史
- **按图书检索**：调用 `/admin/search_books?q=`，显示图书状态及当前借阅人
- **代录借书**：弹窗搜索在架图书（`/admin/books?available=1&q=`），点击选中后调用 `/borrow`
- **代录还书**：从 `/reader_borrows` 获取读者在借列表，调用 `/return_book`
- **重置密码**：调用 `/admin/reset_reader_password`，默认重置为 `123456`
- **以该用户身份进入**：将选中的读者信息写入 AppState，跳转到用户主菜单

### 5.4 离线回退机制

当前端 `fetch` 请求失败（服务器未启动或网络断开）时，自动回退到 `AppState.mockBooks` 和 `AppState.mockUsers` 中的本地模拟数据进行操作，保证终端在无网络环境下仍可演示基本功能。

---

## 六、项目特色与创新点

1. **多模态认证**：支持 RFID 刷卡、扫码、账号密码三种登录方式，适应不同读者习惯和设备场景。
2. **ESP32 嵌入式适配**：后端专门设计了 `/rfid_login`、`/register_token`、`/check_token`、`/health` 等接口供 ESP32 终端使用，轻量可靠。
3. **大小写不敏感 RFID 匹配**：所有 RFID 查询统一使用 `UPPER(TRIM())` 处理，兼容不同读写器输出格式。
4. **零统计表设计**：管理后台所有统计数字通过 SQL 聚合实时计算，无需维护额外的统计表，数据一致性强。
5. **SOPA + 模拟数据双轨**：正常联网走数据库，离线回退走本地 mock，无需额外配置即可演示。
6. **无刷新页面切换**：通过 CSS `hidden` 类和 DOM 操作实现 SPA 效果，响应迅速。
7. **可搜索选书的代录借书**：管理后台代录借书弹窗支持关键词实时搜索可借图书列表，避免下拉框海量数据难选的问题。

---

## 七、部署与运行

### 7.1 后端启动

```bash
cd graduate_website
pip install flask pymysql
python server.py
# 输出: 图书馆服务器  gradata.tb_reader
# 服务运行于 http://0.0.0.0:5000
```

### 7.2 前端访问

```
# 方式一：直接打开（可能有跨域问题）
直接用浏览器打开 index.html

# 方式二：通过 Live Server（推荐）
# VS Code 安装 Live Server 扩展 → 右键 index.html → "Open with Live Server"
# 访问 http://localhost:5500

# 方式三：ESP32 终端直接访问
http://<电脑IP>:5000
```

### 7.3 数据库初始化

数据库 `gradata` 需提前创建，三张表结构参见本报告「数据库设计」章节。示例初始化数据包括：
- 约 23 个读者账户（含管理员账号）
- 约 25 本图书（涵盖 computer / literature / science / history / art / economics 分类）
- 若干借阅记录用于演示

---

## 八、已知限制与后续改进方向

1. **无服务端鉴权**：管理后台 `/admin/*` 接口未加 token 验证，生产环境需补充管理员身份校验。
2. **内存 token**：扫码登录 token 存储在 Python 内存中，服务重启后所有 pending token 丢失，需扫码重新登录。
3. **无图书封面**：当前仅以书名/颜色块展示图书信息，可后续接入 ISBN 封面 API。
4. **无借阅数量限制**：系统未限制读者同时借阅册数（毕设简化，未做此业务规则）。
5. **无逾期罚款**：借阅超期后系统不自动计算罚款金额，后续可扩展。
6. **无日志审计**：所有操作均无持久化审计日志，生产环境建议增加操作日志表。
