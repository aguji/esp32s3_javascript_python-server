# 图书馆扫码 + RFID 后端（毕业设计简化版）
# 依赖: pip install flask pymysql
from flask import Flask, request, render_template, jsonify
import json
import secrets

app = Flask(__name__)


@app.after_request
def add_cors_headers(response):
    """允许浏览器跨域访问（Live Server / 手机与 PC 不同端口或域名时必需）"""
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type"
    return response


# ========== MySQL（改成你自己的密码）==========
try:
    import pymysql
    from pymysql.cursors import DictCursor
except ImportError:
    pymysql = None
    DictCursor = None

DB_CONFIG = {
    "host": "127.0.0.1",
    "user": "root",
    "password": "YOUR_MYSQL_PASSWORD",
    "database": "gradata",
    "charset": "utf8mb4",
    "cursorclass": DictCursor,
}


def get_db():
    if pymysql is None:
        raise RuntimeError("请执行: pip install pymysql")
    return pymysql.connect(**DB_CONFIG)


# 扫码登录 token（内存，重启清空）
tokens = {}


@app.route("/health")
def health():
    """不依赖数据库的健康检查（用于ESP32连通性测试）"""
    db_ok = False
    try:
        conn = get_db()
        conn.ping(reconnect=True)
        conn.close()
        db_ok = True
    except Exception:
        pass
    return jsonify({"status": "ok", "db": "ok" if db_ok else "disconnected"})


