// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebTemplates.h"

namespace WebTemplates {

// NOTE: keep this raw HTML block as source for scripts/generate_dashboard_gzip.py.
// It is intentionally excluded from compilation to avoid duplicate flash usage.
#if 0
const char kDashboardPageTemplateAp[] PROGMEM = R"HTML_DASH_AP(
<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Aura Dashboard</title>
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHZpZXdCb3g9JzAgMCAxNjkzLjMyIDE2OTMuMzInPjxwYXRoIGZpbGw9JyNFNkI4NzMnIGZpbGwtcnVsZT0nZXZlbm9kZCcgY2xpcC1ydWxlPSdldmVub2RkJyBkPSdNODQ2LjY2IDE2NDIuNTNjNDM5LjU1LDAgNzk1Ljg3LC0zNTYuMzIgNzk1Ljg3LC03OTUuODcgMCwtNDM5LjU1IC0zNTYuMzIsLTc5NS44NyAtNzk1Ljg3LC03OTUuODcgLTQzOS41NSwwIC03OTUuODcsMzU2LjMyIC03OTUuODcsNzk1Ljg3IDAsNDM5LjU1IDM1Ni4zMiw3OTUuODcgNzk1Ljg3LDc5NS44N3ptLTQxOS4zMiAtMTM3MC4yOGMzMjcuMTcsLTYwLjE4IDY3NS4xNCwxNTAuNDQgNjkwLjYzLDUxMi44IDU2LjE2LC0xODAuOSAtMTEuMTksLTM3My44MiAtMTQzLjQxLC01MDMuNTIgLTY2LjY4LC02NS40IC0xNDkuNjgsLTExNC4zNiAtMjQxLjE0LC0xMzcuMDcgLTExMy4xNywxOC4xMSAtMjE3LjQsNjIuOTQgLTMwNi4wOCwxMjcuNzl6bTU4Ny44NyAzNTMuMjZjLTcyLjgsLTE3NC42NCAtMjQ5LjE4LC0yNzkuNTUgLTQzMy42OCwtMjkzLjkxIC05My4xNiwtNy4yNSAtMTg4LjA3LDguNDQgLTI3Mi40OCw0OS41NCAtNzIuNSw4My42NSAtMTI1LjgsMTg0LjM3IC0xNTMuMDUsMjk1LjM2IDIxMi4wOCwtMjU2LjcxIDYxNC4yOSwtMzE5LjMzIDg1OS4yMSwtNTAuOTl6bS0xMjAuNiAtNDg4LjQ1YzEyOC42OCw3My4xOCAyMzAuMDQsMTk0IDI3OSwzMzMuMjQgNjAuODgsMTczLjE0IDM1LjAyLDM1NC44MiAtNzkuNjgsNTAzLjcxIDE1OS4xNywtMTAyLjIzIDIzMS44NiwtMjkzLjk1IDIxMy45OCwtNDc4LjEgLTkuMDIsLTkyLjkgLTQxLjEsLTE4My43IC05Ni41MSwtMjU5Ljg3IC05My43NSwtNTYuMTQgLTIwMS41MSwtOTEuMyAtMzE2Ljc5LC05OC45OHptNDQ0LjkzIDE5Ni45N2MxMTUuODMsMzEyLjA3IC0zMC44NCw2OTEuMzEgLTM4NS41NSw3NjkuMTUgMTg3LjU3LDI0LjE0IDM2Ni43MSwtNzYuMDkgNDcxLjM0LC0yMjguNjEgNTIuOTQsLTc3LjE3IDg2LjgsLTE2Ny40NSA5My4yOCwtMjYxLjM1IC0zNy4xNiwtMTA2Ljk4IC05OS4xNiwtMjAyLjMyIC0xNzkuMDcsLTI3OS4xOXptMjE0LjI3IDQzNi41N2MtMTExLjU0LDMxMy41MiAtNDY3LjgzLDUwOS43MiAtNzg5LjQ0LDM0MS44MSAxMjguMjQsMTM4Ljc3IDMyOS44MiwxNzcuMSA1MDcuNzcsMTI3LjU5IDkwLjAzLC0yNS4wNSAxNzQuMDEsLTcyLjUzIDIzOS40NCwtMTQwLjUxIDI5LjksLTc4LjU1IDQ2LjI4LC0xNjMuNzggNDYuMjgsLTI1Mi44MyAwLC0yNS43IC0xLjM5LC01MS4wNyAtNC4wNSwtNzYuMDZ6bS0xMTYuNTcgNDcyLjM4Yy0yODcuMTIsMTY4LjIzIC02ODYuMjIsODkuOTYgLTgyNC41NCwtMjQ1Ljk4IDguNzEsMTg5LjA4IDEzOC40NSwzNDcuNzUgMzA2LjkzLDQyNC4zNiA4NS4xMywzOC43MSAxNzkuODQsNTYuMzggMjczLjQzLDQ2LjUzIDk4LjAzLC01NC43OCAxODEuNzUsLTEzMi4wOSAyNDQuMTgsLTIyNC45MXptLTM5Mi42NSAyODYuOTRjLTMyOC4xLC01NS4yOCAtNTgzLjU4LC0zNzIuMzUgLTQ3My44NywtNzE4LjM5IC0xMTQuNzgsMTUwLjY2IC0xMTcuNCwzNTUuNDIgLTM3LjU5LDUyMi40IDQwLjMxLDg0LjMzIDEwMS41NCwxNTguNzUgMTc5LjY4LDIxMS4zNCA0My4zNSw4LjI2IDg4LjEsMTIuNTkgMTMzLjg1LDEyLjU5IDY4LjY5LDAgMTM1LjEsLTkuNzcgMTk3LjkzLC0yNy45NHptLTQ4NS4zOCAtMzIuNTdjLTIxNS44MSwtMjUzLjM0IC0yMDgsLTY2MC4yOSA5OC42NCwtODU0Ljg4IC0xODQuNjksNDEuNjUgLTMxOC40MywxOTYuNzIgLTM2NC41OCwzNzUuODUgLTIzLjI5LDkwLjQgLTI0LjIsMTg2LjczIDEuODQsMjc3LjMyIDcwLjQ1LDg2LjQ0IDE2MC44MSwxNTYuMDEgMjY0LjEsMjAxLjcxem0tMzUxLjA0IC0zMzcuMTZjLTIuMTgsLTMzMy4wNiAyNjUuMzYsLTYzOS43MSA2MjUuNDgsLTU5MS4yNiAtMTY4LjE0LC04Ny4wMyAtMzcwLjYyLC01NC4wNCAtNTIxLjM0LDUzLjY2IC01OS43Miw0Mi42OCAtMTc2Ljg1LDE2NS4xNiAtMTc2Ljg1LDIyNC4wNyAwLDExMi41MyAyNi4xOCwyMTguOTQgNzIuNzEsMzEzLjUzem02MzguNDkgLTEyMy4yM2MxMDUuMSwwIDE5MC4zLC04NS4yIDE5MC4zLC0xOTAuMyAwLC0xMDUuMSAtODUuMiwtMTkwLjMgLTE5MC4zLC0xOTAuMyAtMTA1LjEsMCAtMTkwLjMsODUuMiAtMTkwLjMsMTkwLjMgMCwxMDUuMSA4NS4yLDE5MC4zIDE5MC4zLDE5MC4zeicvPjwvc3ZnPg==" />
  <style>
    :root {
      color-scheme: dark;
      --bg: #04080b;
      --surface: #11181d;
      --surface-raised: #141e22;
      --surface-recessed: #071014;
      --border: rgba(169,199,196,.12);
      --border-medium: rgba(169,199,196,.18);
      --text: #f9fafb;
      --muted: #d1d5db;
      --subtle: #9ca3af;
      --teal: #2bb4bd;
      --teal-hover: #57c4c2;
      --mist: #a9c7c4;
      --good: #6fc98f;
      --card-shadow: inset 0 1px 0 rgba(226,250,245,.04), 0 10px 26px rgba(0,0,0,.18);
    }
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
    html, body {
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      font-size: 14px;
      line-height: 1.5;
      background: var(--bg);
    }
    body { padding: 16px; }
    @media (min-width: 768px) { body { padding: 16px 24px; } }
    @media (min-width: 1024px) { body { padding: 16px 32px; } }
    .wrap { width: 100%; max-width: 448px; margin: 0 auto; }
    @media (min-width: 768px) { .wrap { max-width: 768px; } }
    @media (min-width: 1024px) { .wrap { max-width: 1152px; } }

