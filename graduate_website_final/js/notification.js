/* 通知系统：页面顶部短暂显示提示条。各页面需有 id="notification" 和 id="notification-message" 的 DOM */
function showNotification(message, type = 'info') {
    const notification = document.getElementById('notification');
    const messageElement = document.getElementById('notification-message');
    if (!notification || !messageElement) return;
    messageElement.textContent = message;
    const colors = { info: '#2196F3', success: '#4CAF50', warning: '#FF9800', error: '#F44336' };
    notification.style.backgroundColor = colors[type] || colors.info;
    notification.classList.remove('hidden');
    setTimeout(() => notification.classList.add('hidden'), 3000);
}