// ===== 帮助页面功能 =====
function changeFontSize(size) {
    AppState.settings.fontSize = size;
    document.querySelectorAll('.font-size-controls .btn-small').forEach(btn => btn.classList.remove('active'));
    event.target.classList.add('active');
    const sizes = { small: '14px', medium: '16px', large: '18px' };
    document.documentElement.style.setProperty('--font-base', sizes[size]);
    showNotification(`字体大小已调整为${size === 'small' ? '小' : size === 'medium' ? '中' : '大'}`, 'info');
}

function changeLanguage(lang) {
    AppState.language = lang;
    showNotification(`语言已切换为${lang === 'zh' ? '中文' : '英文'}`, 'info');
}