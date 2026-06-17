/* 智能图书馆导览机器人 - 管理后台逻辑
 * 对应页面：pages/admin/admin.html
 * 数据来自 Flask + MySQL（/admin/*、/borrow、/return_book、/reader_borrows 等）
 */
var currentQueryUser = null;

document.addEventListener('DOMContentLoaded', function() {
    if (!requireAdmin()) {
        window.location.href = '../user/main-menu.html';
        return;
    }

    updateTime();
    setInterval(updateTime, 60000);
    if (typeof updateUserStatus === 'function') updateUserStatus();

    Promise.all([renderStats(), renderUserList(), renderBookList()]).catch(function() {});

    bindCollapsibleSections();

    var searchInput = document.getElementById('admin-search-input');
    if (searchInput) {
        searchInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') doSearch();
        });
    }

    var radios = document.querySelectorAll('input[name="search-type"]');
    radios.forEach(function(r) {
        r.addEventListener('change', function() {
            var desc = document.getElementById('search-desc');
            var placeholder = document.getElementById('admin-search-input');
            if (this.value === 'user') {
                if (desc) desc.textContent = '输入卡号或姓名，查看读者信息并代录借还、或以该身份进入自助端。';
                if (placeholder) placeholder.placeholder = '输入卡号或姓名';
            } else {
                if (desc) desc.textContent = '输入书名、ISBN 或作者（可部分匹配），查看在架状态与借阅人，可代为还书。';
                if (placeholder) placeholder.placeholder = '输入书名 / ISBN / 作者';
            }
            document.getElementById('admin-result-user').classList.add('hidden');
            document.getElementById('admin-result-book').classList.add('hidden');
        });
    });
});

function setText(id, value) {
    var el = document.getElementById(id);
    if (el) el.textContent = value;
}

async function renderStats() {
    try {
        var r = await fetch(API_BASE_URL + '/admin/stats');
        var d = await r.json();
        if (d.error) throw new Error(d.error);
        setText('stat-users', String(d.reader_count != null ? d.reader_count : 0));
        setText('stat-books', String(d.book_count != null ? d.book_count : 0));
        setText('stat-borrowing', String(d.active_reader_count != null ? d.active_reader_count : 0));
        setText('stat-total-borrows', String(d.total_borrow_records != null ? d.total_borrow_records : 0));
    } catch (e) {
        console.error(e);
        showNotification('统计数据加载失败，请确认已启动 server.py', 'error');
    }
}

async function renderUserList() {
    var tbody = document.getElementById('user-list-body');
    if (!tbody) return;
    tbody.innerHTML = '<tr><td colspan="6">加载中…</td></tr>';
    try {
        var r = await fetch(API_BASE_URL + '/admin/readers');
        var d = await r.json();
        var readers = d.readers || [];
        tbody.innerHTML = '';
        readers.forEach(function(u) {
            var tr = document.createElement('tr');
            var role = u.is_admin ? 'admin' : 'user';
            tr.innerHTML =
                '<td>' + escapeHtml(u.card_number) + '</td>' +
                '<td>' + escapeHtml(u.name) + '</td>' +
                '<td><span class="role-' + role + '">' + (u.is_admin ? '管理员' : '读者') + '</span></td>' +
                '<td>' + (u.current_borrows != null ? u.current_borrows : 0) + '</td>' +
                '<td>' + (u.total_borrows != null ? u.total_borrows : 0) + '</td>' +
                '<td>' + (u.phone ? escapeHtml(u.phone) : '-') + '</td>';
            tr.style.cursor = 'pointer';
            tr.title = '点击填入检索框并查询';
            tr.onclick = function() {
                document.getElementById('admin-search-input').value = u.card_number;
                var radio = document.querySelector('input[name="search-type"][value="user"]');
                if (radio) radio.checked = true;
                queryUser(u.card_number);
            };
            tbody.appendChild(tr);
        });
        var countEl = document.getElementById('user-list-count');
        if (countEl) countEl.textContent = '共 ' + readers.length + ' 条';
    } catch (e) {
        console.error(e);
        tbody.innerHTML = '<tr><td colspan="6">加载失败</td></tr>';
    }
}