@app.route("/reader_profile")
def api_reader_profile():
    """
    获取读者个人资料（只读字段，绝不返回 password）
    """
    card_number = (request.args.get("card_number") or "").strip()
    if not card_number:
        return jsonify({"error": "缺少 card_number"}), 400

    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT name, card_number, phone, is_admin, created_at
                FROM tb_reader
                WHERE card_number = %s
                LIMIT 1
                """,
                (card_number,),
            )
            row = cur.fetchone()
        conn.close()

        if not row:
            return jsonify({"error": "读者不存在"}), 404

        return jsonify({
            "name": row.get("name") or "",
            "card_number": row.get("card_number") or "",
            "phone": row.get("phone") or "",
            "is_admin": bool(row.get("is_admin") or 0),
            "created_at": str(row.get("created_at") or ""),
        })

    except Exception as e:
        print(f"[SERVER] reader_profile error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/reader_borrows")
def api_reader_borrows():
    """获取读者的当前借阅信息"""
    card_number = (request.args.get("card_number") or "").strip()
    if not card_number:
        return jsonify({"borrows": [], "count": 0})
    
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("SELECT id FROM tb_reader WHERE card_number = %s LIMIT 1", (card_number,))
            row_r = cur.fetchone()
            if not row_r:
                conn.close()
                return jsonify({"borrows": [], "count": 0})
            
            reader_id = row_r["id"]
            cur.execute(
                """
                SELECT b.id, b.borrow_time, b.due_time, bk.title AS book_title, bk.isbn,
                       bk.rfid_uid AS book_uid
                FROM tb_borrow b
                JOIN tb_book bk ON b.book_id = bk.id
                WHERE b.reader_id = %s AND b.status = 0 AND b.return_time IS NULL
                ORDER BY b.borrow_time DESC
                """,
                (reader_id,),
            )
            rows = cur.fetchall()
            
            # 统计总借阅数
            cur.execute(
                "SELECT COUNT(*) as total FROM tb_borrow WHERE reader_id = %s",
                (reader_id,),
            )
            total_row = cur.fetchone()
        conn.close()
        
        borrows = []
        for row in rows:
            borrows.append({
                "id": row.get("id", 0),
                "book_title": row.get("book_title") or "",
                "isbn": row.get("isbn") or "",
                "book_uid": (row.get("book_uid") or "").strip(),
                "borrow_time": str(row.get("borrow_time") or ""),
                "due_time": str(row.get("due_time") or ""),
            })
        
        return jsonify({
            "borrows": borrows,
            "count": len(borrows),
            "total": total_row.get("total", 0) if total_row else 0
        })
    except Exception as e:
        print(f"[SERVER] reader_borrows error: {e}")
        return jsonify({"borrows": [], "count": 0, "error": str(e)}), 500


@app.route("/reader_history")
def api_reader_history():
    """获取读者的借阅历史"""
    card_number = (request.args.get("card_number") or "").strip()
    if not card_number:
        return jsonify({"history": []})
    
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("SELECT id FROM tb_reader WHERE card_number = %s LIMIT 1", (card_number,))
            row_r = cur.fetchone()
            if not row_r:
                conn.close()
                return jsonify({"history": []})
            
            reader_id = row_r["id"]
            cur.execute(
                """
                SELECT b.id, b.borrow_time, b.due_time, b.return_time, b.status,
                       bk.title AS book_title, bk.isbn
                FROM tb_borrow b
                JOIN tb_book bk ON b.book_id = bk.id
                WHERE b.reader_id = %s
                ORDER BY b.borrow_time DESC
                LIMIT 50
                """,
                (reader_id,),
            )
            rows = cur.fetchall()
        conn.close()
        
        history = []
        for row in rows:
            history.append({
                "id": row.get("id", 0),
                "book_title": row.get("book_title") or "",
                "isbn": row.get("isbn") or "",
                "borrow_time": str(row.get("borrow_time") or ""),
                "due_time": str(row.get("due_time") or ""),
                "return_time": str(row.get("return_time") or "") if row.get("return_time") else None,
                "status": int(row.get("status") or 0),
            })
        
        return jsonify({"history": history})
    except Exception as e:
        print(f"[SERVER] reader_history error: {e}")
        return jsonify({"history": [], "error": str(e)}), 500


# ========== 管理后台（聚合统计 + 列表 + 检索；无单独统计表，用 COUNT 计算）==========


@app.route("/admin/stats")
def admin_stats():
    """用户总数、图书总数、当前有未还借阅的读者人数、累计借阅笔数"""
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("SELECT COUNT(*) AS n FROM tb_reader")
            reader_count = int((cur.fetchone() or {}).get("n") or 0)
            cur.execute("SELECT COUNT(*) AS n FROM tb_book")
            book_count = int((cur.fetchone() or {}).get("n") or 0)
            cur.execute(
                """
                SELECT COUNT(DISTINCT reader_id) AS n FROM tb_borrow
                WHERE status = 0 AND return_time IS NULL
                """
            )
            active_reader_count = int((cur.fetchone() or {}).get("n") or 0)
            cur.execute("SELECT COUNT(*) AS n FROM tb_borrow")
            total_borrow_records = int((cur.fetchone() or {}).get("n") or 0)
        conn.close()
        return jsonify({
            "reader_count": reader_count,
            "book_count": book_count,
            "active_reader_count": active_reader_count,
            "total_borrow_records": total_borrow_records,
        })
    except Exception as e:
        print(f"[SERVER] admin_stats error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/admin/readers")
def admin_readers():
    """读者列表（可选 q：卡号或姓名模糊）"""
    q = (request.args.get("q") or "").strip()
    try:
        conn = get_db()
        with conn.cursor() as cur:
            if q:
                like = f"%{q}%"
                cur.execute(
                    """
                    SELECT r.id, r.card_number, r.name, r.phone, r.is_admin,
                        COALESCE(c.cnt, 0) AS current_borrows,
                        COALESCE(t.cnt, 0) AS total_borrows
                    FROM tb_reader r
                    LEFT JOIN (
                        SELECT reader_id, COUNT(*) AS cnt FROM tb_borrow
                        WHERE status = 0 AND return_time IS NULL GROUP BY reader_id
                    ) c ON c.reader_id = r.id
                    LEFT JOIN (
                        SELECT reader_id, COUNT(*) AS cnt FROM tb_borrow GROUP BY reader_id
                    ) t ON t.reader_id = r.id
                    WHERE r.card_number LIKE %s OR r.name LIKE %s
                    ORDER BY (r.card_number = %s) DESC, r.id
                    LIMIT 50
                    """,
                    (like, like, q),
                )
            else:
                cur.execute(
                    """
                    SELECT r.id, r.card_number, r.name, r.phone, r.is_admin,
                        COALESCE(c.cnt, 0) AS current_borrows,
                        COALESCE(t.cnt, 0) AS total_borrows
                    FROM tb_reader r
                    LEFT JOIN (
                        SELECT reader_id, COUNT(*) AS cnt FROM tb_borrow
                        WHERE status = 0 AND return_time IS NULL GROUP BY reader_id
                    ) c ON c.reader_id = r.id
                    LEFT JOIN (
                        SELECT reader_id, COUNT(*) AS cnt FROM tb_borrow GROUP BY reader_id
                    ) t ON t.reader_id = r.id
                    ORDER BY r.id
                    LIMIT 500
                    """
                )
            rows = cur.fetchall()
        conn.close()
        readers = []
        for row in rows:
            readers.append({
                "id": row.get("id"),
                "card_number": row.get("card_number") or "",
                "name": row.get("name") or "",
                "phone": row.get("phone") or "",
                "is_admin": bool(row.get("is_admin") or 0),
                "current_borrows": int(row.get("current_borrows") or 0),
                "total_borrows": int(row.get("total_borrows") or 0),
            })
        return jsonify({"readers": readers})
    except Exception as e:
        print(f"[SERVER] admin_readers error: {e}")
        return jsonify({"readers": [], "error": str(e)}), 500


@app.route("/admin/books")
def admin_books():
    """图书列表；available=1 时仅 status=0（在架可借）"""
    only_available = (request.args.get("available") or "").strip() == "1"
    q = (request.args.get("q") or "").strip()
    try:
        conn = get_db()
        with conn.cursor() as cur:
            if q:
                like = f"%{q}%"
                if only_available:
                    cur.execute(
                        """
                        SELECT id, title, author, isbn, location, rfid_uid, status
                        FROM tb_book
                        WHERE status = 0 AND (title LIKE %s OR isbn LIKE %s OR author LIKE %s)
                        ORDER BY id LIMIT 100
                        """,
                        (like, like, like),
                    )
                else:
                    cur.execute(
                        """
                        SELECT id, title, author, isbn, location, rfid_uid, status
                        FROM tb_book
                        WHERE title LIKE %s OR isbn LIKE %s OR author LIKE %s
                        ORDER BY id LIMIT 100
                        """,
                        (like, like, like),
                    )
            else:
                if only_available:
                    cur.execute(
                        """
                        SELECT id, title, author, isbn, location, rfid_uid, status
                        FROM tb_book WHERE status = 0 ORDER BY id LIMIT 500
                        """
                    )
                else:
                    cur.execute(
                        """
                        SELECT id, title, author, isbn, location, rfid_uid, status
                        FROM tb_book ORDER BY id LIMIT 500
                        """
                    )
            rows = cur.fetchall()
        conn.close()
        books = []
        for row in rows:
            books.append({
                "id": row.get("id", 0),
                "title": row.get("title") or "",
                "author": row.get("author") or "",
                "isbn": row.get("isbn") or "",
                "location": row.get("location") or "",
                "rfid_uid": (row.get("rfid_uid") or "").strip(),
                "status": int(row.get("status") or 0),
            })
        return jsonify({"books": books})
    except Exception as e:
        print(f"[SERVER] admin_books error: {e}")
        return jsonify({"books": [], "error": str(e)}), 500


@app.route("/admin/search_books")
def admin_search_books():
    """按书名/ISBN/作者模糊查书，并附带当前借阅人（若有）"""
    q = (request.args.get("q") or "").strip()
    if not q:
        return jsonify({"books": []})
    like = f"%{q}%"
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT bk.id, bk.title, bk.author, bk.isbn, bk.location, bk.rfid_uid, bk.status,
                       r.card_number AS borrower_card,
                       r.name AS borrower_name,
                       b.borrow_time
                FROM tb_book bk
                LEFT JOIN tb_borrow b ON b.book_id = bk.id
                    AND b.status = 0 AND b.return_time IS NULL
                LEFT JOIN tb_reader r ON r.id = b.reader_id
                WHERE bk.title LIKE %s OR bk.isbn LIKE %s OR bk.author LIKE %s
                ORDER BY bk.id
                LIMIT 40
                """,
                (like, like, like),
            )
            rows = cur.fetchall()
        conn.close()
        books = []
        for row in rows:
            books.append({
                "id": row.get("id", 0),
                "title": row.get("title") or "",
                "author": row.get("author") or "",
                "isbn": row.get("isbn") or "",
                "location": row.get("location") or "",
                "rfid_uid": (row.get("rfid_uid") or "").strip(),
                "status": int(row.get("status") or 0),
                "borrower_card": row.get("borrower_card") or "",
                "borrower_name": row.get("borrower_name") or "",
                "borrow_time": str(row.get("borrow_time") or "") if row.get("borrow_time") else "",
            })
        return jsonify({"books": books})
    except Exception as e:
        print(f"[SERVER] admin_search_books error: {e}")
        return jsonify({"books": [], "error": str(e)}), 500


