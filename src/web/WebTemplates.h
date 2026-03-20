// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>

namespace WebTemplates {

extern const char kDashboardAssetVersion[];
extern const char kDashboardStylesCssPath[];
extern const char kDashboardAppJsPath[];
extern const uint8_t kDashboardShellHtmlGzip[] PROGMEM;
extern const size_t kDashboardShellHtmlGzipSize;
extern const uint8_t kDashboardStylesCssGzip[] PROGMEM;
extern const size_t kDashboardStylesCssGzipSize;
extern const uint8_t kDashboardAppJsGzip[] PROGMEM;
extern const size_t kDashboardAppJsGzipSize;
extern const char kThemeAssetVersion[];
extern const char kThemeStylesCssPath[];
extern const char kThemeAppJsPath[];
extern const uint8_t kThemeShellHtmlGzip[] PROGMEM;
extern const size_t kThemeShellHtmlGzipSize;
extern const uint8_t kThemeStylesCssGzip[] PROGMEM;
extern const size_t kThemeStylesCssGzipSize;
extern const uint8_t kThemeAppJsGzip[] PROGMEM;
extern const size_t kThemeAppJsGzipSize;
extern const char kDacAssetVersion[];
extern const char kDacStylesCssPath[];
extern const char kDacAppJsPath[];
extern const uint8_t kDacShellHtmlGzip[] PROGMEM;
extern const size_t kDacShellHtmlGzipSize;
extern const uint8_t kDacStylesCssGzip[] PROGMEM;
extern const size_t kDacStylesCssGzipSize;
extern const uint8_t kDacAppJsGzip[] PROGMEM;
extern const size_t kDacAppJsGzipSize;

static const char kWifiListScanning[] PROGMEM = R"HTML(
<div class="network-item disabled">
    <div class="network-icon">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="animation: spin 1s linear infinite"><path d="M21 2v6h-6"></path><path d="M3 12a9 9 0 0 1 15-6.7L21 8"></path><path d="M3 22v-6h6"></path><path d="M21 12a9 9 0 0 1-15 6.7L3 16"></path></svg>
    </div>
    <div class="network-info">
        <span class="network-name">Scanning...</span>
        <span class="network-meta">Please wait</span>
    </div>
</div>
)HTML";

static const char kWifiListEmpty[] PROGMEM = R"HTML(
<div class="network-item disabled">
    <div class="network-icon">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20"></line></svg>
    </div>
    <div class="network-info">
        <span class="network-name">No networks found</span>
        <span class="network-meta">Try rescan</span>
    </div>
</div>
)HTML";

static const char kWifiPageTemplate[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Project Aura | WiFi Setup</title>
    <style>
        :root {
            --bg: #0f172a;
            --panel: rgba(30, 41, 59, 0.7);
            --border: rgba(255, 255, 255, 0.1);
            --primary: #6366f1;
            --primary-hover: #818cf8;
            --text: #f1f5f9;
            --text-dim: #94a3b8;
            --item-hover: rgba(255, 255, 255, 0.05);
            --item-selected: rgba(99, 102, 241, 0.15);
            --danger: #ef4444;
        }

        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }

        body, html {
            margin: 0; padding: 0;
            height: 100%; width: 100%;
            overflow: hidden;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg);
            color: var(--text);
        }

        body {
            display: flex; align-items: center; justify-content: center;
        }

        .aura {
            position: fixed; width: 300px; height: 300px; border-radius: 50%;
            background: radial-gradient(circle, var(--primary) 0%, transparent 70%);
            filter: blur(60px); opacity: 0.15; z-index: -1;
            animation: move 10s infinite alternate;
        }
        .aura-1 { top: -50px; left: -50px; }
        .aura-2 { bottom: -50px; right: -50px; animation-delay: -5s; }

        @keyframes move {
            from { transform: translate(0, 0); }
            to { transform: translate(50px, 50px); }
        }

        .container {
            width: 100%; max-width: 420px; height: 100%;
            padding: 20px;
            display: flex; flex-direction: column;
            justify-content: center;
        }

        .card {
            width: 100%;
            background: var(--panel);
            backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--border);
            border-radius: 32px;
            padding: 32px;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
            display: flex; flex-direction: column;
            max-height: 95vh;
            position: relative;
            overflow: hidden;
        }

        .header {
            text-align: center; margin-bottom: 20px; flex-shrink: 0;
            display: flex; flex-direction: column; align-items: center;
        }

        .logo {
            width: 48px; height: 48px;
            background: rgba(99, 102, 241, 0.15);
            border: 1px solid rgba(99, 102, 241, 0.3);
            border-radius: 16px;
            display: flex; align-items: center; justify-content: center;
            margin-bottom: 12px; color: var(--primary);
        }

        h2 { margin: 0; font-size: 24px; font-weight: 700; letter-spacing: -0.025em; }
        .subtitle { color: var(--text-dim); font-size: 13px; margin-top: 4px; }

        form {
            display: flex; flex-direction: column;
            flex: 1; min-height: 0;
        }

        .label-row {
            display: flex; justify-content: space-between; align-items: center;
            margin-bottom: 8px; padding: 0 4px;
        }

        label {
            font-size: 10px; font-weight: 700; text-transform: uppercase;
            letter-spacing: 0.08em; color: var(--text-dim);
        }

        .rescan-btn {
            background: none; border: none; padding: 4px 8px; margin-right: -8px;
            font-size: 10px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.05em;
            color: var(--primary);
            cursor: pointer; display: flex; align-items: center; gap: 4px;
            border-radius: 6px; transition: opacity 0.2s;
            text-decoration: none;
        }
        .rescan-btn:hover { opacity: 0.8; }

        .network-list-container {
            flex: 1;
            min-height: 120px;
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--border);
            border-radius: 16px;
            overflow: hidden;
            display: flex; flex-direction: column;
            margin-bottom: 16px;
        }

        .network-list {
            flex: 1; overflow-y: auto;
            scrollbar-width: thin; scrollbar-color: var(--border) transparent;
        }
        .network-list::-webkit-scrollbar { width: 4px; }
        .network-list::-webkit-scrollbar-thumb { background: var(--border); border-radius: 10px; }

        .network-item {
            display: flex; align-items: center; padding: 12px 16px;
            cursor: pointer; transition: all 0.2s;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
        }
        .network-item:last-child { border-bottom: none; }
        .network-item:hover { background: var(--item-hover); }

        .network-item.selected {
            background: var(--item-selected);
            border-left: 3px solid var(--primary);
            padding-left: 13px;
        }

        .network-icon {
            margin-right: 12px;
            color: var(--text-dim);
            display: flex;
            width: 16px;
            height: 16px;
            flex: 0 0 16px;
            position: relative;
        }
        .network-icon:empty::before {
            content: '';
            position: absolute;
            left: 1px;
            bottom: 1px;
            width: 3px;
            height: 5px;
            border-radius: 1px;
            background: currentColor;
            box-shadow:
                4px -2px 0 0 currentColor,
                8px -4px 0 0 currentColor;
        }
        .network-item.selected .network-icon { color: var(--primary); }

        .network-info { flex: 1; min-width: 0; }
        .network-name { display: block; font-size: 14px; font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        .network-meta { font-size: 11px; color: var(--text-dim); margin-top: 2px; }

        .pass-section { flex-shrink: 0; margin-bottom: 8px; }

        .input-wrapper { position: relative; }

        input[type="password"], input[type="text"] {
            width: 100%;
            background: rgba(15, 23, 42, 0.8);
            border: 1px solid var(--border);
            border-radius: 14px;
            padding: 14px 44px 14px 14px;
            color: white; font-size: 15px;
            transition: all 0.2s; outline: none;
        }
        input:focus {
            border-color: var(--primary);
        }

        .eye-btn {
            position: absolute; right: 8px; top: 50%; transform: translateY(-50%);
            background: none; border: none; padding: 6px;
            color: var(--text-dim); cursor: pointer;
            border-radius: 8px; display: flex; width: 34px; height: 34px;
            align-items: center; justify-content: center;
        }
        .eye-btn:hover { background: rgba(255, 255, 255, 0.05); color: var(--text); }

        button[type="submit"] {
            width: 100%;
            background: var(--primary); color: white;
            border: none; border-radius: 16px; padding: 16px;
            font-size: 15px; font-weight: 700;
            cursor: pointer; transition: all 0.2s;
            box-shadow: 0 10px 15px -3px rgba(99, 102, 241, 0.3);
            flex-shrink: 0; margin-top: 8px;
        }
        button[type="submit"]:hover { background: var(--primary-hover); }
        button[type="submit"]:disabled {
            opacity: 0.5; cursor: not-allowed; box-shadow: none;
            background: #334155; color: #94a3b8;
        }

        .footer {
            margin-top: 16px; text-align: center;
            font-size: 11px; color: var(--text-dim); opacity: 0.6;
            flex-shrink: 0;
        }

        @media (max-width: 480px) {
            .card { padding: 24px 20px; border-radius: 28px; }
            .logo { width: 40px; height: 40px; }
            h2 { font-size: 20px; }
        }

        @keyframes spin { 100% { transform: rotate(360deg); } }
    </style>
</head>
<body>
    <div class="aura aura-1"></div>
    <div class="aura aura-2"></div>

    <div class="container">
        <div class="card">
            <div class="header">
                <div class="logo">
                    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                        <path d="M5 12.55a11 11 0 0 1 14.08 0"></path>
                        <path d="M1.42 9a16 16 0 0 1 21.16 0"></path>
                        <path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path>
                        <line x1="12" y1="20" x2="12.01" y2="20"></line>
                    </svg>
                </div>
                <h2>Project Aura</h2>
                <div class="subtitle">WiFi Configuration</div>
            </div>

            <form method="POST" action="/save" id="wifi-form">
                <input type="hidden" name="ssid" id="selected-ssid">

                <div class="label-row">
                    <label>Select Network</label>
                    <a href="/wifi?scan=1" class="rescan-btn" id="rescan-btn">
                        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21 2v6h-6"></path><path d="M3 12a9 9 0 0 1 15-6.7L21 8"></path><path d="M3 22v-6h6"></path><path d="M21 12a9 9 0 0 1-15 6.7L3 16"></path></svg>
                        Rescan
                    </a>
                </div>

                <div class="network-list-container">
                    <div class="network-list" id="network-list">
                        {{SSID_ITEMS}}
                        <div class="network-item" data-manual="1">
                            <div class="network-icon">
                                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path><path d="M18.5 2.5a2.121 2.121 0 1 1 3 3L12 15l-4 1 1-4 9.5-9.5z"></path></svg>
                            </div>
                            <div class="network-info">
                                <span class="network-name">Manual Entry</span>
                                <span class="network-meta">Add hidden network</span>
                            </div>
                        </div>
                    </div>
                </div>

                <div class="pass-section">
                    <div class="label-row">
                        <label for="pass">Password</label>
                    </div>
                    <div class="input-wrapper">
                        <input type="password" name="pass" id="pass" placeholder="Enter password" autocomplete="current-password">
                        <button type="button" class="eye-btn" onclick="togglePass()" title="Show password">
                            <svg id="eye-svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>
                        </button>
                    </div>
                </div>

                <button type="submit" id="submit-btn" disabled>Connect Now</button>
            </form>
        </div>

        <div class="footer">
            Powered by 21CNCStudio
        </div>
    </div>

    <script>
        function selectNetwork(ssid, element) {
            if (!ssid) return;
            document.getElementById('selected-ssid').value = ssid;

            var items = document.querySelectorAll('.network-item');
            items.forEach(function(item) { item.classList.remove('selected'); });
            element.classList.add('selected');

            var submitBtn = document.getElementById('submit-btn');
            submitBtn.disabled = false;
            submitBtn.textContent = 'Connect to ' + (ssid.length > 15 ? ssid.substring(0, 12) + '...' : ssid);

            setTimeout(function() { document.getElementById('pass').focus(); }, 100);
        }

        function handleManualEntry(element) {
            var ssid = prompt("Enter Network Name (SSID):");
            if (ssid === null || ssid.length === 0) return;

            var nameEl = element.querySelector('.network-name');
            var metaEl = element.querySelector('.network-meta');

            if (nameEl) nameEl.textContent = ssid;
            if (metaEl) metaEl.textContent = "Manual Entry";

            element.setAttribute('data-ssid', ssid);
            selectNetwork(ssid, element);
        }

        function watchScanProgress() {
            var poll = function() {
                fetch('/wifi?scan_status=1&ts=' + Date.now(), { cache: 'no-store' })
                    .then(function(response) {
                        if (!response.ok) throw new Error('HTTP ' + response.status);
                        return response.json();
                    })
                    .then(function(payload) {
                        if (!payload || payload.success !== true) return;
                        if (payload.scan_in_progress === false) {
                            window.location.replace('/wifi');
                            return;
                        }
                        setTimeout(poll, 5000);
                    })
                    .catch(function() {
                        setTimeout(poll, 8000);
                    });
            };
            setTimeout(poll, 2000);
        }

        document.addEventListener('DOMContentLoaded', function() {
            setupNetworkItems();

            var scanInProgress = {{SCAN_IN_PROGRESS}};
            if (scanInProgress) {
                var rescanBtn = document.getElementById('rescan-btn');
                if (rescanBtn) {
                    rescanBtn.innerHTML = '<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="animation: spin 1s linear infinite"><path d="M21 2v6h-6"></path><path d="M3 12a9 9 0 0 1 15-6.7L21 8"></path><path d="M3 22v-6h6"></path><path d="M21 12a9 9 0 0 1-15 6.7L3 16"></path></svg> Scanning...';
                    rescanBtn.style.pointerEvents = 'none';
                    rescanBtn.style.opacity = '0.7';
                }
                watchScanProgress();
            }
        });

        function setupNetworkItems() {
            document.querySelectorAll('.network-item').forEach(function(item) {
                item.addEventListener('click', function() {
                    if (this.getAttribute('data-manual') === '1') {
                        handleManualEntry(this);
                    } else {
                        selectNetwork(this.getAttribute('data-ssid'), this);
                    }
                });
            });
        }

        function togglePass() {
            var x = document.getElementById("pass");
            var svg = document.getElementById("eye-svg");
            if (x.type === "password") {
                x.type = "text";
                svg.innerHTML = '<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line>';
            } else {
                x.type = "password";
                svg.innerHTML = '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle>';
            }
        }
    </script>
</body>
</html>
)HTML";