async function renderBookList() {
    var tbody = document.getElementById('book-list-body');
    if (!tbody) return;
    tbody.innerHTML = '<tr><td colspan="6">加载中…</td></tr>';
    try {
        var r = await fetch(API_BASE_URL + '/admin/books');
        var d = await r.json();
        var books = d.books || [];
        tbody.innerHTML = '';
        books.forEach(function(b) {
            var tr = document.createElement('tr');
            var onShelf = Number(b.status) === 0;
            var statusText = onShelf ? '可借' : '已借出';
            var statusClass = onShelf ? 'available' : 'borrowed';
            tr.innerHTML =
                '<td>' + escapeHtml(String(b.id)) + '</td>' +
                '<td>' + escapeHtml(b.title || '-') + '</td>' +
                '<td>' + escapeHtml(b.author || '-') + '</td>' +
                '<td>' + escapeHtml(b.isbn || '-') + '</td>' +
                '<td><span class="status-' + statusClass + '">' + statusText + '</span></td>' +
                '<td>' + escapeHtml(b.location || '-') + '</td>';
            tbody.appendChild(tr);
        });
        var countEl = document.getElementById('book-list-count');
        if (countEl) countEl.textContent = '共 ' + books.length + ' 条';
    } catch (e) {
        console.error(e);
        tbody.innerHTML = '<tr><td colspan="6">加载失败</td></tr>';
    }
}

function bindCollapsibleSections() {
    document.querySelectorAll('.admin-section-collapsible .admin-section-head, .admin-section-collapsible .admin-toggle-btn').forEach(function(el) {
        el.addEventListener('click', function(e) {
            e.preventDefault();
            var section = el.closest('.admin-section-collapsible');
            if (section) section.classList.toggle('collapsed');
            var btn = section && section.querySelector('.admin-toggle-btn');
            if (btn) btn.setAttribute('aria-expanded', section.classList.contains('collapsed') ? 'false' : 'true');
        });
    });
}

function doSearch() {
    var keyword = (document.getElementById('admin-search-input').value || '').trim();
    if (!keyword) {
        showNotification('请输入检索内容', 'warning');
        return;
    }
    var type = (document.querySelector('input[name="search-type"]:checked') || {}).value || 'user';
    if (type === 'user') {
        queryUser(keyword);
    } else {
        queryBook(keyword);
    }
}

async function queryUser(keyword) {
    keyword = (keyword || (document.getElementById('admin-search-input').value || '')).trim();
    if (!keyword) {
        showNotification('请输入卡号或姓名', 'warning');
        return;
    }
    try {
        var r = await fetch(API_BASE_URL + '/admin/readers?q=' + encodeURIComponent(keyword));
        var d = await r.json();
        var readers = d.readers || [];
        if (!readers.length) {
            showNotification('未找到该读者', 'warning');
            document.getElementById('admin-result-user').classList.add('hidden');
            document.getElementById('admin-result-book').classList.add('hidden');
            return;
        }
        if (readers.length > 1) {
            showNotification('找到 ' + readers.length + ' 位读者，已显示排序最靠前的一条（卡号完全匹配优先）', 'info');
        }
        var u = readers[0];
        currentQueryUser = {
            id: u.card_number,
            card_number: u.card_number,
            name: u.name,
            is_admin: u.is_admin,
            role: u.is_admin ? 'admin' : 'user',
            phone: u.phone || ''
        };
        document.getElementById('admin-result-book').classList.add('hidden');
        document.getElementById('admin-result-user').classList.remove('hidden');
        await showUserDetailFromApi(currentQueryUser);
    } catch (e) {
        console.error(e);
        showNotification('读者检索失败', 'error');
    }
}

