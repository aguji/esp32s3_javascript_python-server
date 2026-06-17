/* 智能图书馆导览机器人 - 自助借书功能
 * 支持从服务器数据库借书
 */
let currentBorrowStep = 1;
let scannedBook = null;

// 初始化借书页面
function initBorrowPage() {
    if (!AppState.currentUser || !AppState.isLoggedIn) {
        showNotification('请先登录', 'warning');
        setTimeout(() => showPage('welcome-page'), 1500);
        return;
    }
    resetBorrowProcess();
    updateBorrowStatus('欢迎 ' + AppState.currentUser.name + '，请扫描书籍条形码');
}

function scanRFID() {
    showNotification('正在识别校园卡...', 'info');
    updateBorrowStatus('正在识别...');
    setTimeout(() => {
        showNotification('身份识别成功', 'success');
        updateBorrowStatus('请扫描书籍条形码');
    }, 1500);
}

function startBarcodeScan() {
    if (!AppState.currentUser || !AppState.isLoggedIn) {
        showNotification('请先登录', 'warning');
        return;
    }
    
    showNotification('请将书籍条形码对准摄像头...', 'info');
    const cameraPlaceholder = document.getElementById('camera-placeholder');
    if (cameraPlaceholder) {
        safeSetInnerHTML(cameraPlaceholder, '<div style="text-align: center;"><span style="font-size: 3rem; display: block; margin-bottom: var(--spacing-md);">📷</span><p style="font-size: var(--font-lg);">扫描中...</p></div>');
    }
    
    // 模拟扫描过程，实际应连接硬件
    setTimeout(async () => {
        // 从本地mockBooks随机选一本（实际应用中这里应从RFID扫描获取）
        const availableBooks = AppState.mockBooks.filter(book => book.status === 'available' || book.status === 0);
        
        if (availableBooks.length > 0) {
            scannedBook = availableBooks[Math.floor(Math.random() * availableBooks.length)];
            
            showNotification(`扫描成功：《${scannedBook.title}》`, 'success');
            if (cameraPlaceholder) {
                safeSetInnerHTML(cameraPlaceholder, '<div style="text-align: center;"><span style="font-size: 3rem; display: block; margin-bottom: var(--spacing-md); color: var(--secondary-color);">✓</span><p style="font-size: var(--font-lg); color: var(--secondary-color);">扫描成功！</p></div>');
            }
            
            const bookInfoDiv = document.getElementById('book-info');
            if (bookInfoDiv) {
                safeSetInnerHTML(bookInfoDiv, `
                    <div style="text-align: center;">
                        <h4 style="font-size: var(--font-xl); margin-bottom: var(--spacing-md);">${escapeHtml(scannedBook.title)}</h4>
                        <p style="font-size: var(--font-base); margin-bottom: var(--spacing-xs);">作者：${escapeHtml(scannedBook.author || '')}</p>
                        <p style="font-size: var(--font-base); margin-bottom: var(--spacing-xs);">ISBN：${escapeHtml(scannedBook.isbn || '')}</p>
                        <p style="font-size: var(--font-base); margin-bottom: var(--spacing-md);">位置：${escapeHtml(scannedBook.location || '')}</p>
                        <div style="display: inline-block; padding: var(--spacing-sm) var(--spacing-md); background-color: #d4edda; color: #155724; border-radius: var(--border-radius); font-weight: 600;">可借阅</div>
                    </div>
                `);
            }
            nextBorrowStep();
            updateBorrowStatus('请确认借阅信息');
        } else {
            showNotification('未找到可借阅的图书', 'error');
            if (cameraPlaceholder) {
                safeSetInnerHTML(cameraPlaceholder, '<span style="font-size: 3rem;">📷</span><p>摄像头预览</p>');
            }
        }
    }, 2000);
}

async function confirmBorrow() {
    if (!scannedBook || !AppState.currentUser) {
        showNotification('借书信息不完整', 'error');
        return;
    }
    
    showNotification('正在处理借书请求...', 'info');
    updateBorrowStatus('正在处理借书请求...');
    
    try {
        // 优先尝试服务器借书
        const serverConnected = await checkServerConnection();
        if (serverConnected) {
            const readerCard = AppState.currentUser.id || AppState.currentUser.card_number;
            const result = await borrowBook(readerCard, scannedBook.rfid_uid || scannedBook.id);
            
            if (result.ok) {
                showNotification(`借书成功！《${scannedBook.title}》`, 'success');
                updateBorrowStatus('借书成功！请取走书籍');
                
                // 更新本地状态
                if (AppState.currentUser) {
                    AppState.currentUser.currentBorrows = (AppState.currentUser.currentBorrows || 0) + 1;
                    AppState.currentUser.totalBorrows = (AppState.currentUser.totalBorrows || 0) + 1;
                }
                
                setTimeout(() => {
                    resetBorrowProcess();
                    showPage('main-menu');
                }, 2000);
                return;
            } else {
                showNotification(result.message || '借书失败', 'error');
                updateBorrowStatus(result.message || '借书失败');
                return;
            }
        }
    } catch (error) {
        console.error('[BORROW] 借书失败:', error);
    }
    
    // 回退：本地模拟借书
    scannedBook.status = 'borrowed';
    if (AppState.currentUser) {
        AppState.currentUser.currentBorrows = (AppState.currentUser.currentBorrows || 0) + 1;
        AppState.currentUser.totalBorrows = (AppState.currentUser.totalBorrows || 0) + 1;
    }
    
    showNotification(`借书成功（离线模式）！《${scannedBook.title}》`, 'success');
    updateBorrowStatus('借书成功！请取走书籍');
    
    setTimeout(() => {
        resetBorrowProcess();
        showPage('main-menu');
    }, 2000);
}

function nextBorrowStep() {
    const currentStep = document.getElementById(`step${currentBorrowStep}`);
    if (currentStep) currentStep.classList.remove('active');
    currentBorrowStep++;
    const nextStep = document.getElementById(`step${currentBorrowStep}`);
    if (nextStep) nextStep.classList.add('active');
}

function updateBorrowStatus(message) {
    const statusMessage = document.getElementById('borrow-status-message');
    if (statusMessage) {
        statusMessage.textContent = message;
    }
}

function resetBorrowProcess() {
    currentBorrowStep = 1;
    scannedBook = null;
    document.querySelectorAll('.borrow-steps .step').forEach(step => step.classList.remove('active'));
    const firstStep = document.getElementById('step1');
    if (firstStep) firstStep.classList.add('active');
    const cameraPlaceholder = document.getElementById('camera-placeholder');
    if (cameraPlaceholder) {
        safeSetInnerHTML(cameraPlaceholder, '<span style="font-size: 3rem;">📷</span><p>摄像头预览</p>');
    }
    const bookInfoDiv = document.getElementById('book-info');
    if (bookInfoDiv) safeSetInnerHTML(bookInfoDiv, '<!-- 书籍信息将动态显示 -->');
    updateBorrowStatus('请扫描书籍条形码');
}
