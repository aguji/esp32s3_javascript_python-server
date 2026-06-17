// ===== 返回按钮逻辑 =====
function goBackToWelcomeOrMenu() {
    if (AppState.isLoggedIn && AppState.currentUser) {
        showPage('main-menu');
    } else {
        showPage('welcome-page');
    }
}