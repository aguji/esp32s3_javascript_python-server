// ===== 系统功能 =====
function updateTime() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', hour12: false });
    const timeElement = document.getElementById('current-time');
    if (timeElement) timeElement.textContent = timeString;
}

function updateWifiStatus() {
    if (Math.random() > 0.95) {
        AppState.wifiConnected = !AppState.wifiConnected;
        const wifiStatusElement = document.getElementById('wifi-status-text');
        if (wifiStatusElement) {
            wifiStatusElement.textContent = AppState.wifiConnected ? '已连接' : '未连接';
            wifiStatusElement.style.color = AppState.wifiConnected ? '#FFF' : '#FF9800';
        }
        if (!AppState.wifiConnected) showNotification('WiFi连接已断开', 'warning');
    }
}

function updateBatteryLevel() {
    if (AppState.batteryLevel > 0 && Math.random() > 0.7) {
        AppState.batteryLevel = Math.max(0, AppState.batteryLevel - 0.1);
        const batteryElement = document.getElementById('battery-level');
        if (batteryElement) {
            batteryElement.textContent = `${Math.round(AppState.batteryLevel)}%`;
            if (AppState.batteryLevel < 20) {
                batteryElement.style.color = '#F44336';
                if (AppState.batteryLevel < 10 && Math.random() > 0.8) {
                    showNotification('电量不足，请及时充电', 'warning');
                }
            }
        }
    }
}