static const char kWifiSavePage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Project Aura | Setup</title>
    <style>
        :root {
            --bg: #0f172a;
            --panel: rgba(30, 41, 59, 0.8);
            --border: rgba(255, 255, 255, 0.1);
            --primary: #6366f1;
            --text: #f1f5f9;
            --text-dim: #94a3b8;
        }
        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
        body, html {
            margin: 0; padding: 0; height: 100%; width: 100%;
            overflow: hidden; font-family: -apple-system, system-ui, sans-serif;
            background-color: var(--bg); color: var(--text);
        }
        body {
            display: flex; align-items: center; justify-content: center;
        }
        .aura {
            position: fixed; width: 300px; height: 300px; border-radius: 50%;
            background: radial-gradient(circle, var(--primary) 0%, transparent 70%);
            filter: blur(60px); opacity: 0.15; z-index: -1;
            animation: move 10s infinite alternate cubic-bezier(0.45, 0, 0.55, 1);
        }
        .aura-1 { top: -50px; left: -50px; }
        .aura-2 { bottom: -50px; right: -50px; animation-delay: -5s; }
        @keyframes move { from { transform: translate(0, 0); } to { transform: translate(50px, 50px); } }

        .container {
            width: 100%; max-width: 420px; height: 100%; padding: 20px;
            display: flex; flex-direction: column; align-items: center; justify-content: center;
        }

        .card {
            width: 100%; padding: 40px 32px;
            background: var(--panel); backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--border); border-radius: 32px;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
            text-align: center;
        }
        .logo {
            width: 56px; height: 56px; background: rgba(99, 102, 241, 0.15);
            border: 1px solid rgba(99, 102, 241, 0.3); border-radius: 18px;
            display: flex; align-items: center; justify-content: center;
            margin: 0 auto 16px; color: var(--primary);
        }
        h2 { margin: 0; font-size: 24px; font-weight: 700; letter-spacing: -0.025em; }
        .main-text { font-size: 20px; font-weight: 700; margin: 16px 0 8px; }
        .sub-text { font-size: 14px; color: var(--text-dim); line-height: 1.5; margin: 0; }
        .hint { margin-top: 16px; font-size: 12px; color: var(--text-dim); opacity: 0.8; }
        .footer { margin-top: 16px; font-size: 11px; color: var(--text-dim); opacity: 0.6; }
    </style>
</head>
<body>
    <div class="aura aura-1"></div>
    <div class="aura aura-2"></div>

    <div class="container">
        <div class="card">
            <div class="logo">
                <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20"></line></svg>
            </div>
            <h2>Project Aura</h2>
            <div class="main-text">Settings sent</div>
            <p class="sub-text">
                Aura is switching from AP to your Wi-Fi network.
                Wait about {{WAIT_SECONDS}} seconds, then open:
                <br><code>http://&lt;ip&gt;/dashboard</code>
            </p>
            <div class="hint">
                Use the IP shown on the device screen or router DHCP list.
                You can also try: <code>{{HOSTNAME_DASHBOARD_URL}}</code>
                <br>
                AP address <code>http://192.168.4.1</code> is only valid in setup mode.
            </div>
        </div>
        <div class="footer">Powered by 21CNCStudio</div>
    </div>
</body>
</html>
)HTML";

