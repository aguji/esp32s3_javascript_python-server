/* 智能图书馆导览机器人 - 用户认证
 * 支持账号密码登录和RFID刷卡登录
 */

// ===== 用户认证 =====

// 检查服务器连接状态
async function checkServerConnection() {
    const health = await checkServerHealth();
    if (!health.serverOk) {
        console.warn('[AUTH] 服务器连接失败，使用本地模式');
        return false;
    }
    console.log('[AUTH] 服务器连接正常');
    return true;
}

function loginWithRFID() {
    showNotification('请将校园卡靠近读卡器...', 'info');
    setTimeout(async () => {
        try {
            // 尝试通过服务器登录
            const serverConnected = await checkServerConnection();
            if (serverConnected && typeof rfidLogin === 'function') {
                // 这里需要从ESP32获取UID，暂时模拟
                const mockUid = 'RFID_UID_' + Date.now();
                const result = await rfidLogin(mockUid);
                if (result.status === 'success') {
                    const user = {
                        id: result.user_id,
                        name: result.user_name,
                        role: 'user'
                    };
                    AppState.currentUser = user;
                    AppState.isLoggedIn = true;
                    localStorage.setItem('isLoggedIn', 'true');
                    localStorage.setItem('currentUser', JSON.stringify(user));
                    showNotification(`欢迎回来，${user.name}！`, 'success');
                    updateUserStatus();
                    showPage('main-menu');
                    return;
                }
            }
        } catch (e) {
            console.log('[AUTH] RFID登录失败，使用模拟用户');
        }
        // 回退到模拟用户
        const mockUser = AppState.mockUsers[0];
        AppState.currentUser = mockUser;
        AppState.isLoggedIn = true;
        localStorage.setItem('isLoggedIn', 'true');
        localStorage.setItem('currentUser', JSON.stringify(mockUser));
        showNotification(`欢迎回来，${mockUser.name}！`, 'success');
        updateUserStatus();
        showPage('main-menu');
    }, 2000);
}

function showQRCodeScanner() {
    document.getElementById('password-login-form').classList.add('hidden');
    document.getElementById('qrcode-container').classList.remove('hidden');
    const qrcodeDiv = document.getElementById('qrcode');
    qrcodeDiv.innerHTML = '<i class="fas fa-qrcode fa-5x"></i>';
    setTimeout(async () => {
        // 使用服务器二维码登录
        try {
            const serverConnected = await checkServerConnection();
            if (serverConnected && typeof passwordLogin === 'function') {
                // 二维码登录时使用token
                const tokenData = await apiGet('/request_token');
                const token = tokenData.token || tokenData;
                // 模拟扫码成功，直接使用第一个模拟用户登录
                const mockUser = AppState.mockUsers[0];
                AppState.currentUser = mockUser;
                AppState.isLoggedIn = true;
                localStorage.setItem('isLoggedIn', 'true');
                localStorage.setItem('currentUser', JSON.stringify(mockUser));
                showNotification(`扫码成功，欢迎${mockUser.name}！`, 'success');
                updateUserStatus();
                showPage('main-menu');
                return;
            }
        } catch (e) {
            console.log('[AUTH] 二维码登录失败，使用模拟用户');
        }
        // 回退到模拟用户
        const mockUser = AppState.mockUsers[0];
        AppState.currentUser = mockUser;
        AppState.isLoggedIn = true;
        localStorage.setItem('isLoggedIn', 'true');
        localStorage.setItem('currentUser', JSON.stringify(mockUser));
        showNotification(`扫码成功，欢迎${mockUser.name}！`, 'success');
        updateUserStatus();
        showPage('main-menu');
    }, 3000);
}

function showPasswordLogin() {
    document.getElementById('qrcode-container').classList.add('hidden');
    document.getElementById('password-login-form').classList.remove('hidden');
}

function hidePasswordLogin() {
    document.getElementById('password-login-form').classList.add('hidden');
}

function hideQRCodeScanner() {
    document.getElementById('qrcode-container').classList.add('hidden');
}

// 处理表单登录
document.addEventListener('DOMContentLoaded', function() {
    const loginForm = document.getElementById('login-form');
    if (loginForm) {
        loginForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            
            if (!username || !password) {
                showNotification('请输入账号和密码', 'warning');
                return;
            }
            
            showNotification('正在验证...', 'info');
            
            try {
                // 优先尝试服务器登录
                const serverConnected = await checkServerConnection();
                if (serverConnected && typeof passwordLogin === 'function') {
                    const result = await passwordLogin(username, password);
                    if (result.success) {
                        AppState.currentUser = result.user;
                        AppState.isLoggedIn = true;
                        localStorage.setItem('isLoggedIn', 'true');
                        localStorage.setItem('currentUser', JSON.stringify(result.user));
                        showNotification(`登录成功，欢迎${result.user.name}！`, 'success');
                        updateUserStatus();
                        showPage('main-menu');
                        this.reset();
                        hidePasswordLogin();
                        return;
                    }
                }
            } catch (e) {
                console.log('[AUTH] 服务器登录失败，尝试本地验证');
            }
            
            // 回退到本地模拟用户验证
            const mockUser = AppState.mockUsers.find(user => user.id === username && user.password === password);
            if (mockUser) {
                AppState.currentUser = mockUser;
                AppState.isLoggedIn = true;
                localStorage.setItem('isLoggedIn', 'true');
                localStorage.setItem('currentUser', JSON.stringify(mockUser));
                showNotification(`登录成功，欢迎${mockUser.name}！`, 'success');
                updateUserStatus();
                showPage('main-menu');
                this.reset();
                hidePasswordLogin();
            } else {
                showNotification('账号或密码错误，请重试', 'error');
            }
        });
    }
});

function logout() {
    AppState.currentUser = null;
    AppState.isLoggedIn = false;
    localStorage.removeItem('isLoggedIn');
    localStorage.removeItem('currentUser');
    showNotification('已退出登录', 'info');
    updateUserStatus();
    window.location.href = 'index.html';
}

function updateUserStatus() {
    const userNameElement = document.getElementById('user-name');
    const loggedUserElement = document.getElementById('logged-user');
    if (AppState.isLoggedIn && AppState.currentUser) {
        if (userNameElement) userNameElement.textContent = AppState.currentUser.name;
        if (loggedUserElement) loggedUserElement.textContent = '欢迎，' + AppState.currentUser.name;
    } else {
        if (userNameElement) userNameElement.textContent = '未登录';
        if (loggedUserElement) loggedUserElement.textContent = '欢迎您！';
    }
}

function updateUserInfo() {
    // 个人中心信息由 loadProfileInfo / loadBorrowedBooks 接管，此函数不再直接写 DOM
}
