/* 智能图书馆导览机器人 - API通信模块
 * 封装所有与后端服务器的通信请求
 */

// API服务器地址：本地开发走 127.0.0.1:5000，局域网其他设备走 YOUR_SERVER_IP:5000
const API_BASE_URL = (function () {
    const h = window.location.hostname;
    if (h === '127.0.0.1' || h === 'localhost' || h === '') {
        return 'http://127.0.0.1:5000';
    }
    return 'http://YOUR_SERVER_IP:5000';
})();

/* ===================== 通用请求函数 ===================== */

/**
 * 发送GET请求
 */
async function apiGet(endpoint, params = {}) {
    const url = new URL(`${API_BASE_URL}${endpoint}`);
    Object.keys(params).forEach(key => url.searchParams.append(key, params[key]));
    
    try {
        const response = await fetch(url.toString(), {
            method: 'GET',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'omit'
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error(`[API] GET ${endpoint} 失败:`, error);
        throw error;
    }
}

/**
 * 发送POST请求
 */
async function apiPost(endpoint, data = {}) {
    try {
        const response = await fetch(`${API_BASE_URL}${endpoint}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data),
            credentials: 'omit'
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const text = await response.text();
        try {
            return JSON.parse(text);
        } catch {
            return { raw: text };
        }
    } catch (error) {
        console.error(`[API] POST ${endpoint} 失败:`, error);
        throw error;
    }
}

/* ===================== 健康检查 ===================== */

/**
 * 检查服务器和数据库连接状态
 */
async function checkServerHealth() {
    try {
        const result = await apiGet('/health');
        return {
            serverOk: result.status === 'ok',
            dbOk: result.db === 'ok'
        };
    } catch {
        return { serverOk: false, dbOk: false };
    }
}

/* ===================== 用户认证 ===================== */

/**
 * RFID刷卡登录（通过ESP32触发）
 * @param {string} uid - 校园卡RFID UID
 */
async function rfidLogin(uid) {
    return await apiGet('/rfid_login', { uid });
}

/**
 * 账号密码登录
 * @param {string} username - 借书证号（card_number）
 * @param {string} password - 密码
 */
async function passwordLogin(username, password) {
    try {
        const tokenData = await apiGet('/request_token');
        const token = tokenData.token || tokenData;

        const formData = new URLSearchParams();
        formData.append('username', username);
        formData.append('password', password);

        const response = await fetch(`${API_BASE_URL}/login?token=${token}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData,
            credentials: 'omit'
        });

        const data = await response.json();
        if (data.success) {
            return {
                success: true,
                user: {
                    id: data.card_number,
                    name: data.name || data.card_number,
                    card_number: data.card_number,
                    is_admin: data.is_admin || false,
                },
                token: token
            };
        } else {
            return { success: false, message: response.statusText || '登录失败' };
        }
    } catch (error) {
        return { success: false, message: error.message };
    }
}

/* ===================== 图书搜索 ===================== */

/**
 * 搜索图书（按书名、作者、ISBN模糊搜索）
 * @param {string} keyword - 搜索关键词
 */
async function searchBooks(keyword) {
    const result = await apiGet('/search', { keyword });
    return result.books || [];
}

/**
 * 按分类搜索图书
 * @param {string} category - 分类名称
 */
async function searchBooksByCategory(category) {
    const result = await apiGet('/search_by_category', { category });
    return result.books || [];
}

/* ===================== 借书还书 ===================== */

/**
 * 借书
 * @param {string} readerCard - 读者借书证号
 * @param {string} bookUid - 图书RFID UID
 */
async function borrowBook(readerCard, bookUid) {
    return await apiPost('/borrow', {
        reader_card: readerCard,
        book_uid: bookUid
    });
}

/**
 * 还书
 * @param {string} readerCard - 读者借书证号
 * @param {string} bookUid - 图书RFID UID
 */
async function returnBook(readerCard, bookUid) {
    return await apiPost('/return_book', {
        reader_card: readerCard,
        book_uid: bookUid
    });
}

/**
 * 获取读者个人资料
 * @param {string} cardNumber - 借书证号
 */
async function getReaderProfile(cardNumber) {
    try {
        const result = await apiGet('/reader_profile', { card_number: cardNumber });
        if (result.error) return null;
        return result;
    } catch {
        return null;
    }
}

/* ===================== 用户信息 ===================== */

/**
 * 获取读者当前借阅信息
 * @param {string} readerCard - 读者借书证号
 */
async function getReaderBorrows(readerCard) {
    try {
        const result = await apiGet('/reader_borrows', { card_number: readerCard });
        return result;
    } catch {
        return { borrows: [], count: 0 };
    }
}

/**
 * 获取读者借阅历史
 * @param {string} readerCard - 读者借书证号
 */
async function getReaderHistory(readerCard) {
    try {
        const result = await apiGet('/reader_history', { card_number: readerCard });
        return result.history || [];
    } catch {
        return [];
    }
}