async function showUserDetailFromApi(user) {
    setText('query-user-name', user.name + '（' + user.card_number + '）');
    setText('query-user-meta', '加载中…');
    var currentList = document.getElementById('query-current-borrows');
    var historyList = document.getElementById('query-borrow-history');
    if (currentList) currentList.innerHTML = '';
    if (historyList) historyList.innerHTML = '';

    try {
        var card = encodeURIComponent(user.card_number);
        var rb = await fetch(API_BASE_URL + '/reader_borrows?card_number=' + card);
        var rbData = await rb.json();
        var rh = await fetch(API_BASE_URL + '/reader_history?card_number=' + card);
        var rhData = await rh.json();

        var current = rbData.borrows || [];
        var total = rbData.total != null ? rbData.total : 0;
        var meta = '当前在借册数：' + current.length + ' | 历史借阅笔数：' + total;
        if (user.phone) meta += ' | 电话：' + user.phone;
        setText('query-user-meta', meta);

        if (currentList) {
            if (current.length === 0) {
                currentList.innerHTML = '<li>无</li>';
            } else {
                current.forEach(function(b) {
                    var li = document.createElement('li');
                    li.textContent = (b.book_title || '') + '（ISBN ' + (b.isbn || '-') + '）';
                    currentList.appendChild(li);
                });
            }
        }

        if (historyList) {
            var history = (rhData.history || []).slice(0, 10);
            if (history.length === 0) {
                historyList.innerHTML = '<li>无</li>';
            } else {
                history.forEach(function(h) {
                    var li = document.createElement('li');
                    var ret = h.return_time ? '已还' : '未还';
                    li.textContent = (h.book_title || '') + ' — ' + ret;
                    historyList.appendChild(li);
                });
            }
        }
    } catch (e) {
        console.error(e);
        setText('query-user-meta', '详情加载失败');
        if (currentList) currentList.innerHTML = '<li>加载失败</li>';
        if (historyList) historyList.innerHTML = '<li>加载失败</li>';
    }
}

async function queryBook(keyword) {
    keyword = (keyword || (document.getElementById('admin-search-input').value || '')).trim();
    if (!keyword) {
        showNotification('请输入检索词', 'warning');
        return;
    }
    document.getElementById('admin-result-user').classList.add('hidden');
    var wrap = document.getElementById('admin-result-book');
    var listEl = document.getElementById('admin-book-result-list');
    listEl.innerHTML = '<p>加载中…</p>';
    wrap.classList.remove('hidden');

    try {
        var r = await fetch(API_BASE_URL + '/admin/search_books?q=' + encodeURIComponent(keyword));
        var d = await r.json();
        var matched = d.books || [];
        listEl.innerHTML = '';
        if (matched.length === 0) {
            listEl.innerHTML = '<p class="no-result">未找到匹配的图书</p>';
            return;
        }
        matched.forEach(function(book) {
            var card = document.createElement('div');
            card.className = 'book-result-card';
            var onLoan = Number(book.status) === 1;
            var statusText = onLoan ? '已借出' : '可借';
            var borrowerHtml = '';
            if (onLoan && book.borrower_card && book.rfid_uid) {
                borrowerHtml =
                    '<p class="book-borrower">借阅人：' + escapeHtml(book.borrower_name || '') +
                    '（' + escapeHtml(book.borrower_card) + '）</p>' +
                    '<button type="button" class="btn-primary btn-small" onclick="setBookReturned(' +
                    JSON.stringify(book.borrower_card) + ',' + JSON.stringify(book.rfid_uid) +
                    ')"><i class="fas fa-check"></i> 代为还书</button>';
            }
            card.innerHTML =
                '<div class="book-result-title">' + escapeHtml(book.title || '') + '</div>' +
                '<p class="book-result-meta">' + escapeHtml(book.author || '') + ' · ' +
                escapeHtml(book.location || '') + ' · 状态：' + statusText + '</p>' +
                borrowerHtml;
            listEl.appendChild(card);
        });
    } catch (e) {
        console.error(e);
        listEl.innerHTML = '<p class="no-result">检索失败</p>';
    }
}

async function setBookReturned(borrowerCard, bookUid) {
    try {
        var res = await fetch(API_BASE_URL + '/return_book', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ reader_card: borrowerCard, book_uid: bookUid })
        });
        var d = await res.json().catch(function() { return {}; });
        if (!d.ok) {
            showNotification(d.message || '还书失败', 'error');
            return;
        }
        showNotification('已代为还书', 'success');
        await Promise.all([renderStats(), renderUserList(), renderBookList()]);
        var kw = (document.getElementById('admin-search-input').value || '').trim();
        await queryBook(kw);
        if (currentQueryUser) await showUserDetailFromApi(currentQueryUser);
    } catch (e) {
        console.error(e);
        showNotification('还书请求失败', 'error');
    }
}