static const char kDiagPageTemplate[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Project Aura | Diagnostics</title>
    <style>
        :root {
            --bg: #0f172a;
            --panel: rgba(30, 41, 59, 0.72);
            --border: rgba(255, 255, 255, 0.12);
            --text: #f1f5f9;
            --text-dim: #94a3b8;
            --ok: #4ade80;
            --warn: #fbbf24;
            --err: #f87171;
        }
        * { box-sizing: border-box; }
        body {
            margin: 0;
            padding: 14px;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            color: var(--text);
            background: radial-gradient(900px 500px at -20% -20%, rgba(99, 102, 241, 0.22), transparent 60%), var(--bg);
        }
        .wrap {
            max-width: 900px;
            margin: 0 auto;
            display: grid;
            gap: 12px;
        }
        .head {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            gap: 10px;
        }
        .title {
            margin: 0;
            font-size: 24px;
            font-weight: 750;
            letter-spacing: -0.02em;
        }
        .stamp {
            color: var(--text-dim);
            font-size: 12px;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 12px;
        }
        .card {
            background: var(--panel);
            border: 1px solid var(--border);
            border-radius: 14px;
            padding: 12px;
            backdrop-filter: blur(8px);
            -webkit-backdrop-filter: blur(8px);
        }
        .card h3 {
            margin: 0 0 10px;
            font-size: 13px;
            color: var(--text-dim);
            letter-spacing: 0.06em;
            text-transform: uppercase;
        }
        .rows { display: grid; gap: 6px; }
        .row {
            display: grid;
            grid-template-columns: 160px 1fr;
            gap: 8px;
            align-items: baseline;
            font-size: 13px;
        }
        .k {
            color: var(--text-dim);
            text-transform: uppercase;
            letter-spacing: 0.04em;
            font-size: 11px;
        }
        .v { word-break: break-word; }
        .badge {
            display: inline-flex;
            align-items: center;
            border-radius: 999px;
            padding: 2px 9px;
            font-weight: 700;
            font-size: 11px;
            text-transform: uppercase;
        }
        .badge.ok { color: var(--ok); background: rgba(34,197,94,0.18); border: 1px solid rgba(34,197,94,0.3); }
        .badge.warn { color: var(--warn); background: rgba(251,191,36,0.18); border: 1px solid rgba(251,191,36,0.3); }
        .badge.err { color: var(--err); background: rgba(248,113,113,0.18); border: 1px solid rgba(248,113,113,0.3); }
        pre {
            margin: 0;
            white-space: pre-wrap;
            font-size: 12px;
            line-height: 1.45;
            color: #dbeafe;
            background: rgba(2, 6, 23, 0.35);
            border: 1px solid var(--border);
            border-radius: 10px;
            padding: 10px;
            max-height: 260px;
            overflow: auto;
        }
        .mono {
            font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace;
        }
        @media (max-width: 760px) {
            .grid { grid-template-columns: 1fr; }
            .row { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="wrap">
        <div class="head">
            <h1 class="title">AURA Diagnostics</h1>
            <div id="stamp" class="stamp">waiting...</div>
        </div>

        <div class="grid">
            <section class="card">
                <h3>Network</h3>
                <div id="networkRows" class="rows"></div>
            </section>
            <section class="card">
                <h3>System</h3>
                <div id="systemRows" class="rows"></div>
            </section>
            <section class="card">
                <h3>OTA</h3>
                <div id="otaRows" class="rows"></div>
            </section>
            <section class="card">
                <h3>Web Stream</h3>
                <div id="webRows" class="rows"></div>
            </section>
            <section class="card">
                <h3>Last Errors</h3>
                <pre id="errors" class="mono">No warnings or errors yet.</pre>
            </section>
        </div>
    </div>

    <script>
        function esc(value) {
            return String(value == null ? '' : value)
                .replace(/&/g, '&amp;')
                .replace(/</g, '&lt;')
                .replace(/>/g, '&gt;');
        }

        function row(key, value) {
            return '<div class="row"><div class="k">' + esc(key) + '</div><div class="v">' + value + '</div></div>';
        }

        function badge(text, cls) {
            return '<span class="badge ' + cls + '">' + esc(text) + '</span>';
        }

        function formatErrors(items) {
            if (!Array.isArray(items) || items.length === 0) {
                return 'No warnings or errors yet.';
            }
            return items.map(function(it) {
                var ts = (it && typeof it.ts_ms === 'number') ? (it.ts_ms + ' ms') : '--';
                var level = (it && it.level) ? it.level : '?';
                var tag = (it && it.tag) ? it.tag : 'SYSTEM';
                var msg = (it && it.message) ? it.message : '';
                return '[' + ts + '] [' + level + '] [' + tag + '] ' + msg;
            }).join('\n');
        }

        function setRows(id, html) {
            var el = document.getElementById(id);
            if (el) el.innerHTML = html;
        }

        var diagPollOkDelayMs = 3000;
        var diagPollRetryDelayMs = 6000;
        var diagPollRetryMaxMs = 10000;
        var diagPollTimer = null;

        function scheduleDiagRefresh(delayMs) {
            if (diagPollTimer) {
                clearTimeout(diagPollTimer);
            }
            diagPollTimer = setTimeout(refreshDiag, delayMs);
        }

        async function refreshDiag() {
            var stamp = document.getElementById('stamp');
            try {
                var res = await fetch('/api/diag', { cache: 'no-store' });
                if (!res.ok) throw new Error('HTTP ' + res.status);
                var data = await res.json();
                if (!data || data.success !== true) throw new Error('Invalid payload');

                var net = data.network || {};
                var heap = data.heap || {};
                var otaBusy = !!data.ota_busy;
                var web = data.web_stream || {};

                setRows('networkRows',
                    row('Mode', esc(net.mode || '--').toUpperCase()) +
                    row('WiFi enabled', net.wifi_enabled ? 'yes' : 'no') +
                    row('SSID', '<span class="mono">' + esc(net.wifi_ssid || '--') + '</span>') +
                    row('IP', '<span class="mono">' + esc(net.ip || '--') + '</span>') +
                    row('Hostname', '<span class="mono">' + esc(net.hostname || '--') + '</span>') +
                    row('RSSI', (typeof net.rssi === 'number') ? (net.rssi + ' dBm') : '--') +
                    row('STA status', String(typeof net.sta_status === 'number' ? net.sta_status : '--')) +
                    row('Scan in progress', net.scan_in_progress ? 'yes' : 'no')
                );

                setRows('systemRows',
                    row('Uptime', (typeof data.uptime_s === 'number' ? data.uptime_s : 0) + ' s') +
                    row('Free heap', (typeof heap.free === 'number' ? heap.free : 0) + ' B') +
                    row('Min free heap', (typeof heap.min_free === 'number' ? heap.min_free : 0) + ' B')
                );

                setRows('otaRows',
                    row('Status', otaBusy ? badge('OTA in progress', 'warn') : badge('Idle', 'ok'))
                );

                setRows('webRows',
                    row('OK', String(typeof web.ok_count === 'number' ? web.ok_count : 0)) +
                    row('Aborts', String(typeof web.abort_count === 'number' ? web.abort_count : 0)) +
                    row('Slow writes', String(typeof web.slow_count === 'number' ? web.slow_count : 0)) +
                    row('Active transfers', String(typeof web.active_transfers === 'number' ? web.active_transfers : 0)) +
                    row('MQTT pause', (typeof web.mqtt_pause_remaining_ms === 'number' && web.mqtt_pause_remaining_ms > 0)
                        ? (web.mqtt_pause_remaining_ms + ' ms')
                        : 'idle') +
                    row('MQTT connect defers', String(typeof web.mqtt_connect_deferred_count === 'number'
                        ? web.mqtt_connect_deferred_count
                        : 0)) +
                    row('MQTT publish defers', String(typeof web.mqtt_publish_deferred_count === 'number'
                        ? web.mqtt_publish_deferred_count
                        : 0)) +
                    row('Last reason', '<span class="mono">' + esc(web.last_abort_reason || 'none') + '</span>') +
                    row('Sent ratio', (typeof web.last_sent_ratio === 'number') ? Math.round(web.last_sent_ratio * 100) + '%' : '--') +
                    row('Last errno', String(typeof web.last_errno === 'number' ? web.last_errno : '--')) +
                    row('Max write', (typeof web.last_max_write_ms === 'number') ? (web.last_max_write_ms + ' ms') : '--') +
                    row('Last URI', '<span class="mono">' + esc(web.last_uri || '--') + '</span>')
                );

                var errorsEl = document.getElementById('errors');
                if (errorsEl) {
                    errorsEl.textContent = formatErrors(data.last_errors);
                }

                if (stamp) stamp.textContent = 'updated ' + new Date().toLocaleTimeString();
                diagPollRetryDelayMs = 6000;
                scheduleDiagRefresh(diagPollOkDelayMs);
            } catch (err) {
                if (stamp) stamp.textContent = 'diag fetch failed: ' + (err && err.message ? err.message : 'error');
                setRows('otaRows', row('Status', badge('No data', 'err')));
                setRows('webRows', row('Status', badge('No data', 'err')));
                var nextRetryMs = diagPollRetryDelayMs;
                diagPollRetryDelayMs = Math.min(diagPollRetryMaxMs, diagPollRetryDelayMs + 2000);
                scheduleDiagRefresh(nextRetryMs);
            }
        }

        refreshDiag();
    </script>
</body>
</html>
)HTML";

static const char kMqttPageTemplate[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Project Aura | MQTT Setup</title>
    <style>
        :root {
            --bg: #0f172a;
            --panel: rgba(30, 41, 59, 0.7);
            --border: rgba(255, 255, 255, 0.1);
            --primary: #6366f1;
            --primary-hover: #818cf8;
            --text: #f1f5f9;
            --text-dim: #94a3b8;
        }

        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }

        body, html {
            margin: 0; padding: 0; min-height: 100%; width: 100%;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg); color: var(--text);
        }

        body {
            display: flex; align-items: center; justify-content: center; padding: 20px;
        }

        /* Анимация фона */
        .aura {
            position: fixed; width: 300px; height: 300px; border-radius: 50%;
            background: radial-gradient(circle, var(--primary) 0%, transparent 70%);
            filter: blur(60px); opacity: 0.15; z-index: -1;
            animation: move 10s infinite alternate;
        }

        /* Верхняя аура удалена по запросу. Оставлена только нижняя для глубины */
        .aura-2 { bottom: -50px; right: -50px; animation-delay: -5s; }

        @keyframes move {
            from { transform: translate(0, 0); }
            to { transform: translate(50px, 50px); }
        }

        .container { width: 100%; max-width: 480px; }

        .card {
            width: 100%; background: var(--panel);
            backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--border); border-radius: 32px;
            padding: 32px; box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
        }

        .header { text-align: center; margin-bottom: 24px; }

        .logo {
            width: 48px; height: 48px; background: rgba(99, 102, 241, 0.15);
            border: 1px solid rgba(99, 102, 241, 0.3); border-radius: 16px;
            display: flex; align-items: center; justify-content: center;
            margin: 0 auto 12px; color: var(--primary);
        }

        h2 { margin: 0; font-size: 24px; font-weight: 700; letter-spacing: -0.025em; }
        .subtitle { color: var(--text-dim); font-size: 13px; margin-top: 4px; }

        .info-section {
            background: rgba(15, 23, 42, 0.6); border: 1px solid var(--border);
            border-radius: 16px; padding: 16px; margin-bottom: 20px;
        }

        .info-row {
            display: flex; justify-content: space-between; align-items: center;
            padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.05);
        }
        .info-row:last-child { border-bottom: none; }

        .info-label { font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-dim); }
        .info-value { font-size: 13px; color: var(--text); font-weight: 600; font-family: 'Courier New', monospace; }

        .status-badge {
            padding: 4px 10px; border-radius: 8px; font-size: 11px; font-weight: 700; text-transform: uppercase;
        }
        /* Динамические классы статуса будут вставлены сервером */
        .status-connected { background: rgba(34, 197, 94, 0.2); color: #4ade80; }
        .status-disconnected { background: rgba(239, 68, 68, 0.2); color: #f87171; }

        form { display: flex; flex-direction: column; }
        .form-group { margin-bottom: 16px; }

        label {
            display: block; font-size: 10px; font-weight: 700; text-transform: uppercase;
            letter-spacing: 0.08em; color: var(--text-dim); margin-bottom: 6px; margin-left: 4px;
        }

        input[type="text"], input[type="number"], input[type="password"] {
            width: 100%; background: rgba(15, 23, 42, 0.8); border: 1px solid var(--border);
            border-radius: 14px; padding: 14px; color: white; font-size: 15px;
            transition: all 0.2s; outline: none;
        }

        input:focus { border-color: var(--primary); }
        input:disabled { opacity: 0.5; cursor: not-allowed; background: rgba(15, 23, 42, 0.4); }

        .input-container { position: relative; }
        .input-container input[type="password"] { padding-right: 50px; }

        .eye-icon {
            position: absolute; right: 8px; top: 50%; transform: translateY(-50%);
            background: none; border: none; color: var(--text-dim);
            padding: 6px; cursor: pointer; display: flex; align-items: center; justify-content: center;
            border-radius: 8px; width: 34px; height: 34px; z-index: 10;
        }
        .eye-icon:hover { background: rgba(255, 255, 255, 0.05); color: var(--text); }

        /* ИСПРАВЛЕНО: Checkbox logic */
        .checkbox-wrapper {
            display: flex; align-items: center; gap: 10px; padding: 12px 16px;
            background: rgba(15, 23, 42, 0.6); border: 1px solid var(--border);
            border-radius: 14px; cursor: pointer; transition: all 0.2s;
        }
        .checkbox-wrapper:hover { background: rgba(15, 23, 42, 0.8); }

        input[type="checkbox"] {
            width: 18px; height: 18px; cursor: pointer; accent-color: var(--primary);
        }

        .checkbox-label-text {
            font-size: 14px; font-weight: 600; color: var(--text); cursor: pointer;
        }

        .form-row { display: grid; grid-template-columns: 2fr 1fr; gap: 12px; }

        button {
            width: 100%; border: none; border-radius: 16px; padding: 16px;
            font-size: 15px; font-weight: 700; cursor: pointer; transition: all 0.2s;
        }

        button[type="submit"] {
            background: var(--primary); color: white;
            box-shadow: 0 10px 15px -3px rgba(99, 102, 241, 0.3);
        }
        button[type="submit"]:hover { background: var(--primary-hover); }

        button[type="button"] {
            background: rgba(99, 102, 241, 0.15); color: var(--primary); margin-bottom: 12px;
        }
        button[type="button"]:hover { background: rgba(99, 102, 241, 0.25); }

        button:disabled { opacity: 0.6; cursor: wait; }

        .section-title {
            font-size: 12px; font-weight: 700; text-transform: uppercase;
            letter-spacing: 0.08em; color: var(--text-dim); margin: 24px 0 12px 4px;
        }

        .footer { margin-top: 16px; text-align: center; font-size: 11px; color: var(--text-dim); opacity: 0.6; }

        @media (max-width: 480px) {
            .card { padding: 24px 20px; border-radius: 28px; }
            .form-row { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <!-- Верхняя аура удалена -->
    <div class="aura aura-2"></div>

    <div class="container">
        <div class="card">
            <div class="header">
                <div class="logo">
                    <!-- Home Icon -->
                    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                        <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"></path>
                        <polyline points="9 22 9 12 15 12 15 22"></polyline>
                    </svg>
                </div>
                <h2>Project Aura</h2>
                <div class="subtitle">MQTT Configuration</div>
            </div>

            <div class="info-section">
                <div class="info-row">
                    <span class="info-label">Status</span>
                    <span class="status-badge {{STATUS_CLASS}}">{{STATUS}}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Device ID</span>
                    <span class="info-value">{{DEVICE_ID}}</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Device IP</span>
                    <span class="info-value">{{DEVICE_IP}}</span>
                </div>
            </div>

            <form method="POST" action="/mqtt" id="mqtt-form">
                <div class="section-title">Broker Settings</div>

                <div class="form-row">
                    <div class="form-group">
                        <label for="host">Broker Address</label>
                        <input type="text" name="host" id="host" value="{{MQTT_HOST}}" placeholder="192.168.1.100" required>
                    </div>
                    <div class="form-group">
                        <label for="port">Port</label>
                        <input type="number" name="port" id="port" value="{{MQTT_PORT}}" placeholder="1883" min="1" max="65535" required>
                    </div>
                </div>

                <div class="section-title">Authentication</div>

                <div class="form-group">
                    <label class="checkbox-wrapper" for="anonymous">
                        <input type="checkbox" id="anonymous" name="anonymous" {{ANONYMOUS_CHECKED}} onchange="updateAuthFields()">
                        <span class="checkbox-label-text">Anonymous (no authentication)</span>
                    </label>
                </div>

                <div class="form-group">
                    <label for="user">Username</label>
                    <input type="text" name="user" id="user" value="{{MQTT_USER}}" placeholder="username" autocomplete="username">
                </div>

                <div class="form-group">
                    <label for="pass">Password</label>
                    <div class="input-container">
                        <input type="password" name="pass" id="pass" value="{{MQTT_PASS}}" placeholder="password" autocomplete="current-password">
                        <button type="button" class="eye-icon" onclick="togglePass()" aria-label="Toggle password visibility">
                            <svg id="eye-svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
                                <circle cx="12" cy="12" r="3"></circle>
                            </svg>
                        </button>
                    </div>
                </div>

                <div class="section-title">Topics</div>

                <div class="form-group">
                    <label for="name">Device Name</label>
                    <input type="text" name="name" id="name" value="{{MQTT_NAME}}" placeholder="Project Aura" required>
                </div>

                <div class="form-group">
                    <label for="topic">Base Topic</label>
                    <input type="text" name="topic" id="topic" value="{{MQTT_TOPIC}}" placeholder="project_aura/room1" required>
                </div>

                <div class="section-title">Integration</div>

                <div class="form-group">
                    <label class="checkbox-wrapper" for="discovery">
                        <input type="checkbox" id="discovery" name="discovery" {{DISCOVERY_CHECKED}}>
                        <span class="checkbox-label-text">Enable Home Assistant Discovery</span>
                    </label>
                </div>

                <button type="submit">Save Settings</button>
            </form>
        </div>
        <div class="footer">
            Powered by 21CNCStudio
        </div>
    </div>

    <script>
        function updateAuthFields() {
            var checkbox = document.getElementById('anonymous');
            var userField = document.getElementById('user');
            var passField = document.getElementById('pass');
            var isAnonymous = checkbox.checked;

            userField.disabled = isAnonymous;
            passField.disabled = isAnonymous;
        }

        function togglePass() {
            var x = document.getElementById("pass");
            var svg = document.getElementById("eye-svg");

            if (x.type === "password") {
                x.type = "text";
                svg.innerHTML = '<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line>';
            } else {
                x.type = "password";
                svg.innerHTML = '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle>';
            }
        }

        updateAuthFields();

        document.getElementById('mqtt-form').addEventListener('submit', function(e) {
            var anonymous = document.getElementById('anonymous').checked;
            var user = document.getElementById('user').value;
            var pass = document.getElementById('pass').value;

            if (!anonymous && (user === '' || pass === '')) {
                e.preventDefault();
                alert('Username and password are required when anonymous mode is disabled');
                return false;
            }
        });

        document.addEventListener('DOMContentLoaded', function() {
            updateAuthFields();
        });
    </script>
</body>
</html>
)HTML";

static const char kMqttLockedPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Project Aura | MQTT</title>
  <style>
    :root {
      --bg: #0f172a;
      --panel: rgba(15, 23, 42, 0.7);
      --border: rgba(255, 255, 255, 0.08);
      --text: #f1f5f9;
      --text-dim: #94a3b8;
    }
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
    body, html {
      margin: 0; padding: 0;
      width: 100%; height: 100%;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg); color: var(--text);
    }
    body {
      display: flex; align-items: center; justify-content: center;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 24px;
      padding: 28px;
      text-align: center;
      box-shadow: 0 24px 48px -24px rgba(0, 0, 0, 0.6);
      max-width: 360px;
      margin: 16px;
    }
    .title {
      font-size: 20px;
      font-weight: 700;
      margin-bottom: 8px;
    }
    .subtitle {
      font-size: 13px;
      color: var(--text-dim);
      line-height: 1.5;
    }
    .btn {
      margin-top: 16px;
      padding: 10px 18px;
      border-radius: 14px;
      border: 1px solid var(--border);
      background: rgba(255, 255, 255, 0.06);
      color: var(--text);
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
    }
    .btn:hover { background: rgba(255, 255, 255, 0.12); }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">MQTT Locked</div>
    <div class="subtitle">Open MQTT screen to enable</div>
    <button class="btn" type="button" onclick="location.reload()">Refresh</button>
    <div class="subtitle" style="margin-top: 10px;">Open screen on device, then press Refresh</div>
  </div>
</body>
</html>
)HTML";

static const char kMqttSavePage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Project Aura | MQTT Setup</title>
    <style>
        :root {
            --bg: #0f172a;
            --panel: rgba(30, 41, 59, 0.8);
            --border: rgba(255, 255, 255, 0.1);
            --primary: #6366f1;
            --text: #f1f5f9;
            --text-dim: #94a3b8;
        }
        * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
        body, html {
            margin: 0; padding: 0; height: 100%; width: 100%;
            overflow: hidden; font-family: -apple-system, system-ui, sans-serif;
            background-color: var(--bg); color: var(--text);
        }
        body {
            display: flex; align-items: center; justify-content: center;
        }
        .aura {
            position: fixed; width: 300px; height: 300px; border-radius: 50%;
            background: radial-gradient(circle, var(--primary) 0%, transparent 70%);
            filter: blur(60px); opacity: 0.15; z-index: -1;
            animation: move 10s infinite alternate cubic-bezier(0.45, 0, 0.55, 1);
        }
        .aura-1 { top: -50px; left: -50px; }
        .aura-2 { bottom: -50px; right: -50px; animation-delay: -5s; }
        @keyframes move { from { transform: translate(0, 0); } to { transform: translate(50px, 50px); } }

        .container {
            width: 100%; max-width: 420px; height: 100%; padding: 20px;
            display: flex; flex-direction: column; align-items: center; justify-content: center;
        }

        .card {
            width: 100%; padding: 40px 32px;
            background: var(--panel); backdrop-filter: blur(16px); -webkit-backdrop-filter: blur(16px);
            border: 1px solid var(--border); border-radius: 32px;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
            text-align: center;
        }
        .logo {
            width: 56px; height: 56px; background: rgba(99, 102, 241, 0.15);
            border: 1px solid rgba(99, 102, 241, 0.3); border-radius: 18px;
            display: flex; align-items: center; justify-content: center;
            margin: 0 auto 16px; color: var(--primary);
        }
        h2 { margin: 0; font-size: 24px; font-weight: 700; letter-spacing: -0.025em; }
        .main-text { font-size: 20px; font-weight: 700; margin: 16px 0 8px; }
        .sub-text { font-size: 14px; color: var(--text-dim); line-height: 1.5; margin: 0; }
        .hint { margin-top: 16px; font-size: 12px; color: var(--text-dim); opacity: 0.8; }
        .footer { margin-top: 16px; font-size: 11px; color: var(--text-dim); opacity: 0.6; }
    </style>
</head>
<body>
    <div class="aura aura-1"></div>
    <div class="aura aura-2"></div>

    <div class="container">
        <div class="card">
            <div class="logo">
                <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M18 8h1a4 4 0 0 1 0 8h-1"></path>
                    <path d="M2 8h16v9a4 4 0 0 1-4 4H6a4 4 0 0 1-4-4V8z"></path>
                    <line x1="6" y1="1" x2="6" y2="4"></line>
                    <line x1="10" y1="1" x2="10" y2="4"></line>
                    <line x1="14" y1="1" x2="14" y2="4"></line>
                </svg>
            </div>
            <h2>Project Aura</h2>
            <div class="main-text">Settings saved</div>
            <p class="sub-text">
                MQTT settings have been saved. The device will reconnect to the broker with the new configuration.
            </p>
            <div class="hint">You can close this tab now.</div>
        </div>
        <div class="footer">Powered by 21CNCStudio</div>
    </div>
</body>
</html>
)HTML";