@app.route("/admin/reset_reader_password", methods=["POST", "OPTIONS"])
def admin_reset_reader_password():
    if request.method == "OPTIONS":
        return "", 204
    try:
        data = request.get_json() or {}
        card_number = (data.get("card_number") or "").strip()
        new_password = data.get("new_password") or ""
        if not card_number or len(new_password) < 6:
            return jsonify({"ok": False, "message": "参数无效"}), 400
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                "UPDATE tb_reader SET password = %s WHERE card_number = %s",
                (new_password, card_number),
            )
            n = cur.rowcount
        conn.commit()
        conn.close()
        if n == 0:
            return jsonify({"ok": False, "message": "读者不存在"}), 404
        return jsonify({"ok": True, "message": "已重置"})
    except Exception as e:
        print(f"[SERVER] admin_reset_reader_password error: {e}")
        if "conn" in locals() and conn is not None:
            try:
                conn.rollback()
                conn.close()
            except Exception:
                pass
        return jsonify({"ok": False, "message": str(e)}), 500


@app.route("/ping")
def ping():
    return "ok"


@app.route("/register_token", methods=["GET", "POST"])
def register_token():
    token = request.args.get("token")
    if token:
        tokens[token] = {"status": "pending", "user_id": "", "user_name": ""}
        print(f"[SERVER] Token registered: {token}")
        return "ok"
    return "missing token", 400


