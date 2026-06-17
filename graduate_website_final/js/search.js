/* 智能图书馆导览机器人 - 图书检索功能 */

function initSearchPage() {
    loadCategories();
    const searchInput = document.getElementById('search-input');
    if (searchInput) {
        searchInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') performSearch();
        });
    }
}

async function loadCategories() {
    const select = document.getElementById('category-filter');
    if (!select) return;
    try {
        const res = await fetch(API_BASE_URL + '/categories');
        const data = await res.json();
        const categories = data.categories || [];
        // 保留第一项"所有分类"
        select.innerHTML = '<option value="">所有分类</option>';
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
            showNotification('找到 ' + books.length + ' 本相关图书', 'success');
        } else {
            showNotification('未找到相关图书', 'info');
        }
    } catch (error) {
        console.error('[SEARCH] 搜索失败:', error);
        showNotification('搜索失败，请检查网络连接', 'error');
    }
}

function displaySearchResults(books) {
    const resultsList = document.getElementById('results-list');
    const resultsCount = document.getElementById('results-count');

    if (!resultsList || !resultsCount) return;

    if (!books || books.length === 0) {
        safeSetInnerHTML(resultsList, '<div class="no-results"><i class="fas fa-book-open fa-3x"></i><p>未找到相关图书</p></div>');
        resultsCount.textContent = '0 条结果';
        return;
    }

    resultsCount.textContent = books.length + ' 条结果';

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

async function showBookDetail(bookId) {
    const modal = document.getElementById('book-detail-modal');
    const content = document.getElementById('book-detail-content');
    if (!modal || !content) return;

    showNotification('加载中...', 'info');
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
            '<tr><td class="label">作者</td><td>' + escapeHtml(book.author || '-') + '</td></tr>' +
            '<tr><td class="label">ISBN</td><td>' + escapeHtml(book.isbn || '-') + '</td></tr>' +
            '<tr><td class="label">出版社</td><td>' + escapeHtml(book.publisher || '-') + '</td></tr>' +
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

function clearSearch() {
    document.getElementById('search-input').value = '';
    document.getElementById('category-filter').value = '';
    displaySearchResults([]);
}

function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function safeSetInnerHTML(el, html) {
    if (!el) return;
    el.innerHTML = html;
}