// NOTE: keep this raw HTML block as source for scripts/generate_theme_gzip.py.
// It is intentionally excluded from compilation to avoid duplicate flash usage.
#if 0
static const char kThemePageTemplate[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Project Aura | Custom Theme Studio</title>
  <style>
    :root {
      --primary-accent: #6366f1;
      --bg-dark: #0f172a;
      --card-bg: rgba(30, 41, 59, 0.7);
      --border-soft: rgba(255, 255, 255, 0.1);
      --text-main: #f1f5f9;
      --text-muted: #94a3b8;
    }

    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }

    body, html {
      margin: 0; padding: 0;
      height: 100%; width: 100%;
      overflow: hidden;
      font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background-color: var(--bg-dark);
      color: var(--text-main);
    }

    body {
      display: flex; align-items: center; justify-content: center;
    }

    .app-viewport {
      width: 100vw; height: 100vh;
      display: flex; align-items: center; justify-content: center;
      position: relative;
      overflow: hidden;
      background-color: var(--bg-dark);
    }

    .aura {
      position: absolute; width: 80vw; height: 80vw; border-radius: 50%;
      filter: blur(80px); opacity: 0.1; z-index: 0;
      animation: move 20s infinite alternate ease-in-out;
      pointer-events: none;
      background: radial-gradient(circle, var(--primary-accent) 0%, transparent 70%);
    }
    .aura-1 { top: -20%; left: -10%; }
    .aura-2 { bottom: -20%; right: -10%; animation-delay: -10s; }

    @keyframes move {
      from { transform: translate(0, 0) scale(1); }
      to { transform: translate(10%, 10%) scale(1.1); }
    }

    .main-container {
      width: 100%; max-width: 960px;
      padding: 16px; z-index: 10;
      animation: fadeIn 0.6s ease-out;
      display: flex; flex-direction: column; height: 100%; justify-content: center;
    }

    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }

    .main-card {
      background: var(--card-bg);
      backdrop-filter: blur(32px); -webkit-backdrop-filter: blur(32px);
      border: 1px solid var(--border-soft);
      border-radius: 28px;
      padding: 24px;
      box-shadow: 0 40px 100px -20px rgba(0,0,0,0.6);
      max-height: 94vh;
      overflow-y: auto;
      scrollbar-width: none;
    }
    .main-card::-webkit-scrollbar { display: none; }

    .main-header { text-align: center; margin-bottom: 24px; }
    .brand-icon {
      width: 48px; height: 48px; margin: 0 auto 12px;
      border: 1px solid rgba(255,255,255,0.2); border-radius: 14px;
      display: flex; align-items: center; justify-content: center;
      background: rgba(255,255,255,0.03);
      color: #fff;
    }

    .brand-title { margin: 0; font-size: 26px; font-weight: 800; letter-spacing: -0.04em; }
    .brand-subtitle { margin: 4px 0 0; font-size: 11px; color: var(--text-muted); font-weight: 600; text-transform: uppercase; letter-spacing: 0.08em; }

    .theme-editor-grid {
      display: grid; grid-template-columns: 1fr; gap: 24px;
    }

    @media (min-width: 800px) {
      .theme-editor-grid { grid-template-columns: 1.4fr 1fr; gap: 32px; }
      .editor-footer { grid-column: 1 / -1; }
      .brand-title { font-size: 32px; }
      .main-card { padding: 32px; border-radius: 32px; }
      .main-container { padding: 24px; }
    }

    .control-group { margin-bottom: 16px; }
    .control-group.two-cols { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }

    .field-label {
      display: block; font-size: 10px; font-weight: 800; text-transform: uppercase;
      letter-spacing: 0.1em; color: var(--text-muted); margin-bottom: 8px;
    }

    .label-with-toggle {
      display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;
    }

    .field-box {
      background: rgba(0,0,0,0.25); border: 1px solid var(--border-soft);
      border-radius: 14px; padding: 10px 14px; transition: 0.2s;
    }
    .field-box:focus-within { border-color: var(--primary-accent); background: rgba(0,0,0,0.35); }

    .field-name { display: block; font-size: 9px; font-weight: 700; color: var(--text-muted); margin-bottom: 4px; }

    .color-input-wrapper { display: flex; align-items: center; gap: 10px; }

    input[type="color"] {
      -webkit-appearance: none; width: 28px; height: 28px; padding: 0; border: none;
      background: none; cursor: pointer;
    }
    input[type="color"]::-webkit-color-swatch-wrapper { padding: 0; }
    input[type="color"]::-webkit-color-swatch { border-radius: 6px; border: 1px solid rgba(255,255,255,0.1); }

    input[type="text"] {
      flex: 1; background: transparent; border: none; color: #fff;
      font-family: monospace; font-size: 13px; outline: none; padding: 0;
      width: 100%; min-width: 0;
    }

    .toggle-switch {
      display: flex;
      align-items: center;
      gap: 10px;
    }

    .switch {
      position: relative;
      display: inline-block;
      width: 38px;
      height: 20px;
      flex-shrink: 0;
    }

    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }

    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: rgba(255, 255, 255, 0.1);
      transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
      border-radius: 20px;
      border: 1px solid rgba(255, 255, 255, 0.05);
    }

    .slider:before {
      position: absolute;
      content: "";
      height: 14px;
      width: 14px;
      left: 2px;
      bottom: 2px;
      background-color: white;
      transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
      border-radius: 50%;
      box-shadow: 0 2px 4px rgba(0,0,0,0.3);
    }

    input:checked + .slider {
      background-color: var(--primary-accent);
      border-color: transparent;
    }

    input:checked + .slider:before {
      transform: translateX(18px);
    }

    .preview-container {
      border-radius: 20px; padding: 24px; min-height: 240px;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      border: 1px solid var(--border-soft); position: relative;
      transition: background 0.4s ease;
    }

    .preview-card-mockup {
      width: 100%; max-width: 240px; border-radius: 18px; border: 1px solid;
      padding: 24px; text-align: center; transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
    }

    .mockup-label { font-size: 10px; text-transform: uppercase; font-weight: 700; opacity: 0.6; display: block; margin-bottom: 4px; }
    .mockup-value { font-size: 36px; font-weight: 800; margin-bottom: 8px; letter-spacing: -0.05em; }

    .submit-button {
      width: 100%; padding: 16px; border-radius: 16px; border: none;
      background: var(--primary-accent); color: #fff; font-size: 15px; font-weight: 700;
      cursor: pointer; transition: 0.3s; box-shadow: 0 8px 24px -4px rgba(99, 102, 241, 0.4);
      margin-top: 10px;
    }
    .submit-button:hover:not(:disabled) { transform: translateY(-1px); box-shadow: 0 12px 32px -4px rgba(99, 102, 241, 0.5); }
    .submit-button:disabled { opacity: 0.7; cursor: not-allowed; }

    .toast-notification {
      position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%) translateY(100px);
      background: #10b981; color: white; padding: 12px 24px; border-radius: 100px;
      display: flex; align-items: center; gap: 10px; font-weight: 600; font-size: 14px;
      box-shadow: 0 16px 32px -8px rgba(16, 185, 129, 0.4);
      transition: 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275); opacity: 0; z-index: 1000;
    }
    .toast-notification.visible { transform: translateX(-50%) translateY(0); opacity: 1; }
    .toast-notification.error {
      background: #ef4444;
      box-shadow: 0 16px 32px -8px rgba(239, 68, 68, 0.45);
    }

    .hidden { display: none !important; }
    .mt-3 { margin-top: 12px; }
  </style>