@app.route("/request_token")
def request_token():
    token = secrets.token_urlsafe(16)
    tokens[token] = {"status": "pending", "user_id": "", "user_name": ""}
    return token


@app.route("/check_token")
def check_token():
    token = request.args.get("token")
    if token not in tokens:
        return jsonify({"status": "invalid", "user_id": "", "user_name": ""})
    d = tokens[token]
    return jsonify(
        {"status": d["status"], "user_id": d["user_id"], "user_name": d["user_name"]}
    )


@app.route("/rfid_login")
def rfid_login():
    """ESP32 刷卡：按 tb_reader.rfid_uid 查读者（大小写不敏感）"""
    uid = (request.args.get("uid") or "").strip()
    if not uid:
        return jsonify({"status": "invalid", "user_id": "", "user_name": ""}), 400
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT id, card_number, name, is_admin
                FROM tb_reader
                WHERE UPPER(TRIM(rfid_uid)) = UPPER(%s)
                LIMIT 1
                """,
                (uid,),
            )
            row = cur.fetchone()
        conn.close()
        if not row:
            return jsonify({"status": "invalid", "user_id": "", "user_name": ""})
        cn_raw = row.get("card_number")
        cn = str(cn_raw).strip() if cn_raw is not None else ""
        user_id_out = cn if cn else str(row["id"])
        return jsonify(
            {
                "status": "success",
                "user_id": user_id_out,
                "user_name": (row.get("name") or "").strip(),
            }
        )
    except Exception as e:
        print(f"[SERVER] rfid_login DB error: {e}")
        return jsonify({"status": "invalid", "user_id": "", "user_name": ""}), 500


@app.route("/borrow", methods=["POST", "OPTIONS"])
def api_borrow():
    """
    借书：当前登录用户（reader_card）+ 图书 RFID。
    tb_book.status：0=可借阅，1=已借出。借书成功后置为 1。
    """
    if request.method == "OPTIONS":
        return "", 204
    raw = request.get_data(cache=True)
    data = {}
    if raw:
        try:
            data = json.loads(raw.decode("utf-8"))
        except Exception:
            data = {}
    if not isinstance(data, dict):
        data = {}
    reader_card = (data.get("reader_card") or "").strip()
    book_uid = (data.get("book_uid") or "").strip()
    if not reader_card or not book_uid:
        print(
            f"[SERVER] borrow 400: raw_len={len(raw)} keys={list(data.keys())!r} "
            f"reader_card={reader_card!r} book_uid={book_uid!r} raw_head={raw[:160]!r}"
        )
        return jsonify({"ok": False, "message": "missing reader_card or book_uid"}), 400
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                "SELECT id FROM tb_reader WHERE card_number = %s LIMIT 1",
                (reader_card,),
            )
            row_r = cur.fetchone()
            if not row_r:
                conn.close()
                return jsonify({"ok": False, "message": "reader not found"}), 404

            reader_id = row_r["id"]
            cur.execute(
                """
                SELECT id, title, status FROM tb_book
                WHERE UPPER(TRIM(rfid_uid)) = UPPER(%s)
                LIMIT 1
                """,
                (book_uid,),
            )
            row_b = cur.fetchone()
            if not row_b:
                conn.close()
                return jsonify({"ok": False, "message": "book not found"}), 404

            book_id = row_b["id"]
            title = row_b.get("title") or ""
            if int(row_b.get("status") or 0) != 0:
                conn.close()
                return jsonify({"ok": False, "message": "book already borrowed"}), 409

            cur.execute(
                """
                SELECT id FROM tb_borrow
                WHERE book_id = %s AND status = 0 AND return_time IS NULL
                LIMIT 1
                """,
                (book_id,),
            )
            if cur.fetchone():
                conn.close()
                return jsonify({"ok": False, "message": "book already on loan"}), 409

            cur.execute(
                """
                INSERT INTO tb_borrow
                (reader_id, book_id, borrow_time, due_time, return_time, status)
                VALUES (%s, %s, NOW(), DATE_ADD(NOW(), INTERVAL 30 DAY), NULL, 0)
                """,
                (reader_id, book_id),
            )
            cur.execute(
                "UPDATE tb_book SET status = 1 WHERE id = %s",
                (book_id,),
            )
        conn.commit()
        conn.close()
        print(f"[SERVER] BORROW ok reader={reader_card} book_uid={book_uid} title={title}")
        return jsonify({"ok": True, "message": "borrowed", "title": title})
    except Exception as e:
        print(f"[SERVER] borrow error: {e}")
        if "conn" in locals() and conn is not None:
            try:
                conn.rollback()
                conn.close()
            except Exception:
                pass
        return jsonify({"ok": False, "message": str(e)}), 500


# ========== 图书搜索 ==========

@app.route("/search")
def api_search():
    """
    搜索图书：支持按书名(title)、作者(author)、ISBN(isbn)搜索
    参数: keyword (搜索关键词)
    """
    keyword = (request.args.get("keyword") or "").strip()
    if not keyword:
        return jsonify({"books": [], "count": 0})

    try:
        conn = get_db()
        with conn.cursor() as cur:
            search_pattern = f"%{keyword}%"
            cur.execute(
                """
                SELECT id, isbn, title, author, publisher, location, category, rfid_uid, status
                FROM tb_book
                WHERE title LIKE %s OR author LIKE %s OR isbn LIKE %s
                LIMIT 50
                """,
                (search_pattern, search_pattern, search_pattern),
            )
            rows = cur.fetchall()
        conn.close()

        books = []
        for row in rows:
            books.append({
                "id": row.get("id", 0),
                "isbn": row.get("isbn") or "",
                "title": row.get("title") or "",
                "author": row.get("author") or "",
                "publisher": row.get("publisher") or "",
                "location": row.get("location") or "",
                "category": row.get("category") or "",
                "rfid_uid": row.get("rfid_uid") or "",
                "status": int(row.get("status") or 0),
            })

        print(f"[SERVER] Search: keyword={keyword!r}, found {len(books)} books")
        return jsonify({"books": books, "count": len(books)})

    except Exception as e:
        print(f"[SERVER] search error: {e}")
        return jsonify({"books": [], "count": 0, "error": str(e)}), 500


@app.route("/categories")
def api_categories():
    """
    获取所有图书分类
    """
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("SELECT DISTINCT TRIM(category) as name FROM tb_book WHERE TRIM(category) != '' ORDER BY name")
            rows = cur.fetchall()
        conn.close()
        categories = [row["name"] for row in rows if row["name"]]
        return jsonify({"categories": categories})
    except Exception as e:
        print(f"[SERVER] categories error: {e}")
        return jsonify({"categories": [], "error": str(e)}), 500


@app.route("/book/<int:book_id>")
def api_book_detail(book_id):
    """
    获取图书详细信息
    """
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT id, isbn, title, author, publisher, location, category, rfid_uid, status
                FROM tb_book WHERE id = %s LIMIT 1
                """,
                (book_id,),
            )
            row = cur.fetchone()
        conn.close()
        if not row:
            return jsonify({"error": "图书不存在"}), 404
        return jsonify({
            "id": row.get("id", 0),
            "isbn": row.get("isbn") or "",
            "title": row.get("title") or "",
            "author": row.get("author") or "",
            "publisher": row.get("publisher") or "",
            "location": row.get("location") or "",
            "category": row.get("category") or "",
            "rfid_uid": row.get("rfid_uid") or "",
            "status": int(row.get("status") or 0),
        })
    except Exception as e:
        print(f"[SERVER] book_detail error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/search_by_category")
