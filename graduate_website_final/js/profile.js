/* 个人中心功能 - 数据全部来自数据库 */

async function initProfilePage() {
    await loadProfileInfo();
    await loadBorrowedBooks();
}

async function loadProfileInfo() {
    var nameEl = document.getElementById('profile-name');
    var cardEl = document.getElementById('profile-card');
    var phoneEl = document.getElementById('profile-phone');
    if (!nameEl) return;

    var user = AppState.currentUser;
    if (!user) {
        nameEl.textContent = '未登录';
        cardEl.textContent = '卡号: --';
        if (phoneEl) phoneEl.textContent = '电话: --';
        return;
    }

    var cardNumber = user.card_number || user.id || '';
    try {
        var res = await fetch(API_BASE_URL + '/reader_profile?card_number=' + encodeURIComponent(cardNumber));
        var data = await res.json();
        if (data.error) {
            nameEl.textContent = user.name || user.username || '读者';
            cardEl.textContent = '卡号: ' + cardNumber;
            if (phoneEl) phoneEl.textContent = '电话: --';
            return;
        }
        nameEl.textContent = data.name || '读者';
        cardEl.textContent = '卡号: ' + (data.card_number || cardNumber);
        if (phoneEl) phoneEl.textContent = '电话: ' + (data.phone || '--');
    } catch (e) {
        console.error('加载个人资料失败', e);
        nameEl.textContent = user.name || user.username || '读者';
        cardEl.textContent = '卡号: ' + cardNumber;
        if (phoneEl) phoneEl.textContent = '电话: --';
    }
}

async function loadBorrowedBooks() {
    var listEl = document.getElementById('borrowed-books-list');
    var currentEl = document.getElementById('current-borrows');
    var totalEl = document.getElementById('total-borrows');
    if (!listEl) return;

    var user = AppState.currentUser;
    if (!user) {
        safeSetInnerHTML(listEl, '<div class="no-results"><i class="fas fa-exclamation-triangle fa-3x"></i><p>请先登录</p></div>');
        if (currentEl) currentEl.textContent = '-';
        if (totalEl) totalEl.textContent = '-';
        return;
    }

    var cardNumber = user.card_number || user.id || '';
    try {
        var res = await fetch(API_BASE_URL + '/reader_borrows?card_number=' + encodeURIComponent(cardNumber));
        var data = await res.json();
        var borrows = data.borrows || [];
        if (currentEl) currentEl.textContent = borrows.length;
        if (totalEl) totalEl.textContent = data.total || borrows.length;

        if (borrows.length === 0) {
            safeSetInnerHTML(listEl, '<div class="no-results"><i class="fas fa-book-open fa-3x"></i><p>暂无借阅书籍</p></div>');
            return;
        }

        var colors = ['#4CAF50', '#2196F3', '#FF9800', '#9C27B0', '#009688', '#F44336', '#795548', '#607D8B'];
        var html = borrows.map(function(b, i) {
            var dueDate = new Date(b.due_time);
            var now = new Date();
            var isOverdue = now > dueDate;
            var color = colors[i % colors.length];
            return '<div class="book-item">' +
                '<div class="book-cover" style="background-color:' + color + ';"><i class="fas fa-book"></i></div>' +
                '<div class="book-details">' +
                '<div class="book-title">' + escapeHtml(b.book_title || '未知书籍') + '</div>' +
                '<div class="book-meta">ISBN: ' + escapeHtml(b.isbn || '--') + '</div>' +
                '<div class="book-meta">应还日期: ' + (b.due_time ? dueDate.toLocaleDateString() : '--') + '</div>' +
                (isOverdue ? '<div class="book-status status-borrowed" style="color:#F44336;">已逾期</div>' : '') +
                '</div></div>';
        }).join('');
        safeSetInnerHTML(listEl, html);
    } catch (e) {
        console.error('加载借阅信息失败', e);
        safeSetInnerHTML(listEl, '<div class="no-results"><i class="fas fa-exclamation-triangle fa-3x"></i><p>加载失败，请检查网络</p></div>');
        if (currentEl) currentEl.textContent = '-';
        if (totalEl) totalEl.textContent = '-';
    }
}

function showBorrowHistory() {
    showPage('borrow-history-page');
    loadBorrowHistory();
}

async function loadBorrowHistory() {
    var listEl = document.getElementById('borrow-history-list');
    if (!listEl) return;

    var user = AppState.currentUser;
    if (!user) {
        safeSetInnerHTML(listEl, '<div class="no-results"><i class="fas fa-exclamation-triangle fa-3x"></i><p>请先登录</p></div>');
        return;
    }

    var cardNumber = user.card_number || user.id || '';
    try {
        var res = await fetch(API_BASE_URL + '/reader_history?card_number=' + encodeURIComponent(cardNumber));
        var data = await res.json();
        var history = data.history || [];

        if (history.length === 0) {
            safeSetInnerHTML(listEl, '<div class="no-results"><i class="fas fa-history fa-3x"></i><p>暂无借阅历史</p></div>');
            return;
        }

        var html = history.map(function(r) {
            var returned = !!r.return_time;
            var borrowDate = r.borrow_time ? new Date(r.borrow_time).toLocaleDateString() : '--';
            var dueDate = r.due_time ? new Date(r.due_time).toLocaleDateString() : '--';
            var returnDate = r.return_time ? new Date(r.return_time).toLocaleDateString() : null;
            return '<div class="history-item">' +
                '<div class="history-book-info">' +
                '<div class="history-book-title">' + escapeHtml(r.book_title || '未知书籍') + '</div>' +
                '<div class="history-book-meta">' +
                '<span class="history-meta-item">借阅: ' + borrowDate + '</span>' +
                '<span class="history-meta-item">应还: ' + dueDate + '</span>' +
                (returnDate ? '<span class="history-meta-item">已还: ' + returnDate + '</span>' : '<span class="history-meta-item" style="color:#FF9800;">未归还</span>') +
                '</div></div>' +
                '<div class="history-status-badge ' + (returned ? 'status-returned' : 'status-borrowed') + '">' +
                (returned ? '已归还' : '借阅中') +
                '</div></div>';
        }).join('');
        safeSetInnerHTML(listEl, html);
    } catch (e) {
        console.error('加载借阅历史失败', e);
        safeSetInnerHTML(listEl, '<div class="no-results"><i class="fas fa-exclamation-triangle fa-3x"></i><p>加载失败，请检查网络</p></div>');
    }
}

function showSettings() {
    showNotification('设置功能开发中', 'info');
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function safeSetInnerHTML(el, html) {
    if (!el) return;
    el.innerHTML = html;
}