</head>
<body>
  <div class="app-viewport">
    <div class="aura aura-1"></div>
    <div class="aura aura-2"></div>

    <div class="main-container">
      <div class="main-card">
        <header class="main-header">
          <div class="brand-icon">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20"></line></svg>
          </div>
          <h1 class="brand-title">Project Aura</h1>
          <p class="brand-subtitle">Custom Theme Studio</p>
        </header>

        <div class="theme-editor-grid">
          <div class="editor-controls">

            <div class="control-group">
              <label class="field-label">General Colors</label>
              <div class="field-box">
                <span class="field-name">Screen Background</span>
                <div class="color-input-wrapper">
                  <input type="color" data-field="bg_color" id="bg_picker">
                  <input type="text" data-field="bg_color" id="bg_text">
                </div>
              </div>
            </div>

            <div class="control-group">
              <div class="label-with-toggle">
                <label class="field-label" style="margin-bottom: 0;">Card Background</label>
                <div class="toggle-switch">
                  <span style="font-size: 10px; font-weight: 700; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em;">Gradient</span>
                  <label class="switch">
                    <input type="checkbox" id="grad-toggle">
                    <span class="slider"></span>
                  </label>
                </div>
              </div>

              <div class="field-box">
                <span class="field-name" id="top-label">Top Color</span>
                <div class="color-input-wrapper">
                  <input type="color" data-field="card_top" id="top_picker">
                  <input type="text" data-field="card_top" id="top_text">
                </div>
              </div>

              <div class="field-box mt-3" id="bottom-container">
                <span class="field-name">Bottom Color</span>
                <div class="color-input-wrapper">
                  <input type="color" data-field="card_bottom" id="bottom_picker">
                  <input type="text" data-field="card_bottom" id="bottom_text">
                </div>
              </div>
            </div>

            <div class="control-group two-cols">
              <div class="field-box">
                <span class="field-name">Border</span>
                <div class="color-input-wrapper">
                  <input type="color" data-field="card_border" id="border_picker">
                  <input type="text" data-field="card_border" id="border_text">
                </div>
              </div>
              <div class="field-box">
                <span class="field-name">Shadow</span>
                <div class="color-input-wrapper">
                  <input type="color" data-field="shadow_color" id="shadow_picker">
                  <input type="text" data-field="shadow_color" id="shadow_text">
                </div>
              </div>
            </div>

            <div class="control-group">
              <div class="field-box">
                <span class="field-name">Primary Text</span>
                <div class="color-input-wrapper">
                  <input type="color" data-field="text_color" id="text_picker">
                  <input type="text" data-field="text_color" id="text_text">
                </div>
              </div>
            </div>
          </div>

          <div class="editor-preview">
            <label class="field-label">Real-time Preview</label>
            <div class="preview-container" id="preview-bg">
              <div class="preview-card-mockup" id="preview-mockup">
                <span class="mockup-label">Aura Status</span>
                <div class="mockup-value">84%</div>
                <div style="font-size: 12px; opacity: 0.7; font-weight: 500;">Live Theme Preview</div>
              </div>
            </div>
            <p style="font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 16px; line-height: 1.5;">
              Colors are updated instantly in the preview. Use the Apply button to synchronize with the device hardware.
            </p>
          </div>

          <div class="editor-footer">
            <button class="submit-button" id="apply-btn">Apply & Sync Changes</button>
            <p style="font-size: 11px; color: var(--text-muted); text-align: center; margin-top: 12px;">
              Changes take effect immediately on your hardware.
            </p>
          </div>
        </div>
      </div>
    </div>

    <div class="toast-notification" id="toast">
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>
      <span>Theme Synchronized!</span>
    </div>
  </div>

  <script>
    const state = {
      bg_color: "#0f172a",
      card_top: "#1e293b",
      card_bottom: "#334155",
      card_gradient: true,
      card_border: "#334155",
      shadow_color: "#0f172a",
      text_color: "#f1f5f9"
    };

    const elements = {
      previewBg: document.getElementById('preview-bg'),
      previewMockup: document.getElementById('preview-mockup'),
      bottomContainer: document.getElementById('bottom-container'),
      topLabel: document.getElementById('top-label'),
      gradToggle: document.getElementById('grad-toggle'),
      applyBtn: document.getElementById('apply-btn'),
      toast: document.getElementById('toast')
    };
    const THEME_STATE_MAX_LOAD_RETRIES = 8;
    const THEME_STATE_RETRY_MS = 1200;
    let themeStateLoaded = false;
    let themeStateLoadAttempts = 0;
    let themeStateLoadRetryTimer = null;

    const normalizeHex = (v) => {
      if (!v) return "#000000";
      let val = v.trim();
      if (val[0] !== "#") val = "#" + val;
      if (val.length === 4) val = "#" + val[1] + val[1] + val[2] + val[2] + val[3] + val[3];
      return val.length === 7 ? val.toLowerCase() : "#000000";
    };

    const applyRemoteState = (payload) => {
      if (!payload || typeof payload !== "object") return;
      if (typeof payload.bg_color === "string") state.bg_color = normalizeHex(payload.bg_color);
      if (typeof payload.card_top === "string") state.card_top = normalizeHex(payload.card_top);
      if (typeof payload.card_bottom === "string") state.card_bottom = normalizeHex(payload.card_bottom);
      if (typeof payload.card_border === "string") state.card_border = normalizeHex(payload.card_border);
      if (typeof payload.shadow_color === "string") state.shadow_color = normalizeHex(payload.shadow_color);
      if (typeof payload.text_color === "string") state.text_color = normalizeHex(payload.text_color);
      if (typeof payload.card_gradient === "boolean") state.card_gradient = payload.card_gradient;
      if (typeof payload.card_gradient === "number") state.card_gradient = payload.card_gradient !== 0;
    };

    const render = () => {
      document.querySelectorAll('input[data-field]').forEach(input => {
        const field = input.dataset.field;
        if (input.type === 'color') {
          input.value = normalizeHex(state[field]);
        } else if (input.type === 'text') {
          input.value = state[field];
        }
      });

      elements.previewBg.style.backgroundColor = state.bg_color;
      elements.previewMockup.style.borderColor = state.card_border;
      elements.previewMockup.style.boxShadow = `0 15px 40px -5px ${state.shadow_color}88`;
      elements.previewMockup.style.color = state.text_color;

      if (state.card_gradient) {
        elements.previewMockup.style.background = `linear-gradient(180deg, ${state.card_top}, ${state.card_bottom})`;
        elements.bottomContainer.classList.remove('hidden');
        elements.topLabel.innerText = "Top Color";
      } else {
        elements.previewMockup.style.background = state.card_top;
        elements.bottomContainer.classList.add('hidden');
        elements.topLabel.innerText = "Background Color";
      }
      elements.gradToggle.checked = state.card_gradient;
    };

    document.querySelectorAll('input[data-field]').forEach(input => {
      input.addEventListener('input', (e) => {
        const field = e.target.dataset.field;
        if (e.target.type === 'color') {
          state[field] = e.target.value;
        } else {
          const normalized = normalizeHex(e.target.value);
          if (normalized !== "#000000" || e.target.value === "#000" || e.target.value === "#000000") {
            state[field] = normalized;
          }
        }
        render();
      });
    });

    elements.gradToggle.addEventListener('change', (e) => {
      state.card_gradient = e.target.checked;
      render();
    });

    const showToast = (message, isError) => {
      const textEl = elements.toast.querySelector('span');
      if (textEl) {
        textEl.textContent = message || (isError ? "Failed to sync theme" : "Theme Synchronized!");
      }
      elements.toast.classList.toggle('error', !!isError);
      elements.toast.classList.add('visible');
      setTimeout(() => elements.toast.classList.remove('visible'), 3000);
    };

    const setApplyButtonReady = (ready) => {
      if (!elements.applyBtn) return;
      elements.applyBtn.disabled = !ready;
      elements.applyBtn.innerText = ready ? "Apply & Sync Changes" : "Loading theme...";
    };

    const scheduleThemeStateRetry = (lastErrorMessage) => {
      if (themeStateLoaded) return;
      if (themeStateLoadAttempts >= THEME_STATE_MAX_LOAD_RETRIES) {
        showToast(lastErrorMessage || "Failed to load current theme. Refresh page and retry.", true);
        return;
      }
      if (themeStateLoadRetryTimer) {
        clearTimeout(themeStateLoadRetryTimer);
      }
      themeStateLoadRetryTimer = setTimeout(() => {
        themeStateLoadRetryTimer = null;
        loadThemeState();
      }, THEME_STATE_RETRY_MS);
    };

    elements.applyBtn.addEventListener('click', async () => {
      if (!themeStateLoaded) {
        showToast("Theme state is still loading. Please wait.", true);
        return;
      }
      elements.applyBtn.disabled = true;
      const originalText = elements.applyBtn.innerText;
      elements.applyBtn.innerText = "Synchronizing...";

      const payload = {
        bg: state.bg_color,
        card_top: state.card_top,
        card_bottom: state.card_bottom,
        card_gradient: state.card_gradient ? 1 : 0,
        border: state.card_border,
        shadow: state.shadow_color,
        text: state.text_color
      };

      try {
        const res = await fetch("/theme/apply", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload)
        });
        if (!res.ok) {
          let message = "";
          try {
            const body = await res.json();
            if (body && typeof body.error === "string" && body.error) {
              message = body.error;
            }
          } catch (_) {}
          if (!message) {
            if (res.status === 503) {
              message = "OTA in progress. Theme update is temporarily locked.";
            } else {
              message = "Failed to apply theme (HTTP " + res.status + ")";
            }
          }
          showToast(message, true);
          return;
        }
        showToast("Theme synchronized!", false);
      } catch (_) {
        showToast("Theme sync failed. Check connection and retry.", true);
      } finally {
        elements.applyBtn.disabled = !themeStateLoaded;
        elements.applyBtn.innerText = originalText;
      }
    });

    const loadThemeState = async () => {
      if (themeStateLoaded) return;
      themeStateLoadAttempts += 1;
      try {
        const res = await fetch("/theme/state", { cache: "no-store" });
        if (!res.ok) {
          let message = "Failed to load current theme (HTTP " + res.status + ")";
          try {
            const body = await res.json();
            if (body && typeof body.error === "string" && body.error) {
              message = body.error;
            }
          } catch (_) {}
          scheduleThemeStateRetry(message);
          return;
        }
        const body = await res.json();
        if (!body || body.success !== true) {
          scheduleThemeStateRetry("Invalid theme state response");
          return;
        }
        applyRemoteState(body);
        themeStateLoaded = true;
        if (themeStateLoadRetryTimer) {
          clearTimeout(themeStateLoadRetryTimer);
          themeStateLoadRetryTimer = null;
        }
        setApplyButtonReady(true);
      } catch (_) {}
      if (!themeStateLoaded) {
        scheduleThemeStateRetry("Failed to load current theme. Refresh page and retry.");
      }
      render();
    };

    setApplyButtonReady(false);
    render();
    loadThemeState();
  </script>