def api_search_by_category():
    """
    按 tb_book.category 精确匹配（与终端按钮 Science Tech / Literature 等一致）
    参数: category
    """
    category = (request.args.get("category") or "").strip()
    if not category:
        return jsonify({"books": [], "count": 0})

    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT id, isbn, title, author, publisher, location, category, rfid_uid, status
                FROM tb_book
                WHERE TRIM(category) = %s
                LIMIT 50
                """,
                (category,),
            )
            rows = cur.fetchall()
        conn.close()

        books = []
        for row in rows:
            books.append({
                "id": row.get("id", 0),
                "isbn": row.get("isbn") or "",
                "title": row.get("title") or "",
                "author": row.get("author") or "",
                "publisher": row.get("publisher") or "",
                "location": row.get("location") or "",
                "category": row.get("category") or "",
                "rfid_uid": row.get("rfid_uid") or "",
                "status": int(row.get("status") or 0),
            })

        print(f"[SERVER] Search by category={category!r}, found {len(books)} books")
        return jsonify({"books": books, "count": len(books)})

    except Exception as e:
        print(f"[SERVER] search_by_category error: {e}")
        return jsonify({"books": [], "count": 0, "error": str(e)}), 500


@app.route("/return_book", methods=["POST", "OPTIONS"])
def api_return_book():
    """
    还书：更新 tb_borrow 归还时间；tb_book.status 置回 0（可借阅）。
    """
    if request.method == "OPTIONS":
        return "", 204
    raw = request.get_data(cache=True)
    data = {}
    if raw:
        try:
            data = json.loads(raw.decode("utf-8"))
        except Exception:
            data = {}
    if not isinstance(data, dict):
        data = {}
    reader_card = (data.get("reader_card") or "").strip()
    book_uid = (data.get("book_uid") or "").strip()
    if not reader_card or not book_uid:
        return jsonify({"ok": False, "message": "missing reader_card or book_uid"}), 400
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                "SELECT id FROM tb_reader WHERE card_number = %s LIMIT 1",
                (reader_card,),
            )
            row_r = cur.fetchone()
            if not row_r:
                conn.close()
                return jsonify({"ok": False, "message": "reader not found"}), 404

            reader_id = row_r["id"]
            cur.execute(
                """
                SELECT id FROM tb_book
                WHERE UPPER(TRIM(rfid_uid)) = UPPER(%s)
                LIMIT 1
                """,
                (book_uid,),
            )
            row_b = cur.fetchone()
            if not row_b:
                conn.close()
                return jsonify({"ok": False, "message": "book not found"}), 404

            book_id = row_b["id"]
            cur.execute(
                """
                UPDATE tb_borrow
                SET return_time = NOW(), status = 1
                WHERE reader_id = %s AND book_id = %s
                  AND status = 0 AND return_time IS NULL
                """,
                (reader_id, book_id),
            )
            if cur.rowcount == 0:
                cur.execute(
                    """
                    SELECT id, reader_id, book_id, status, return_time
                    FROM tb_borrow
                    WHERE book_id = %s AND status = 0 AND return_time IS NULL
                    ORDER BY id DESC
                    LIMIT 3
                    """,
                    (book_id,),
                    )
                dbg = cur.fetchall()
                print(
                    f"[SERVER] return_book 409: reader_card={reader_card!r} reader_id={reader_id} "
                    f"book_uid={book_uid!r} book_id={book_id} active_rows_for_book={dbg!r}"
                )
                conn.close()
                return jsonify({"ok": False, "message": "no active borrow for this book"}), 409

            cur.execute(
                "UPDATE tb_book SET status = 0 WHERE id = %s",
                (book_id,),
            )
        conn.commit()
        conn.close()
        print(f"[SERVER] RETURN ok reader={reader_card} book_uid={book_uid}")
        return jsonify({"ok": True, "message": "returned"})
    except Exception as e:
        print(f"[SERVER] return_book error: {e}")
        if "conn" in locals() and conn is not None:
            try:
                conn.rollback()
                conn.close()
            except Exception:
                pass
        return jsonify({"ok": False, "message": str(e)}), 500


@app.route("/verify_password", methods=["POST", "OPTIONS"])
def verify_password():
    """简单的密码验证接口（直接验证，返回用户信息）"""
    if request.method == "OPTIONS":
        return "", 204
    try:
        data = request.get_json() or {}
        username = (data.get("username") or "").strip()
        password = data.get("password") or ""
        
        if not username or not password:
            return jsonify({"success": False, "message": "请输入账号和密码"})
        
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT id, card_number, name, is_admin
                FROM tb_reader
                WHERE card_number = %s AND password = %s
                LIMIT 1
                """,
                (username, password),
            )
            row = cur.fetchone()
        conn.close()
        
        if row:
            return jsonify({
                "success": True,
                "name": row.get("name") or username,
                "card_number": row.get("card_number") or row["id"],
                "is_admin": bool(row.get("is_admin"))
            })
        else:
            return jsonify({"success": False, "message": "账号或密码错误"})
    except Exception as e:
        print(f"[SERVER] verify_password error: {e}")
        return jsonify({"success": False, "message": "服务器错误"}), 500