function adminRecordBorrowClose() {
    document.getElementById('modal-record-borrow').classList.add('hidden');
}

async function adminRecordBorrowOpen() {
    if (!currentQueryUser) {
        showNotification('请先检索并选中一位读者', 'warning');
        return;
    }
    document.getElementById('modal-borrow-user').textContent = '读者：' + currentQueryUser.name + '（' + currentQueryUser.card_number + '）';
    document.getElementById('modal-borrow-search').value = '';
    document.getElementById('modal-borrow-results').innerHTML = '';
    document.getElementById('modal-borrow-book-uid').value = '';
    document.getElementById('modal-borrow-selected').textContent = '';
    document.getElementById('modal-record-borrow').classList.remove('hidden');

    // 回车触发搜索
    var input = document.getElementById('modal-borrow-search');
    input.focus();
    var onEnter = function(e) {
        if (e.key === 'Enter') {
            e.preventDefault();
            adminSearchBorrowBooks();
            input.removeEventListener('keypress', onEnter);
        }
    };
    input.addEventListener('keypress', onEnter);
}

async function adminSearchBorrowBooks() {
    var keyword = (document.getElementById('modal-borrow-search').value || '').trim();
    var resultsEl = document.getElementById('modal-borrow-results');
    resultsEl.innerHTML = '<p class="modal-borrow-hint" style="padding:8px 12px;">搜索中…</p>';
    try {
        var url = API_BASE_URL + '/admin/books?available=1';
        if (keyword) url += '&q=' + encodeURIComponent(keyword);
        var r = await fetch(url);
        var d = await r.json().catch(function() { return {}; });
        var books = d.books || [];
        resultsEl.innerHTML = '';
        if (!books.length) {
            resultsEl.innerHTML = '<p class="modal-borrow-hint" style="padding:8px 12px;">无匹配结果</p>';
            return;
        }
        var hasRfid = false;
        books.forEach(function(b) {
            var btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'admin-borrow-pick';
            if (!b.rfid_uid) {
                btn.disabled = true;
                btn.textContent = (b.title || '-') + ' — ' + (b.author || '') + '（无 RFID，无法借阅）';
            } else {
                hasRfid = true;
                btn.textContent = (b.title || '-') + ' — ' + (b.author || '') + ' — ISBN ' + (b.isbn || '-') + ' — ' + (b.location || '');
                btn.onclick = function() {
                    var prev = resultsEl.querySelector('.is-selected');
                    if (prev) prev.classList.remove('is-selected');
                    btn.classList.add('is-selected');
                    document.getElementById('modal-borrow-book-uid').value = b.rfid_uid;
                    document.getElementById('modal-borrow-selected').textContent = '已选：《' + b.title + '》';
                };
            }
            resultsEl.appendChild(btn);
        });
        if (!hasRfid) {
            resultsEl.innerHTML = '<p class="modal-borrow-hint" style="padding:8px 12px;">无匹配结果（或匹配的图书均缺少 RFID）</p>';
        }
    } catch (e) {
        console.error(e);
        resultsEl.innerHTML = '<p class="modal-borrow-hint" style="padding:8px 12px;">加载失败</p>';
    }
}

async function adminRecordBorrowSubmit() {
    var bookUid = (document.getElementById('modal-borrow-book-uid').value || '').trim();
    if (!currentQueryUser || !bookUid) {
        showNotification('请先搜索并选中一本图书', 'warning');
        return;
    }
    try {
        var res = await fetch(API_BASE_URL + '/borrow', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ reader_card: currentQueryUser.card_number, book_uid: bookUid })
        });
        var d = await res.json().catch(function() { return {}; });
        if (!d.ok) {
            showNotification(d.message || '借书失败', 'error');
            return;
        }
        adminRecordBorrowClose();
        showNotification('已代录借书' + (d.title ? '：《' + d.title + '》' : ''), 'success');
        await Promise.all([renderStats(), renderUserList(), renderBookList()]);
        await showUserDetailFromApi(currentQueryUser);
    } catch (e) {
        console.error(e);
        showNotification('借书请求失败', 'error');
    }
}

