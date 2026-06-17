/* 智能图书馆导览机器人 - JavaScript核心逻辑 */
/* 支持多页面应用、硬件交互模拟、API通信 */

// ===== 全局状态和配置 =====
const AppState = {
    // 从localStorage读取登录状�?    currentUser: JSON.parse(localStorage.getItem('currentUser')) || null,
    isLoggedIn: localStorage.getItem('isLoggedIn') === 'true' || false,
    currentPage: 'welcome-page',
    wifiConnected: true,
    batteryLevel: 100,
    language: 'zh',
    settings: {
        fontSize: 'medium',
        voiceGuide: true,
        highContrast: false
    },
    mockBooks: [
        { id: 1, title: 'JavaScript高级程序设计', author: '尼古拉斯·泽卡�?, isbn: '9787115275790', category: 'computer', status: 'available', location: '三楼A区书�?-2', coverColor: '#4CAF50' },
        { id: 2, title: 'Python编程从入门到实践', author: '埃里克·马瑟斯', isbn: '9787115428028', category: 'computer', status: 'available', location: '三楼A区书�?-5', coverColor: '#2196F3' },
        { id: 3, title: '百年孤独', author: '加西亚·马尔克�?, isbn: '9787544253994', category: 'literature', status: 'borrowed', location: '二楼B区书�?-7', coverColor: '#FF9800' },
        { id: 4, title: '人类简�?, author: '尤瓦尔·赫拉利', isbn: '9787508647357', category: 'history', status: 'available', location: '四楼C区书�?-1', coverColor: '#9C27B0' },
        { id: 5, title: '时间简�?, author: '史蒂芬·霍�?, isbn: '9787535732309', category: 'science', status: 'available', location: '四楼C区书�?-3', coverColor: '#009688' }
    ],
    mockUsers: [
        { id: 'admin', name: '系统管理�?, department: '图书馆管理系�?, currentBorrows: 0, totalBorrows: 0, creditScore: 100, borrowedBooks: [], borrowHistory: [], role: 'admin', password: 'YOUR_PASSWORD' },
        { id: '20230001', name: '张三', department: '计算机科学与技术学�?, currentBorrows: 3, totalBorrows: 42, creditScore: 95, borrowedBooks: [1, 3], borrowHistory: [
            { bookId: 1, bookTitle: 'JavaScript高级程序设计', borrowDate: '2024-01-15', dueDate: '2024-02-15', returnDate: null, status: '借阅�? },
            { bookId: 3, bookTitle: '百年孤独', borrowDate: '2024-02-01', dueDate: '2024-03-01', returnDate: null, status: '借阅�? },
            { bookId: 2, bookTitle: 'Python编程从入门到实践', borrowDate: '2024-01-10', dueDate: '2024-02-10', returnDate: '2024-02-05', status: '已归�? }
        ], role: 'user', password: 'YOUR_PASSWORD' },
        { id: '20230002', name: '李四', department: '文学�?, currentBorrows: 2, totalBorrows: 25, creditScore: 88, borrowedBooks: [2], borrowHistory: [
            { bookId: 2, bookTitle: 'Python编程从入门到实践', borrowDate: '2024-02-10', dueDate: '2024-03-10', returnDate: null, status: '借阅�? },
            { bookId: 4, bookTitle: '人类简�?, borrowDate: '2024-01-20', dueDate: '2024-02-20', returnDate: '2024-02-15', status: '已归�? }
        ], role: 'user', password: 'YOUR_PASSWORD' },
        { id: '20230003', name: '王五', department: '理学�?, currentBorrows: 1, totalBorrows: 18, creditScore: 92, borrowedBooks: [], borrowHistory: [
            { bookId: 5, bookTitle: '时间简�?, borrowDate: '2024-01-25', dueDate: '2024-02-25', returnDate: '2024-02-20', status: '已归�? }
        ], role: 'user', password: 'YOUR_PASSWORD' }
    ]
};

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

// ===== 权限检查函�?=====
function isAdmin() {
    return AppState.currentUser && AppState.currentUser.role === 'admin';
}

function requireAdmin() {
    if (!isAdmin()) {
        showNotification('需要管理员权限才能访问此功�?, 'error');
        return false;
    }
    return true;
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

// ===== 页面管理 =====
function showPage(pageId) {
    document.querySelectorAll('.page').forEach(page => page.classList.remove('active'));
    const targetPage = document.getElementById(pageId);
    if (targetPage) {
        targetPage.classList.add('active');
        AppState.currentPage = pageId;
        switch(pageId) {
            case 'main-menu': updateUserInfo(); break;
            case 'search-page': initSearchPage(); break;
            case 'profile-page': initProfilePage(); break;
            case 'navigation-page': initNavigationPage(); break;
            case 'borrow-history-page': loadBorrowHistory(); break;
        }
        document.getElementById('app').scrollTop = 0;
    }
}

// 页面加载时检查登录状态并显示相应页面
document.addEventListener('DOMContentLoaded', function() {
    // 更新用户状态显�?    updateUserStatus();
    
    // 如果用户已登录，显示主菜单页�?    if (AppState.isLoggedIn && AppState.currentUser) {
        // 检查当前页面是否有主菜单元�?        const mainMenuPage = document.getElementById('main-menu');
        if (mainMenuPage) {
            showPage('main-menu');
        }
    } else {
        // 用户未登录，显示欢迎页面
        const welcomePage = document.getElementById('welcome-page');
        if (welcomePage) {
            showPage('welcome-page');
        }
    }
    
    // 初始化其他功�?    updateTime();
    setInterval(updateTime, 60000);
    setInterval(updateWifiStatus, 10000);
    setInterval(updateBatteryLevel, 30000);
});

// ===== 用户认证 =====
function loginWithRFID() {
    showNotification('请将校园卡靠近读卡器...', 'info');
    setTimeout(() => {
        const mockUser = AppState.mockUsers[0];
        AppState.currentUser = mockUser;
        AppState.isLoggedIn = true;
        // 保存到localStorage
        localStorage.setItem('isLoggedIn', 'true');
        localStorage.setItem('currentUser', JSON.stringify(mockUser));
        showNotification(`欢迎回来�?{mockUser.name}！`, 'success');
        updateUserStatus();
        showPage('main-menu');
    }, 2000);
}

function showQRCodeScanner() {
    document.getElementById('password-login-form').classList.add('hidden');
    document.getElementById('qrcode-container').classList.remove('hidden');
    const qrcodeDiv = document.getElementById('qrcode');
    qrcodeDiv.innerHTML = '<i class="fas fa-qrcode fa-5x"></i>';
    setTimeout(() => {
        const mockUser = AppState.mockUsers[0];
        AppState.currentUser = mockUser;
        AppState.isLoggedIn = true;
        // 保存到localStorage
        localStorage.setItem('isLoggedIn', 'true');
        localStorage.setItem('currentUser', JSON.stringify(mockUser));
        showNotification(`扫码成功，欢�?{mockUser.name}！`, 'success');
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
        loginForm.addEventListener('submit', function(e) {
            e.preventDefault();
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            if (username && password) {
                const mockUser = AppState.mockUsers.find(user => user.id === username);
                if (mockUser) {
                    AppState.currentUser = mockUser;
                    AppState.isLoggedIn = true;
                    // 保存到localStorage
                    localStorage.setItem('isLoggedIn', 'true');
                    localStorage.setItem('currentUser', JSON.stringify(mockUser));
                    showNotification(`登录成功，欢�?{mockUser.name}！`, 'success');
                    updateUserStatus();
                    showPage('main-menu');
                    this.reset();
                    hidePasswordLogin();
                } else {
                    showNotification('账号或密码错误，请重�?, 'error');
                }
            }
        });
    }
});

function logout() {
    AppState.currentUser = null;
    AppState.isLoggedIn = false;
    // 清除localStorage
    localStorage.removeItem('isLoggedIn');
    localStorage.removeItem('currentUser');
    showNotification('已退出登�?, 'info');
    updateUserStatus();
    window.location.href = 'index.html';
}

function updateUserStatus() {
    const userNameElement = document.getElementById('user-name');
    const loggedUserElement = document.getElementById('logged-user');
    if (AppState.isLoggedIn && AppState.currentUser) {
        if (userNameElement) userNameElement.textContent = AppState.currentUser.name;
        if (loggedUserElement) loggedUserElement.textContent = '欢迎�? + AppState.currentUser.name;
    } else {
        if (userNameElement) userNameElement.textContent = '未登�?;
        if (loggedUserElement) loggedUserElement.textContent = '欢迎您！';
    }
}

function updateUserInfo() {
    // 个人中心信息�?loadProfileInfo / loadBorrowedBooks 接管
    // 此函数保留给其他页面同步顶部状态栏使用
}

// ===== 图书检索功�?=====
function initSearchPage() {
    loadCategories();
    const searchInput = document.getElementById('search-input');
    if (searchInput) {
        searchInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') performSearch();
        });
    }
}

// ===== 图书检索功�?=====
async function performSearch() {
    const searchTerm = document.getElementById('search-input').value.trim();
    const category = document.getElementById('category-filter').value;

    if (!searchTerm && !category) {
        displaySearchResults([]);
        return;
    }

    showNotification('正在搜索...', 'info');

    try {
        let books = [];
        if (searchTerm) {
            const res = await fetch(API_BASE_URL + '/search?keyword=' + encodeURIComponent(searchTerm));
            const data = await res.json();
            books = data.books || [];
            if (category) {
                books = books.filter(function(b) {
                    return (b.category || '').trim() === category;
                });
            }
        } else if (category) {
            const res = await fetch(API_BASE_URL + '/search_by_category?category=' + encodeURIComponent(category));
            const data = await res.json();
            books = data.books || [];
        }

        displaySearchResults(books);

        if (books.length > 0) {
            showNotification('找到 ' + books.length + ' 本相关图�?, 'success');
        } else {
            showNotification('未找到相关图�?, 'info');
        }
    } catch (error) {
        console.error('[SEARCH] 搜索失败:', error);
        showNotification('搜索失败，请检查网络连�?, 'error');
    }
}

function displaySearchResults(books) {
    const resultsList = document.getElementById('results-list');
    const resultsCount = document.getElementById('results-count');

    if (!resultsList || !resultsCount) return;

    if (!books || books.length === 0) {
        safeSetInnerHTML(resultsList, '<div class="no-results"><i class="fas fa-book-open fa-3x"></i><p>未找到相关图�?/p></div>');
        resultsCount.textContent = '0 条结�?;
        return;
    }

    resultsCount.textContent = books.length + ' 条结�?;

    const colors = ['#4CAF50', '#2196F3', '#FF9800', '#9C27B0', '#009688', '#F44336', '#795548', '#607D8B'];

    const html = books.map(function(book, i) {
        const color = colors[i % colors.length];
        return '<div class="book-item" onclick="showBookDetail(' + book.id + ')">' +
            '<div class="book-cover" style="background-color:' + color + ';"><i class="fas fa-book"></i></div>' +
            '<div class="book-details">' +
            '<div class="book-title">' + escapeHtml(book.title || '') + '</div>' +
            '<div class="book-author">' + escapeHtml(book.author || '') + '</div>' +
            '<div class="book-meta"><span>ISBN: ' + escapeHtml(book.isbn || '') + '</span></div>' +
            '<div class="book-meta"><span>位置: ' + escapeHtml(book.location || '') + '</span></div>' +
            '</div></div>';
    }).join('');

    safeSetInnerHTML(resultsList, html);
}

async function loadCategories() {
    const select = document.getElementById('category-filter');
    if (!select) return;
    try {
        const res = await fetch(API_BASE_URL + '/categories');
        const data = await res.json();
        const categories = data.categories || [];
        select.innerHTML = '<option value="">所有分�?/option>';
        categories.forEach(function(cat) {
            var opt = document.createElement('option');
            opt.value = cat;
            opt.textContent = cat;
            select.appendChild(opt);
        });
    } catch (e) {
        console.error('加载分类失败', e);
    }
}

async function showBookDetail(bookId) {
    const modal = document.getElementById('book-detail-modal');
    const content = document.getElementById('book-detail-content');
    if (!modal || !content) return;

    showNotification('加载�?..', 'info');
    try {
        const res = await fetch(API_BASE_URL + '/book/' + bookId);
        const book = await res.json();
        if (book.error) {
            showNotification(book.error, 'error');
            return;
        }

        safeSetInnerHTML(content, '<div class="book-detail-card">' +
            '<div class="book-detail-title">' + escapeHtml(book.title || '') + '</div>' +
            '<table class="book-detail-table">' +
            '<tr><td class="label">作�?/td><td>' + escapeHtml(book.author || '-') + '</td></tr>' +
            '<tr><td class="label">ISBN</td><td>' + escapeHtml(book.isbn || '-') + '</td></tr>' +
            '<tr><td class="label">出版�?/td><td>' + escapeHtml(book.publisher || '-') + '</td></tr>' +
            '<tr><td class="label">分类</td><td>' + escapeHtml(book.category || '-') + '</td></tr>' +
            '<tr><td class="label">馆藏位置</td><td>' + escapeHtml(book.location || '-') + '</td></tr>' +
            '<tr><td class="label">RFID编号</td><td>' + escapeHtml(book.rfid_uid || '-') + '</td></tr>' +
            '</table></div>');

        modal.classList.remove('hidden');
    } catch (e) {
        console.error('加载详情失败', e);
        showNotification('加载详情失败', 'error');
    }
}

function closeBookDetail() {
    const modal = document.getElementById('book-detail-modal');
    if (modal) modal.classList.add('hidden');
}

// 清空搜索条件
function clearSearch() {
    document.getElementById('search-input').value = '';
    document.getElementById('category-filter').value = '';
    displaySearchResults([]);
}

// 高级搜索（示例功能）
function advancedSearch() {
    showNotification('高级搜索功能开发中，敬请期待！', 'info');
    // 这里可以扩展为显示高级搜索面�?}

// ===== 自助借书功能 =====
let currentBorrowStep = 1;
let scannedBook = null;

function scanRFID() {
    showNotification('正在识别校园�?..', 'info');
    setTimeout(() => {
        if (AppState.currentUser) {
            showNotification(`识别成功�?{AppState.currentUser.name}`, 'success');
        } else {
            const mockUser = AppState.mockUsers[0];
            AppState.currentUser = mockUser;
            AppState.isLoggedIn = true;
            updateUserStatus();
            showNotification(`识别成功�?{mockUser.name}`, 'success');
        }
        nextBorrowStep();
    }, 1500);
}

function startBarcodeScan() {
    showNotification('请将书籍条形码对准摄像头...', 'info');
    const cameraPlaceholder = document.querySelector('#step2 .camera-placeholder');
    if (cameraPlaceholder) safeSetInnerHTML(cameraPlaceholder, '<div class="scanning-animation"><i class="fas fa-barcode fa-3x"></i><p>扫描�?..</p><div class="scan-line"></div></div>');
    
    setTimeout(() => {
        const availableBooks = AppState.mockBooks.filter(book => book.status === 'available');
        if (availableBooks.length > 0) {
            scannedBook = availableBooks[Math.floor(Math.random() * availableBooks.length)];
            showNotification(`扫描成功：�?{scannedBook.title}》`, 'success');
            if (cameraPlaceholder) safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-check-circle fa-3x text-success"></i><p>扫描成功�?/p>');
            const bookInfoDiv = document.getElementById('book-info');
            if (bookInfoDiv) safeSetInnerHTML(bookInfoDiv, `<h4>${escapeHtml(scannedBook.title)}</h4><p>作者：${escapeHtml(scannedBook.author)}</p><p>ISBN�?{escapeHtml(scannedBook.isbn)}</p><p>位置�?{escapeHtml(scannedBook.location)}</p><div class="book-status status-available">可借阅</div>`);
            nextBorrowStep();
        } else {
            showNotification('未找到可借阅的图�?, 'error');
        }
    }, 2000);
}

function confirmBorrow() {
    if (!scannedBook || !AppState.currentUser) {
        showNotification('借书信息不完�?, 'error');
        return;
    }
    showNotification('正在处理借书请求...', 'info');
    setTimeout(() => {
        scannedBook.status = 'borrowed';
        if (AppState.currentUser) {
            AppState.currentUser.currentBorrows++;
            AppState.currentUser.totalBorrows++;
            AppState.currentUser.borrowedBooks.push(scannedBook.id);
        }
        showNotification(`借书成功！�?{scannedBook.title}》`, 'success');
        setTimeout(() => {
            resetBorrowProcess();
            window.location.href = 'pages/user/main-menu.html';
        }, 500); // 缩短�?00毫秒
    }, 1500);
}

function nextBorrowStep() {
    const currentStep = document.getElementById(`step${currentBorrowStep}`);
    if (currentStep) currentStep.classList.remove('active');
    currentBorrowStep++;
    const nextStep = document.getElementById(`step${currentBorrowStep}`);
    if (nextStep) nextStep.classList.add('active');
    const statusMessage = document.getElementById('borrow-status-message');
    if (statusMessage) {
        const messages = ['请刷卡识别身�?, '请扫描书籍条形码', '请确认借阅信息'];
        statusMessage.textContent = messages[currentBorrowStep - 1] || '借书流程完成';
    }
}

function resetBorrowProcess() {
    currentBorrowStep = 1;
    scannedBook = null;
    document.querySelectorAll('.borrow-steps .step').forEach(step => step.classList.remove('active'));
    const firstStep = document.getElementById('step1');
    if (firstStep) firstStep.classList.add('active');
    const cameraPlaceholder = document.querySelector('#step2 .camera-placeholder');
    if (cameraPlaceholder) safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-camera fa-3x"></i><p>摄像头预�?/p>');
    const bookInfoDiv = document.getElementById('book-info');
    if (bookInfoDiv) safeSetInnerHTML(bookInfoDiv, '<!-- 书籍信息将动态显�?-->');
    const statusMessage = document.getElementById('borrow-status-message');
    if (statusMessage) statusMessage.textContent = '请按照步骤完成借书流程';
}

// ===== 自助还书功能 =====
let currentReturnStep = 1;
let returnBook = null;

function startReturnScan() {
    showNotification('请将书籍条形码对准摄像头...', 'info');
    const cameraPlaceholder = document.querySelector('#return-step1 .camera-placeholder');
    if (cameraPlaceholder) safeSetInnerHTML(cameraPlaceholder, '<div class="scanning-animation"><i class="fas fa-barcode fa-3x"></i><p>扫描�?..</p><div class="scan-line"></div></div>');
    
    setTimeout(() => {
        const borrowedBooks = AppState.mockBooks.filter(book => book.status === 'borrowed');
        if (borrowedBooks.length > 0) {
            returnBook = borrowedBooks[Math.floor(Math.random() * borrowedBooks.length)];
            showNotification(`扫描成功：�?{returnBook.title}》`, 'success');
            if (cameraPlaceholder) safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-check-circle fa-3x text-success"></i><p>扫描成功�?/p>');
            const bookInfoDiv = document.getElementById('return-book-info');
            if (bookInfoDiv) safeSetInnerHTML(bookInfoDiv, `<h4>${escapeHtml(returnBook.title)}</h4><p>作者：${escapeHtml(returnBook.author)}</p><p>ISBN�?{escapeHtml(returnBook.isbn)}</p><div class="book-status status-borrowed">待归�?/div>`);
            nextReturnStep();
        } else {
            showNotification('未找到待归还的图�?, 'error');
        }
    }, 2000);
}

function detectBookReturn() {
    showNotification('正在检测书籍放�?..', 'info');
    setTimeout(() => {
        showNotification('检测到书籍已放入还书口', 'success');
        nextReturnStep();
    }, 1500);
}

function completeReturn() {
    if (!returnBook) {
        showNotification('还书信息不完�?, 'error');
        return;
    }
    showNotification('正在处理还书请求...', 'info');
    setTimeout(() => {
        returnBook.status = 'available';
        if (AppState.currentUser) AppState.currentUser.currentBorrows = Math.max(0, AppState.currentUser.currentBorrows - 1);
        showNotification(`还书成功！�?{returnBook.title}》`, 'success');
        setTimeout(() => {
            resetReturnProcess();
            window.location.href = 'pages/user/main-menu.html';
        }, 2000);
    }, 1500);
}

function nextReturnStep() {
    const currentStep = document.getElementById(`return-step${currentReturnStep}`);
    if (currentStep) currentStep.classList.remove('active');
    currentReturnStep++;
    const nextStep = document.getElementById(`return-step${currentReturnStep}`);
    if (nextStep) nextStep.classList.add('active');
    const statusMessage = document.getElementById('return-status-message');
    if (statusMessage) {
        const messages = ['请扫描书籍条形码', '请将书籍放入还书�?, '还书完成'];
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
    if (cameraPlaceholder) safeSetInnerHTML(cameraPlaceholder, '<i class="fas fa-camera fa-3x"></i><p>摄像头预�?/p>');
    const bookInfoDiv = document.getElementById('return-book-info');
    if (bookInfoDiv) safeSetInnerHTML(bookInfoDiv, '<!-- 还书信息将动态显�?-->');
    const statusMessage = document.getElementById('return-status-message');
    if (statusMessage) statusMessage.textContent = '请按照步骤完成还书流�?;
}

// ===== 导航指引功能 =====
function initNavigationPage() {
    // 楼层按钮点击切换地图
    document.querySelectorAll('.floor-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.querySelectorAll('.floor-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            drawLibraryMap(this.getAttribute('data-floor'));
        });
    });
    // 初始绘制一楼地�?    drawLibraryMap('1');
}

function drawLibraryMap(floor) {
    const svg = document.getElementById('library-map-svg');
    if (!svg) return;
    svg.innerHTML = '';
    
    // 添加背景
    const bg = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    bg.setAttribute('width', '800');
    bg.setAttribute('height', '300');
    bg.setAttribute('fill', '#f5f5f5');
    svg.appendChild(bg);
    
    // 添加楼层标题
    const title = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    title.setAttribute('x', '400');
    title.setAttribute('y', '30');
    title.setAttribute('text-anchor', 'middle');
    title.setAttribute('font-size', '22');
    title.setAttribute('fill', '#333');
    title.setAttribute('font-weight', 'bold');
    title.textContent = `图书�?${floor}�?平面图`;
    svg.appendChild(title);
    
    // 定义各楼层书架布局
    const floorLayouts = {
        '1': [
            { x: 80, y: 70, width: 140, height: 55, label: 'A�? },
            { x: 280, y: 70, width: 140, height: 55, label: 'B�? },
            { x: 480, y: 70, width: 140, height: 55, label: 'C�? },
            { x: 80, y: 170, width: 140, height: 55, label: 'D�? },
            { x: 280, y: 170, width: 140, height: 55, label: 'E�? },
            { x: 480, y: 170, width: 140, height: 55, label: 'F�? },
            { x: 680, y: 70, width: 90, height: 155, label: '服务�? }
        ],
        '2': [
            { x: 80, y: 70, width: 140, height: 55, label: 'G�? },
            { x: 280, y: 70, width: 140, height: 55, label: 'H�? },
            { x: 480, y: 70, width: 140, height: 55, label: 'I�? },
            { x: 80, y: 170, width: 140, height: 55, label: 'J�? },
            { x: 280, y: 170, width: 140, height: 55, label: 'K�? },
            { x: 480, y: 170, width: 140, height: 55, label: 'L�? },
            { x: 680, y: 70, width: 90, height: 155, label: '阅览�? }
        ],
        '3': [
            { x: 80, y: 70, width: 180, height: 55, label: 'M�? },
            { x: 320, y: 70, width: 180, height: 55, label: 'N�? },
            { x: 560, y: 70, width: 160, height: 55, label: 'O�? },
            { x: 80, y: 170, width: 180, height: 55, label: 'P�? },
            { x: 320, y: 170, width: 180, height: 55, label: 'Q�? },
            { x: 560, y: 170, width: 160, height: 55, label: 'R�? }
        ],
        '4': [
            { x: 120, y: 70, width: 200, height: 55, label: 'S�? },
            { x: 380, y: 70, width: 200, height: 55, label: 'T�? },
            { x: 120, y: 170, width: 200, height: 55, label: 'U�? },
            { x: 380, y: 170, width: 200, height: 55, label: 'V�? },
            { x: 640, y: 100, width: 130, height: 100, label: '自习�? }
        ]
    };
    
    const areas = floorLayouts[floor] || floorLayouts['1'];
    
    // 绘制书架区域
    areas.forEach(area => {
        const shelf = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        shelf.setAttribute('x', area.x.toString());
        shelf.setAttribute('y', area.y.toString());
        shelf.setAttribute('width', area.width.toString());
        shelf.setAttribute('height', area.height.toString());
        shelf.setAttribute('fill', '#FF9800');
        shelf.setAttribute('stroke', '#E65100');
        shelf.setAttribute('stroke-width', '2');
        shelf.setAttribute('rx', '8');
        svg.appendChild(shelf);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        label.setAttribute('x', (area.x + area.width / 2).toString());
        label.setAttribute('y', (area.y + area.height / 2).toString());
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('dominant-baseline', 'middle');
        label.setAttribute('font-size', '18');
        label.setAttribute('font-weight', 'bold');
        label.setAttribute('fill', '#FFF');
        label.textContent = area.label;
        svg.appendChild(label);
    });
}

// ===== 帮助页面功能 =====
function changeFontSize(size) {
    AppState.settings.fontSize = size;
    document.querySelectorAll('.font-size-controls .btn-small').forEach(btn => btn.classList.remove('active'));
    event.target.classList.add('active');
    const sizes = { small: '14px', medium: '16px', large: '18px' };
    document.documentElement.style.setProperty('--font-base', sizes[size]);
    showNotification(`字体大小已调整为${size === 'small' ? '�? : size === 'medium' ? '�? : '�?}`, 'info');
}

function changeLanguage(lang) {
    AppState.language = lang;
    showNotification(`语言已切换为${lang === 'zh' ? '中文' : '英文'}`, 'info');
}

// ===== 通知系统 =====
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
            wifiStatusElement.textContent = AppState.wifiConnected ? '已连�? : '未连�?;
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

// ===== 返回按钮逻辑 =====
function goBackToWelcomeOrMenu() {
    if (AppState.isLoggedIn && AppState.currentUser) {
        showPage('main-menu');
    } else {
        showPage('welcome-page');
    }
}

// ===== 初始化应�?=====
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
            showNotification(`语音提示${this.checked ? '开�? : '关闭'}`, 'info');
        });
    }
    
    if (highContrastCheckbox) {
        highContrastCheckbox.checked = AppState.settings.highContrast;
        highContrastCheckbox.addEventListener('change', function() {
            AppState.settings.highContrast = this.checked;
            document.body.classList.toggle('high-contrast', this.checked);
            showNotification(`高对比度模式${this.checked ? '开�? : '关闭'}`, 'info');
        });
    }
    
    updateUserStatus();
    showNotification('智能图书馆导览系统已就绪', 'success');
}

// 启动应用
document.addEventListener('DOMContentLoaded', initApp);
