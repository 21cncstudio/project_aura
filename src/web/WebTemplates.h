// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once
#include <Arduino.h>

namespace WebTemplates {

static const char kWifiIconSvg[] PROGMEM = R"SVG(<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20"></line></svg>)SVG";

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

        .network-icon { margin-right: 12px; color: var(--text-dim); display: flex; }
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
                    <a href="/?scan=1" class="rescan-btn" id="rescan-btn">
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
            if (!ssid || !ssid.trim()) return;
            ssid = ssid.trim();

            var nameEl = element.querySelector('.network-name');
            var metaEl = element.querySelector('.network-meta');

            if (nameEl) nameEl.textContent = ssid;
            if (metaEl) metaEl.textContent = "Manual Entry";

            element.setAttribute('data-ssid', ssid);
            selectNetwork(ssid, element);
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
                setTimeout(function() { window.location.href = '/'; }, 2500);
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
                Wi-Fi settings have been sent to the device. Please check the device screen to confirm the connection status.
            </p>
            <div class="hint">You can close this tab now.</div>
        </div>
        <div class="footer">Powered by 21CNCStudio</div>
    </div>
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

                <button type="button" onclick="testConnection()">Test Connection</button>
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
            var user = document.getElementById('user').value.trim();
            var pass = document.getElementById('pass').value.trim();

            if (!anonymous && (user === '' || pass === '')) {
                e.preventDefault();
                alert('Username and password are required when anonymous mode is disabled');
                return false;
            }
        });

        function testConnection() {
            var btn = event.currentTarget;
            var originalText = btn.textContent;

            btn.disabled = true;
            btn.textContent = 'Testing...';

            var form = document.getElementById('mqtt-form');
            var formData = new FormData(form);
            formData.append('test', '1');

            fetch('/mqtt', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (!response.ok) throw new Error('Network response was not ok');
                return response.json();
            })
            .then(data => {
                btn.textContent = data.success ? 'Success! 🟢' : 'Failed! 🔴';
                setTimeout(() => {
                    btn.textContent = originalText;
                    btn.disabled = false;
                }, 2000);
            })
            .catch(error => {
                console.error('Error:', error);
                btn.textContent = 'Error! ⚠️';
                setTimeout(() => {
                    btn.textContent = originalText;
                    btn.disabled = false;
                }, 2000);
            });
        }

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
      bg_color: "{{BG_COLOR}}",
      card_top: "{{CARD_TOP}}",
      card_bottom: "{{CARD_BOTTOM}}",
      card_gradient: {{CARD_GRADIENT_BOOL}},
      card_border: "{{CARD_BORDER}}",
      shadow_color: "{{SHADOW_COLOR}}",
      text_color: "{{TEXT_COLOR}}"
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

    const normalizeHex = (v) => {
      if (!v) return "#000000";
      let val = v.trim();
      if (val[0] !== "#") val = "#" + val;
      if (val.length === 4) val = "#" + val[1] + val[1] + val[2] + val[2] + val[3] + val[3];
      return val.length === 7 ? val.toLowerCase() : "#000000";
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

    const showToast = () => {
      elements.toast.classList.add('visible');
      setTimeout(() => elements.toast.classList.remove('visible'), 3000);
    };

    elements.applyBtn.addEventListener('click', async () => {
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
        if (res.ok) {
          showToast();
        }
      } finally {
        elements.applyBtn.disabled = false;
        elements.applyBtn.innerText = originalText;
      }
    });

    render();
  </script>
</body>
</html>
)HTML";

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

} // namespace WebTemplates