async function adminRecordReturnOpen() {
    if (!currentQueryUser) {
        showNotification('请先检索并选中一位读者', 'warning');
        return;
    }
    var select = document.getElementById('modal-return-borrow');
    select.innerHTML = '<option value="">加载中…</option>';
    document.getElementById('modal-return-user').textContent = '读者：' + currentQueryUser.name + '（' + currentQueryUser.card_number + '）';
    document.getElementById('modal-record-return').classList.remove('hidden');

    try {
        var r = await fetch(API_BASE_URL + '/reader_borrows?card_number=' + encodeURIComponent(currentQueryUser.card_number));
        var d = await r.json();
        var borrows = d.borrows || [];
        select.innerHTML = '<option value="">请选择</option>';
        borrows.forEach(function(b) {
            if (!b.book_uid) return;
            var opt = document.createElement('option');
            opt.value = b.book_uid;
            opt.textContent = (b.book_title || '') + '（应还 ' + (b.due_time || '') + '）';
            select.appendChild(opt);
        });
        if (borrows.length === 0) {
            showNotification('该读者当前无在借图书记录', 'warning');
        }
    } catch (e) {
        console.error(e);
        select.innerHTML = '<option value="">加载失败</option>';
    }
}

function adminRecordReturnClose() {
    document.getElementById('modal-record-return').classList.add('hidden');
}

async function adminRecordReturnSubmit() {
    var bookUid = (document.getElementById('modal-return-borrow').value || '').trim();
    if (!currentQueryUser || !bookUid) {
        showNotification('请选择要归还的借阅', 'warning');
        return;
    }
    try {
        var res = await fetch(API_BASE_URL + '/return_book', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ reader_card: currentQueryUser.card_number, book_uid: bookUid })
        });
        var d = await res.json().catch(function() { return {}; });
        if (!d.ok) {
            showNotification(d.message || '还书失败', 'error');
            return;
        }
        adminRecordReturnClose();
        showNotification('已代录还书', 'success');
        await Promise.all([renderStats(), renderUserList(), renderBookList()]);
        await showUserDetailFromApi(currentQueryUser);
    } catch (e) {
        console.error(e);
        showNotification('还书请求失败', 'error');
    }
}

var ADMIN_DEFAULT_RESET_PASSWORD = '123456';

function adminResetUserPasswordOpen() {
    if (!currentQueryUser) {
        showNotification('请先检索并选中一位用户', 'warning');
        return;
    }
    document.getElementById('modal-reset-pwd-user').textContent = '用户：' + currentQueryUser.name + '（' + currentQueryUser.card_number + '）';
    document.getElementById('modal-reset-password').classList.remove('hidden');
}

function adminResetUserPasswordClose() {
    document.getElementById('modal-reset-password').classList.add('hidden');
}

async function submitAdminResetPasswordToDefault() {
    if (!currentQueryUser) return;
    try {
        var res = await fetch(API_BASE_URL + '/admin/reset_reader_password', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                card_number: currentQueryUser.card_number,
                new_password: ADMIN_DEFAULT_RESET_PASSWORD
            })
        });
        var d = await res.json().catch(function() { return {}; });
        if (!d.ok) {
            showNotification(d.message || '重置失败', 'error');
            return;
        }
        adminResetUserPasswordClose();
        showNotification('已将该用户密码重置为 123456', 'success');
    } catch (err) {
        console.error(err);
        showNotification('重置密码请求失败', 'error');
    }
}

function loginAsUser() {
    if (!currentQueryUser) {
        showNotification('请先检索并选中一位读者', 'warning');
        return;
    }
    if (currentQueryUser.is_admin) {
        showNotification('不能以管理员账号切换到用户端', 'warning');
        return;
    }
    var user = {
        id: currentQueryUser.card_number,
        name: currentQueryUser.name,
        card_number: currentQueryUser.card_number,
        role: 'user'
    };
    localStorage.setItem('isLoggedIn', 'true');
    localStorage.setItem('currentUser', JSON.stringify(user));
    showNotification('已切换为「' + user.name + '」，将跳转到用户主菜单', 'success');
    setTimeout(function() {
        window.location.href = '../user/main-menu.html';
    }, 800);
}