</body>
</html>
)HTML";
#endif

static const char kThemeLockedPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Project Aura | Custom Theme</title>
  <style>
    :root {
      --bg: #0f172a;
      --panel: rgba(15, 23, 42, 0.7);
      --border: rgba(255, 255, 255, 0.08);
      --text: #f1f5f9;
      --text-dim: #94a3b8;
    }
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
    body, html {
      margin: 0; padding: 0;
      width: 100%; height: 100%;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg); color: var(--text);
    }
    body {
      display: flex; align-items: center; justify-content: center;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 24px;
      padding: 28px;
      text-align: center;
      box-shadow: 0 24px 48px -24px rgba(0, 0, 0, 0.6);
      max-width: 360px;
      margin: 16px;
    }
    .title {
      font-size: 20px;
      font-weight: 700;
      margin-bottom: 8px;
    }
    .subtitle {
      font-size: 13px;
      color: var(--text-dim);
      line-height: 1.5;
    }
    .btn {
      margin-top: 16px;
      padding: 10px 18px;
      border-radius: 14px;
      border: 1px solid var(--border);
      background: rgba(255, 255, 255, 0.06);
      color: var(--text);
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
    }
    .btn:hover { background: rgba(255, 255, 255, 0.12); }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">Custom Theme Locked</div>
    <div class="subtitle">Open Custom Theme screen to enable</div>
    <button class="btn" type="button" onclick="location.reload()">Refresh</button>
    <div class="subtitle" style="margin-top: 10px;">Open screen on device, then press Refresh</div>
  </div>
</body>
</html>
)HTML";

