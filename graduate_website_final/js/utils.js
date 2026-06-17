/* 智能图书馆导览机器人 - 工具函数
 * 提供：安全显示文本(escapeHtml)、权限判断(isAdmin/requireAdmin) 等，供各页面共用。
 */
// ===== 安全工具函数 =====
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function createElementFromHTML(htmlString) {
    const div = document.createElement('div');
    div.innerHTML = htmlString;
    return div.firstElementChild;
}

function safeSetInnerHTML(element, html) {
    // 清空元素
    while (element.firstChild) {
        element.removeChild(element.firstChild);
    }
    
    // 创建临时容器
    const tempDiv = document.createElement('div');
    tempDiv.innerHTML = html;
    
    // 安全地添加所有子节点
    while (tempDiv.firstChild) {
        element.appendChild(tempDiv.firstChild);
    }
}

// ===== 权限检查函数（管理后台会用到） =====
function isAdmin() {
    return AppState.currentUser && AppState.currentUser.role === 'admin';
}

function requireAdmin() {
    if (!isAdmin()) {
        showNotification('需要管理员权限才能访问此功能', 'error');
        return false;
    }
    return true;
}

/** 返回菜单的 URL：管理员从 admin-menu 进入的页面应回到管理员首页，否则回到用户主菜单 */
function getBackMenuUrl() {
    if (typeof AppState !== 'undefined' && AppState.currentUser && AppState.currentUser.role === 'admin') {
        return '../admin/admin-menu.html';
    }
    return 'main-menu.html';
}

function checkPermission(requiredRole = 'user') {
    if (!AppState.currentUser) {
        showNotification('请先登录', 'error');
        return false;
    }
    
    const roleHierarchy = { 'user': 1, 'admin': 2 };
    const userRoleLevel = roleHierarchy[AppState.currentUser.role] || 0;
    const requiredRoleLevel = roleHierarchy[requiredRole] || 0;
    
    if (userRoleLevel < requiredRoleLevel) {
        showNotification('权限不足', 'error');
        return false;
    }
    
    return true;
}