    /* ── Header ── */
    .hdr { display: flex; align-items: center; justify-content: space-between; gap: 12px; margin-bottom: 20px; flex-wrap: wrap; }
    .hdr-left { min-width: 0; }
    .brand { display: flex; align-items: center; gap: 9px; }
    .brand-dot {
      position: relative;
      width: 14px;
      height: 14px;
      border-radius: 999px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      flex: 0 0 auto;
    }
    .brand-dot::before {
      content: "";
      position: absolute;
      inset: 0;
      border-radius: 999px;
      border: 1px solid rgba(43, 180, 189, 0.75);
      box-shadow: 0 0 10px rgba(43, 180, 189, 0.58);
      animation: pulse-dot 1.8s ease-in-out infinite;
    }
    .brand-dot::after {
      content: "";
      width: 8px;
      height: 8px;
      border-radius: 999px;
      background: var(--teal);
      box-shadow: 0 0 8px rgba(43, 180, 189, 0.75);
    }
    @keyframes pulse-dot {
      0% { transform: scale(0.88); opacity: 0.72; }
      70% { transform: scale(1.1); opacity: 0.2; }
      100% { transform: scale(0.88); opacity: 0.72; }
    }
    .brand-name { font-size: 20px; font-weight: 700; color: var(--text); letter-spacing: .01em; }
    .brand-sub {
      margin-left: 22px;
      margin-top: 3px;
      font-size: 13px;
      color: var(--subtle);
      font-weight: 600;
      letter-spacing: .03em;
      text-transform: none;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      max-width: 220px;
    }
    .hdr-right { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; justify-content: flex-end; }
    .clock { text-align: right; }
    .clock-time { font-size: 20px; font-weight: 700; line-height: 1; letter-spacing: .03em; }
    .clock-date { margin-top: 4px; color: var(--subtle); font-size: 11px; font-weight: 600; letter-spacing: .12em; text-transform: uppercase; }
    @media (min-width: 768px) {
      .hdr { margin-bottom: 24px; }
      .brand-name { font-size: 24px; }
      .brand-sub { font-size: 14px; }
      .clock-time { font-size: 24px; }
      .clock-date { font-size: 12px; }
    }
    .hdr-actions { display: flex; gap: 8px; }
    .btn {
      cursor: pointer;
      border-radius: 10px;
      padding: 9px 14px;
      font-size: 13px;
      font-weight: 700;
      border: 1px solid var(--border-medium);
      background: rgba(17,24,29,.78);
      color: var(--muted);
      transition: border-color .15s, background .15s, color .15s;
    }
    .btn:hover { border-color: rgba(43,180,189,.34); background: rgba(43,180,189,.08); color: var(--text); }
    .btn:disabled { opacity: .5; cursor: not-allowed; }
    .btn-cyan { background: rgba(43,180,189,.12); border-color: rgba(43,180,189,.45); color: var(--teal-hover); }
    .btn-cyan:hover { background: rgba(43,180,189,.2); border-color: rgba(43,180,189,.62); color: var(--mist); }
    .btn-danger { background: rgba(239,68,68,.12); border-color: rgba(239,68,68,.45); color: #fca5a5; }
    .btn-danger:hover { background: rgba(239,68,68,.2); border-color: rgba(239,68,68,.62); color: #fecaca; }
    .btn-amber { background: rgba(245,158,11,.12); border-color: rgba(245,158,11,.45); color: #fcd34d; }
    .btn-amber:hover { background: rgba(245,158,11,.22); border-color: rgba(245,158,11,.62); color: #fde68a; }
    .link-btn { text-decoration: none; display: inline-flex; align-items: center; }
    .btn.disabled, .btn[aria-disabled="true"] { opacity: .5; pointer-events: none; cursor: not-allowed; }
    .net-status {
      margin: 16px 0 0;
      padding: 9px 12px;
      border-radius: 10px;
      border: 1px solid var(--border-medium);
      background: rgba(17, 24, 29, 0.72);
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
    }
    .net-status-text { font-size: 12px; font-weight: 700; letter-spacing: 0.01em; }
    .net-status-meta { font-size: 11px; color: var(--subtle); }
    .net-status.ok { border-color: rgba(111,201,143,.45); background: rgba(111,201,143,.1); }
    .net-status.ok .net-status-text { color: var(--good); }
    .net-status.warn { border-color: rgba(245,158,11,.45); background: rgba(120,53,15,.28); }
    .net-status.warn .net-status-text { color: #fcd34d; }
    .net-status.err { border-color: rgba(239,68,68,.45); background: rgba(127,29,29,.32); }
    .net-status.err .net-status-text { color: #fca5a5; }
    .ota-overlay {
      position: fixed;
      inset: 0;
      z-index: 9999;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 20px;
      background: rgba(4, 8, 11, 0.78);
      backdrop-filter: blur(3px);
      -webkit-backdrop-filter: blur(3px);
    }
    .ota-overlay.show { display: flex; }
    .ota-overlay-card {
      width: 100%;
      max-width: 460px;
      border-radius: 14px;
      border: 1px solid rgba(245,158,11,.45);
      background: linear-gradient(160deg, rgba(20,30,34,.98), rgba(7,16,20,.98));
      box-shadow: 0 18px 42px rgba(0,0,0,.42);
      padding: 18px 20px;
      color: #e5e7eb;
      text-align: center;
    }
    .ota-overlay-title {
      font-size: 16px;
      font-weight: 700;
      color: #fcd34d;
      letter-spacing: .01em;
    }
    .ota-overlay-text {
      margin-top: 8px;
      font-size: 13px;
      color: #cbd5e1;
      line-height: 1.45;
    }

    /* ── Tab nav ── */
    .tab-nav { display: flex; flex-wrap: wrap; background: var(--surface-recessed); padding: 4px; border-radius: 12px; margin-bottom: 16px; border: 1px solid var(--border); gap: 4px; box-shadow: inset 0 1px 0 rgba(226,250,245,.035); }
    .tab-btn { display: flex; align-items: center; justify-content: center; gap: 7px; flex: 1; min-width: 48%; padding: 8px 12px; border-radius: 8px; border: none; background: transparent; color: var(--subtle); font-size: 13px; font-weight: 700; cursor: pointer; transition: background .15s, color .15s; }
    @media (min-width:640px) { .tab-btn { min-width: 132px; flex: none; } }
    .tab-btn:hover { color: var(--text); }
    .tab-btn.active { background: var(--teal); color: #041012; box-shadow: 0 3px 12px rgba(43,180,189,.16); }
    .tab-btn svg { width: 16px; height: 16px; flex-shrink: 0; }
    .tab-panel { display: none; }
    .tab-panel.active { display: block; }

    /* ── Cards & grids ── */
    .card-g8 {
      background: linear-gradient(135deg, var(--surface) 0%, var(--surface-recessed) 100%);
      border: 1px solid var(--border-medium);
      border-radius: 16px;
      box-shadow: var(--card-shadow);
    }
    .card-g7 { background: var(--surface); border: 1px solid var(--border); border-radius: 12px; box-shadow: var(--card-shadow); }
    .mini-card { background: rgba(28,40,45,.58); border: 1px solid var(--border-medium); border-radius: 12px; padding: 12px 14px; box-shadow: inset 0 1px 0 rgba(226,250,245,.025); }

    /* ── HeroMetric ── */
    .hero-wrap { padding: 20px 22px 16px; }
    .hero-label { font-size: 11px; text-transform: uppercase; letter-spacing: .08em; color: #9ca3af; font-weight: 600; }
    .hero-val-row { display: flex; align-items: flex-end; gap: 8px; margin-top: 10px; }
    .hero-val { font-size: 64px; font-weight: 600; line-height: 1; }
    .hero-unit { font-size: 16px; color: #9ca3af; padding-bottom: 6px; }
    .hero-pill { margin-top: 2px; }
    .hero-progress-track { margin-top: 14px; height: 10px; background: rgba(28,40,45,.8); border-radius: 999px; overflow: hidden; }
    .hero-progress-fill { height: 100%; border-radius: 999px; transition: width .7s; }
    .hero-advice { margin-top: 10px; font-size: 14px; color: #d1d5db; }
    .hero-trend-sep { margin-top: 18px; padding-top: 14px; border-top: 1px solid var(--border); }
    .hero-trend-hdr { display: flex; align-items: center; justify-content: space-between; }
    .hero-trend-label { font-size: 10px; text-transform: uppercase; letter-spacing: .08em; color: #9ca3af; font-weight: 600; }
    .hero-trend-delta { font-size: 13px; font-weight: 600; }
    .hero-minmax { display: flex; justify-content: space-between; margin-top: 6px; font-size: 11px; color: #6b7280; }

    /* ── Status pill ── */
    .pill { display: inline-flex; align-items: center; padding: 3px 9px; border-radius: 999px; border: 1px solid; font-size: 11px; font-weight: 600; white-space: nowrap; }
    .pill-sm { font-size: 10px; padding: 2px 7px; }

    /* ── ClimateOverview ── */
    .climate-wrap { padding: 20px 22px 14px; display: flex; flex-direction: column; height: 100%; }
    .climate-head { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
    .climate-title { font-size: 12px; text-transform: uppercase; letter-spacing: .06em; color: #d1d5db; font-weight: 600; }
    .climate-grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 14px; }
    .climate-grid3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; margin-top: 10px; }
    .mini-label { font-size: 10px; text-transform: uppercase; letter-spacing: .08em; color: #9ca3af; font-weight: 600; }
    .mini-val-row { display: flex; align-items: flex-end; gap: 6px; margin-top: 7px; }
    .mini-val { font-size: 28px; font-weight: 600; line-height: 1; }
    .mini-unit { font-size: 14px; color: #d1d5db; padding-bottom: 2px; }
    .pressure-mini { margin-top: 10px; }
    .pressure-row { display: flex; flex-direction: column; gap: 6px; }
    @media (min-width: 560px) { .pressure-row { flex-direction: row; align-items: flex-end; justify-content: space-between; } }
    .pressure-delta-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; min-width: 180px; }
    .pdelta-box { border-radius: 6px; border: 1px solid; padding: 8px 12px; }
    .pdelta-lbl { font-size: 11px; color: #9ca3af; line-height: 1; }
    .pdelta-val { font-size: 17px; font-weight: 600; line-height: 1; margin-top: 4px; }

    /* ── GasMetricCard grid ── */
    .gas-grid { display: grid; grid-template-columns: 1fr; gap: 10px; margin-top: 14px; }
    @media (min-width: 640px) { .gas-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
    @media (min-width: 1024px) { .gas-grid { grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); } }
    .gas-card { padding: 12px 14px; }
    .gas-head { display: flex; align-items: center; justify-content: space-between; }
    .gas-name { font-size: 11px; text-transform: uppercase; letter-spacing: .06em; color: #d1d5db; font-weight: 600; }
    .gas-unit { font-size: 10px; color: #6b7280; }
    .gas-val-row { display: flex; align-items: flex-end; justify-content: space-between; margin-top: 6px; }
    .gas-val { font-size: 28px; font-weight: 600; line-height: 1; }
    .gas-progress { margin-top: 10px; height: 6px; background: var(--surface-raised); border-radius: 999px; overflow: hidden; }
    .gas-bar { height: 100%; border-radius: 999px; }

    /* ── PM compact grid ── */
    .pm-grid { display: grid; grid-template-columns: 1fr; gap: 8px; margin-top: 12px; }
    @media (min-width: 640px) { .pm-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
    @media (min-width: 1024px) { .pm-grid { grid-template-columns: repeat(3, minmax(0, 1fr)); } }
    @media (min-width: 1280px) { .pm-grid { grid-template-columns: repeat(5, minmax(0, 1fr)); } }
    .pm-card { padding: 10px 12px; }
    .pm-head { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
    .pm-label { font-size: 10px; text-transform: uppercase; letter-spacing: .06em; color: #9ca3af; font-weight: 600; }
    .pm-val { font-size: 22px; font-weight: 600; line-height: 1; }
    .pm-unit { font-size: 10px; color: #6b7280; padding-bottom: 0; }
    .pm-val-row { display: flex; align-items: flex-end; justify-content: space-between; gap: 5px; margin-top: 5px; }
    .pm-val-left { display: flex; align-items: flex-end; gap: 5px; }
    .pm-progress { margin-top: 6px; height: 5px; background: var(--surface-raised); border-radius: 999px; overflow: hidden; }
    .pm-bar { height: 100%; border-radius: 999px; }

    /* ── Section title (sensors tab) ── */
    .sec-title { font-size: 11px; text-transform: uppercase; letter-spacing: .08em; color: #6b7280; font-weight: 700; margin-bottom: 12px; }

    /* ── Sensors layout ── */
    .sensor-grid { display: grid; grid-template-columns: 1fr; gap: 16px; margin-top: 0; }
    @media (min-width: 768px) { .sensor-grid { gap: 20px; } }
    .sensor-hero, .sensor-climate { min-width: 0; }
    @media (min-width: 1280px) {
      .sensor-grid { grid-template-columns: repeat(12, minmax(0, 1fr)); }
      .sensor-hero { grid-column: span 7 / span 7; }
      .sensor-climate { grid-column: span 5 / span 5; }
    }

    /* ── Chart section ── */
    .chart-controls {
      display: grid;
      grid-template-columns: 1fr;
      gap: 12px;
      margin-bottom: 14px;
      align-items: start;
    }
    @media (min-width: 1280px) {
      .chart-controls {
        grid-template-columns: 320px minmax(0, 1fr);
        gap: 16px;
      }
    }
    .chart-controls .seg-ctrl {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 4px;
      gap: 4px;
    }
    .seg-ctrl { display: flex; background: rgba(7,16,20,.72); border: 1px solid var(--border-medium); border-radius: 8px; padding: 2px; gap: 2px; }
    .seg-btn { border: none; background: transparent; color: #9ca3af; padding: 5px 11px; font-size: 12px; font-weight: 600; border-radius: 6px; cursor: pointer; transition: background .12s, color .12s; }
    .chart-controls .seg-btn { flex: 1; padding: 6px 10px; font-size: 12px; font-weight: 700; color: #6b7280; }
    .chart-controls .seg-btn:hover { color: #d1d5db; }
    .seg-btn.active { background: var(--teal); color: #041012; }
    #cgroupSeg .seg-btn.active,
    #crangeSeg .seg-btn.active {
      background: var(--surface-raised);
      color: var(--text);
      box-shadow: 0 1px 3px rgba(0,0,0,.35), inset 0 0 0 1px var(--border-medium);
    }
    .chart-grid { display: grid; grid-template-columns: 1fr; gap: 12px; }
    @media (min-width: 768px) { .chart-grid { gap: 16px; } }
    @media (min-width: 1280px) { .chart-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
    .chart-box { padding: 12px; }
    @media (min-width: 768px) { .chart-box { padding: 16px; } }
    .chart-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
    .chart-name { font-size: 12px; font-weight: 700; color: #9ca3af; }
    .chart-latest-row { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; justify-content: flex-end; }
    .chart-latest-item { font-size: 13px; font-weight: 600; }
    .chart-svg-wrap { position: relative; height: 140px; width: 100%; }
    .chart-svg-wrap svg { width: 100%; height: 100%; display: block; overflow: visible; }
    .chart-point { fill: transparent; stroke: transparent; pointer-events: none; }
    .chart-vline {
      position: absolute;
      top: 0;
      bottom: 0;
      width: 1px;
      border-left: 1px dashed rgba(100, 116, 139, 0.72);
      opacity: 0;
      pointer-events: none;
      transition: opacity .12s;
    }
    .chart-vline.show { opacity: 1; }
    .chart-focus-layer {
      position: absolute;
      inset: 0;
      pointer-events: none;
      z-index: 5;
    }
    .chart-focus {
      position: absolute;
      width: 8px;
      height: 8px;
      border-radius: 999px;
      border: 1px solid var(--bg);
      background: #22c55e;
      box-shadow: 0 0 0 4px rgba(34, 197, 94, 0.28);
      transform: translate(-50%, -50%);
      opacity: 0;
      pointer-events: none;
      transition: opacity .12s;
    }
    .chart-focus.show { opacity: 1; }
    .chart-tooltip {
      position: absolute;
      z-index: 6;
      pointer-events: none;
      background: rgba(17, 24, 29, 0.97);
      border: 1px solid var(--border-medium);
      box-shadow: 0 10px 24px rgba(0, 0, 0, 0.4);
      border-radius: 8px;
      color: #f9fafb;
      font-size: 11px;
      line-height: 1.35;
      padding: 8px 10px;
      min-width: 120px;
      max-width: 250px;
      transform: translate(-50%, 0);
      display: none;
    }
    .chart-tooltip.show { display: block; }
    .chart-tooltip-time { color: #9ca3af; font-size: 11px; font-weight: 500; margin-bottom: 4px; }
    .chart-tooltip-rows { display: flex; flex-direction: column; gap: 4px; }
    .chart-tooltip-row { display: flex; align-items: baseline; justify-content: space-between; gap: 12px; }
    .chart-tooltip-name { font-size: 11px; font-weight: 600; }
    .chart-tooltip-value { color: #f8fafc; font-size: 11px; font-weight: 700; white-space: nowrap; }
    .chart-minmax { display: flex; justify-content: space-between; margin-top: 6px; font-size: 11px; color: #6b7280; }
    .no-data { color: #6b7280; font-size: 13px; padding: 24px 0; text-align: center; }

    /* ── Events ── */
    .events-shell { padding: 16px 18px; }
    .events-title { margin-bottom: 12px; }
    .events-list { display: flex; flex-direction: column; gap: 0; }
    .alert-item { display: flex; flex-direction: column; padding: 10px 14px; border-left: 4px solid; border-radius: 0 8px 8px 0; margin-bottom: 8px; }
    .alert-item.sev-warning { border-color: #eab308; background: rgba(234,179,8,.1); }
    .alert-item.sev-danger { border-color: #f97316; background: rgba(249,115,22,.1); }
    .alert-item.sev-critical { border-color: #ef4444; background: rgba(239,68,68,.1); }
    .alert-item.sev-info { border-color: #3b82f6; background: rgba(59,130,246,.1); }
    .alert-msg { font-size: 14px; color: #f9fafb; font-weight: 500; line-height: 1.4; }
    .alert-meta { display: flex; align-items: center; justify-content: space-between; gap: 8px; margin-top: 6px; }
    .alert-type { font-size: 11px; color: #9ca3af; font-weight: 700; text-transform: uppercase; }
    .alert-time { font-size: 12px; color: #a5f3fc; font-weight: 600; white-space: nowrap; }

    /* ── Settings ── */
    .sg { background: var(--surface); border: 1px solid var(--border); border-radius: 12px; padding: 16px 18px; box-shadow: var(--card-shadow); }
    .sg + .sg { margin-top: 12px; }
    .sg-title { font-size: 11px; text-transform: uppercase; letter-spacing: .08em; color: #6b7280; font-weight: 700; margin-bottom: 12px; }
    .sg-rows { display: flex; flex-direction: column; gap: 12px; }
    .toggle-row { display: flex; align-items: center; justify-content: space-between; cursor: pointer; user-select: none; }
    .toggle-row.disabled { opacity: .6; cursor: not-allowed; }
    .toggle-label { font-size: 14px; color: #d1d5db; }
    .toggle-sw { width: 40px; height: 20px; border-radius: 999px; background: var(--surface-raised); position: relative; transition: background .2s; flex-shrink: 0; }
    .toggle-sw.on { background: var(--teal); }
    .toggle-knob { position: absolute; top: 3px; left: 3px; width: 14px; height: 14px; border-radius: 50%; background: #fff; transition: left .2s; box-shadow: 0 1px 3px rgba(0,0,0,.4); }
    .toggle-sw.on .toggle-knob { left: 23px; }
    .toggle-msg { min-height: 14px; font-size: 11px; color: #6b7280; }
    .toggle-msg.err { color: #fca5a5; }
    .seg-row { display: flex; align-items: center; justify-content: space-between; }
    .seg-label { font-size: 14px; color: #d1d5db; }
    .stepper-row { display: flex; align-items: center; justify-content: space-between; }
    .stepper-label { font-size: 14px; color: #d1d5db; }
    .stepper-ctrl { display: flex; align-items: center; gap: 8px; }
    .stepper-btn { width: 30px; height: 30px; border-radius: 8px; border: 1px solid var(--border-medium); background: var(--surface-recessed); color: var(--muted); font-size: 18px; cursor: pointer; display: flex; align-items: center; justify-content: center; line-height: 1; }
    .stepper-btn:hover { border-color: rgba(43,180,189,.34); }
    .stepper-val { min-width: 52px; text-align: center; font-size: 14px; font-weight: 600; color: #f9fafb; font-family: Consolas, "Courier New", monospace; }
    .text-field-row { display: flex; flex-direction: column; gap: 6px; }
    .text-field-lbl { font-size: 12px; color: #9ca3af; }
    .text-input { width: 100%; background: var(--surface-recessed); border: 1px solid var(--border-medium); border-radius: 8px; padding: 8px 12px; color: var(--text); font-size: 14px; }
    .text-input:focus { outline: none; border-color: var(--teal); }
    .field-hint { font-size: 11px; color: #6b7280; line-height: 1.4; }
    .save-btn {
      margin-top: 14px;
      padding: 9px 18px;
      border-radius: 10px;
      border: 1px solid var(--border-medium);
      font-size: 13px;
      font-weight: 700;
      cursor: pointer;
      transition: background .15s, border-color .15s, color .15s;
    }
    .save-btn.idle { background: rgba(17,24,29,.72); color: #6b7280; border-color: var(--border); cursor: default; }
    .save-btn.dirty { background: rgba(43,180,189,.12); color: var(--teal-hover); border-color: rgba(43,180,189,.45); }
    .save-btn.dirty:hover { background: rgba(43,180,189,.2); border-color: rgba(43,180,189,.62); }
    .save-btn.saving { background: rgba(17,24,29,.88); color: #6b7280; border-color: var(--border); cursor: not-allowed; }
    .save-btn.saved { background: rgba(111,201,143,.12); color: var(--good); border-color: rgba(111,201,143,.45); }
    .save-btn.error { background: rgba(239,68,68,.12); color: #fca5a5; border-color: rgba(239,68,68,.45); }
    .settings-grid { display: grid; grid-template-columns: 1fr; gap: 12px; align-items: start; }
    .settings-col { display: flex; flex-direction: column; gap: 12px; }
    .settings-col .sg + .sg { margin-top: 0; }
    .conn-rows { display: flex; flex-direction: column; gap: 0; }
    .conn-rows .info-row:last-child { border-bottom: none; }
    @media (min-width: 768px) {
      .settings-grid { gap: 16px; }
      .settings-col { gap: 16px; }
    }
    @media (min-width: 1024px) { .settings-grid { grid-template-columns: 1fr 1fr; } }

    /* ── System tab ── */
    .system-grid { display: grid; grid-template-columns: 1fr; gap: 12px; align-items: start; }
    .system-col { display: flex; flex-direction: column; gap: 12px; }
    @media (min-width: 768px) { .system-grid { gap: 16px; } }
    @media (min-width: 768px) { .system-col { gap: 16px; } }
    @media (min-width: 1024px) { .system-grid { grid-template-columns: 1fr 1fr; } }
    .info-rows { display: flex; flex-direction: column; gap: 0; }
    .info-row { display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 8px 0; border-bottom: 1px solid var(--border); font-size: 13px; }
    .info-row:last-child { border-bottom: none; }
    .info-key { color: #9ca3af; flex-shrink: 0; }
    .info-val { color: #f9fafb; text-align: right; font-weight: 500; }
    .info-val.ok { color: #4ade80; }
    .info-val.err { color: #f87171; }
    .sys-btn-row { display: flex; flex-direction: column; gap: 8px; margin-top: 14px; }
    .sys-btn-row .btn,
    .sys-btn-row .link-btn { width: 100%; justify-content: center; }
    .sd-action-row { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 14px; }
    .sd-action-row .btn { flex: 1 1 150px; justify-content: center; }
    .sd-note { min-height: 16px; margin-top: 10px; color: #9ca3af; font-size: 12px; line-height: 1.4; }
    .sd-note.ok { color: #86efac; }
    .sd-note.err { color: #fca5a5; }
    .sd-note.warn { color: #fcd34d; }

    /* ── OTA progress ── */
    .progress-track { height: 8px; background: var(--surface-recessed); border-radius: 999px; overflow: hidden; border: 1px solid var(--border); margin-top: 10px; }
    .progress-fill { height: 100%; background: linear-gradient(90deg, var(--teal), var(--teal-hover)); border-radius: 999px; width: 0%; transition: width .25s; }
    .ota-status { font-size: 12px; color: #9ca3af; margin-top: 6px; }
    .ota-status.ok { color: #4ade80; }
    .ota-status.warn { color: #fcd34d; }
    .ota-status.err { color: #f87171; }
    .ota-precheck {
      margin-bottom: 4px;
      padding: 8px 10px;
      border-radius: 8px;
      border: 1px solid var(--border);
      background: rgba(17, 24, 29, 0.72);
      color: #9ca3af;
      font-size: 12px;
      line-height: 1.35;
    }
    .ota-precheck.ok { border-color: rgba(34,197,94,.45); color: #86efac; background: rgba(20,83,45,.28); }
    .ota-precheck.warn { border-color: rgba(245,158,11,.45); color: #fcd34d; background: rgba(120,53,15,.24); }
    .ota-precheck.err { border-color: rgba(239,68,68,.45); color: #fca5a5; background: rgba(127,29,29,.28); }
    .file-input {
      display: block;
      width: 100%;
      font-size: 12px;
      color: #d1d5db;
    }
    .file-input::file-selector-button {
      margin-right: 10px;
      border-radius: 6px;
      border: 1px solid var(--border-medium);
      background: var(--surface);
      color: #e5e7eb;
      padding: 6px 10px;
      font-size: 12px;
      font-weight: 600;
      cursor: pointer;
    }
    .file-input:disabled { opacity: .6; cursor: not-allowed; }
    .file-input:disabled::file-selector-button { cursor: not-allowed; opacity: .8; }

    /* ── Misc ── */
    .muted { color: #6b7280; }
    @media (max-width: 560px) { .brand-name { font-size: 19px; } .clock-time { font-size: 19px; } .hero-val { font-size: 52px; } .mini-val { font-size: 24px; } }
  </style>
</head>
<body>
<div class="wrap">

  <!-- Header -->
  <div class="hdr">
    <div class="hdr-left">
      <div class="brand">
        <span class="brand-dot"></span>
        <span class="brand-name">AURA AQ</span>
      </div>
      <div id="deviceNameLabel" class="brand-sub">Device</div>
    </div>
    <div class="hdr-right">
      <div class="clock">
        <div id="headerTime" class="clock-time">--:--</div>
        <div id="headerDate" class="clock-date">-- --- ----</div>
      </div>
    </div>
  </div>

  <!-- Tab nav -->
  <div class="tab-nav">
    <button class="tab-btn active" type="button" data-tab="sensors">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="4"/><circle cx="12" cy="12" r="1.6"/><path d="M12 4.5v2.5M12 17v2.5M4.5 12H7M17 12h2.5"/></svg>
      Sensors
    </button>
    <button class="tab-btn" type="button" data-tab="charts">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 18h16"/><path d="M6 14.5 10 10l3 2.5 5-6"/><path d="M18 6h-2.5v2.5"/></svg>
      Charts
    </button>
    <button class="tab-btn" type="button" data-tab="events">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 15.5V11a6 6 0 0 0-12 0v4.5L4.5 17h15L18 15.5z"/><path d="M10 18a2 2 0 0 0 4 0"/></svg>
      Events
    </button>
    <button class="tab-btn" type="button" data-tab="settings">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7h16"/><circle cx="9" cy="7" r="1.6"/><path d="M4 12h16"/><circle cx="15" cy="12" r="1.6"/><path d="M4 17h16"/><circle cx="11" cy="17" r="1.6"/></svg>
      Settings
    </button>
    <button class="tab-btn" type="button" data-tab="system">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><rect x="4.5" y="5" width="15" height="10.5" rx="2.2"/><path d="M8 9h5M8 11.8h7"/><circle cx="16.2" cy="9.4" r="1.2"/><path d="M9 19h6M12 15.5V19"/></svg>
      System
    </button>
  </div>

  <!-- === SENSORS === -->
  <div id="tab-sensors" class="tab-panel active">
    <div class="sensor-grid">
      <div id="heroMetric" class="card-g8 sensor-hero"></div>
      <div id="climateOverview" class="card-g8 sensor-climate"></div>
    </div>
    <div style="margin-top:14px"><div id="gasGrid"></div></div>
    <div style="margin-top:14px"><div id="pmGrid"></div></div>
  </div>

  <!-- === CHARTS === -->
  <div id="tab-charts" class="tab-panel">
    <div class="chart-controls">
      <div class="seg-ctrl" id="crangeSeg">
        <button class="seg-btn" type="button" data-range="1h">1h</button>
        <button class="seg-btn" type="button" data-range="3h">3h</button>
        <button class="seg-btn active" type="button" data-range="24h">24h</button>
      </div>
      <div class="seg-ctrl" id="cgroupSeg">
        <button class="seg-btn active" type="button" data-group="core">Core</button>
        <button class="seg-btn" type="button" data-group="gases">Gases</button>
        <button class="seg-btn" type="button" data-group="pm">PM</button>
      </div>
    </div>
    <div id="chartGrid" class="chart-grid"></div>
  </div>

  <!-- === EVENTS === -->
  <div id="tab-events" class="tab-panel">
    <div class="card-g7 events-shell">
      <div class="sg-title events-title">System Log</div>
      <div id="eventsList" class="events-list"><div class="no-data">Loading...</div></div>
    </div>
  </div>

  <!-- === SETTINGS === -->
  <div id="tab-settings" class="tab-panel">
    <div class="settings-grid">
      <div class="settings-col">
        <div class="sg">
          <div class="sg-title">Display</div>
          <div class="sg-rows">
            <div class="toggle-row" id="nightModeToggle">
              <span class="toggle-label">Night Mode</span>
              <div class="toggle-sw" id="nightModeSw"><div class="toggle-knob"></div></div>
            </div>
            <div class="toggle-row" id="backlightToggle">
              <span class="toggle-label">Backlight ON</span>
              <div class="toggle-sw on" id="backlightSw"><div class="toggle-knob"></div></div>
            </div>
            <div id="displayToggleMsg" class="toggle-msg" aria-live="polite"></div>
          </div>
        </div>
        <div class="sg">
          <div class="sg-title">Measurements</div>
          <div class="sg-rows">
            <div class="seg-row">
              <span class="seg-label">Unit system</span>
              <div class="seg-ctrl" id="tempUnitSeg">
                <button class="seg-btn active" type="button" data-unit="c">Metric</button>
                <button class="seg-btn" type="button" data-unit="f">US</button>
              </div>
            </div>
            <div class="stepper-row">
              <span class="stepper-label" id="tempOffsetLabel">Temp offset (&deg;C)</span>
              <div class="stepper-ctrl">
                <button class="stepper-btn" type="button" id="tempOffsetMinus">&minus;</button>
                <span class="stepper-val" id="tempOffsetVal">0.0</span>
                <button class="stepper-btn" type="button" id="tempOffsetPlus">+</button>
              </div>
            </div>
            <div class="stepper-row">
              <span class="stepper-label">Humidity offset (%)</span>
              <div class="stepper-ctrl">
                <button class="stepper-btn" type="button" id="humOffsetMinus">&minus;</button>
                <span class="stepper-val" id="humOffsetVal">0</span>
                <button class="stepper-btn" type="button" id="humOffsetPlus">+</button>
              </div>
            </div>
            <button class="save-btn idle" type="button" id="saveSettingsBtn">No changes</button>
          </div>
        </div>
      </div>
      <div class="settings-col">
        <div class="sg">
          <div class="sg-title">Connectivity</div>
          <div class="conn-rows">
            <div class="info-row"><span class="info-key">Mode</span><span class="info-val" id="cfg-mode">--</span></div>
            <div class="info-row"><span class="info-key">WiFi SSID</span><span class="info-val" id="cfg-ssid">--</span></div>
            <div class="info-row"><span class="info-key">Hostname</span><span class="info-val" id="cfg-hostname">--</span></div>
            <div class="info-row"><span class="info-key">IP Address</span><span class="info-val" id="cfg-ip">--</span></div>
            <div class="info-row"><span class="info-key">Signal</span><span class="info-val" id="cfg-rssi">--</span></div>
            <div class="info-row"><span class="info-key">MQTT Broker</span><span class="info-val" id="cfg-mqtt-broker">--</span></div>
            <div class="info-row"><span class="info-key">MQTT Status</span><span class="info-val" id="cfg-mqtt-status">--</span></div>
            <div class="info-row"><span class="info-key">Last Sync</span><span class="info-val" id="cfg-last-sync">--</span></div>
          </div>
        </div>
        <div class="sg">
          <div class="sg-title">Device</div>
          <div class="sg-rows">
            <div class="text-field-row">
              <label class="text-field-lbl" for="displayNameInput">Display name</label>
              <input class="text-input" type="text" id="displayNameInput" maxlength="32" placeholder="aura" />
            </div>
            <button class="save-btn idle" type="button" id="saveNameBtn">No changes</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- === SYSTEM === -->
  <div id="tab-system" class="tab-panel">
    <div class="system-grid">
      <div class="system-col">
        <div class="sg">
          <div class="sg-title">System Info</div>
          <div class="info-rows" id="systemInfoRows">
            <div class="info-row"><span class="info-key">Mode</span><span class="info-val" id="si-mode">--</span></div>
            <div class="info-row"><span class="info-key">IP Address</span><span class="info-val" id="si-ip">--</span></div>
            <div class="info-row"><span class="info-key">Hostname</span><span class="info-val" id="si-hostname">--</span></div>
            <div class="info-row"><span class="info-key">Firmware</span><span class="info-val" id="si-firmware">--</span></div>
            <div class="info-row"><span class="info-key">Build</span><span class="info-val" id="si-build">--</span></div>
            <div class="info-row"><span class="info-key">Uptime</span><span class="info-val" id="si-uptime">--</span></div>
            <div class="info-row"><span class="info-key">DAC</span><span class="info-val" id="si-dac">--</span></div>
          </div>
          <div class="sys-btn-row">
            <button class="btn btn-danger" type="button" id="rebootBtn">Reboot Device</button>
            <button class="btn" type="button" id="openThemeBtn" disabled>Open Theme Studio</button>
            <button class="btn" type="button" id="openDacBtn" disabled>Open Fan Control Page</button>
            <a class="btn link-btn" href="/thresholds">Display Thresholds</a>
            <a class="btn link-btn" href="/dashboard" id="networkActionBtn">Open Dashboard</a>
          </div>
        </div>
        <div class="sg">
          <div class="sg-title">SD History</div>
          <div class="info-rows">
            <div class="info-row"><span class="info-key">Card</span><span class="info-val" id="sd-status">--</span></div>
            <div class="info-row"><span class="info-key">Card size</span><span class="info-val" id="sd-capacity">--</span></div>
            <div class="info-row"><span class="info-key">Free space</span><span class="info-val" id="sd-free-space">--</span></div>
            <div class="info-row"><span class="info-key">Current day</span><span class="info-val" id="sd-day">--</span></div>
            <div class="info-row"><span class="info-key">CSV units</span><span class="info-val" id="sd-csv-units">--</span></div>
            <div class="info-row"><span class="info-key">Metric samples</span><span class="info-val" id="sd-samples">--</span></div>
            <div class="info-row"><span class="info-key">Pending days</span><span class="info-val" id="sd-pending-days">--</span></div>
            <div class="info-row"><span class="info-key">State file</span><span class="info-val" id="sd-state-file">--</span></div>
            <div class="info-row"><span class="info-key">Daily CSV</span><span class="info-val" id="sd-daily-csv">--</span></div>
            <div class="info-row"><span class="info-key">Last write</span><span class="info-val" id="sd-last-write">--</span></div>
            <div class="info-row"><span class="info-key">SD error</span><span class="info-val" id="sd-runtime-error">--</span></div>
          </div>
          <div class="sd-action-row">
            <button class="btn" type="button" id="sdRefreshBtn">Refresh SD</button>
            <a class="btn btn-cyan link-btn disabled"
               href="/api/history/current-day.csv"
               id="sdExportCurrentBtn"
               aria-disabled="true"
               tabindex="-1">Export current day</a>
            <a class="btn btn-cyan link-btn disabled"
               href="/api/history/daily.csv"
               id="sdExportCsvBtn"
               aria-disabled="true"
               tabindex="-1">Export daily CSV</a>
            <button class="btn btn-danger" type="button" id="sdClearCurrentBtn">Delete current day</button>
            <button class="btn btn-danger" type="button" id="sdClearHistoryBtn">Clear history</button>
          </div>
          <div class="sd-note" id="sd-note" aria-live="polite">Open System to load SD history status.</div>
        </div>
      </div>
      <div class="system-col">
        <div class="sg">
          <div class="sg-title">Firmware OTA</div>
          <div class="sg-rows">
            <div id="otaPrecheck" class="ota-precheck warn">Waiting for device state before OTA.</div>
            <div class="text-field-row">
              <label class="text-field-lbl" for="otaFile">Firmware file (.bin)</label>
              <input class="file-input" type="file" id="otaFile" accept=".bin,application/octet-stream" />
            </div>
            <button class="btn" type="button" id="otaUploadBtn">Upload firmware</button>
            <div class="progress-track"><div class="progress-fill" id="otaProgress"></div></div>
            <div class="ota-status" id="otaStatus">No upload in progress.</div>
          </div>
        </div>
        <div class="sg">
          <div class="sg-title">Time Sync</div>
          <div class="sg-rows">
            <div class="toggle-row" id="ntpEnabledToggle">
              <span class="toggle-label">Auto Sync (NTP)</span>
              <div class="toggle-sw on" id="ntpEnabledSw"><div class="toggle-knob"></div></div>
            </div>
            <div class="text-field-row">
              <label class="text-field-lbl" for="ntpServerInput">NTP Server</label>
              <input class="text-input"
                     type="text"
                     id="ntpServerInput"
                     maxlength="64"
                     placeholder="pool.ntp.org or 192.168.1.1" />
              <div class="field-hint">Leave empty to use default public servers.</div>
            </div>
            <div class="info-rows">
              <div class="info-row"><span class="info-key">Status</span><span class="info-val" id="ntp-status">--</span></div>
              <div class="info-row"><span class="info-key">Last Sync</span><span class="info-val" id="ntp-last-sync">--</span></div>
            </div>
            <button class="save-btn idle" type="button" id="saveTimeSyncBtn">No changes</button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div id="netStatusBar" class="net-status warn">
    <span id="netStatusText" class="net-status-text">Connecting to device...</span>
    <span id="netStatusMeta" class="net-status-meta">No live state yet.</span>
  </div>

</div><!-- .wrap -->
<div id="otaGlobalOverlay" class="ota-overlay" aria-live="assertive" role="status">
  <div class="ota-overlay-card">
    <div class="ota-overlay-title">OTA in progress</div>
    <div id="otaGlobalOverlayText" class="ota-overlay-text">
      Another client is updating firmware. Live dashboard updates are paused.
    </div>
  </div>
</div>
<script>
// ─────────────────────────────────────────────
// Thresholds & status
// ─────────────────────────────────────────────
const thresholds = {
  co2:             { good: 800,  moderate: 1000, bad: 1500 },
  pm05:            { good: 250,  moderate: 600,  bad: 1200 },
  pm25:            { good: 12,   moderate: 35,   bad: 55 },
  pm1:             { good: 10,   moderate: 25,   bad: 50 },
  pm4:             { good: 25,   moderate: 50,   bad: 75 },
  pm10:            { good: 54,   moderate: 154,  bad: 254 },
  voc:             { good: 150,  moderate: 250,  bad: 350 },
  nox:             { good: 50,   moderate: 100,  bad: 200 },
  hcho:            { good: 30,   moderate: 60,   bad: 100 },
  co:              { good: 9,    moderate: 35,   bad: 100 },
  temp:            { good: [20, 25], moderate: [18, 26], bad: [16, 28] },
  rh:              { good: [40, 60], moderate: [30, 65], bad: [20, 70] },
  dewPoint:        { good: [10, 16], moderate: [8, 18],  bad: [5, 21] },
  ah:              { good: [7, 15],  moderate: [5, 18],  bad: [4, 20] },
  mold:            { good: 2,    moderate: 4,    bad: 7 },
  pressureDelta3h: { good: 1.0,  moderate: 3.0,  bad: 6.0 },
  pressureDelta24h:{ good: 2.0,  moderate: 6.0,  bad: 10.0 },
};

function assignHighThreshold(key, source) {
  const target = thresholds[key];
  if (!target || !source) return;
  if (isNum(source.green)) target.good = source.green;
  if (isNum(source.yellow)) target.moderate = source.yellow;
  if (isNum(source.orange)) target.bad = source.orange;
}

function assignRangeThreshold(key, source) {
  const target = thresholds[key];
  if (!target || !source) return;
  if ([source.good_min, source.good_max].every(isNum)) target.good = [source.good_min, source.good_max];
  if ([source.yellow_min, source.yellow_max].every(isNum)) target.moderate = [source.yellow_min, source.yellow_max];
  if ([source.orange_min, source.orange_max].every(isNum)) target.bad = [source.orange_min, source.orange_max];
}

function applyThresholdsFromState(stateThresholds) {
  const metrics = stateThresholds && stateThresholds.metrics;
  if (!metrics || typeof metrics !== 'object') return;
  assignHighThreshold('co2', metrics.co2);
  assignHighThreshold('hcho', metrics.hcho);
  assignHighThreshold('co', metrics.co);
  assignHighThreshold('voc', metrics.voc);
  assignHighThreshold('nox', metrics.nox);
  assignRangeThreshold('temp', metrics.temp);
  assignRangeThreshold('rh', metrics.rh);
  assignRangeThreshold('dewPoint', metrics.dew_point);
  assignRangeThreshold('ah', metrics.ah);
  refreshGasMetricMaxes();
}

const statusColors  = { good:'#22c55e', moderate:'#facc15', bad:'#f97316', critical:'#ef4444' };
const statusLabels  = { good:'Good', moderate:'Moderate', bad:'Poor', critical:'Hazard' };
const fallbackColor = '#9ca3af';

function getStatus(value, thr) {
  if (Array.isArray(thr.good)) {
    if (value >= thr.good[0]     && value <= thr.good[1])     return 'good';
    if (value >= thr.moderate[0] && value <= thr.moderate[1]) return 'moderate';
    if (value >= thr.bad[0]      && value <= thr.bad[1])      return 'bad';
    return 'critical';
  }
  if (value <= thr.good)     return 'good';
  if (value <= thr.moderate) return 'moderate';
  if (value <= thr.bad)      return 'bad';
  return 'critical';
}

function statusColorOf(s) { return statusColors[s] || fallbackColor; }

function hexToRgb(hex) {
  if (typeof hex !== 'string' || !hex.startsWith('#')) return null;
  let h = hex.slice(1);
  if (h.length === 3) h = h.split('').map(c => c+c).join('');
  if (h.length !== 6) return null;
  const n = parseInt(h, 16);
  return { r:(n>>16)&255, g:(n>>8)&255, b:n&255 };
}

function rgba(hex, a) {
  const rgb = hexToRgb(hex);
  if (!rgb) return `rgba(156,163,175,${a})`;
  return `rgba(${rgb.r},${rgb.g},${rgb.b},${a})`;
}

function isNum(v) { return typeof v === 'number' && Number.isFinite(v); }
function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

function metricStatus(value, thr, round) {
  if (!isNum(value)) return null;
  return getStatus(round ? Math.round(value) : value, thr);
}

function fmtVal(v, d) { return isNum(v) ? Number(v).toFixed(d) : 'N/A'; }
function displayDigitsForValue(key, v, d) {
  if (key === 'optional_gas' && d === 2 && isNum(v) && v >= 1) return 1;
  return d;
}
function fmtValForKey(key, v, d) {
  return isNum(v) ? Number(v).toFixed(displayDigitsForValue(key, v, d)) : 'N/A';
}
function fmtSigned(v, d) {
  if (!isNum(v)) return 'N/A';
  return (v > 0 ? '+' : '') + Number(v).toFixed(d);
}
function fmtMM(v, unit) {
  if (!isNum(v)) return '-';
  const s = unit === 'inHg'
    ? v.toFixed(2)
    : (Math.abs(v) >= 100 ? v.toFixed(0) : v.toFixed(1));
  return unit ? s + ' ' + unit : s;
}
function formatBytes(bytes) {
  if (!isNum(bytes) || bytes < 0) return '--';
  const units = ['B', 'KB', 'MB', 'GB'];
  let value = Number(bytes);
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }
  const digits = unitIndex === 0 ? 0 : (value >= 100 ? 0 : 1);
  return value.toFixed(digits) + ' ' + units[unitIndex];
}
function formatDayKey(dayKey) {
  if (!isNum(dayKey) || dayKey <= 0) return '--';
  const text = String(Math.trunc(dayKey));
  if (text.length !== 8) return text;
  return text.slice(0, 4) + '-' + text.slice(4, 6) + '-' + text.slice(6, 8);
}
function formatFileStatus(fileInfo) {
  if (fileInfo && typeof fileInfo.error === 'string' && fileInfo.error) {
    return 'Check failed: ' + fileInfo.error;
  }
  if (!fileInfo || fileInfo.exists !== true) return 'Not available';
  return formatBytes(Number(fileInfo.size_bytes || 0));
}
function isUsUnitSystem(tempUnit) { return tempUnit === 'f'; }
function tempToDisplay(tempC, tempUnit) {
  return isUsUnitSystem(tempUnit) ? (tempC * 9 / 5) + 32 : tempC;
}
function pressureAltitudeConfigured() {
  return settings.pressureAltitudeSet === true && isNum(settings.pressureAltitudeM);
}
function pressureAbsoluteToMslHpa(pressureHpa, altitudeM) {
  if (!isNum(pressureHpa)) return pressureHpa;
  if (!isNum(altitudeM)) return pressureHpa;
  const base = 1 - (altitudeM / 44330.0);
  if (!Number.isFinite(base) || base <= 0) return pressureHpa;
  const corrected = pressureHpa / Math.pow(base, 5.255);
  return Number.isFinite(corrected) ? corrected : pressureHpa;
}
function pressureLabelText() {
  return pressureAltitudeConfigured() ? 'MSL Pressure' : 'Pressure';
}
function pressureToDisplay(pressureHpa, tempUnit) {
  const sourceHpa = pressureAltitudeConfigured()
    ? pressureAbsoluteToMslHpa(pressureHpa, settings.pressureAltitudeM)
    : pressureHpa;
  if (!isNum(sourceHpa)) return sourceHpa;
  return isUsUnitSystem(tempUnit) ? (sourceHpa * 0.0295299830714) : sourceHpa;
}
function pressureAbsoluteToDisplay(pressureHpa, tempUnit) {
  if (!isNum(pressureHpa)) return pressureHpa;
  return isUsUnitSystem(tempUnit) ? (pressureHpa * 0.0295299830714) : pressureHpa;
}
function pressureDeltaToMslHpa(deltaHpa) {
  return pressureAltitudeConfigured()
    ? pressureAbsoluteToMslHpa(deltaHpa, settings.pressureAltitudeM)
    : deltaHpa;
}
function pressureDeltaToDisplay(deltaHpa, tempUnit) {
  const sourceHpa = pressureDeltaToMslHpa(deltaHpa);
  return isUsUnitSystem(tempUnit) ? (sourceHpa * 0.0295299830714) : sourceHpa;
}
function temperatureUnitLabel(tempUnit) {
  return isUsUnitSystem(tempUnit) ? '\u00B0F' : '\u00B0C';
}
function pressureUnitLabel(tempUnit) {
  return isUsUnitSystem(tempUnit) ? 'inHg' : 'hPa';
}
function pressureDigits(tempUnit) {
  return isUsUnitSystem(tempUnit) ? 2 : 1;
}
function convertChartValueByKey(key, value, tempUnit) {
  if (!isNum(value)) return value;
  if (key === 'temp' || key === 'temperature') return tempToDisplay(value, tempUnit);
  if (key === 'pressure') return pressureToDisplay(value, tempUnit);
  return value;
}

function formatChartTime(ts) {
  if (!isNum(ts)) return '--:--';
  const ms = ts > 1000000000000 ? ts : ts * 1000;
  const d = new Date(ms);
  if (Number.isNaN(d.getTime())) return '--:--';
  return d.toLocaleTimeString([], { hour:'2-digit', minute:'2-digit', hour12:false });
}

function esc(v) {
  return String(v == null ? '' : v)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
    .replace(/"/g,'&quot;').replace(/'/g,'&#39;');
}

function pillHtml(status, compact) {
  const color = statusColorOf(status);
  const label = statusLabels[status] || 'N/A';
  const sizeClass = compact ? 'pill pill-sm' : 'pill';
  const style = `color:${color};border-color:${rgba(color,.38)};background:${rgba(color,.14)};`;
  return `<span class="${sizeClass}" style="${style}">${esc(label)}</span>`;
}

function trendToken(delta, is24h) {
  if (!isNum(delta)) {
    return { textStyle:'color:'+fallbackColor, surfaceStyle:'color:'+fallbackColor+';border-color:rgba(169,199,196,.18);background:rgba(28,40,45,.42)' };
  }
  const absDelta = Math.abs(pressureDeltaToMslHpa(delta));
  const status = getStatus(absDelta, is24h ? thresholds.pressureDelta24h : thresholds.pressureDelta3h);
  const color = statusColorOf(status);
  return {
    textStyle: 'color:' + color,
    surfaceStyle: 'color:' + color + ';border-color:' + rgba(color,.32) + ';background:' + rgba(color,.12),
  };
}

// ─────────────────────────────────────────────
// SVG bezier chart engine
// ─────────────────────────────────────────────
function splitSegments(points) {
  const segs = []; let cur = [];
  points.forEach(p => {
    if (!isNum(p.v)) { if (cur.length) segs.push(cur); cur = []; }
    else cur.push(p);
  });
  if (cur.length) segs.push(cur);
  return segs;
}

function buildSmoothPath(seg) {
  if (!seg || !seg.length) return '';
  if (seg.length === 1) return `M ${seg[0].x.toFixed(2)} ${seg[0].y.toFixed(2)}`;
  if (seg.length === 2) return `M ${seg[0].x.toFixed(2)} ${seg[0].y.toFixed(2)} L ${seg[1].x.toFixed(2)} ${seg[1].y.toFixed(2)}`;
  let d = `M ${seg[0].x.toFixed(2)} ${seg[0].y.toFixed(2)}`;
  for (let i = 0; i < seg.length - 1; i++) {
    const p0 = seg[i-1] || seg[i];
    const p1 = seg[i];
    const p2 = seg[i+1];
    const p3 = seg[i+2] || p2;
    const cp1x = p1.x + (p2.x - p0.x) / 6;
    const cp1y = clamp(p1.y + (p2.y - p0.y) / 6, 0, 100);
    const cp2x = p2.x - (p3.x - p1.x) / 6;
    const cp2y = clamp(p2.y - (p3.y - p1.y) / 6, 0, 100);
    d += ` C ${cp1x.toFixed(2)} ${cp1y.toFixed(2)}, ${cp2x.toFixed(2)} ${cp2y.toFixed(2)}, ${p2.x.toFixed(2)} ${p2.y.toFixed(2)}`;
  }
  return d;
}

function buildChartSvg(data, keys, colors, height, options) {
  const h = height || 140;
  const opts = options || {};
  const showPoints = opts.showPoints === true;
  const lineNames = Array.isArray(opts.lineNames) ? opts.lineNames : [];
  const lineUnits = Array.isArray(opts.lineUnits) ? opts.lineUnits : [];
  const lineDigits = Array.isArray(opts.lineDigits) ? opts.lineDigits : [];
  const normalRange = Array.isArray(opts.normalRange) && opts.normalRange.length === 2 &&
    isNum(opts.normalRange[0]) && isNum(opts.normalRange[1]) && opts.normalRange[0] < opts.normalRange[1]
      ? opts.normalRange
      : null;

  const allVals = [];
  keys.forEach(k => data.forEach(row => { if (isNum(row[k])) allVals.push(row[k]); }));
  if (!allVals.length) return '<div class="no-data">No data</div>';

  let yMin = Math.min(...allVals);
  let yMax = Math.max(...allVals);
  if (normalRange) {
    yMin = Math.min(yMin, normalRange[0] - 0.5);
    yMax = Math.max(yMax, normalRange[1] + 0.5);
  }
  const spread = yMax - yMin;
  const pad = spread > 1e-6 ? spread * 0.12 : Math.max(Math.abs(yMax) * 0.08, 1);
  yMin -= pad;
  yMax += pad;
  if (allVals.every(v => v >= 0) && yMin < 0) yMin = 0;
  if (Math.abs(yMax - yMin) < 1e-6) { yMin -= 1; yMax += 1; }

  const xFor = i => data.length <= 1 ? 0 : (i / (data.length - 1)) * 100;
  const yFor = v => ((yMax - v) / (yMax - yMin)) * 100;

  const uid = Math.random().toString(36).slice(2, 9);
  let defs = '<defs>';
  keys.forEach((k, ki) => {
    const c = colors[ki];
    defs += `<linearGradient id="g${uid}${ki}" x1="0" y1="0" x2="0" y2="1">
      <stop offset="5%" stop-color="${c}" stop-opacity="0.28"/>
      <stop offset="95%" stop-color="${c}" stop-opacity="0"/>
    </linearGradient>`;
  });
  defs += '</defs>';

  const grid = `<line x1="0" y1="25" x2="100" y2="25" stroke="#35464a" stroke-width="0.7" stroke-dasharray="2.5 2.5" vector-effect="non-scaling-stroke"/>
    <line x1="0" y1="50" x2="100" y2="50" stroke="#35464a" stroke-width="0.7" stroke-dasharray="2.5 2.5" vector-effect="non-scaling-stroke"/>
    <line x1="0" y1="75" x2="100" y2="75" stroke="#35464a" stroke-width="0.7" stroke-dasharray="2.5 2.5" vector-effect="non-scaling-stroke"/>`;

  let referenceBands = '';
  if (normalRange) {
    const upperY = clamp(yFor(normalRange[1]), 0, 100);
    const lowerY = clamp(yFor(normalRange[0]), 0, 100);
    referenceBands =
      `<rect x="0" y="0" width="100" height="${upperY.toFixed(2)}" fill="#ef4444" opacity="0.10"/>` +
      `<rect x="0" y="${upperY.toFixed(2)}" width="100" height="${Math.max(0, lowerY - upperY).toFixed(2)}" fill="#22c55e" opacity="0.12"/>` +
      `<rect x="0" y="${lowerY.toFixed(2)}" width="100" height="${Math.max(0, 100 - lowerY).toFixed(2)}" fill="#ef4444" opacity="0.10"/>`;
  }

  let areas = '';
  let lines2 = '';
  let pointsHtml = '';
  keys.forEach((k, ki) => {
    const c = colors[ki];
    const digits = Number.isInteger(lineDigits[ki]) ? lineDigits[ki] : 1;
    const name = lineNames[ki] || k.toUpperCase();
    const unit = lineUnits[ki] || '';
    const pts = data.map((row, i) => ({
      x: xFor(i),
      y: isNum(row[k]) ? yFor(row[k]) : null,
      v: row[k],
      t: row._time || '--:--',
    }));

    const segs = splitSegments(pts);
    segs.forEach(seg => {
      const linePath = buildSmoothPath(seg);
      if (!linePath) return;
      lines2 += `<path d="${linePath}" fill="none" stroke="${c}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" vector-effect="non-scaling-stroke"/>`;
      if (seg.length >= 4) {
        const first = seg[0];
        const last = seg[seg.length - 1];
        areas += `<path d="${linePath} L ${last.x.toFixed(2)} 100 L ${first.x.toFixed(2)} 100 Z" fill="url(#g${uid}${ki})"/>`;
      }
    });

    if (showPoints) {
      pts.forEach(p => {
        if (!isNum(p.v) || !isNum(p.y)) return;
        const valueText = fmtValForKey(k, p.v, digits);
        pointsHtml += `<circle class="chart-point" data-name="${esc(name)}" data-time="${esc(p.t)}" data-value="${esc(valueText)}" data-unit="${esc(unit)}" data-color="${esc(c)}" data-x="${p.x.toFixed(2)}" data-y="${p.y.toFixed(2)}" cx="${p.x.toFixed(2)}" cy="${p.y.toFixed(2)}" r="3.0" fill="transparent" stroke="transparent" stroke-width="0"></circle>`;
      });
    }
  });

  return `<svg viewBox="0 0 100 100" preserveAspectRatio="none" style="width:100%;height:${h}px;display:block;overflow:visible;">${defs}${referenceBands}${grid}${areas}${lines2}${pointsHtml}</svg>`;
}

function buildMiniChartSvg(data, key, color) {
  const meta = CHART_META[key] || {};
  return buildChartSvg(
    data,
    [key],
    [color],
    80,
    { lineNames:[meta.label || key.toUpperCase()], lineUnits:[meta.unit || ''], lineDigits:[Number.isInteger(meta.digits) ? meta.digits : 1], showPoints:false }
  );
}

// ─────────────────────────────────────────────
// Chart metadata
// ─────────────────────────────────────────────
const CHART_META = {
  co2:         { label:'CO2',         unit:'ppm',             digits:0, color:'#10b981' },
  temperature: { label:'Temperature', unit:'\u00B0C',        digits:1, color:'#f59e0b' },
  humidity:    { label:'Humidity',    unit:'%',               digits:0, color:'#3b82f6' },
  pressure:    { label:'Pressure',    unit:'hPa',             digits:0, color:'#0ea5e9' },
  co:          { label:'CO',          unit:'ppm',             digits:1, color:'#f97316' },
  voc:         { label:'VOC',         unit:'idx',             digits:0, color:'#ef4444' },
  nox:         { label:'NOx',         unit:'idx',             digits:0, color:'#f43f5e' },
  hcho:        { label:'HCHO',        unit:'ppb',             digits:0, color:'#d946ef' },
  pm05:        { label:'PM0.5',       unit:'#/cm\u00B3',     digits:0, color:'#14b8a6' },
  pm1:         { label:'PM1.0',       unit:'\u00B5g/m\u00B3', digits:1, color:'#a78bfa' },
  pm25:        { label:'PM2.5',       unit:'\u00B5g/m\u00B3', digits:1, color:'#8b5cf6' },
  pm4:         { label:'PM4.0',       unit:'\u00B5g/m\u00B3', digits:1, color:'#0ea5e9' },
  pm10:        { label:'PM10',        unit:'\u00B5g/m\u00B3', digits:1, color:'#6d28d9' },
};

// ─────────────────────────────────────────────
// Render: HeroMetric
// ─────────────────────────────────────────────
function renderHeroMetric(sensors, historyRows) {
  const v = sensors && isNum(sensors.co2) ? sensors.co2 : null;
  const status = v !== null ? getStatus(v, thresholds.co2) : null;
  const color = statusColorOf(status);
  const advice = {
    good: 'Air is stable. Ventilation is optional.',
    moderate: 'Ventilation recommended in the next hour.',
    bad: 'Open windows or increase airflow now.',
    critical: 'Poor air quality. Ventilate immediately.',
  }[status] || 'No live CO2 data.';

  const co2Data = (historyRows && historyRows.length) ? historyRows : (v !== null ? [{co2: v}] : []);
  const validPts = co2Data.filter(r => isNum(r.co2));
  const mmMin = validPts.length ? Math.min(...validPts.map(r => r.co2)) : null;
  const mmMax = validPts.length ? Math.max(...validPts.map(r => r.co2)) : null;
  const firstCo2 = validPts.length ? validPts[0].co2 : null;
  const delta3h = (isNum(v) && isNum(firstCo2)) ? v - firstCo2 : null;
  const deltaColor = !isNum(delta3h) ? '#9ca3af' : delta3h > 20 ? '#fdba74' : delta3h < -20 ? '#a5f3fc' : '#d1d5db';
  const progressW = v !== null
    ? Math.min(100, Math.max(0, ((v - 400) / (2000 - 400)) * 100))
    : 0;

  const miniSvg = co2Data.length >= 2 ? buildMiniChartSvg(co2Data, 'co2', color) : '';

  document.getElementById('heroMetric').innerHTML =
    `<div class="hero-wrap">
      <div class="hero-label">CO2 Level</div>
      <div style="display:flex;align-items:flex-start;justify-content:space-between;gap:12px">
        <div>
          <div class="hero-val-row">
            <span class="hero-val" style="color:${color}">${fmtVal(v, 0)}</span>
            <span class="hero-unit">ppm</span>
          </div>
        </div>
        ${pillHtml(status, false)}
      </div>
      <div class="hero-progress-track">
        <div class="hero-progress-fill" style="width:${progressW.toFixed(1)}%;background:${color};"></div>
      </div>
      <div class="hero-advice">${esc(advice)}</div>
      <div class="hero-trend-sep">
        <div class="hero-trend-hdr">
          <span class="hero-trend-label">3h Trend</span>
          <span class="hero-trend-delta" style="color:${deltaColor}">${isNum(delta3h) ? (delta3h > 0 ? '+' : '') + delta3h.toFixed(0) + ' ppm' : 'N/A'}</span>
        </div>
        <div style="margin-top:8px;height:80px;">${miniSvg || '<div class="no-data" style="height:100%;display:flex;align-items:center;justify-content:center;">Awaiting data</div>'}</div>
        <div class="hero-minmax">
          <span>min ${fmtMM(mmMin, 'ppm')}</span>
          <span>max ${fmtMM(mmMax, 'ppm')}</span>
        </div>
      </div>
    </div>`;
}

// ─────────────────────────────────────────────
// Render: ClimateOverview
// ─────────────────────────────────────────────
function renderClimateOverview(sensors, derived) {
  const temp     = sensors && isNum(sensors.temp)     ? sensors.temp     : null;
  const rh       = sensors && isNum(sensors.rh)       ? sensors.rh       : null;
  const pressure = sensors && isNum(sensors.pressure) ? sensors.pressure : null;
  const mold     = derived && isNum(derived.mold)     ? derived.mold     : null;
  const dewPoint = derived && isNum(derived.dew_point)? derived.dew_point: null;
  const ah       = derived && isNum(derived.ah)       ? derived.ah       : null;
  const delta3h  = derived && isNum(derived.pressure_delta_3h)  ? derived.pressure_delta_3h  : null;
  const delta24h = derived && isNum(derived.pressure_delta_24h) ? derived.pressure_delta_24h : null;

  const tempStatus  = metricStatus(temp, thresholds.temp);
  const rhStatus    = metricStatus(rh, thresholds.rh);
  const moldStatus  = metricStatus(mold, thresholds.mold);
  const dpStatus    = metricStatus(dewPoint, thresholds.dewPoint);
  const ahStatus    = metricStatus(ah, thresholds.ah);
  const statusRank  = { good:0, moderate:1, bad:2, critical:3 };
  const candidates  = [tempStatus, rhStatus, moldStatus].filter(s => s in statusRank);
  const climateStatus = candidates.length ? candidates.reduce((w, s) => statusRank[s] > statusRank[w] ? s : w, candidates[0]) : null;

  const t3  = trendToken(delta3h, false);
  const t24 = trendToken(delta24h, true);
  const tempDisplay = isNum(temp) ? tempToDisplay(temp, settings.tempUnit) : temp;
  const dewPointDisplay = isNum(dewPoint) ? tempToDisplay(dewPoint, settings.tempUnit) : dewPoint;
  const pressureDisplay = isNum(pressure) ? pressureToDisplay(pressure, settings.tempUnit) : pressure;
  const delta3hDisplay = isNum(delta3h) ? pressureDeltaToDisplay(delta3h, settings.tempUnit) : delta3h;
  const delta24hDisplay = isNum(delta24h) ? pressureDeltaToDisplay(delta24h, settings.tempUnit) : delta24h;
  const tempUnitText = temperatureUnitLabel(settings.tempUnit);
  const pressureUnitText = pressureUnitLabel(settings.tempUnit);
  const pressureValueDigits = pressureDigits(settings.tempUnit);
  const pressureLabel = pressureLabelText();

  document.getElementById('climateOverview').innerHTML =
    `<div class="climate-wrap">
      <div class="climate-head">
        <span class="climate-title">Climate</span>
        ${pillHtml(climateStatus, false)}
      </div>
      <div class="climate-grid2">
        <div class="mini-card">
          <div class="mini-label">Temperature</div>
          <div class="mini-val-row">
            <span class="mini-val" style="color:${statusColorOf(tempStatus)}">${fmtVal(tempDisplay,1)}</span>
            <span class="mini-unit">${tempUnitText}</span>
          </div>
        </div>
        <div class="mini-card">
          <div class="mini-label">Humidity</div>
          <div class="mini-val-row">
            <span class="mini-val" style="color:${statusColorOf(rhStatus)}">${fmtVal(rh,0)}</span>
            <span class="mini-unit">%</span>
          </div>
        </div>
      </div>
      <div class="climate-grid3">
        <div class="mini-card">
          <div class="mini-label">Mold Risk</div>
          <div class="mini-val-row">
            <span class="mini-val" style="color:${statusColorOf(moldStatus)}">${fmtVal(mold,1)}</span>
            <span class="mini-unit">/10</span>
          </div>
        </div>
        <div class="mini-card">
          <div class="mini-label">Dew Point</div>
          <div class="mini-val-row">
            <span class="mini-val" style="color:${statusColorOf(dpStatus)}">${fmtVal(dewPointDisplay,1)}</span>
            <span class="mini-unit">${tempUnitText}</span>
          </div>
        </div>
        <div class="mini-card">
          <div class="mini-label">Abs Humidity</div>
          <div class="mini-val-row">
            <span class="mini-val" style="color:${statusColorOf(ahStatus)}">${fmtVal(ah,1)}</span>
            <span class="mini-unit">g/m³</span>
          </div>
        </div>
      </div>
      <div class="pressure-mini mini-card" style="margin-top:10px;">
        <div class="pressure-row">
          <div>
            <div class="mini-label">${pressureLabel}</div>
            <div class="mini-val-row">
              <span class="mini-val" style="font-size:22px;color:#f9fafb">${fmtVal(pressureDisplay,pressureValueDigits)}</span>
              <span class="mini-unit">${pressureUnitText}</span>
            </div>
          </div>
          <div class="pressure-delta-grid">
            <div class="pdelta-box" style="${t3.surfaceStyle}">
              <div class="pdelta-lbl">3h</div>
              <div class="pdelta-val" style="${t3.textStyle}">${fmtSigned(delta3hDisplay,pressureValueDigits)}</div>
            </div>
            <div class="pdelta-box" style="${t24.surfaceStyle}">
              <div class="pdelta-lbl">24h</div>
              <div class="pdelta-val" style="${t24.textStyle}">${fmtSigned(delta24hDisplay,pressureValueDigits)}</div>
            </div>
          </div>
        </div>
      </div>
    </div>`;
}

// ─────────────────────────────────────────────
// Render: Gas grid
// ─────────────────────────────────────────────
const GAS_METRICS = [
  { key:'voc',  label:'VOC',  unit:'idx', max:thresholds.voc.bad,  thr:thresholds.voc,  digits:0 },
  { key:'nox',  label:'NOx',  unit:'idx', max:thresholds.nox.bad,  thr:thresholds.nox,  digits:0 },
  { key:'hcho', label:'HCHO', unit:'ppb', max:thresholds.hcho.bad, thr:thresholds.hcho, digits:0 },
  { key:'co',   label:'CO',   unit:'ppm', max:thresholds.co.bad,   thr:thresholds.co,   digits:1 },
];

function refreshGasMetricMaxes() {
  GAS_METRICS.forEach(m => {
    if (m && thresholds[m.key] && isNum(thresholds[m.key].bad)) {
      m.max = thresholds[m.key].bad;
    }
  });
}

// Keep in sync with UiOptionalGasProfile.cpp until these values are served by firmware state/API.
const OPTIONAL_GAS_PROFILES = {
  NH3: { label:'NH3', unit:'ppm', max:35.0, thr:{ good:5.0, moderate:25.0, bad:35.0 }, digits:0 },
  SO2: { label:'SO2', unit:'ppm', max:2.0,  thr:{ good:0.05, moderate:0.10, bad:2.0 }, digits:1 },
  NO2: { label:'NO2', unit:'ppm', max:1.0,  thr:{ good:0.05, moderate:0.10, bad:1.0 }, digits:1 },
  H2S: { label:'H2S', unit:'ppm', max:10.0, thr:{ good:0.5, moderate:1.0, bad:10.0 }, digits:0 },
  O3:  { label:'O3',  unit:'ppm', max:0.5,  thr:{ good:0.05, moderate:0.10, bad:0.5 }, digits:1 },
  O2:  { label:'O2',  unit:'%Vol', max:25.0, thr:{ good:[19.5,23.5], moderate:[19.5,23.5], bad:[19.5,23.5] }, normalRange:[19.5,23.5], digits:1 },
};

function optionalGasDigits(sensors, profile, fallback) {
  const raw = sensors
    ? (sensors.optional_gas_decimals ?? sensors.optional_gas_ppm_decimals)
    : undefined;
  if (raw !== null && raw !== undefined) {
    const d = Number(raw);
    if (Number.isInteger(d) && d >= 0 && d <= 2) return d;
  }
  if (profile && Number.isInteger(profile.digits)) return profile.digits;
  return Number.isInteger(fallback) ? fallback : 1;
}

function buildGasMetrics(sensors) {
  const s = sensors || {};
  const metrics = GAS_METRICS.filter(m => {
    if (m.key === 'co') return s.co_sensor_present === true;
    if (m.key === 'hcho') return s.hcho_sensor_present === true;
    return true;
  });

  if (s.optional_gas_sensor_present === true) {
    const type = typeof s.optional_gas_type === 'string' ? s.optional_gas_type : '';
    const profile = OPTIONAL_GAS_PROFILES[type] || {
      label: type || 'Gas',
      unit: 'ppm',
      max: 3.0,
      thr: { good: 1.0, moderate: 2.0, bad: 3.0 },
      digits: 1,
    };
    metrics.push({
      key: 'optional_gas',
      label: profile.label,
      unit: typeof s.optional_gas_unit === 'string' ? s.optional_gas_unit : profile.unit,
      max: profile.max,
      thr: profile.thr,
      digits: optionalGasDigits(s, profile, 1),
    });
  }

  return metrics;
}

function renderGasGrid(sensors) {
  const s = sensors || {};
  const metrics = buildGasMetrics(s);
  document.getElementById('gasGrid').innerHTML =
    `<div class="gas-grid">` +
    metrics.map(m => {
      const v = isNum(s[m.key]) ? s[m.key] : null;
      const status = v !== null ? getStatus(v, m.thr) : null;
      const color = statusColorOf(status);
      const bar = v !== null ? Math.min((v / m.max) * 100, 100).toFixed(1) : 0;
      return `<div class="card-g7 gas-card">
        <div class="gas-head">
          <span class="gas-name">${esc(m.label)}</span>
          <span class="gas-unit">${esc(m.unit)}</span>
        </div>
        <div class="gas-val-row">
          <span class="gas-val" style="color:${color}">${fmtValForKey(m.key, v, m.digits)}</span>
          ${pillHtml(status, true)}
        </div>
        <div class="gas-progress"><div class="gas-bar" style="width:${bar}%;background:${color};"></div></div>
      </div>`;
    }).join('') +
    `</div>`;
}

// ─────────────────────────────────────────────
// Render: PM grid
// ─────────────────────────────────────────────
const PM_METRICS = [
  { key:'pm05', label:'PM0.5', unit:'#/cm\u00B3', max:thresholds.pm05.bad, thr:thresholds.pm05, digits:0 },
  { key:'pm1',  label:'PM1.0', unit:'\u00B5g/m\u00B3', max:thresholds.pm1.bad,  thr:thresholds.pm1,  digits:1 },
  { key:'pm25', label:'PM2.5', unit:'\u00B5g/m\u00B3', max:thresholds.pm25.bad, thr:thresholds.pm25, digits:1 },
  { key:'pm4',  label:'PM4.0', unit:'\u00B5g/m\u00B3', max:thresholds.pm4.bad,  thr:thresholds.pm4,  digits:1 },
  { key:'pm10', label:'PM10',  unit:'\u00B5g/m\u00B3', max:thresholds.pm10.bad, thr:thresholds.pm10, digits:1 },
];

function renderPMGrid(sensors) {
  const s = sensors || {};
  document.getElementById('pmGrid').innerHTML =
    `<div class="pm-grid">` +
    PM_METRICS.map(m => {
      const v = isNum(s[m.key]) ? s[m.key] : null;
      const status = v !== null ? getStatus(v, m.thr) : null;
      const color = statusColorOf(status);
      const bar = v !== null ? Math.min((v / m.max) * 100, 100).toFixed(1) : 0;
      return `<div class="card-g7 pm-card">
        <div class="pm-head">
          <span class="pm-label">${esc(m.label)}</span>
          <span class="pm-unit">${esc(m.unit)}</span>
        </div>
        <div class="pm-val-row">
          <div class="pm-val-left">
            <span class="pm-val" style="color:${color}">${fmtVal(v, m.digits)}</span>
          </div>
          ${pillHtml(status, true)}
        </div>
        <div class="pm-progress"><div class="pm-bar" style="width:${bar}%;background:${color};"></div></div>
      </div>`;
    }).join('') +
    `</div>`;
}

// ─────────────────────────────────────────────
// Render: Charts
// ─────────────────────────────────────────────
const CHART_LAYOUTS = {
  core: [
    { title:'CO2 Concentration', unit:'ppm', lines:[{ key:'co2', name:'CO2', color:'#10b981', digits:0, unit:'ppm' }] },
    { title:'Temperature', unit:'\u00B0C', lines:[{ key:'temperature', name:'Temp', color:'#f59e0b', digits:1, unit:'\u00B0C' }] },
    { title:'Humidity', unit:'%', lines:[{ key:'humidity', name:'RH', color:'#3b82f6', digits:0, unit:'%' }] },
    { title:'Pressure', unit:'hPa', lines:[{ key:'pressure', name:'Pressure', color:'#0ea5e9', digits:1, unit:'hPa' }] },
  ],
  gases: [
    { title:'VOC Index', unit:'idx', lines:[{ key:'voc', name:'VOC', color:'#ef4444', digits:0, unit:'idx' }] },
    { title:'NOx Index', unit:'idx', lines:[{ key:'nox', name:'NOx', color:'#f43f5e', digits:0, unit:'idx' }] },
    { title:'Formaldehyde (HCHO)', unit:'ppb', lines:[{ key:'hcho', name:'HCHO', color:'#d946ef', digits:0, unit:'ppb' }] },
    { title:'Carbon Monoxide (CO)', unit:'ppm', lines:[{ key:'co', name:'CO', color:'#f97316', digits:1, unit:'ppm' }] },
    { title:'Optional DFRobot Gas', unit:'ppm', lines:[{ key:'optional_gas', name:'Optional Gas', color:'#22c55e', digits:1, unit:'ppm' }] },
  ],
  pm: [
    { title:'PM0.5', unit:'#/cm\u00B3', lines:[{ key:'pm05', name:'PM0.5', color:'#14b8a6', digits:0, unit:'#/cm\u00B3' }] },
    { title:'PM1.0', unit:'\u00B5g/m\u00B3', lines:[{ key:'pm1', name:'PM1.0', color:'#a78bfa', digits:1, unit:'\u00B5g/m\u00B3' }] },
    { title:'PM2.5', unit:'\u00B5g/m\u00B3', lines:[{ key:'pm25', name:'PM2.5', color:'#8b5cf6', digits:1, unit:'\u00B5g/m\u00B3' }] },
    {
      title:'PM10 + PM4.0',
      unit:'\u00B5g/m\u00B3',
      lines:[
        { key:'pm10', name:'PM10', color:'#6d28d9', digits:1, unit:'\u00B5g/m\u00B3' },
        { key:'pm4',  name:'PM4.0', color:'#0ea5e9', digits:1, unit:'\u00B5g/m\u00B3' },
      ],
    },
  ],
};

function mapSeriesKeyToSensorKey(key) {
  if (key === 'temperature') return 'temp';
  if (key === 'humidity') return 'rh';
  return key;
}

function shouldShowChartCard(card) {
  if (!card || !Array.isArray(card.lines)) return false;
  const sensors = stateCache && stateCache.sensors ? stateCache.sensors : {};
  const keys = card.lines.map(line => line && line.key).filter(Boolean);
  if (keys.includes('co')) return sensors.co_sensor_present === true;
  if (keys.includes('hcho')) return sensors.hcho_sensor_present === true;
  if (keys.includes('optional_gas')) return sensors.optional_gas_sensor_present === true;
  return true;
}

function chartCardForState(card) {
  if (!card || !Array.isArray(card.lines)) return card;
  const optionalGasCard = card.lines.some(line => line && line.key === 'optional_gas');
  if (!optionalGasCard) return card;

  const sensors = stateCache && stateCache.sensors ? stateCache.sensors : {};
  const type = typeof sensors.optional_gas_type === 'string' ? sensors.optional_gas_type : '';
  const profile = OPTIONAL_GAS_PROFILES[type] || {
    label: type || 'Optional Gas',
    unit: 'ppm',
    digits: 1,
  };
  const unit = typeof sensors.optional_gas_unit === 'string'
    ? sensors.optional_gas_unit
    : (profile.unit || card.unit || 'ppm');
  return {
    ...card,
    title: profile.label + ' Concentration',
    unit,
    normalRange: Array.isArray(profile.normalRange) ? profile.normalRange : null,
    lines: card.lines.map(line => {
      if (!line || line.key !== 'optional_gas') return line;
      return {
        ...line,
        name: profile.label,
        unit,
        digits: optionalGasDigits(sensors, profile, line.digits),
      };
    }),
  };
}

function bindChartPointTooltips(root) {
  if (!root) return;
  root.querySelectorAll('.chart-svg-wrap').forEach(wrap => {
    const tooltip = wrap.querySelector('.chart-tooltip');
    const vline = wrap.querySelector('.chart-vline');
    const focusLayer = wrap.querySelector('.chart-focus-layer');
    if (!tooltip || !vline || !focusLayer) return;
    const pointEls = Array.from(wrap.querySelectorAll('.chart-point'));
    if (!pointEls.length) return;

    const groups = new Map();
    pointEls.forEach(point => {
      const x = Number(point.getAttribute('data-x'));
      const y = Number(point.getAttribute('data-y'));
      const name = point.getAttribute('data-name') || '';
      const time = point.getAttribute('data-time') || '--:--';
      const value = point.getAttribute('data-value') || '';
      const unit = point.getAttribute('data-unit') || '';
      const color = point.getAttribute('data-color') || '#22c55e';
      if (!Number.isFinite(x) || !Number.isFinite(y) || !value) return;
      const key = x.toFixed(2);
      if (!groups.has(key)) groups.set(key, []);
      groups.get(key).push({ x, y, name, time, value, unit, color });
    });
    const xKeys = Array.from(groups.keys()).map(Number).filter(Number.isFinite);
    if (!xKeys.length) return;

    const hide = () => {
      tooltip.classList.remove('show');
      tooltip.innerHTML = '';
      vline.classList.remove('show');
      focusLayer.innerHTML = '';
    };

    const showAtClientX = clientX => {
      const rect = wrap.getBoundingClientRect();
      if (!rect.width) { hide(); return; }
      const xPct = clamp(((clientX - rect.left) / rect.width) * 100, 0, 100);

      let nearest = xKeys[0];
      let bestDist = Math.abs(xKeys[0] - xPct);
      for (let i = 1; i < xKeys.length; ++i) {
        const d = Math.abs(xKeys[i] - xPct);
        if (d < bestDist) { bestDist = d; nearest = xKeys[i]; }
      }

      const entries = groups.get(nearest.toFixed(2)) || [];
      if (!entries.length) { hide(); return; }

      const focusEntry = entries[0];
      const timeText = focusEntry.time || '--:--';
      const rowsHtml = entries.map(entry =>
        `<div class="chart-tooltip-row"><span class="chart-tooltip-name" style="color:${esc(entry.color || '#22c55e')}">${esc(entry.name || '--')}</span><span class="chart-tooltip-value">${esc(entry.value)}${entry.unit ? ' ' + esc(entry.unit) : ''}</span></div>`
      ).join('');
      tooltip.innerHTML = `<div class="chart-tooltip-time">${esc(timeText)}</div><div class="chart-tooltip-rows">${rowsHtml}</div>`;

      const xPxRaw = (nearest / 100) * rect.width;
      const xPx = clamp(xPxRaw, 12, Math.max(12, rect.width - 12));
      const tooltipLeftPct = clamp(nearest, 12, 88);
      tooltip.style.left = `${tooltipLeftPct}%`;
      tooltip.style.top = '6px';
      tooltip.classList.add('show');

      vline.style.left = `${xPx}px`;
      vline.classList.add('show');

      focusLayer.innerHTML = entries.map(entry => {
        const yPx = clamp((entry.y / 100) * rect.height, 8, Math.max(8, rect.height - 8));
        const color = entry.color || '#22c55e';
        return `<div class="chart-focus show" style="left:${xPx}px;top:${yPx}px;background:${esc(color)};box-shadow:0 0 0 4px ${rgba(color,0.28)}"></div>`;
      }).join('');
    };

    wrap.addEventListener('mousemove', e => showAtClientX(e.clientX));
    wrap.addEventListener('touchstart', e => {
      if (!e.touches || !e.touches[0]) return;
      showAtClientX(e.touches[0].clientX);
    }, { passive: true });
    wrap.addEventListener('touchmove', e => {
      if (!e.touches || !e.touches[0]) return;
      showAtClientX(e.touches[0].clientX);
    }, { passive: true });
    wrap.addEventListener('mouseleave', hide);
    wrap.addEventListener('touchend', hide);
    wrap.addEventListener('touchcancel', hide);
  });
}

function renderCharts(payload) {
  const timestamps = Array.isArray(payload && payload.timestamps) ? payload.timestamps : [];
  const series = Array.isArray(payload && payload.series) ? payload.series : [];
  const el = document.getElementById('chartGrid');
  if (!el) return;

  if (!series.length) { el.innerHTML = '<div class="no-data">No chart data</div>'; return; }

  const seriesByKey = {};
  series.forEach(s => { if (s && typeof s.key === 'string') seriesByKey[s.key] = s; });

  const rows = timestamps.map((ts, i) => {
    const row = { _ts: ts, _time: formatChartTime(ts) };
    series.forEach(s => {
      if (s && typeof s.key === 'string') {
        const rawValue = (Array.isArray(s.values) && isNum(s.values[i])) ? s.values[i] : null;
        row[s.key] = convertChartValueByKey(s.key, rawValue, settings.tempUnit);
      }
    });
    return row;
  });

  const layout = (CHART_LAYOUTS[chartGroup] || CHART_LAYOUTS.core)
    .filter(shouldShowChartCard)
    .map(chartCardForState);
  el.innerHTML = layout.map(card => {
    const pressureCard = card.lines.some(line => line.key === 'pressure');
    const cardTitle = pressureCard ? pressureLabelText() : card.title;
    const lineKeys = card.lines.map(line => line.key);
    const lineColors = card.lines.map(line => line.color || '#3dd6c6');
    const lineNames = card.lines.map(line => {
      if (line.key === 'pressure') return pressureLabelText();
      return line.name || line.key.toUpperCase();
    });
    const lineUnits = card.lines.map(line => {
      if (line.key === 'temperature') return temperatureUnitLabel(settings.tempUnit);
      if (line.key === 'pressure') return pressureUnitLabel(settings.tempUnit);
      return line.unit || card.unit || '';
    });
    const lineDigits = card.lines.map(line => {
      if (line.key === 'pressure') return pressureDigits(settings.tempUnit);
      return Number.isInteger(line.digits) ? line.digits : 1;
    });
    const cardUnit = card.lines.length === 1
      ? lineUnits[0]
      : (card.unit || '');

    const latestItems = card.lines.map((line, idx) => {
      const liveKey = mapSeriesKeyToSensorKey(line.key);
      const liveVal = stateCache && stateCache.sensors ? stateCache.sensors[liveKey] : null;
      const apiLatest = seriesByKey[line.key] && isNum(seriesByKey[line.key].latest) ? seriesByKey[line.key].latest : null;
      const rawLatest = isNum(liveVal) ? liveVal : apiLatest;
      const v = convertChartValueByKey(line.key, rawLatest, settings.tempUnit);
      return {
        key: line.key,
        value: v,
        digits: lineDigits[idx],
        color: lineColors[idx],
      };
    });

    const vals = [];
    rows.forEach(row => {
      lineKeys.forEach(k => { if (isNum(row[k])) vals.push(row[k]); });
    });
    const mmMin = vals.length ? Math.min(...vals) : null;
    const mmMax = vals.length ? Math.max(...vals) : null;

    const svgHtml = rows.length >= 2
      ? buildChartSvg(rows, lineKeys, lineColors, 140, {
          lineNames,
          lineUnits,
          lineDigits,
          normalRange: card.normalRange,
          showPoints: true,
        })
      : '<div class="no-data">Awaiting data</div>';

    return `<div class="card-g7 chart-box">
      <div class="chart-head">
        <span class="chart-name">${esc(cardTitle)}</span>
        <div class="chart-latest-row">
          ${latestItems.map(item =>
            `<span class="chart-latest-item" style="color:${item.color}">${isNum(item.value) ? fmtValForKey(item.key, item.value, item.digits) : '-'}</span>`
          ).join('')}
        </div>
      </div>
      <div class="chart-svg-wrap">${svgHtml}<div class="chart-vline"></div><div class="chart-focus-layer"></div><div class="chart-tooltip"></div></div>
      <div class="chart-minmax">
        <span>min ${fmtMM(mmMin, cardUnit)}</span>
        <span>max ${fmtMM(mmMax, cardUnit)}</span>
      </div>
    </div>`;
  }).join('');
  bindChartPointTooltips(el);
}

// ─────────────────────────────────────────────
// Render: Events
// ─────────────────────────────────────────────
function renderEvents(payload) {
  const events = Array.isArray(payload && payload.events) ? payload.events : [];
  const el = document.getElementById('eventsList');
  if (!events.length) { el.innerHTML = '<div class="no-data">No events</div>'; return; }

  const uptimeS = isNum(payload && payload.uptime_s) ? payload.uptime_s : null;
  const severityClass = { warning:'sev-warning', danger:'sev-danger', critical:'sev-critical', info:'sev-info' };

  const items = events.slice(0, 40).reverse().map(e => {
    const sev = (e && typeof e.severity === 'string') ? e.severity : 'info';
    const sevClass = severityClass[sev] || 'sev-info';
    const tsMs = isNum(e && e.ts_ms) ? e.ts_ms : null;
    let timeText = '--';
    if (uptimeS !== null && tsMs !== null) {
      const ageS = Math.max(0, uptimeS - Math.floor(tsMs / 1000));
      if (ageS < 60)    timeText = ageS + 's ago';
      else if (ageS < 3600)  timeText = Math.floor(ageS/60) + 'm ago';
      else if (ageS < 86400) timeText = Math.floor(ageS/3600) + 'h ago';
      else timeText = Math.floor(ageS/86400) + 'd ago';
    }
    const type = (e && e.type) || 'SYSTEM';
    const msg  = (e && e.message) || 'Event';
    return `<div class="alert-item ${sevClass}">
      <span class="alert-msg">${esc(msg)}</span>
      <div class="alert-meta">
        <span class="alert-type">${esc(type)}</span>
        <span class="alert-time">${esc(timeText)}</span>
      </div>
    </div>`;
  }).join('');

  el.innerHTML = items || '<div class="no-data">No events</div>';
}

// ─────────────────────────────────────────────
// Render: System info
// ─────────────────────────────────────────────
function renderSystemMeta(payload) {
  const network = (payload && payload.network) || {};
  const system  = (payload && payload.system)  || {};
  const derived = (payload && payload.derived) || {};

  setInfoValue('si-mode', String(network.mode || '--').toUpperCase());
  setInfoValue('si-ip',   network.ip || '--');
  setInfoValue('si-hostname', network.hostname || '--');
  setInfoValue('si-firmware', system.firmware || '--');
  setInfoValue('si-build', [system.build_date, system.build_time].filter(Boolean).join(' ') || '--');
  setInfoValue('si-uptime', system.uptime || derived.uptime || '--');
  setInfoValue('si-dac', system.dac_available ? 'Yes' : 'No', system.dac_available ? 'ok' : '');

  const rssi = isNum(network.rssi) ? network.rssi : null;
  const mqttConnected = network.mqtt_connected === true;
  const mqttEnabled = network.mqtt_enabled === true;
  setInfoValue('cfg-mode', formatMode(network.mode), network.mode === 'off' ? 'err' : '');
  setInfoValue('cfg-ssid', (typeof network.wifi_ssid === 'string' && network.wifi_ssid) ? network.wifi_ssid : '--');
  setInfoValue('cfg-hostname', (typeof network.hostname === 'string' && network.hostname) ? network.hostname : '--');
  setInfoValue('cfg-ip', (typeof network.ip === 'string' && network.ip) ? network.ip : '--');
  if (rssi !== null) {
    setInfoValue('cfg-rssi', rssi + ' dBm (' + rssiQuality(rssi) + ')', rssi <= -78 ? 'err' : (rssi <= -70 ? '' : 'ok'));
  } else {
    setInfoValue('cfg-rssi', '--');
  }
  setInfoValue('cfg-mqtt-broker', (typeof network.mqtt_broker === 'string' && network.mqtt_broker) ? network.mqtt_broker : '--');
  if (mqttEnabled) {
    setInfoValue('cfg-mqtt-status', mqttConnected ? 'Connected' : 'Disconnected', mqttConnected ? 'ok' : 'err');
  } else {
    setInfoValue('cfg-mqtt-status', 'Disabled');
  }
  updateNetworkActionButton(network);
  renderTimeSyncMeta(payload);
  updateLastSyncUi();
  updateOtaPrecheck(network);

  const themeBtn = document.getElementById('openThemeBtn');
  if (themeBtn) themeBtn.disabled = network.mode !== 'sta';
  const dacBtn = document.getElementById('openDacBtn');
  if (dacBtn) dacBtn.disabled = system.dac_available !== true;
}

function setSdNote(text, cls) {
  const el = document.getElementById('sd-note');
  if (!el) return;
  el.textContent = text || '';
  el.className = 'sd-note' + (cls ? (' ' + cls) : '');
}

function setLinkButtonEnabled(id, enabled) {
  const btn = document.getElementById(id);
  if (!btn) return;
  btn.classList.toggle('disabled', !enabled);
  btn.setAttribute('aria-disabled', enabled ? 'false' : 'true');
  if (enabled) {
    btn.removeAttribute('tabindex');
  } else {
    btn.setAttribute('tabindex', '-1');
  }
}

function setSdExportEnabled(enabled) {
  setLinkButtonEnabled('sdExportCsvBtn', enabled);
}

function setSdCurrentExportEnabled(enabled) {
  setLinkButtonEnabled('sdExportCurrentBtn', enabled);
}

function sdCsvUnitLabel(unitsC) {
  return unitsC === false ? 'Imperial (\u00B0F, inHg)' : 'Metric (\u00B0C, hPa)';
}

function sdCsvUnitShortLabel(unitsC) {
  return unitsC === false ? 'Imperial' : 'Metric';
}

function formatSdCsvUnits(daily) {
  const hasCurrentDay = Number(daily && daily.current_day || 0) > 0;
  const currentUnitsC = !(daily && daily.current_day_units_c === false);
  const preferredUnitsC = !(daily && daily.preferred_units_c === false);
  if (!hasCurrentDay) return 'Next: ' + sdCsvUnitLabel(preferredUnitsC);
  if (currentUnitsC === preferredUnitsC) return sdCsvUnitLabel(currentUnitsC);
  return 'Current: ' + sdCsvUnitShortLabel(currentUnitsC) +
         '; next: ' + sdCsvUnitShortLabel(preferredUnitsC);
}

function formatSdRuntimeError(sd) {
  if (!sd || (sd.write_fault !== true && sd.runtime_healthy !== false)) return 'None';
  const operation = typeof sd.last_operation === 'string' ? sd.last_operation : '';
  const stage = typeof sd.last_stage === 'string' ? sd.last_stage : '';
  const kind = typeof sd.runtime_error_kind === 'string' ? sd.runtime_error_kind : '';
  const code = Number(sd.runtime_error_code || 0);
  const detail = typeof sd.runtime_error === 'string' ? sd.runtime_error : '';
  const where = [operation, stage].filter(Boolean).join(' / ');
  const cause = [kind && kind !== 'none' ? kind : '', code ? ('code ' + code) : '', detail]
    .filter(Boolean)
    .join(': ');
  return [where, cause].filter(Boolean).join(' - ') || 'Unknown SD error';
}

function renderSdHistoryStatus(payload) {
  const sd = (payload && payload.sd) || {};
  const daily = (payload && payload.daily) || {};
  const files = (payload && payload.files) || {};
  const mounted = sd.mounted === true;
  const attempted = sd.attempted === true;
  const sdState = typeof sd.state === 'string'
    ? sd.state
    : (mounted ? 'mounted' : (attempted ? 'fault' : 'not_attempted'));
  const sdFault = sdState === 'fault' || sdState === 'manager_unavailable';
  const dailyCsv = files.daily_csv || null;
  const stateFile = files.current_day_state || null;
  const lastError = typeof sd.last_error === 'string' ? sd.last_error : '';
  const runtimeFault = sd.write_fault === true || sd.runtime_healthy === false;
  const pendingDays = Number(daily.pending_day_count || 0);
  const droppedDays = Number(daily.dropped_pending_days || 0);
  const retryMs = Number(daily.pending_retry_ms || 0);

  let statusText = 'Not attempted';
  let statusClass = '';
  if (mounted) {
    statusText = runtimeFault ? 'Mounted - degraded' : 'Mounted';
    statusClass = runtimeFault ? 'err' : 'ok';
  } else if (sdState === 'not_detected') {
    statusText = 'Not detected';
  } else if (sdState === 'board_unavailable') {
    statusText = 'Unavailable';
    statusClass = 'warn';
  } else if (sdFault) {
    statusText = 'Fault';
    statusClass = 'err';
  }

  setInfoValue('sd-status', statusText, statusClass);
  setInfoValue('sd-capacity', mounted ? formatBytes(Number(sd.card_size_bytes || 0)) : '--');
  setInfoValue('sd-free-space', mounted ? formatBytes(Number(sd.filesystem_free_bytes || 0)) : '--');
  setInfoValue('sd-day', formatDayKey(Number(daily.current_day || 0)));
  setInfoValue('sd-csv-units', formatSdCsvUnits(daily));
  setInfoValue('sd-samples', isNum(daily.current_sample_count) ? String(daily.current_sample_count) : '--');
  const pendingText = pendingDays > 0
    ? String(pendingDays) + ' (oldest ' + formatDayKey(Number(daily.oldest_pending_day || 0)) + ')'
    : '0';
  setInfoValue('sd-pending-days', pendingText, pendingDays > 0 ? 'warn' : 'ok');
  setInfoValue('sd-state-file', formatFileStatus(stateFile), stateFile && stateFile.error ? 'err' : (stateFile && stateFile.exists === true ? 'ok' : ''));
  setInfoValue('sd-daily-csv', formatFileStatus(dailyCsv), dailyCsv && dailyCsv.error ? 'err' : (dailyCsv && dailyCsv.exists === true ? 'ok' : ''));
  setInfoValue('sd-last-write',
               mounted ? (daily.last_write_ok === true ? 'OK' : 'Failed') : 'Unavailable',
               mounted ? (daily.last_write_ok === true ? 'ok' : 'err') : '');
  setInfoValue('sd-runtime-error', mounted ? formatSdRuntimeError(sd) : '--', runtimeFault ? 'err' : (mounted ? 'ok' : ''));

  const currentReady = isNum(daily.current_sample_count) && daily.current_sample_count > 0;
  const exportReady = mounted && dailyCsv && dailyCsv.exists === true && Number(dailyCsv.size_bytes || 0) > 0;
  setSdCurrentExportEnabled(currentReady);
  setSdExportEnabled(exportReady);
  ['sdClearCurrentBtn','sdClearHistoryBtn'].forEach(id => {
    const btn = document.getElementById(id);
    if (btn) btn.disabled = !mounted;
  });

  if (sdState === 'not_detected') {
    setSdNote('Optional SD card not detected. Device continues without SD history.', '');
  } else if (sdState === 'board_unavailable') {
    setSdNote('SD storage unavailable because the board did not initialize.', 'warn');
  } else if (!mounted) {
    setSdNote(lastError || (sdFault ? 'SD storage fault.' : 'SD card was not checked.'),
              sdFault ? 'err' : '');
  } else if (droppedDays > 0) {
    setSdNote(String(droppedDays) + ' daily record(s) were dropped after the pending queue filled. Check or replace the SD card.', 'err');
  } else if (runtimeFault || daily.last_write_ok === false) {
    setSdNote('SD operation failed: ' + formatSdRuntimeError(sd) + '. Today\'s measurements continue in memory and Aura will retry automatically.', 'err');
  } else if (pendingDays > 0) {
    const retryText = retryMs > 0 ? (' Next retry in ' + Math.ceil(retryMs / 1000) + ' s.') : ' Retry is due now.';
    setSdNote(String(pendingDays) + ' completed day(s) are waiting for SD. Current-day collection continues.' + retryText, 'warn');
  } else if (!exportReady) {
    setSdNote('Daily CSV will appear after the first day rollover. Use Export current day to preview today.', 'warn');
  } else {
    setSdNote('Daily CSV is ready for export.', 'ok');
  }
}

async function refreshSdHistory(options) {
  if (otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending || sdHistoryRefreshBusy) return;
  sdHistoryRefreshBusy = true;
  const silent = options && options.silent === true;
  const btn = document.getElementById('sdRefreshBtn');
  if (btn && !silent) {
    btn.disabled = true;
    btn.textContent = 'Refreshing...';
  }
  try {
    const payload = await getJson('/api/history/daily/status');
    renderSdHistoryStatus(payload);
  } catch (error) {
    setSdCurrentExportEnabled(false);
    setSdExportEnabled(false);
    setSdNote((error && error.message) ? error.message : 'Failed to load SD history status.', 'err');
  } finally {
    if (btn) {
      btn.disabled = false;
      btn.textContent = 'Refresh SD';
    }
    sdHistoryRefreshBusy = false;
  }
}

// ─────────────────────────────────────────────
// State
// ─────────────────────────────────────────────
function setSdServiceButtonsDisabled(disabled) {
  ['sdClearCurrentBtn','sdClearHistoryBtn','sdRefreshBtn'].forEach(id => {
    const btn = document.getElementById(id);
    if (btn) btn.disabled = !!disabled;
  });
}

async function runSdServiceAction(url, confirmText, progressText, successText) {
  if (confirmText && !window.confirm(confirmText)) return;
  setSdServiceButtonsDisabled(true);
  setSdNote(progressText || 'Working...', 'warn');
  try {
    const result = await postJson(url, {});
    if (result && result.success === false) {
      throw new Error(result.error || 'SD action failed.');
    }
    setSdNote(successText || 'Done.', 'ok');
    await refreshSdHistory({ silent: true });
  } catch (error) {
    setSdNote((error && error.message) ? error.message : 'SD action failed.', 'err');
  } finally {
    setSdServiceButtonsDisabled(false);
  }
}

let activeTab = 'sensors';
let chartGroup = 'core';
let chartRange = '24h';
let stateCache = null;
let historyCache = null;
let refreshBusy = false;
let refreshTimer = null;
let sdHistoryRefreshBusy = false;
let otaUploadInFlight = false;
let otaAwaitingPhysicalConfirm = false;
let otaAwaitingDeviceOutcome = false;
let otaRestartPending = false;
let otaRecoveryTimer = null;
let otaRecoveryActive = false;
let otaRecoveryProbeController = null;
const STATE_REFRESH_VISIBLE_MS = 10000;
const STATE_REFRESH_HIDDEN_MS = 30000;
const OTA_PREPARE_TIMEOUT_MS = 35000;
const OTA_RECOVERY_PROBE_TIMEOUT_MS = 1500;
const OTA_STALE_STATE_THRESHOLD_S = 45;
const OTA_RECONNECT_GRACE_MS = 120000;
const OTA_UPLOAD_FIRST_PROGRESS_TIMEOUT_MS = 30000;
const OTA_UPLOAD_NO_PROGRESS_TIMEOUT_MS = 90000;
const OTA_UPLOAD_RESPONSE_TIMEOUT_FALLBACK_MS = 930000;
let deviceClockRef = null;
let lastStateOkAtMs = 0;
let lastStateError = '';
let lastStateOtaBusy = false;
let otaReconnectGraceUntilMs = 0;

// Settings state
const settings = {
  nightMode: false, nightModeLocked: false,
  backlight: true,
  ntpEnabled: true,
  ntpServer: '',
  tempUnit: 'c',
  timeFormat24h: true,
  tempOffset: 0, humOffset: 0,
  pressureAltitudeSet: false,
  pressureAltitudeM: 0,
  displayName: '',
};
const savedSettings = Object.assign({}, settings);
let settingsSaving = false;
let settingsSaveStatus = 'idle'; // idle|dirty|saving|saved|error
let nameDirty = false;
let timeSyncDirty = false;
let timeSyncSaving = false;
let timeSyncSaveStatus = 'idle'; // idle|dirty|saving|saved|error
const toggleRequestState = {
  night_mode: { inFlight: false, queued: null },
  backlight_on: { inFlight: false, queued: null },
};
let toggleMsgTimer = null;
let chartsRefreshToken = 0;
let chartsRefreshController = null;

function resolveHeaderDeviceName() {
  const displayName =
    (typeof settings.displayName === 'string' && settings.displayName.trim())
      ? settings.displayName.trim()
      : '';
  const net = (stateCache && stateCache.network) || {};
  return displayName || net.hostname || 'aura';
}

function renderHeaderDeviceName() {
  const el = document.getElementById('deviceNameLabel');
  if (!el) return;
  el.textContent = resolveHeaderDeviceName();
}

function safeStateNetwork() {
  return (stateCache && stateCache.network) || {};
}

function formatMode(mode) {
  if (mode === 'ap') return 'AP';
  if (mode === 'sta') return 'STA';
  if (mode === 'off') return 'OFF';
  return '--';
}

function rssiQuality(rssi) {
  if (!isNum(rssi)) return '';
  if (rssi >= -60) return 'Excellent';
  if (rssi >= -67) return 'Good';
  if (rssi >= -75) return 'Fair';
  return 'Weak';
}

function setInfoValue(id, value, cls) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = value;
  el.className = 'info-val' + (cls ? (' ' + cls) : '');
}

function secondsSince(tsMs) {
  if (!tsMs) return null;
  return Math.max(0, Math.floor((Date.now() - tsMs) / 1000));
}

function formatSyncAge(ageS) {
  if (!isNum(ageS)) return '--';
  if (ageS < 60) return ageS + 's ago';
  if (ageS < 3600) return Math.floor(ageS / 60) + 'm ago';
  return Math.floor(ageS / 3600) + 'h ago';
}

function updateLastSyncUi() {
  const ageS = secondsSince(lastStateOkAtMs);
  setInfoValue('cfg-last-sync',
               formatSyncAge(ageS),
               ageS === null ? '' : (ageS <= 20 ? 'ok' : (ageS <= OTA_STALE_STATE_THRESHOLD_S ? '' : 'err')));
}

function renderTimeSyncMeta(payload) {
  const system = (payload && payload.system) || {};
  const uptimeS = payload && isNum(payload.uptime_s) ? Number(payload.uptime_s) : null;
  const status = typeof system.ntp_status === 'string' ? system.ntp_status : 'off';
  const lastSyncMs = isNum(system.ntp_last_sync_ms) ? Number(system.ntp_last_sync_ms) : null;

  let statusText = 'Off';
  let statusClass = '';
  if (status === 'syncing') {
    statusText = 'Syncing';
  } else if (status === 'ok') {
    statusText = 'OK';
    statusClass = 'ok';
  } else if (status === 'error') {
    statusText = 'Error';
    statusClass = 'err';
  }
  setInfoValue('ntp-status', statusText, statusClass);

  let lastSyncText = 'Never';
  let lastSyncClass = '';
  if (status === 'syncing') {
    lastSyncText = 'In progress';
  } else if (lastSyncMs !== null && uptimeS !== null) {
    const ageS = Math.max(0, Math.floor(uptimeS - (lastSyncMs / 1000)));
    lastSyncText = formatSyncAge(ageS);
    lastSyncClass = ageS <= 3600 ? 'ok' : '';
  } else if (status === 'error') {
    lastSyncClass = 'err';
  }
  setInfoValue('ntp-last-sync', lastSyncText, lastSyncClass);
}

function updateNetworkActionButton(network) {
  const btn = document.getElementById('networkActionBtn');
  if (!btn) return;
  const mode = network && typeof network.mode === 'string' ? network.mode : '';
  if (mode === 'ap') {
    btn.style.display = 'inline-flex';
    btn.removeAttribute('aria-hidden');
    btn.removeAttribute('tabindex');
    btn.href = '/';
    btn.textContent = 'WiFi Setup';
    return;
  }
  if (mode === 'sta') {
    btn.style.display = 'none';
    btn.setAttribute('aria-hidden', 'true');
    btn.setAttribute('tabindex', '-1');
    return;
  }
  btn.style.display = 'inline-flex';
  btn.removeAttribute('aria-hidden');
  btn.removeAttribute('tabindex');
  btn.href = '/dashboard';
  btn.textContent = 'Open Dashboard';
}

function updateOtaPrecheck(network) {
  const el = document.getElementById('otaPrecheck');
  if (!el) return;

  const mode = network && typeof network.mode === 'string' ? network.mode : '';
  const rssi = network && isNum(network.rssi) ? network.rssi : null;
  const ageS = secondsSince(lastStateOkAtMs);
  const remoteOtaBusy = !!lastStateOtaBusy;
  const reconnectGraceS = Math.max(0, Math.ceil((otaReconnectGraceUntilMs - Date.now()) / 1000));
  const reconnectGraceActive = reconnectGraceS > 0;

  let cls = 'warn';
  let text = 'Waiting for live device state before OTA.';
  if (otaAwaitingPhysicalConfirm) {
    cls = 'warn';
    text = 'Waiting for firmware update confirmation on the device.';
  } else if (otaUploadInFlight) {
    cls = 'warn';
    text = 'OTA upload in progress. Live state checks are paused.';
  } else if (otaAwaitingDeviceOutcome) {
    cls = 'warn';
    text = 'Waiting for device OTA status confirmation.';
  } else if (otaRestartPending || reconnectGraceActive) {
    cls = 'warn';
    text = reconnectGraceActive
      ? 'Waiting for device reboot/reconnect (' + reconnectGraceS + 's grace window).'
      : 'Waiting for device reboot/reconnect.';
  } else if (remoteOtaBusy) {
    cls = 'warn';
    text = 'OTA in progress from another client. Live state checks are paused.';
  } else if (ageS !== null && ageS > OTA_STALE_STATE_THRESHOLD_S) {
    cls = 'err';
    text = 'State data is stale (' + ageS + 's). Wait for reconnect before OTA.';
  } else if (mode === 'off') {
    cls = 'err';
    text = 'WiFi is offline. OTA requires AP or STA connection.';
  } else if (mode === 'ap') {
    cls = 'warn';
    text = 'AP mode active. Keep this client close to Aura and stay on the same AP during OTA.';
  } else if (mode === 'sta') {
    if (rssi !== null && rssi <= -78) {
      cls = 'warn';
      text = 'STA signal is weak (' + rssi + ' dBm). OTA may fail; move closer to router/device.';
    } else {
      cls = 'ok';
      text = 'Ready for OTA over STA' + (rssi !== null ? (' (' + rssi + ' dBm).') : '.');
    }
  }

  el.className = 'ota-precheck ' + cls;
  el.textContent = text;
}

function setOtaGlobalOverlay(visible, message) {
  const overlay = document.getElementById('otaGlobalOverlay');
  if (!overlay) return;
  const textEl = document.getElementById('otaGlobalOverlayText');
  if (textEl && typeof message === 'string' && message) {
    textEl.textContent = message;
  }
  overlay.classList.toggle('show', !!visible);
}

function updateNetStatusBanner() {
  const bar = document.getElementById('netStatusBar');
  const textEl = document.getElementById('netStatusText');
  const metaEl = document.getElementById('netStatusMeta');
  if (!bar || !textEl || !metaEl) return;

  const network = safeStateNetwork();
  const mode = typeof network.mode === 'string' ? network.mode : '';
  const modeText = formatMode(mode);
  const ip = typeof network.ip === 'string' && network.ip ? network.ip : '--';
  const ageS = secondsSince(lastStateOkAtMs);
  const remoteOtaBusy = !!lastStateOtaBusy;

  let cls = 'warn';
  let text = 'Connecting to device...';
  let meta = 'No live state yet.';

  if (otaUploadInFlight) {
    cls = 'warn';
    text = 'OTA upload in progress';
    meta = 'Keep this tab open until upload completes.';
  } else if (otaAwaitingDeviceOutcome) {
    cls = 'warn';
    text = 'Waiting for device OTA status';
    meta = 'Browser lost the direct OTA response. Auto-recovery is running.';
  } else if (otaRestartPending) {
    cls = 'warn';
    text = 'Waiting for device reboot';
    meta = 'Auto-reconnect is running.';
  } else if (remoteOtaBusy) {
    cls = 'warn';
    text = 'OTA in progress';
    meta = 'Another client started OTA. Live updates are paused.';
  } else if (ageS === null) {
    cls = 'warn';
    text = 'Waiting for first state packet';
    meta = lastStateError || 'No live data yet.';
  } else if (ageS <= 20) {
    cls = 'ok';
    text = mode === 'sta' ? ('Connected to Wi-Fi: ' + ip) : (modeText + ' connected at ' + ip);
    meta = 'Last update ' + formatSyncAge(ageS) + '.';
  } else if (ageS <= OTA_STALE_STATE_THRESHOLD_S) {
    cls = 'warn';
    text = modeText + ' data delayed';
    meta = 'Last update ' + formatSyncAge(ageS) + '.';
  } else {
    cls = 'err';
    text = 'Connection lost';
    meta = 'Last update ' + formatSyncAge(ageS) + '. Check network path.';
  }

  if (lastStateError && ageS !== null && ageS > 20) {
    meta += ' ' + lastStateError;
  }

  const showOtaOverlay =
    remoteOtaBusy && !otaUploadInFlight && !otaAwaitingDeviceOutcome && !otaRestartPending;
  setOtaGlobalOverlay(
    showOtaOverlay,
    'Another client is updating firmware. Wait until OTA completes, then this page will recover automatically.'
  );

  bar.className = 'net-status ' + cls;
  textEl.textContent = text;
  metaEl.textContent = meta;
  updateLastSyncUi();
  updateOtaPrecheck(network);
}

function rerenderUnitDependentViews() {
  updateHeaderClock();
  if (stateCache) {
    renderClimateOverview(stateCache.sensors, stateCache.derived);
  }
  if (historyCache) {
    renderCharts(historyCache);
  }
}

// ─────────────────────────────────────────────
// API helpers
// ─────────────────────────────────────────────
async function getJson(url, init) {
  const requestInit = init ? Object.assign({}, init) : {};
  if (!Object.prototype.hasOwnProperty.call(requestInit, 'cache')) {
    requestInit.cache = 'no-store';
  }
  const r = await fetch(url, requestInit);
  if (!r.ok) {
    let errorText = 'HTTP ' + r.status + ' for ' + url;
    let errorCode = '';
    let otaBusy = false;
    try {
      const payload = await r.json();
      if (payload && typeof payload.error === 'string' && payload.error) {
        errorText = payload.error;
      }
      if (payload && typeof payload.error_code === 'string' && payload.error_code) {
        errorCode = payload.error_code;
      }
      otaBusy = !!(payload && payload.ota_busy === true);
    } catch (_) {}
    const error = new Error(errorText);
    error.httpStatus = r.status;
    if (errorCode) error.code = errorCode;
    if (otaBusy) error.otaBusy = true;
    throw error;
  }
  return r.json();
}

async function postJson(url, payload) {
  const r = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  let json = null;
  try { json = await r.json(); } catch (_) {}
  if (!r.ok) throw new Error((json && json.error) || ('HTTP ' + r.status));
  return json;
}

function makeOtaHttpError(response, json) {
  const err = new Error((json && json.error) || ('HTTP ' + response.status));
  err.status = response.status;
  err.code = json && typeof json.error_code === 'string' ? json.error_code : '';
  err.confirmId = json && isNum(json.confirm_id) ? json.confirm_id : null;
  err.retryAfterMs = json && isNum(json.retry_after_ms) ? json.retry_after_ms : 1000;
  err.confirmTimeoutMs = json && isNum(json.confirm_timeout_ms) ? json.confirm_timeout_ms : 300000;
  err.payload = json;
  return err;
}

async function prepareOtaUpload(fileSizeBytes, confirmId) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), OTA_PREPARE_TIMEOUT_MS);
  try {
    const params = new URLSearchParams();
    if (isNum(fileSizeBytes) && fileSizeBytes > 0) {
      params.set('ota_size', String(Math.trunc(fileSizeBytes)));
    }
    if (isNum(confirmId) && confirmId > 0) {
      params.set('ota_confirm_id', String(Math.trunc(confirmId)));
    }
    const url = params.toString() ? ('/api/ota/prepare?' + params.toString()) : '/api/ota/prepare';
    const r = await fetch(url, {
      method: 'POST',
      cache: 'no-store',
      signal: controller.signal,
    });
    let json = null;
    try { json = await r.json(); } catch (_) {}
    if (!r.ok) throw makeOtaHttpError(r, json);
    return json;
  } catch (err) {
    if (err && err.name === 'AbortError') {
      throw new Error('Device did not respond to OTA prepare request.');
    }
    throw err;
  } finally {
    clearTimeout(timeoutId);
  }
}

function sleepMs(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function waitForOtaPhysicalConfirm(fileSizeBytes, firstError, statusEl) {
  const confirmId = firstError && isNum(firstError.confirmId) ? firstError.confirmId : null;
  if (!confirmId) {
    throw firstError || new Error('Device requested confirmation but did not return a confirmation id.');
  }

  const timeoutMs = Math.max(1000, firstError.confirmTimeoutMs || 300000);
  const startedAt = Date.now();
  let retryMs = Math.max(500, Math.min(5000, firstError.retryAfterMs || 1000));

  otaAwaitingPhysicalConfirm = true;
  updateOtaPrecheck(safeStateNetwork());
  statusEl.textContent = 'Confirm update on the device...';
  statusEl.className = 'ota-status warn';

  while (Date.now() - startedAt < timeoutMs) {
    await sleepMs(retryMs);
    try {
      return await prepareOtaUpload(fileSizeBytes, confirmId);
    } catch (err) {
      const code = err && err.code ? err.code : '';
      if (code === 'OTA_PHYSICAL_CONFIRM_REQUIRED') {
        retryMs = Math.max(500, Math.min(5000, err.retryAfterMs || retryMs));
        continue;
      }
      if (code === 'OTA_PHYSICAL_CONFIRM_DENIED') {
        throw new Error('Firmware update was denied on the device.');
      }
      if (code === 'OTA_PHYSICAL_CONFIRM_EXPIRED') {
        throw new Error('Firmware update confirmation expired. Start the upload again.');
      }
      if (code === 'OTA_PHYSICAL_CONFIRM_BUSY') {
        throw new Error('Another firmware update confirmation is already pending on the device.');
      }
      throw err;
    }
  }

  throw new Error('Firmware update confirmation timed out.');
}

function cacheStatePayload(payload) {
  if (payload && payload.thresholds) {
    applyThresholdsFromState(payload.thresholds);
  }
  stateCache = payload;
  lastStateOkAtMs = Date.now();
  lastStateError = '';
  lastStateOtaBusy = !!(payload && payload.ota_busy === true);
  if (isNum(payload && payload.time_epoch_s)) {
    deviceClockRef = { epochMs: payload.time_epoch_s * 1000, capturedAtMs: Date.now() };
  }
}

async function probeLatestOtaState(timeoutMs) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const r = await fetch('/api/state?probe=1', { cache: 'no-store', signal: controller.signal });
    if (!r.ok) return null;
    const payload = await r.json();
    cacheStatePayload(payload);
    updateHeaderClock();
    updateNetStatusBanner();
    updateOtaPrecheck(safeStateNetwork());
    return payload;
  } catch (_) {
    return null;
  } finally {
    clearTimeout(timeoutId);
  }
}

function stopOtaRecoveryWatcher() {
  otaRecoveryActive = false;
  if (otaRecoveryTimer) {
    clearTimeout(otaRecoveryTimer);
    otaRecoveryTimer = null;
  }
  if (otaRecoveryProbeController) {
    otaRecoveryProbeController.abort();
    otaRecoveryProbeController = null;
  }
}

function scheduleOtaRecoveryProbe(delayMs) {
  if (!otaRecoveryActive) return;
  if (otaRecoveryTimer) {
    clearTimeout(otaRecoveryTimer);
    otaRecoveryTimer = null;
  }
  if (otaRecoveryProbeController) {
    otaRecoveryProbeController.abort();
    otaRecoveryProbeController = null;
  }
  otaRecoveryTimer = setTimeout(async () => {
    if (!otaRecoveryActive) return;
    const controller = new AbortController();
    otaRecoveryProbeController = controller;
    const timeoutId = setTimeout(() => {
      controller.abort();
    }, OTA_RECOVERY_PROBE_TIMEOUT_MS);
    try {
      const r = await fetch('/api/state?probe=1', { cache: 'no-store', signal: controller.signal });
      if (r.ok) {
        let payload = null;
        try { payload = await r.json(); } catch (_) {}
        if (payload) {
          cacheStatePayload(payload);
          const ota = payload.ota || {};
          const otaStatus = typeof ota.status === 'string' ? ota.status : '';
          if (otaStatus === 'failed') {
            stopOtaRecoveryWatcher();
            otaAwaitingDeviceOutcome = false;
            otaRestartPending = false;
            otaReconnectGraceUntilMs = 0;
            const uploadBtn = document.getElementById('otaUploadBtn');
            const statusEl = document.getElementById('otaStatus');
            if (uploadBtn) uploadBtn.disabled = false;
            if (statusEl) {
              statusEl.textContent = ota.error || 'OTA failed.';
              statusEl.className = 'ota-status err';
            }
            updateNetStatusBanner();
            updateOtaPrecheck(safeStateNetwork());
            return;
          }
          if (otaStatus === 'uploading') {
            otaAwaitingDeviceOutcome = true;
            otaRestartPending = false;
            updateNetStatusBanner();
            updateOtaPrecheck(safeStateNetwork());
          } else if (otaStatus === 'rebooting' || otaStatus === 'success') {
            otaAwaitingDeviceOutcome = false;
            otaRestartPending = true;
            otaReconnectGraceUntilMs = Date.now() + OTA_RECONNECT_GRACE_MS;
            updateNetStatusBanner();
            updateOtaPrecheck(safeStateNetwork());
          }
          if (!payload.ota_busy && otaStatus !== 'rebooting') {
            window.location.reload();
            return;
          }
        }
      }
    } catch (_) {
    } finally {
      clearTimeout(timeoutId);
      if (otaRecoveryProbeController === controller) {
        otaRecoveryProbeController = null;
      }
    }
    scheduleOtaRecoveryProbe(2000);
  }, delayMs);
}

function startOtaRecoveryWatcher() {
  if (otaRecoveryActive) return;
  otaRecoveryActive = true;
  scheduleOtaRecoveryProbe(1200);
}

function setOtaAwaitingDeviceOutcome(message) {
  const statusEl = document.getElementById('otaStatus');
  const uploadBtn = document.getElementById('otaUploadBtn');
  otaUploadInFlight = false;
  otaAwaitingDeviceOutcome = true;
  otaRestartPending = false;
  otaReconnectGraceUntilMs = 0;
  if (uploadBtn) uploadBtn.disabled = true;
  if (statusEl) {
    statusEl.textContent =
      message || 'Browser lost the direct OTA response. Waiting for device status...';
    statusEl.className = 'ota-status warn';
  }
  updateNetStatusBanner();
  updateOtaPrecheck(safeStateNetwork());
  startOtaRecoveryWatcher();
}

function createOtaUploadTimeoutController(xhr, prepareCfg) {
  let phaseTimer = null;
  let abortMessage = '';
  let settled = false;
  let lastLoaded = 0;
  let responseWaitStarted = false;
  const responseWaitTimeoutMs =
    (prepareCfg && isNum(prepareCfg.response_wait_ms) && prepareCfg.response_wait_ms > 0)
      ? Math.trunc(prepareCfg.response_wait_ms)
      : OTA_UPLOAD_RESPONSE_TIMEOUT_FALLBACK_MS;

  function clearPhaseTimer() {
    if (phaseTimer) {
      clearTimeout(phaseTimer);
      phaseTimer = null;
    }
  }

  function scheduleAbort(timeoutMs, message) {
    clearPhaseTimer();
    phaseTimer = setTimeout(() => {
      if (settled) return;
      abortMessage = message || '';
      try { xhr.abort(); } catch (_) {}
    }, timeoutMs);
  }

  function noteProgress(loaded) {
    if (!isNum(loaded) || loaded <= lastLoaded) return;
    lastLoaded = loaded;
    if (!responseWaitStarted) {
      scheduleAbort(
        OTA_UPLOAD_NO_PROGRESS_TIMEOUT_MS,
        'Upload stalled. Retry closer to device or with stronger WiFi.'
      );
    }
  }

  function noteUploadComplete() {
    if (responseWaitStarted) return;
    responseWaitStarted = true;
    scheduleAbort(
      responseWaitTimeoutMs,
      'Device response timed out after upload. Wait for reconnect before retrying.'
    );
  }

  function consumeAbortMessage() {
    const message = abortMessage;
    abortMessage = '';
    return message;
  }

  function markSettled() {
    if (settled) return;
    settled = true;
    clearPhaseTimer();
  }

  scheduleAbort(
    OTA_UPLOAD_FIRST_PROGRESS_TIMEOUT_MS,
    'Upload did not start in time. Retry closer to device or with stronger WiFi.'
  );

  return {
    noteProgress,
    noteUploadComplete,
    consumeAbortMessage,
    markSettled,
  };
}

// ─────────────────────────────────────────────
// Clock
// ─────────────────────────────────────────────
function updateHeaderClock() {
  const nowMs = Date.now();
  const now = deviceClockRef
    ? new Date(deviceClockRef.epochMs + (nowMs - deviceClockRef.capturedAtMs))
    : new Date(nowMs);
  const timeLocale = settings.timeFormat24h ? (settings.tempUnit === 'f' ? 'en-US' : 'en-GB') : 'en-US';
  document.getElementById('headerTime').textContent = now.toLocaleTimeString(
    timeLocale,
    { hour: settings.timeFormat24h ? '2-digit' : 'numeric', minute:'2-digit', hour12:!settings.timeFormat24h }
  );
  document.getElementById('headerDate').textContent = now.toLocaleDateString(
    settings.tempUnit === 'f' ? 'en-US' : 'en-GB',
    { day:'2-digit', month:'short', year:'numeric' }
  ).toUpperCase();
  updateNetStatusBanner();
}

// ─────────────────────────────────────────────
// Settings UI
// ─────────────────────────────────────────────
function updateToggleDom(swId, enabled) {
  const sw = document.getElementById(swId);
  if (!sw) return;
  sw.classList.toggle('on', !!enabled);
}

function showToggleError(message) {
  const el = document.getElementById('displayToggleMsg');
  if (!el) return;
  el.textContent = message || '';
  el.classList.toggle('err', !!message);
  if (toggleMsgTimer) {
    clearTimeout(toggleMsgTimer);
    toggleMsgTimer = null;
  }
  if (message) {
    toggleMsgTimer = setTimeout(() => {
      const msgEl = document.getElementById('displayToggleMsg');
      if (!msgEl) return;
      msgEl.textContent = '';
      msgEl.classList.remove('err');
      toggleMsgTimer = null;
    }, 3500);
  }
}

function updateSaveBtn() {
  const btn = document.getElementById('saveSettingsBtn');
  if (!btn) return;
  btn.className = 'save-btn ' + settingsSaveStatus;
  const labels = { idle:'No changes', dirty:'Save Settings', saving:'Saving…', saved:'Saved', error:'Save Failed' };
  btn.textContent = labels[settingsSaveStatus] || 'Save';
  btn.disabled = settingsSaveStatus === 'idle' || settingsSaveStatus === 'saving';
}

function markMeasurementsDirty() {
  settingsSaveStatus = 'dirty';
  updateSaveBtn();
}

function tempOffsetCToDisplay(offsetC, tempUnit) {
  return tempUnit === 'f' ? (offsetC * 9 / 5) : offsetC;
}

function tempOffsetDisplayToC(offsetDisplay, tempUnit) {
  return tempUnit === 'f' ? (offsetDisplay * 5 / 9) : offsetDisplay;
}

function nudgeTempOffset(deltaDisplay) {
  const maxDisplayOffset = settings.tempUnit === 'f' ? 9 : 5;
  const displayVal = tempOffsetCToDisplay(settings.tempOffset, settings.tempUnit);
  const nextDisplayVal = Math.max(
    -maxDisplayOffset,
    Math.min(maxDisplayOffset, Math.round((displayVal + deltaDisplay) * 10) / 10)
  );
  settings.tempOffset = Number(tempOffsetDisplayToC(nextDisplayVal, settings.tempUnit).toFixed(1));
}

function nudgeHumOffset(delta) {
  const next = Math.round(settings.humOffset + delta);
  settings.humOffset = Math.max(-10, Math.min(10, next));
}

function updateTempOffsetLabel() {
  const lbl = document.getElementById('tempOffsetLabel');
  if (lbl) lbl.textContent = 'Temp offset (' + (settings.tempUnit === 'f' ? '\u00B0F' : '\u00B0C') + ')';
}

function updateTempOffsetDisplay() {
  const el = document.getElementById('tempOffsetVal');
  if (!el) return;
  const displayVal = tempOffsetCToDisplay(settings.tempOffset, settings.tempUnit);
  el.textContent = displayVal.toFixed(1);
}

function updateHumOffsetDisplay() {
  const el = document.getElementById('humOffsetVal');
  if (el) el.textContent = settings.humOffset.toFixed(0);
}

function updateSegmentDom(segId, attrName, value) {
  const seg = document.getElementById(segId);
  if (!seg) return;
  seg.querySelectorAll('.seg-btn').forEach(btn => {
    btn.classList.toggle('active', btn.getAttribute(attrName) === value);
  });
}

function shouldApplyToggleFromApi(toggleKey, overrideKey) {
  if (overrideKey === toggleKey) return true;
  const queue = toggleRequestState[toggleKey];
  return !(queue && (queue.inFlight || queue.queued !== null));
}

function applySettingsToUI(apiSettings, force, toggleOverrideKey) {
  if (!apiSettings) return;
  if (settingsSaveStatus === 'dirty' && !force) return;
  const prevTempUnit = settings.tempUnit;
  const prevTimeFormat24h = settings.timeFormat24h;
  const prevPressureAltitudeSet = settings.pressureAltitudeSet;
  const prevPressureAltitudeM = settings.pressureAltitudeM;

  if (typeof apiSettings.night_mode === 'boolean' &&
      shouldApplyToggleFromApi('night_mode', toggleOverrideKey)) {
    settings.nightMode = apiSettings.night_mode;
  }
  if (typeof apiSettings.night_mode_locked === 'boolean') settings.nightModeLocked = apiSettings.night_mode_locked;
  if (typeof apiSettings.backlight_on === 'boolean' &&
      shouldApplyToggleFromApi('backlight_on', toggleOverrideKey)) {
    settings.backlight = apiSettings.backlight_on;
  }
  if (!(timeSyncDirty && !force)) {
    if (typeof apiSettings.ntp_enabled === 'boolean') settings.ntpEnabled = apiSettings.ntp_enabled;
    if (typeof apiSettings.ntp_server === 'string') settings.ntpServer = apiSettings.ntp_server;
  }
  if (typeof apiSettings.units_c === 'boolean') settings.tempUnit = apiSettings.units_c ? 'c' : 'f';
  if (typeof apiSettings.time_format_24h === 'boolean') settings.timeFormat24h = apiSettings.time_format_24h;
  if (isNum(apiSettings.temp_offset)) settings.tempOffset = Number(apiSettings.temp_offset.toFixed(1));
  if (isNum(apiSettings.hum_offset)) settings.humOffset = Number(apiSettings.hum_offset.toFixed(0));
  if (typeof apiSettings.pressure_altitude_set === 'boolean') settings.pressureAltitudeSet = apiSettings.pressure_altitude_set;
  if (isNum(apiSettings.pressure_altitude_m)) settings.pressureAltitudeM = Math.round(apiSettings.pressure_altitude_m);
  if (typeof apiSettings.display_name === 'string') settings.displayName = apiSettings.display_name;

  updateToggleDom('nightModeSw', settings.nightMode);
  updateToggleDom('backlightSw', settings.backlight);
  updateToggleDom('ntpEnabledSw', settings.ntpEnabled);
  const nightRow = document.getElementById('nightModeToggle');
  if (nightRow) nightRow.classList.toggle('disabled', settings.nightModeLocked);
  updateSegmentDom('tempUnitSeg', 'data-unit', settings.tempUnit);
  updateTempOffsetLabel();
  updateTempOffsetDisplay();
  updateHumOffsetDisplay();
  if (prevTempUnit !== settings.tempUnit) {
    rerenderUnitDependentViews();
  } else if (
    prevPressureAltitudeSet !== settings.pressureAltitudeSet ||
    prevPressureAltitudeM !== settings.pressureAltitudeM
  ) {
    rerenderUnitDependentViews();
  } else if (prevTimeFormat24h !== settings.timeFormat24h) {
    updateHeaderClock();
  }

  const nameInput = document.getElementById('displayNameInput');
  if (nameInput && (!nameDirty || force)) {
    nameInput.value = settings.displayName || '';
    nameDirty = false;
    updateNameBtn('idle');
  }

  const ntpInput = document.getElementById('ntpServerInput');
  if (ntpInput && (!timeSyncDirty || force)) {
    ntpInput.value = settings.ntpServer || '';
    timeSyncDirty = false;
    if (timeSyncSaveStatus !== 'saving') {
      timeSyncSaveStatus = 'idle';
      updateTimeSyncSaveBtn();
    }
  }

  renderHeaderDeviceName();

  Object.assign(savedSettings, settings);
  if (!force && settingsSaveStatus !== 'error') {
    settingsSaveStatus = 'idle';
    updateSaveBtn();
  }
}

function markTimeSyncDirty() {
  timeSyncDirty = true;
  timeSyncSaveStatus = 'dirty';
  updateTimeSyncSaveBtn();
}

function updateTimeSyncSaveBtn() {
  const btn = document.getElementById('saveTimeSyncBtn');
  if (!btn) return;
  btn.className = 'save-btn ' + timeSyncSaveStatus;
  const labels = {
    idle: 'No changes',
    dirty: 'Save Time Sync',
    saving: 'Saving...',
    saved: 'Saved',
    error: 'Save Failed'
  };
  btn.textContent = labels[timeSyncSaveStatus] || 'Save';
  btn.disabled = timeSyncSaveStatus === 'idle' || timeSyncSaveStatus === 'saving';
}

function updateNameBtn(status) {
  const btn = document.getElementById('saveNameBtn');
  if (!btn) return;
  btn.className = 'save-btn ' + status;
  const labels = { idle:'No changes', dirty:'Save Name', saving:'Saving…', saved:'Saved', error:'Save Failed' };
  btn.textContent = labels[status] || 'Save';
  btn.disabled = status === 'idle' || status === 'saving';
}

function processToggleQueue(key) {
  const queue = toggleRequestState[key];
  if (!queue || queue.inFlight || queue.queued === null) return;

  const target = queue.queued;
  queue.queued = null;
  queue.inFlight = true;

  const payload = {};
  payload[key] = target;
  postJson('/api/settings', payload)
    .then(result => {
      // Avoid forcing a stale response into UI if a newer value is already queued.
      if (result && result.settings && queue.queued === null) {
        applySettingsToUI(result.settings, true, key);
      }
      showToggleError('');
    })
    .catch(() => {
      // Recover from transport/server errors by re-syncing current settings.
      getJson('/api/state')
        .then(statePayload => {
          if (statePayload && statePayload.settings) {
            applySettingsToUI(statePayload.settings, true, key);
          }
        })
        .catch(() => {});
      showToggleError('Toggle update failed. Restored device state.');
    })
    .finally(() => {
      queue.inFlight = false;
      if (queue.queued !== null) {
        processToggleQueue(key);
      }
    });
}

function enqueueToggle(key, value) {
  const queue = toggleRequestState[key];
  if (!queue) return;
  queue.queued = value;
  processToggleQueue(key);
}

async function saveMeasurements() {
  if (settingsSaving) return;
  settingsSaving = true;
  settingsSaveStatus = 'saving';
  updateSaveBtn();
  try {
    const result = await postJson('/api/settings', {
      units_c: settings.tempUnit === 'c',
      temp_offset: Number(settings.tempOffset.toFixed(1)),
      hum_offset: Math.round(settings.humOffset),
    });
    if (result && result.settings) applySettingsToUI(result.settings, true);
    settingsSaveStatus = 'saved';
    updateSaveBtn();
    setTimeout(() => { if (settingsSaveStatus === 'saved') { settingsSaveStatus = 'idle'; updateSaveBtn(); } }, 2500);
  } catch (_) {
    settingsSaveStatus = 'error';
    updateSaveBtn();
  }
  settingsSaving = false;
}

async function saveName() {
  const nameInput = document.getElementById('displayNameInput');
  if (!nameInput) return;
  updateNameBtn('saving');
  try {
    const result = await postJson('/api/settings', { display_name: nameInput.value.trim() });
    if (result && result.settings) applySettingsToUI(result.settings, true);
    updateNameBtn('saved');
    nameDirty = false;
    setTimeout(() => { if (!nameDirty) updateNameBtn('idle'); }, 2500);
  } catch (_) {
    updateNameBtn('error');
  }
}

async function saveTimeSync() {
  if (timeSyncSaving) return;
  const ntpInput = document.getElementById('ntpServerInput');
  if (!ntpInput) return;

  settings.ntpServer = ntpInput.value.trim();
  timeSyncSaving = true;
  timeSyncSaveStatus = 'saving';
  updateTimeSyncSaveBtn();
  try {
    const result = await postJson('/api/settings', {
      ntp_enabled: settings.ntpEnabled,
      ntp_server: settings.ntpServer,
    });
    if (result && result.settings) applySettingsToUI(result.settings, true);
    timeSyncDirty = false;
    timeSyncSaveStatus = 'saved';
    updateTimeSyncSaveBtn();
    setTimeout(() => {
      if (!timeSyncDirty && timeSyncSaveStatus === 'saved') {
        timeSyncSaveStatus = 'idle';
        updateTimeSyncSaveBtn();
      }
    }, 2500);
  } catch (_) {
    timeSyncSaveStatus = 'error';
    updateTimeSyncSaveBtn();
  }
  timeSyncSaving = false;
}

function initSettingsUI() {
  // Night mode toggle
  const nightRow = document.getElementById('nightModeToggle');
  if (nightRow) nightRow.addEventListener('click', () => {
    if (settings.nightModeLocked) return;
    settings.nightMode = !settings.nightMode;
    updateToggleDom('nightModeSw', settings.nightMode);
    enqueueToggle('night_mode', settings.nightMode);
  });

  // Backlight toggle
  const blRow = document.getElementById('backlightToggle');
  if (blRow) blRow.addEventListener('click', () => {
    settings.backlight = !settings.backlight;
    updateToggleDom('backlightSw', settings.backlight);
    enqueueToggle('backlight_on', settings.backlight);
  });

  // Temp unit segment
  const unitSeg = document.getElementById('tempUnitSeg');
  if (unitSeg) unitSeg.querySelectorAll('.seg-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const unit = btn.getAttribute('data-unit');
      if (unit === settings.tempUnit) return;
      settings.tempUnit = unit;
      updateSegmentDom('tempUnitSeg', 'data-unit', unit);
      updateTempOffsetLabel();
      updateTempOffsetDisplay();
      rerenderUnitDependentViews();
      markMeasurementsDirty();
    });
  });

  // Temp offset stepper
  document.getElementById('tempOffsetMinus').addEventListener('click', () => {
    nudgeTempOffset(settings.tempUnit === 'f' ? -0.2 : -0.1);
    updateTempOffsetDisplay();
    markMeasurementsDirty();
  });
  document.getElementById('tempOffsetPlus').addEventListener('click', () => {
    nudgeTempOffset(settings.tempUnit === 'f' ? 0.2 : 0.1);
    updateTempOffsetDisplay();
    markMeasurementsDirty();
  });

  // Hum offset stepper
  document.getElementById('humOffsetMinus').addEventListener('click', () => {
    nudgeHumOffset(-1);
    updateHumOffsetDisplay();
    markMeasurementsDirty();
  });
  document.getElementById('humOffsetPlus').addEventListener('click', () => {
    nudgeHumOffset(1);
    updateHumOffsetDisplay();
    markMeasurementsDirty();
  });

  // Save measurements
  document.getElementById('saveSettingsBtn').addEventListener('click', () => {
    if (settingsSaveStatus === 'dirty') saveMeasurements().catch(() => {});
  });

  // Display name
  const nameInput = document.getElementById('displayNameInput');
  if (nameInput) nameInput.addEventListener('input', () => {
    nameDirty = true;
    updateNameBtn('dirty');
  });
  document.getElementById('saveNameBtn').addEventListener('click', () => {
    if (nameDirty) saveName().catch(() => {});
  });
}

function initTimeSyncUI() {
  const ntpRow = document.getElementById('ntpEnabledToggle');
  if (ntpRow) ntpRow.addEventListener('click', () => {
    settings.ntpEnabled = !settings.ntpEnabled;
    updateToggleDom('ntpEnabledSw', settings.ntpEnabled);
    markTimeSyncDirty();
  });

  const ntpInput = document.getElementById('ntpServerInput');
  if (ntpInput) ntpInput.addEventListener('input', () => {
    settings.ntpServer = ntpInput.value;
    markTimeSyncDirty();
  });

  document.getElementById('saveTimeSyncBtn').addEventListener('click', () => {
    if (timeSyncSaveStatus === 'dirty' || timeSyncSaveStatus === 'error') {
      saveTimeSync().catch(() => {});
    }
  });
}

// ─────────────────────────────────────────────
// OTA upload
// ─────────────────────────────────────────────
function initOtaUI() {
  document.getElementById('otaUploadBtn').addEventListener('click', async () => {
    if (otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending) return;
    const fileInput = document.getElementById('otaFile');
    const file = fileInput.files && fileInput.files[0];
    const statusEl = document.getElementById('otaStatus');
    const progressEl = document.getElementById('otaProgress');
    const uploadBtn = document.getElementById('otaUploadBtn');

    if (!file) {
      statusEl.textContent = 'Select a firmware file first.';
      statusEl.className = 'ota-status';
      return;
    }

    const network = safeStateNetwork();
    const mode = network && typeof network.mode === 'string' ? network.mode : '';
    const ageS = secondsSince(lastStateOkAtMs);
    if (lastStateOtaBusy) {
      statusEl.textContent = 'OTA is already in progress from another client. Wait until it finishes.';
      statusEl.className = 'ota-status warn';
      return;
    }
    if (ageS === null || ageS > OTA_STALE_STATE_THRESHOLD_S) {
      statusEl.textContent = 'Live state is stale. Wait for reconnect before OTA.';
      statusEl.className = 'ota-status err';
      return;
    }
    if (mode === 'off' || !mode) {
      statusEl.textContent = 'WiFi is offline. OTA requires AP or STA connection.';
      statusEl.className = 'ota-status err';
      return;
    }
    if (mode === 'ap') {
      statusEl.textContent = 'AP mode detected. Keep client close to Aura; starting upload...';
      statusEl.className = 'ota-status warn';
    } else if (mode === 'sta' && isNum(network.rssi) && network.rssi <= -78) {
      statusEl.textContent = 'Weak STA signal (' + network.rssi + ' dBm). Upload may fail; starting anyway...';
      statusEl.className = 'ota-status warn';
    }

    stopOtaRecoveryWatcher();
    otaReconnectGraceUntilMs = 0;

    otaUploadInFlight = true;
    otaAwaitingPhysicalConfirm = false;
    uploadBtn.disabled = true;
    fileInput.disabled = true;
    statusEl.textContent = 'Preparing device for upload...';
    statusEl.className = 'ota-status';
    let prepareCfg = null;
    try {
      prepareCfg = await prepareOtaUpload(file.size);
    } catch (err) {
      if (err && err.code === 'OTA_PHYSICAL_CONFIRM_REQUIRED') {
        try {
          prepareCfg = await waitForOtaPhysicalConfirm(file.size, err, statusEl);
        } catch (waitErr) {
          otaAwaitingPhysicalConfirm = false;
          otaUploadInFlight = false;
          uploadBtn.disabled = false;
          fileInput.disabled = false;
          statusEl.textContent =
            (waitErr && waitErr.message) ? waitErr.message : 'Firmware update confirmation failed.';
          statusEl.className = 'ota-status err';
          updateNetStatusBanner();
          updateOtaPrecheck(safeStateNetwork());
          return;
        }
      } else {
        otaUploadInFlight = false;
        uploadBtn.disabled = false;
        fileInput.disabled = false;
        statusEl.textContent =
          (err && err.message) ? err.message : 'Failed to prepare device for upload.';
        statusEl.className = 'ota-status err';
        updateNetStatusBanner();
        updateOtaPrecheck(safeStateNetwork());
        return;
      }
    }
    otaAwaitingPhysicalConfirm = false;

    otaUploadInFlight = true;
    statusEl.textContent = 'Uploading firmware…';
    statusEl.className = 'ota-status';
    progressEl.style.width = '0%';

    const form = new FormData();
    form.append('ota_size', String(file.size));
    if (prepareCfg && isNum(prepareCfg.confirm_id) && prepareCfg.confirm_id > 0) {
      form.append('ota_confirm_id', String(Math.trunc(prepareCfg.confirm_id)));
    }
    form.append('firmware', file, file.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota', true);
    xhr.timeout = 0;
    const uploadTimeouts = createOtaUploadTimeoutController(xhr, prepareCfg);
    let uploadSettled = false;
    let responseWaitAnnounced = false;

    const markUploadSettled = () => {
      if (uploadSettled) return false;
      uploadSettled = true;
      uploadTimeouts.markSettled();
      return true;
    };

    const finishUploadStatus = (message, cls) => {
      otaUploadInFlight = false;
      otaAwaitingPhysicalConfirm = false;
      otaAwaitingDeviceOutcome = false;
      otaRestartPending = false;
      otaReconnectGraceUntilMs = 0;
      stopOtaRecoveryWatcher();
      uploadBtn.disabled = false;
      fileInput.disabled = false;
      statusEl.textContent = message;
      statusEl.className = 'ota-status ' + (cls || 'err');
      updateNetStatusBanner();
      updateOtaPrecheck(safeStateNetwork());
    };

    const finishUploadSuccess = message => {
      otaUploadInFlight = false;
      otaAwaitingPhysicalConfirm = false;
      otaAwaitingDeviceOutcome = false;
      otaRestartPending = true;
      otaReconnectGraceUntilMs = Date.now() + OTA_RECONNECT_GRACE_MS;
      uploadBtn.disabled = true;
      progressEl.style.width = '100%';
      statusEl.textContent = message || 'Firmware uploaded. Device will reboot.';
      statusEl.className = 'ota-status ok';
      fileInput.value = '';
      updateNetStatusBanner();
      updateOtaPrecheck(safeStateNetwork());
      startOtaRecoveryWatcher();
    };

    const reconcileOtaOutcome = async fallbackMessage => {
      const payload = await probeLatestOtaState(2500);
      const ota = payload && payload.ota;
      const status = ota && typeof ota.status === 'string' ? ota.status : '';
      if (status === 'rebooting' || status === 'success') {
        finishUploadSuccess((ota && ota.message) || fallbackMessage);
        return true;
      }
      if (status === 'failed') {
        finishUploadStatus((ota && ota.error) || fallbackMessage, 'err');
        return true;
      }
      if (status === 'uploading') {
        setOtaAwaitingDeviceOutcome(
          'Browser lost the upload response. Device still reports OTA in progress.'
        );
        return true;
      }
      if (status === 'idle' && responseWaitAnnounced && payload && payload.ota_busy !== true) {
        window.location.reload();
        return true;
      }
      return false;
    };

    const announceResponseWait = () => {
      if (responseWaitAnnounced) return;
      responseWaitAnnounced = true;
      progressEl.style.width = '100%';
      statusEl.textContent = 'Browser finished sending file. Waiting for device confirmation...';
      statusEl.className = 'ota-status';
    };

    xhr.upload.onprogress = ev => {
      if (ev.lengthComputable && ev.total > 0) {
        const pct = Math.min(100, Math.round((ev.loaded / ev.total) * 100));
        progressEl.style.width = pct + '%';
      }
      uploadTimeouts.noteProgress(ev.loaded);
      if (ev.lengthComputable && ev.total > 0 && ev.loaded >= ev.total) {
        uploadTimeouts.noteUploadComplete();
        announceResponseWait();
      }
    };
    xhr.upload.onload = () => {
      uploadTimeouts.noteUploadComplete();
      announceResponseWait();
    };
    xhr.onload = () => {
      if (!markUploadSettled()) return;
      let pl = null;
      try { pl = JSON.parse(xhr.responseText || '{}'); } catch (_) {}
      if (xhr.status >= 200 && xhr.status < 300 && pl && pl.success === true) {
        finishUploadSuccess(pl.message || 'Firmware uploaded. Device will reboot.');
        return;
      }
      finishUploadStatus((pl && pl.error) || 'Upload failed (HTTP ' + (xhr.status || 0) + ')', 'err');
    };
    xhr.onerror = async () => {
      if (!markUploadSettled()) return;
      if (await reconcileOtaOutcome('Firmware uploaded. Device will reboot.')) return;
      finishUploadStatus('Upload failed. Check device connection and retry.', 'err');
    };
    xhr.onabort = async () => {
      if (!markUploadSettled()) return;
      const fallbackMessage =
        uploadTimeouts.consumeAbortMessage() ||
        'Upload was interrupted. Check device connection and retry.';
      if (await reconcileOtaOutcome('Firmware uploaded. Device will reboot.')) return;
      finishUploadStatus(fallbackMessage, 'err');
    };
    xhr.ontimeout = async () => {
      if (!markUploadSettled()) return;
      if (await reconcileOtaOutcome('Firmware uploaded. Device will reboot.')) return;
      finishUploadStatus('Upload timed out. Retry closer to device or with stronger WiFi.', 'err');
    };
    xhr.onloadend = async () => {
      if (!markUploadSettled()) return;
      const fallbackMessage =
        uploadTimeouts.consumeAbortMessage() ||
        'Upload ended unexpectedly. Check device connection and retry.';
      if (await reconcileOtaOutcome('Firmware uploaded. Device will reboot.')) return;
      finishUploadStatus(fallbackMessage, 'err');
    };
    xhr.send(form);
  });
}

// ─────────────────────────────────────────────
// Data refresh
// ─────────────────────────────────────────────
async function refreshState() {
  if (otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending) return;
  const payload = await getJson('/api/state');
  cacheStatePayload(payload);
  otaReconnectGraceUntilMs = 0;

  // Header clock
  if (deviceClockRef) {
    updateHeaderClock();
  }

  // Device name
  renderHeaderDeviceName();

  // Sensors tab
  renderHeroMetric(payload.sensors, historyCache);
  renderClimateOverview(payload.sensors, payload.derived);
  renderGasGrid(payload.sensors);
  renderPMGrid(payload.sensors);

  // System tab
  renderSystemMeta(payload);

  // Settings (non-destructive)
  if (payload.settings) applySettingsToUI(payload.settings, false);
  updateNetStatusBanner();
}

async function refreshSensorHistory() {
  if (otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending) return;
  const payload = await getJson('/api/charts?group=core&window=3h');
  if (!payload || !Array.isArray(payload.timestamps)) return;
  // Extract co2 series into simple row array
  const co2Series = (payload.series || []).find(s => s && s.key === 'co2');
  if (!co2Series) return;
  historyCache = payload.timestamps.map((ts, i) => ({
    co2: Array.isArray(co2Series.values) && isNum(co2Series.values[i]) ? co2Series.values[i] : null,
  }));
  // Re-render hero
  if (stateCache) renderHeroMetric(stateCache.sensors, historyCache);
}

async function refreshCharts() {
  if (otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending) return;
  if (chartsRefreshController) {
    chartsRefreshController.abort();
    chartsRefreshController = null;
  }
  const token = ++chartsRefreshToken;
  const controller = new AbortController();
  chartsRefreshController = controller;

  try {
    const payload = await getJson(
      '/api/charts?group=' + encodeURIComponent(chartGroup) + '&window=' + encodeURIComponent(chartRange),
      { signal: controller.signal }
    );
    if (token !== chartsRefreshToken) {
      return;
    }
    renderCharts(payload);
    lastStateError = '';
  } catch (error) {
    if (error && error.name === 'AbortError') {
      return;
    }
    throw error;
  } finally {
    if (chartsRefreshController === controller) {
      chartsRefreshController = null;
    }
  }
}

async function refreshEvents() {
  if (otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending) return;
  const payload = await getJson('/api/events');
  renderEvents(payload);
  lastStateError = '';
}

async function refreshActive() {
  if (refreshBusy || otaUploadInFlight || otaAwaitingDeviceOutcome || otaRestartPending) return;
  refreshBusy = true;
  try { await refreshState(); } catch (error) {
    lastStateOtaBusy = !!(error && (error.code === 'OTA_BUSY' || error.otaBusy === true));
    lastStateError = (error && error.message) ? error.message : 'State refresh failed.';
    updateNetStatusBanner();
  }
  if (!document.hidden) {
    if (activeTab === 'charts')  { try { await refreshCharts(); } catch (_) {} }
    if (activeTab === 'events')  { try { await refreshEvents(); } catch (_) {} }
    if (activeTab === 'sensors') { try { await refreshSensorHistory(); } catch (_) {} }
    if (activeTab === 'system')  { try { await refreshSdHistory({ silent: true }); } catch (_) {} }
  }
  refreshBusy = false;
}

function scheduleRefreshActive(delayMs) {
  if (refreshTimer) clearTimeout(refreshTimer);
  refreshTimer = setTimeout(async () => {
    refreshTimer = null;
    try {
      await refreshActive();
    } finally {
      scheduleRefreshActive(document.hidden ? STATE_REFRESH_HIDDEN_MS : STATE_REFRESH_VISIBLE_MS);
    }
  }, delayMs);
}

// ─────────────────────────────────────────────
// Tab nav
// ─────────────────────────────────────────────
function selectTab(tab) {
  activeTab = tab;
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.classList.toggle('active', btn.getAttribute('data-tab') === tab);
  });
  ['sensors','charts','events','settings','system'].forEach(id => {
    const panel = document.getElementById('tab-' + id);
    if (panel) panel.classList.toggle('active', id === tab);
  });
  if (tab === 'charts') refreshCharts().catch(() => {});
  if (tab === 'events') refreshEvents().catch(() => {});
  if (tab === 'sensors') refreshSensorHistory().catch(() => {});
  if (tab === 'system') refreshSdHistory().catch(() => {});
}

// ─────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────
// Tab buttons
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => selectTab(btn.getAttribute('data-tab')));
});

// Chart group segment
document.querySelectorAll('#cgroupSeg .seg-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    chartGroup = btn.getAttribute('data-group') || 'core';
    document.querySelectorAll('#cgroupSeg .seg-btn').forEach(b => b.classList.toggle('active', b === btn));
    refreshCharts().catch(() => {});
  });
});

// Chart range segment
document.querySelectorAll('#crangeSeg .seg-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    chartRange = btn.getAttribute('data-range') || '24h';
    document.querySelectorAll('#crangeSeg .seg-btn').forEach(b => b.classList.toggle('active', b === btn));
    refreshCharts().catch(() => {});
  });
});

// System buttons
document.getElementById('rebootBtn').addEventListener('click', async () => {
  try { await postJson('/api/settings', { restart: true }); } catch (_) {}
});
document.getElementById('openThemeBtn').addEventListener('click', () => { window.location.href = '/theme'; });
document.getElementById('openDacBtn').addEventListener('click', () => { window.location.href = '/dac'; });
document.getElementById('sdRefreshBtn').addEventListener('click', () => { refreshSdHistory().catch(() => {}); });
document.getElementById('sdClearCurrentBtn').addEventListener('click', () => {
  runSdServiceAction('/api/history/current-day/clear',
                     'Delete current day history snapshot?',
                     'Deleting current day snapshot...',
                     'Current day history deleted.').catch(() => {});
});
document.getElementById('sdClearHistoryBtn').addEventListener('click', () => {
  runSdServiceAction('/api/history/clear',
                     'Delete daily CSV and current day history?',
                     'Deleting history files...',
                     'History deleted.').catch(() => {});
});

initSettingsUI();
initTimeSyncUI();
initOtaUI();
updateHeaderClock();
setInterval(updateHeaderClock, 1000);
scheduleRefreshActive(STATE_REFRESH_VISIBLE_MS);
document.addEventListener('visibilitychange', () => {
  scheduleRefreshActive(document.hidden ? STATE_REFRESH_HIDDEN_MS : 1500);
  if (!document.hidden) refreshActive().catch(() => {});
});

// Initial data load
refreshState().catch(error => {
  lastStateError = (error && error.message) ? error.message : 'Initial state fetch failed.';
  updateNetStatusBanner();
});
if (!document.hidden) refreshSensorHistory().catch(() => {});
</script>
</body>
</html>
)HTML_DASH_AP";
#endif

#include "web/generated/WebTemplatesDashboardApGzip.inc"

} // namespace WebTemplates