// NOTE: keep this raw HTML block as source for scripts/generate_dac_gzip.py.
// It is intentionally excluded from compilation to avoid duplicate flash usage.
#if 0
static const char kDacPageTemplate[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Project Aura | Fan Control Settings</title>
  <style>
    :root {
      --bg: #0f172a;
      --panel: rgba(30, 41, 59, 0.7);
      --panel-strong: rgba(30, 41, 59, 0.8);
      --panel-locked: rgba(15, 23, 42, 0.7);
      --border: rgba(255, 255, 255, 0.1);
      --border-soft: rgba(255, 255, 255, 0.08);
      --primary: #6366f1;
      --primary-hover: #818cf8;
      --primary-soft: rgba(99, 102, 241, 0.15);
      --text: #f1f5f9;
      --text-dim: #94a3b8;
      --success: #4ade80;
      --success-bg: rgba(34, 197, 94, 0.2);
      --error: #f87171;
      --error-bg: rgba(239, 68, 68, 0.2);
      --warning: #fbbf24;
      --orange: #fb923c;
      --warning-bg: rgba(251, 191, 36, 0.2);
      --offline-bg: rgba(148, 163, 184, 0.2);
      --neutral-btn: #334155;
      --input-bg: rgba(15, 23, 42, 0.6);
    }
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
    body, html {
      margin: 0; padding: 0;
      min-height: 100%; width: 100%;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: var(--bg); color: var(--text);
    }
    body { display: flex; justify-content: center; padding: 16px; }
    .container { width: 100%; max-width: 560px; display: flex; flex-direction: column; gap: 12px; }
    .card {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 14px;
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
    }
    .header { display: flex; justify-content: space-between; align-items: center; }
    .title { margin: 0; font-size: 20px; font-weight: 700; letter-spacing: -0.02em; }
    .status {
      padding: 4px 10px; border-radius: 999px; font-size: 11px; font-weight: 700;
      text-transform: uppercase;
    }
    .status-running { background: var(--success-bg); color: var(--success); }
    .status-stopped { background: var(--warning-bg); color: var(--warning); }
    .status-fault { background: var(--error-bg); color: var(--error); }
    .status-ota { background: var(--warning-bg); color: var(--warning); }
    .status-offline { background: var(--offline-bg); color: var(--text-dim); }
    .notice {
      margin-top: 10px;
      border: 1px solid rgba(251, 191, 36, 0.34);
      background: rgba(120, 53, 15, 0.22);
      color: #fcd34d;
      border-radius: 10px;
      padding: 8px 10px;
      font-size: 12px;
      line-height: 1.4;
    }
    .grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 10px; }
    .k { color: var(--text-dim); font-size: 11px; text-transform: uppercase; letter-spacing: 0.06em; }
    .v { font-size: 20px; font-weight: 800; color: var(--primary); margin-top: 4px; }
    .switch {
      display: flex; background: var(--input-bg); border-radius: 12px; padding: 4px; margin-top: 10px;
    }
    .switch button {
      flex: 1; border: none; padding: 10px; border-radius: 10px;
      background: transparent; color: var(--text-dim); font-weight: 700; cursor: pointer;
    }
    .switch button.active { background: var(--primary); color: #fff; }
    .hidden { display: none !important; }
    .speed-grid { display: grid; grid-template-columns: repeat(5, 1fr); gap: 8px; margin-top: 8px; }
    .timer-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-top: 8px; }
    .chip-btn {
      border: 1px solid var(--border); border-radius: 10px; padding: 10px 0;
      background: var(--input-bg); color: var(--text); font-size: 12px; font-weight: 600; cursor: pointer;
    }
    .chip-btn.active { border-color: var(--primary); color: var(--primary); background: var(--primary-soft); }
    .actions { display: flex; gap: 10px; margin-top: 12px; }
    .actions-secondary { margin-top: 10px; }
    .btn {
      width: 100%; border: 1px solid var(--border); border-radius: 12px; padding: 12px;
      background: var(--neutral-btn); color: #fff; font-weight: 700; cursor: pointer;
      transition: all 0.2s;
    }
    .btn.start.active { border-color: var(--success); color: var(--success); background: var(--success-bg); }
    .btn.stop.active { border-color: var(--error); color: var(--error); background: var(--error-bg); }
    .btn.auto.active { border-color: var(--warning); color: var(--warning); background: var(--warning-bg); }
    .btn:disabled { opacity: 0.6; cursor: not-allowed; }

    .sensor-card {
      position: relative;
      padding: 12px 0;
      border-bottom: 1px solid var(--border-soft);
    }
    .sensor-card:last-child { border-bottom: none; }
    .sensor-header {
      display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 10px; font-weight: 600;
    }
    .sensor-right { display: flex; align-items: center; gap: 8px; }
    .sensor-live {
      min-width: 74px;
      text-align: right;
      color: var(--text-dim);
      font-size: 12px;
      font-weight: 700;
      font-variant-numeric: tabular-nums;
    }
    .threshold-row {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 8px;
    }
    .th-col { display: flex; flex-direction: column; gap: 4px; }
    .th-color-bar { height: 4px; border-radius: 2px; width: 100%; }
    .th-label { font-size: 10px; color: var(--text-dim); text-align: center; }

    .th-select {
      width: 100%; background: var(--input-bg); color: var(--text);
      border: 1px solid var(--border); border-radius: 6px;
      padding: 8px 4px; font-size: 11px; text-align: center;
      outline: none; cursor: pointer;
    }

    .ui-switch {
      width: 36px; height: 20px; background: var(--neutral-btn);
      border-radius: 20px; position: relative; cursor: pointer; transition: 0.3s;
      border: none;
    }
    .ui-switch.on { background: var(--primary); }
    .ui-switch::after {
      content: ''; position: absolute; width: 14px; height: 14px; border-radius: 50%;
      background: white; top: 3px; left: 3px; transition: 0.3s;
    }
    .ui-switch.on::after { left: 19px; }

    #sensors_wrapper {
      transition: opacity 0.3s ease;
    }

    .auto-note { color: var(--text-dim); font-size: 12px; line-height: 1.4; margin-top: 10px; }
    .save-row { display: flex; gap: 10px; margin-top: 12px; }
    .btn.save { background: var(--primary); border-color: #818cf8; }
    .save.unsaved { background: var(--primary-hover); }
    .btn.save.saving {
      background: var(--neutral-btn);
      border-color: var(--border);
      color: var(--text-dim);
    }
    .btn.save.saved {
      background: var(--success-bg);
      border-color: var(--success);
      color: var(--success);
    }
    .btn.save.error {
      background: var(--error-bg);
      border-color: var(--error);
      color: var(--error);
    }
    .btn.reset {
      background: transparent;
      border-color: var(--border);
      color: var(--text-dim);
    }
    .btn.reset:hover {
      border-color: var(--text-dim);
      color: var(--text);
    }
    .btn.nav-link {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      text-decoration: none;
      font-family: inherit;
      font-size: inherit;
      line-height: inherit;
      background: transparent;
      border-color: var(--border);
      color: var(--text-dim);
    }
    .btn.nav-link:hover {
      border-color: var(--text-dim);
      color: var(--text);
    }

    .disabled {
      opacity: 0.55;
      pointer-events: none;
    }

    @media (max-width: 420px) {
      .threshold-row { grid-template-columns: repeat(2, 1fr); gap: 10px; }
      .speed-grid { grid-template-columns: repeat(4, 1fr); }
      .timer-grid { grid-template-columns: repeat(3, 1fr); }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div class="header">
        <h1 class="title">Fan Control Settings</h1>
        <div id="status_chip" class="status status-offline">OFFLINE</div>
      </div>
      <div id="ota_notice" class="notice hidden"></div>

      <div class="grid2">
        <div class="card" style="margin:0;padding:10px;background:var(--panel-strong);">
          <div class="k">Fan Output</div>
          <div id="out_val" class="v">UNKNOWN</div>
        </div>
        <div class="card" style="margin:0;padding:10px;background:var(--panel-strong);">
          <div class="k">Timer</div>
          <div id="timer_val" class="v">--:--</div>
        </div>
      </div>

      <div class="switch">
        <button id="mode_manual" onclick="setMode('manual')">MANUAL</button>
        <button id="mode_auto" onclick="setMode('auto')">AUTO</button>
      </div>
    </div>

    <div id="manual_box" class="card">
      <div class="k">Set Fan Speed</div>
      <div class="speed-grid" id="speed_grid"></div>
      <div class="k" style="margin-top:10px;">Operation Timer</div>
      <div class="timer-grid" id="timer_grid"></div>
      <div class="actions">
        <button id="btn_start" class="btn start" onclick="sendAction({action:'start'})">START</button>
        <button id="btn_stop" class="btn stop" onclick="sendAction({action:'stop'})">STOP</button>
      </div>
      <div class="actions-secondary">
        <button id="btn_start_auto" class="btn auto" onclick="sendAction({action:'start_auto'})">START AUTO</button>
      </div>
      <div class="actions actions-secondary">
        <a class="btn nav-link" href="/dashboard">BACK TO DASHBOARD</a>
      </div>
    </div>

    <div id="auto_box" class="card hidden">
      <div class="sensor-header" style="border-bottom: 1px solid var(--border); padding-bottom: 12px; margin-bottom: 12px;">
        <span style="font-size: 1rem;">Enable Auto Control</span>
        <button type="button" id="btn_master_auto" class="ui-switch"></button>
      </div>

      <div id="sensors_wrapper"></div>

      <div class="save-row">
        <button type="button" class="btn reset" onclick="resetAutoDefaults()">RESET DEFAULTS</button>
        <button type="button" id="btn_auto_save" class="btn save" onclick="saveAuto()">SAVE</button>
      </div>
      <div class="actions actions-secondary">
        <button id="btn_auto_start" class="btn auto" onclick="sendAction({action:'start_auto'})">START AUTO</button>
        <button id="btn_auto_stop" class="btn stop" onclick="sendAction({action:'stop'})">STOP</button>
      </div>
      <div class="actions actions-secondary">
        <a class="btn nav-link" href="/dashboard">BACK TO DASHBOARD</a>
      </div>
      <div class="auto-note">Manual has priority over Auto while manual run/timer is active.</div>
    </div>
  </div>

  <script>
    const SPEEDS = [10,20,30,40,50,60,70,80,90,100];
    const TIMERS = [
      {label:"10m", sec:600},
      {label:"30m", sec:1800},
      {label:"1h", sec:3600},
      {label:"2h", sec:7200},
      {label:"4h", sec:14400},
      {label:"8h", sec:28800},
    ];
    const AUTO_META = {
      co2:  {name:"CO2",unit:"ppm",field:"co2",valid:"co2_valid",decimals:0,labels:["<800","800-1k","1k-1.5k",">=1.5k"],defaults:[30,50,70,100]},
      co:   {name:"CO",unit:"ppm",field:"co_ppm",valid:"co_valid",decimals:1,labels:["<9","9-35","35-100",">100"],defaults:[20,50,100,100]},
      pm05: {name:"PM0.5",unit:"#/cm&sup3;",field:"pm05",valid:"pm05_valid",decimals:0,labels:["<=250",">250-600",">600-1200",">1200"],defaults:[20,40,70,100]},
      pm1:  {name:"PM1.0",unit:"&micro;g/m&sup3;",field:"pm1",valid:"pm1_valid",decimals:1,labels:["<=10",">10-25",">25-50",">50"],defaults:[20,40,70,100]},
      pm4:  {name:"PM4.0",unit:"&micro;g/m&sup3;",field:"pm4",valid:"pm4_valid",decimals:1,labels:["<=25",">25-50",">50-75",">75"],defaults:[20,40,70,100]},
      pm25: {name:"PM2.5",unit:"&micro;g/m&sup3;",field:"pm25",valid:"pm25_valid",decimals:1,labels:["<=12",">12-35",">35-55",">55"],defaults:[20,40,70,100]},
      pm10: {name:"PM10",unit:"&micro;g/m&sup3;",field:"pm10",valid:"pm10_valid",decimals:1,labels:["<=54",">54-154",">154-254",">254"],defaults:[20,40,70,100]},
      hcho: {name:"HCHO",unit:"ppb",field:"hcho",valid:"hcho_valid",decimals:1,labels:["<30","30-60","60-100",">100"],defaults:[20,40,70,100]},
      voc:  {name:"VOC Index",unit:"",field:"voc_index",valid:"voc_valid",decimals:0,labels:["<150","151-250","251-350",">350"],defaults:[20,50,80,100]},
      nox:  {name:"NOx Index",unit:"",field:"nox_index",valid:"nox_valid",decimals:0,labels:["<50","50-99","100-199",">=200"],defaults:[20,40,70,100]},
    };
    const AUTO_KEYS = Object.keys(AUTO_META);
    let latest = null;
    let autoDirty = false;
    let updatingFromState = false;
    let autoSaveFeedbackTimer = null;
    let otaBusy = false;
    let stateRefreshTimer = null;
    let stateFetchInFlight = false;
    const DAC_STATE_REFRESH_VISIBLE_MS = 1000;
    const DAC_STATE_REFRESH_HIDDEN_MS = 5000;

    function fmtTimer(sec) {
      if (!sec || sec <= 0) return "--:--";
      const m = Math.floor(sec / 60);
      const s = sec % 60;
      return String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0");
    }

    function setStatus(status) {
      const chip = document.getElementById("status_chip");
      chip.textContent = status;
      chip.className = "status";
      chip.classList.add("status-" + status.toLowerCase());
    }

    function setControlsDisabled(disabled) {
      const modeManual = document.getElementById("mode_manual");
      const modeAuto = document.getElementById("mode_auto");
      const manualBox = document.getElementById("manual_box");
      const autoBox = document.getElementById("auto_box");
      if (modeManual) modeManual.disabled = !!disabled;
      if (modeAuto) modeAuto.disabled = !!disabled;
      if (manualBox) manualBox.classList.toggle("disabled", !!disabled);
      if (autoBox) autoBox.classList.toggle("disabled", !!disabled);
    }

    function setOtaNotice(message) {
      const notice = document.getElementById("ota_notice");
      if (!notice) return;
      if (!message) {
        notice.classList.add("hidden");
        notice.textContent = "";
        return;
      }
      notice.textContent = message;
      notice.classList.remove("hidden");
    }

    function applyOtaBusyState(message) {
      otaBusy = true;
      setStatus("OTA");
      setControlsDisabled(true);
      setOtaNotice(message || "OTA in progress. Controls are temporarily locked.");
    }

    function clearOtaBusyState() {
      if (!otaBusy) return;
      otaBusy = false;
      setControlsDisabled(false);
      setOtaNotice("");
    }

    function clearSaveFeedbackTimer() {
      if (autoSaveFeedbackTimer) {
        clearTimeout(autoSaveFeedbackTimer);
        autoSaveFeedbackTimer = null;
      }
    }

    function scheduleStateRefresh(delayMs) {
      if (stateRefreshTimer) {
        clearTimeout(stateRefreshTimer);
      }
      stateRefreshTimer = setTimeout(() => {
        stateRefreshTimer = null;
        fetchState().catch(() => {});
      }, delayMs);
    }

    function setSaveButtonState(state) {
      const btn = document.getElementById("btn_auto_save");
      if (!btn) return;
      btn.classList.remove("unsaved", "saving", "saved", "error");
      btn.disabled = false;
      if (state === "dirty") {
        btn.classList.add("unsaved");
        btn.textContent = "SAVE";
        return;
      }
      if (state === "saving") {
        btn.classList.add("saving");
        btn.textContent = "SAVING...";
        btn.disabled = true;
        return;
      }
      if (state === "saved") {
        btn.classList.add("saved");
        btn.textContent = "SAVED \u2713";
        return;
      }
      if (state === "error") {
        btn.classList.add("error");
        btn.textContent = "SAVE FAILED";
        return;
      }
      btn.textContent = "SAVE";
    }

    function markAutoDirty() {
      if (updatingFromState) return;
      autoDirty = true;
      clearSaveFeedbackTimer();
      setSaveButtonState("dirty");
    }

    function clearAutoDirty() {
      autoDirty = false;
      setSaveButtonState("idle");
    }

    function setMasterAutoVisual(enabled) {
      const btn = document.getElementById("btn_master_auto");
      btn.classList.toggle("on", !!enabled);
      const wrap = document.getElementById("sensors_wrapper");
      wrap.classList.toggle("disabled", !enabled);
    }

    function buildManualButtons() {
      const speed = document.getElementById("speed_grid");
      SPEEDS.forEach(p => {
        const b = document.createElement("button");
        b.className = "chip-btn speed";
        b.dataset.step = String(Math.round(p / 10));
        b.textContent = (p === 100) ? "MAX" : (p + "%");
        b.onclick = () => sendAction({action:"set_manual_step", step: Number(b.dataset.step)});
        speed.appendChild(b);
      });
      const timer = document.getElementById("timer_grid");
      TIMERS.forEach(t => {
        const b = document.createElement("button");
        b.className = "chip-btn timer";
        b.dataset.sec = String(t.sec);
        b.textContent = t.label;
        b.onclick = () => {
          if (latest && latest.dac && latest.dac.selected_timer_s === t.sec) {
            sendAction({action:"set_timer", seconds:0});
          } else {
            sendAction({action:"set_timer", seconds:t.sec});
          }
        };
        timer.appendChild(b);
      });
    }

    function buildAutoCards() {
      const root = document.getElementById("sensors_wrapper");
      root.innerHTML = "";
      AUTO_KEYS.forEach(key => {
        const meta = AUTO_META[key];
        const card = document.createElement("div");
        card.className = "sensor-card";
        card.dataset.key = key;

        const unit = meta.unit ? ` <small class="text-dim">(${meta.unit})</small>` : "";
        card.innerHTML = `
          <div class="sensor-header">
            <span>${meta.name}${unit}</span>
            <div class="sensor-right">
              <span class="sensor-live">--</span>
              <button type="button" class="ui-switch sensor-en"></button>
            </div>
          </div>
          <div class="threshold-row">
            <div class="th-col">
              <div class="th-label">${meta.labels[0]}</div>
              <div class="th-color-bar" style="background: var(--success);"></div>
              <select class="th-select s-green"></select>
            </div>
            <div class="th-col">
              <div class="th-label">${meta.labels[1]}</div>
              <div class="th-color-bar" style="background: var(--warning);"></div>
              <select class="th-select s-yellow"></select>
            </div>
            <div class="th-col">
              <div class="th-label">${meta.labels[2]}</div>
              <div class="th-color-bar" style="background: var(--orange);"></div>
              <select class="th-select s-orange"></select>
            </div>
            <div class="th-col">
              <div class="th-label">${meta.labels[3]}</div>
              <div class="th-color-bar" style="background: var(--error);"></div>
              <select class="th-select s-red"></select>
            </div>
          </div>
        `;
        root.appendChild(card);

        card.querySelectorAll("select").forEach(sel => {
          for (let p = 0; p <= 100; p += 10) {
            const o = document.createElement("option");
            o.value = String(p);
            o.textContent = `${p}% (${(p / 10).toFixed(1)}V)`;
            sel.appendChild(o);
          }
          sel.addEventListener("change", markAutoDirty);
        });

        const enBtn = card.querySelector(".sensor-en");
        enBtn.addEventListener("click", () => {
          enBtn.classList.toggle("on");
          markAutoDirty();
        });
      });
    }

    function renderSensorLiveValues(sensors) {
      document.querySelectorAll("#sensors_wrapper .sensor-card").forEach(card => {
        const key = card.dataset.key;
        const meta = AUTO_META[key];
        const live = card.querySelector(".sensor-live");
        if (!meta || !live) return;
        const valid = !!sensors[meta.valid];
        const raw = Number(sensors[meta.field]);
        if (!valid || !Number.isFinite(raw)) {
          live.textContent = "--";
          return;
        }
        live.textContent = raw.toFixed(meta.decimals);
      });
    }

    function render(data) {
      clearOtaBusyState();
      latest = data;
      const dac = data.dac || {};
      const autoCfg = data.auto || {};
      const sensors = data.sensors || {};

      setStatus(dac.status || "OFFLINE");
      const mode = dac.mode || "manual";
      document.getElementById("mode_manual").classList.toggle("active", mode === "manual");
      document.getElementById("mode_auto").classList.toggle("active", mode === "auto");
      document.getElementById("manual_box").classList.toggle("hidden", mode !== "manual");
      document.getElementById("auto_box").classList.toggle("hidden", mode !== "auto");

      document.getElementById("btn_start").classList.toggle("active", dac.available && dac.running);
      document.getElementById("btn_stop").classList.toggle("active", dac.available && !dac.running);
      const autoArmed = dac.available && (dac.mode === "auto") && !dac.manual_override && !dac.auto_resume_blocked;
      document.getElementById("btn_start_auto").classList.toggle("active", autoArmed);
      document.getElementById("btn_auto_start").classList.toggle("active", autoArmed);
      document.getElementById("btn_auto_stop").classList.toggle("active", dac.available && !dac.running);

      if (!dac.output_known) {
        document.getElementById("out_val").textContent = "UNKNOWN";
      } else {
        const mv = dac.output_mv || 0;
        const pct = dac.output_percent || 0;
        document.getElementById("out_val").textContent = (mv / 1000).toFixed(1) + "V (" + pct + "%)";
      }
      document.getElementById("timer_val").textContent = fmtTimer(dac.remaining_s || 0);

      document.querySelectorAll(".speed").forEach(btn => {
        btn.classList.toggle("active", Number(btn.dataset.step) === dac.manual_step);
      });
      document.querySelectorAll(".timer").forEach(btn => {
        btn.classList.toggle("active", Number(btn.dataset.sec) === dac.selected_timer_s);
      });

      renderSensorLiveValues(sensors);

      if (!autoDirty) {
        updatingFromState = true;
        setMasterAutoVisual(!!autoCfg.enabled);
        document.querySelectorAll("#sensors_wrapper .sensor-card").forEach(card => {
          const key = card.dataset.key;
          const meta = AUTO_META[key];
          const s = autoCfg[key] || {};
          card.querySelector(".sensor-en").classList.toggle("on", !!s.enabled);
          card.querySelector(".s-green").value = String(s.green ?? meta.defaults[0]);
          card.querySelector(".s-yellow").value = String(s.yellow ?? meta.defaults[1]);
          card.querySelector(".s-orange").value = String(s.orange ?? meta.defaults[2]);
          card.querySelector(".s-red").value = String(s.red ?? meta.defaults[3]);
        });
        updatingFromState = false;
      }
    }

    async function fetchState() {
      if (stateFetchInFlight) {
        return;
      }
      stateFetchInFlight = true;
      if (otaBusy) {
        // Keep polling to auto-recover when OTA completes.
      }
      try {
        const r = await fetch("/dac/state", {cache:"no-store"});
        if (r.status === 503) {
          let payload = null;
          try { payload = await r.json(); } catch (_) {}
          applyOtaBusyState((payload && payload.error) || "OTA in progress");
          return;
        }
        if (!r.ok) return;
        const json = await r.json();
        if (json && json.success) {
          clearOtaBusyState();
          render(json);
        }
      } finally {
        stateFetchInFlight = false;
        scheduleStateRefresh(document.hidden ? DAC_STATE_REFRESH_HIDDEN_MS : DAC_STATE_REFRESH_VISIBLE_MS);
      }
    }

    async function sendAction(payload) {
      if (otaBusy) return;
      const r = await fetch("/dac/action", {
        method: "POST",
        headers: {"Content-Type":"application/json"},
        body: JSON.stringify(payload)
      });
      if (r.status === 503) {
        let body = null;
        try { body = await r.json(); } catch (_) {}
        applyOtaBusyState((body && body.error) || "OTA in progress");
        return;
      }
      if (r.ok) {
        scheduleStateRefresh(80);
      }
    }

    function setMode(mode) {
      sendAction({action:"set_mode", mode});
    }

    function collectAutoPayload() {
      const payload = { auto: { enabled: document.getElementById("btn_master_auto").classList.contains("on") } };
      document.querySelectorAll("#sensors_wrapper .sensor-card").forEach(card => {
        const key = card.dataset.key;
        payload.auto[key] = {
          enabled: card.querySelector(".sensor-en").classList.contains("on"),
          green: Number(card.querySelector(".s-green").value),
          yellow: Number(card.querySelector(".s-yellow").value),
          orange: Number(card.querySelector(".s-orange").value),
          red: Number(card.querySelector(".s-red").value),
        };
      });
      return payload;
    }

    async function saveAuto() {
      if (otaBusy) return;
      clearSaveFeedbackTimer();
      setSaveButtonState("saving");
      const payload = collectAutoPayload();
      payload.rearm = !!(latest &&
                         latest.dac &&
                         latest.dac.mode === "auto" &&
                         latest.dac.running &&
                         !latest.dac.manual_override &&
                         !latest.dac.auto_resume_blocked);
      const r = await fetch("/dac/auto", {
        method: "POST",
        headers: {"Content-Type":"application/json"},
        body: JSON.stringify(payload)
      });
      if (r.status === 503) {
        let body = null;
        try { body = await r.json(); } catch (_) {}
        applyOtaBusyState((body && body.error) || "OTA in progress");
        setSaveButtonState("error");
        return;
      }
      if (r.ok) {
        clearAutoDirty();
        setSaveButtonState("saved");
        autoSaveFeedbackTimer = setTimeout(() => {
          autoSaveFeedbackTimer = null;
          if (autoDirty) setSaveButtonState("dirty");
          else setSaveButtonState("idle");
        }, 1600);
        scheduleStateRefresh(80);
      } else {
        setSaveButtonState("error");
        autoSaveFeedbackTimer = setTimeout(() => {
          autoSaveFeedbackTimer = null;
          if (autoDirty) setSaveButtonState("dirty");
          else setSaveButtonState("idle");
        }, 1800);
      }
    }

    function resetAutoDefaults() {
      if (!confirm("Reset auto settings to factory defaults?")) {
        return;
      }
      updatingFromState = true;
      setMasterAutoVisual(false);
      document.querySelectorAll("#sensors_wrapper .sensor-card").forEach(card => {
        const key = card.dataset.key;
        const meta = AUTO_META[key];
        card.querySelector(".sensor-en").classList.add("on");
        card.querySelector(".s-green").value = String(meta.defaults[0]);
        card.querySelector(".s-yellow").value = String(meta.defaults[1]);
        card.querySelector(".s-orange").value = String(meta.defaults[2]);
        card.querySelector(".s-red").value = String(meta.defaults[3]);
      });
      updatingFromState = false;
      markAutoDirty();
    }

    document.getElementById("btn_master_auto").addEventListener("click", () => {
      const btn = document.getElementById("btn_master_auto");
      btn.classList.toggle("on");
      setMasterAutoVisual(btn.classList.contains("on"));
      markAutoDirty();
    });

    document.addEventListener("visibilitychange", () => {
      if (document.hidden) {
        scheduleStateRefresh(DAC_STATE_REFRESH_HIDDEN_MS);
      } else {
        scheduleStateRefresh(60);
      }
    });

    buildManualButtons();
    buildAutoCards();
    setSaveButtonState("idle");
    fetchState().catch(() => {});
  </script>
</body>
</html>
)HTML";
#endif

} // namespace WebTemplates

