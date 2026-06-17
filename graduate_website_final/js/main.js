// ===== 全局无操作自动返回逻辑 =====
// 无操作超时时间（毫秒），可根据终端需求调整，比如 120000 = 2 分钟
const INACTIVITY_TIMEOUT = 120000;
let inactivityTimer = null;

function resetInactivityTimer() {
    if (inactivityTimer) {
        clearTimeout(inactivityTimer);
    }
    inactivityTimer = setTimeout(function() {
        // 统一返回到合适的首页 / 主菜单
        if (typeof goBackToWelcomeOrMenu === 'function') {
            if (AppState.currentPage !== 'welcome-page' && AppState.currentPage !== 'main-menu') {
                showNotification('长时间未操作，已自动返回主页', 'info');
            }
            goBackToWelcomeOrMenu();
        }
    }, INACTIVITY_TIMEOUT);
}

function registerInactivityEvents() {
    const events = ['click', 'touchstart', 'keydown'];
    events.forEach(eventName => {
        document.addEventListener(eventName, resetInactivityTimer, true);
    });
    // 页面初始化完后先启动一次计时
    resetInactivityTimer();
}

// ===== 初始化应用 =====
function initApp() {
    updateTime();
    setInterval(updateTime, 60000);
    setInterval(updateWifiStatus, 10000);
    setInterval(updateBatteryLevel, 30000);
    
    const voiceGuideCheckbox = document.getElementById('voice-guide');
    const highContrastCheckbox = document.getElementById('high-contrast');
    
    if (voiceGuideCheckbox) {
        voiceGuideCheckbox.checked = AppState.settings.voiceGuide;
        voiceGuideCheckbox.addEventListener('change', function() {
            AppState.settings.voiceGuide = this.checked;
            showNotification(`语音提示${this.checked ? '开启' : '关闭'}`, 'info');
            resetInactivityTimer();
        });
    }
    
    if (highContrastCheckbox) {
        highContrastCheckbox.checked = AppState.settings.highContrast;
        highContrastCheckbox.addEventListener('change', function() {
            AppState.settings.highContrast = this.checked;
            document.body.classList.toggle('high-contrast', this.checked);
            showNotification(`高对比度模式${this.checked ? '开启' : '关闭'}`, 'info');
            resetInactivityTimer();
        });
    }
    
    updateUserStatus();
    showNotification('智能图书馆导览系统已就绪', 'success');
    
    // 注册全局无操作监听，提升终端长期运行稳定性和安全性
    registerInactivityEvents();
}

// 启动应用
document.addEventListener('DOMContentLoaded', initApp);