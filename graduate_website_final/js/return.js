/* 自助还书功能
 * 支持从服务器数据库还书
 */
let currentReturnStep = 1;
let returnBook = null;

async function startReturnScan() {
    showNotification('请将书籍条形码对准摄像头...', 'info');
    const cameraPlaceholder = document.querySelector('#return-step1 .camera-placeholder');
    if (cameraPlaceholder) {
        safeSetInnerHTML(cameraPlaceholder, '<div class="scanning-animation"><i class="fas fa-barcode fa-3x"></i><p>扫描中...</p><div class="scan-line"></div></div>');
    }
    
    // 模拟扫描，实际应连接硬件
    setTimeout(() => {
        // 从本地已借出的书中选择（实际应用中从RFID扫描获取）
        const borrowedBooks = AppState.mockBooks.filter(book => book.status === 'borrowed' || book.status === 1);
        
        if (borrowedBooks.length > 0) {
            returnBook = borrowedBooks[Math.floor(Math.random() * borrowedBooks.length)];
            showNotification(`扫描成功：《${returnBook.title}》`, 'success');
            if (cameraPlaceholder) {
                safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-check-circle fa-3x text-success"></i><p>扫描成功！</p>');
            }
            const bookInfoDiv = document.getElementById('return-book-info');
            if (bookInfoDiv) {
                safeSetInnerHTML(bookInfoDiv, `
                    <div style="text-align: center;">
                        <h4 style="font-size: var(--font-xl); margin-bottom: var(--spacing-md);">${escapeHtml(returnBook.title)}</h4>
                        <p style="font-size: var(--font-base); margin-bottom: var(--spacing-xs);">作者：${escapeHtml(returnBook.author || '')}</p>
                        <p style="font-size: var(--font-base); margin-bottom: var(--spacing-md);">ISBN：${escapeHtml(returnBook.isbn || '')}</p>
                        <div style="display: inline-block; padding: var(--spacing-sm) var(--spacing-md); background-color: #fff3cd; color: #856404; border-radius: var(--border-radius); font-weight: 600;">待归还</div>
                    </div>
                `);
            }
            nextReturnStep();
        } else {
            showNotification('未找到待归还的图书', 'error');
            if (cameraPlaceholder) {
                safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-camera fa-3x"></i><p>摄像头预览</p>');
            }
        }
    }, 2000);
}

function detectBookReturn() {
    showNotification('正在检测书籍放入...', 'info');
    setTimeout(() => {
        showNotification('检测到书籍已放入还书口', 'success');
        nextReturnStep();
    }, 1500);
}

async function completeReturn() {
    if (!returnBook) {
        showNotification('还书信息不完整', 'error');
        return;
    }
    
    showNotification('正在处理还书请求...', 'info');
    
    try {
        // 优先尝试服务器还书
        const serverConnected = await checkServerConnection();
        if (serverConnected) {
            const readerCard = AppState.currentUser ? (AppState.currentUser.id || AppState.currentUser.card_number) : '';
            const result = await returnBook(readerCard, returnBook.rfid_uid || returnBook.id);
            
            if (result.ok) {
                showNotification(`还书成功！《${returnBook.title}》`, 'success');
                
                // 更新本地状态
                if (AppState.currentUser) {
                    AppState.currentUser.currentBorrows = Math.max(0, (AppState.currentUser.currentBorrows || 1) - 1);
                }
                
                setTimeout(() => {
                    resetReturnProcess();
                    showPage('main-menu');
                }, 2000);
                return;
            } else {
                showNotification(result.message || '还书失败', 'error');
                return;
            }
        }
    } catch (error) {
        console.error('[RETURN] 还书失败:', error);
    }
    
    // 回退：本地模拟还书
    returnBook.status = 'available';
    if (AppState.currentUser) {
        AppState.currentUser.currentBorrows = Math.max(0, (AppState.currentUser.currentBorrows || 1) - 1);
    }
    
    showNotification(`还书成功（离线模式）！《${returnBook.title}》`, 'success');
    
    setTimeout(() => {
        resetReturnProcess();
        const targetPage = document.location.pathname.indexOf('pages/user') !== -1 ? 'main-menu.html' : 'pages/user/main-menu.html';
        window.location.href = targetPage;
    }, 2000);
}

function nextReturnStep() {
    const currentStep = document.getElementById(`return-step${currentReturnStep}`);
    if (currentStep) currentStep.classList.remove('active');
    currentReturnStep++;
    const nextStep = document.getElementById(`return-step${currentReturnStep}`);
    if (nextStep) nextStep.classList.add('active');
    
    const statusMessage = document.getElementById('return-status-message');
    if (statusMessage) {
        const messages = ['请扫描书籍条形码', '请将书籍放入还书口', '还书完成'];
        statusMessage.textContent = messages[currentReturnStep - 1] || '还书流程完成';
    }
}

function resetReturnProcess() {
    currentReturnStep = 1;
    returnBook = null;
    document.querySelectorAll('.return-steps .step').forEach(step => step.classList.remove('active'));
    const firstStep = document.getElementById('return-step1');
    if (firstStep) firstStep.classList.add('active');
    const cameraPlaceholder = document.querySelector('#return-step1 .camera-placeholder');
    if (cameraPlaceholder) {
        safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-camera fa-3x"></i><p>摄像头预览</p>');
    }
    const bookInfoDiv = document.getElementById('return-book-info');
    if (bookInfoDiv) safeSetInnerHTML(bookInfoDiv, '<!-- 还书信息将动态显示 -->');
    const statusMessage = document.getElementById('return-status-message');
    if (statusMessage) statusMessage.textContent = '请按照步骤完成还书流程';
}