@app.route("/login", methods=["GET", "POST"])
def login():
    token = request.args.get("token")
    if token not in tokens:
        return "无效的二维码", 404

    if request.method == "POST":
        username = (request.form.get("username") or "").strip()
        password = request.form.get("password") or ""
        try:
            conn = get_db()
            with conn.cursor() as cur:
                cur.execute(
                    """
                    SELECT id, card_number, name, is_admin
                    FROM tb_reader
                    WHERE card_number = %s AND password = %s
                    LIMIT 1
                    """,
                    (username, password),
                )
                row = cur.fetchone()
            conn.close()
            if row:
                tokens[token] = {
                    "status": "success",
                    "user_id": str(row.get("card_number") or row["id"]),
                    "user_name": row.get("name") or "",
                }
                print(f"[SERVER] Login SUCCESS token={token} reader={row.get('name')}")
                return "登录成功，可以关闭页面"
            return "用户名或密码错误"
        except Exception as e:
            print(f"[SERVER] login DB error: {e}")
            return f"数据库错误: {e}", 500

    return render_template("login.html", token=token)


if __name__ == "__main__":
    print("=" * 50)
    print("图书馆服务器  gradata.tb_reader")
    print("=" * 50)
    app.run(host="0.0.0.0", port=5000, debug=False)
