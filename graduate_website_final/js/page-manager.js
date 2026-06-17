/* 智能图书馆导览机器人 - 页面管理 */
// ===== 页面管理 =====
function showPage(pageId) {
    document.querySelectorAll('.page').forEach(page => page.classList.remove('active'));
    const targetPage = document.getElementById(pageId);
    if (targetPage) {
        targetPage.classList.add('active');
        AppState.currentPage = pageId;
        switch(pageId) {
            case 'main-menu':
                updateUserInfo();
                var adminBtn = document.getElementById('admin-menu-btn-index');
                if (adminBtn && typeof isAdmin === 'function') {
                    adminBtn.style.display = isAdmin() ? '' : 'none';
                }
                break;
            case 'search-page': initSearchPage(); break;
            case 'profile-page': initProfilePage(); break;
            case 'navigation-page': initNavigationPage(); break;
            case 'borrow-history-page': loadBorrowHistory(); break;
            case 'borrow-page': initBorrowPage(); break;
        }
        document.getElementById('app').scrollTop = 0;
    }
}

// 页面加载时：index.html 作为入口始终显示欢迎页（首页），不根据 localStorage 自动跳主菜单
document.addEventListener('DOMContentLoaded', function() {
    // 更新用户状态显示（顶栏可显示已登录用户名，但入口页仍为欢迎页）
    updateUserStatus();

    // index.html 是站点首页，直接打开时始终显示欢迎页面，不自动跳到已登录用户的主菜单
    const welcomePage = document.getElementById('welcome-page');
    if (welcomePage) {
        showPage('welcome-page');
    }

    // 初始化其他功能
    updateTime();
    setInterval(updateTime, 60000);
    setInterval(updateWifiStatus, 10000);
    setInterval(updateBatteryLevel, 30000);
});