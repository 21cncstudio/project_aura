from pathlib import Path

from web_asset_split import (
    CSS_PATH_PLACEHOLDER,
    JS_PATH_PLACEHOLDER,
    apply_asset_paths,
    ShellConfig,
    SplitSymbols,
    extract_template,
    get_app_version,
    gzip_bytes,
    make_version_token,
    render_split_inc,
    split_assets,
    write_if_changed,
)

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
SOURCE_CPP = PROJECT_DIR / "src" / "web" / "WebTemplatesDashboardAp.cpp"
OUT_INC = PROJECT_DIR / "src" / "web" / "generated" / "WebTemplatesDashboardApGzip.inc"

START_MARKER = 'const char kDashboardPageTemplateAp[] PROGMEM = R"HTML_DASH_AP(\n'
END_MARKER = ')HTML_DASH_AP";'

SHELL = ShellConfig(
    shell_boot_html="""
<div id="shellBoot" class="shell-boot" aria-live="polite">
  <div class="shell-boot-head">
    <div>
      <div class="shell-boot-title">AURA Dashboard</div>
      <div class="shell-boot-sub">Loading live device data...</div>
    </div>
    <div class="shell-boot-chip">Skeleton</div>
  </div>
  <div class="shell-boot-grid">
    <div class="shell-boot-card shell-boot-card-hero"></div>
    <div class="shell-boot-card"></div>
    <div class="shell-boot-card"></div>
    <div class="shell-boot-card"></div>
  </div>
</div>
""".strip(),
    shell_critical_css="""
:root{color-scheme:dark;--bg:#04080b;--surface:#11181d;--surface-2:#1c282d;--border:rgba(169,199,196,.12);--teal:#2bb4bd;--text:#f9fafb;--subtle:#9ca3af}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
body{padding:16px}
body.shell-loading .wrap{display:none}
body.shell-loading #otaGlobalOverlay{display:none}
.shell-boot{display:none;max-width:1152px;margin:0 auto}
body.shell-loading .shell-boot{display:block}
.shell-boot-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:18px;flex-wrap:wrap}
.shell-boot-title{font-size:24px;font-weight:800;letter-spacing:.02em}
.shell-boot-sub{margin-top:4px;color:var(--subtle);font-size:13px}
.shell-boot-chip{padding:8px 12px;border-radius:999px;border:1px solid rgba(43,180,189,.35);background:rgba(17,24,29,.82);color:#57c4c2;font-size:12px;font-weight:700;text-transform:uppercase;letter-spacing:.08em}
.shell-boot-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px}
.shell-boot-card{height:132px;border-radius:18px;border:1px solid var(--border);background:linear-gradient(110deg,rgba(17,24,29,.96) 8%,rgba(28,40,45,.88) 18%,rgba(17,24,29,.96) 33%);background-size:200% 100%;animation:shell-shimmer 1.2s linear infinite}
.shell-boot-card-hero{min-height:196px;grid-column:span 2}
@keyframes shell-shimmer{to{background-position-x:-200%}}
@media (max-width:720px){body{padding:14px}.shell-boot-title{font-size:21px}.shell-boot-card-hero{grid-column:span 1;min-height:164px}}
""".strip(),
    insert_anchor='<div class="wrap">',
)

SYMBOLS = SplitSymbols(
    version_symbol="kDashboardAssetVersion",
    css_path_symbol="kDashboardStylesCssPath",
    js_path_symbol="kDashboardAppJsPath",
    shell_symbol="kDashboardShellHtmlGzip",
    css_symbol="kDashboardStylesCssGzip",
    js_symbol="kDashboardAppJsGzip",
)


def main() -> None:
    html = extract_template(SOURCE_CPP, START_MARKER, END_MARKER, "Dashboard")
    shell_html, css_text, js_text = split_assets(
        html,
        CSS_PATH_PLACEHOLDER,
        JS_PATH_PLACEHOLDER,
        SHELL,
        "Dashboard",
    )
    version_token = make_version_token(
        get_app_version(env),
        "dashboard\n" + shell_html + "\n" + css_text + "\n" + js_text,
    )
    css_path = f"/assets/dashboard/styles.{version_token}.css"
    js_path = f"/assets/dashboard/app.{version_token}.js"
    shell_html = apply_asset_paths(shell_html, css_path, js_path)

    shell_bytes = shell_html.encode("utf-8")
    css_bytes = css_text.encode("utf-8")
    js_bytes = js_text.encode("utf-8")

    shell_payload = gzip_bytes(shell_bytes)
    css_payload = gzip_bytes(css_bytes)
    js_payload = gzip_bytes(js_bytes)
    inc_content = render_split_inc(
        "scripts/generate_dashboard_gzip.py",
        SYMBOLS,
        version_token,
        css_path,
        js_path,
        shell_payload,
        css_payload,
        js_payload,
    )
    changed = write_if_changed(OUT_INC, inc_content)

    status = "updated" if changed else "up-to-date"
    print(
        "[dashboard-gzip] "
        f"{status}: shell={len(shell_bytes)}->{len(shell_payload)} bytes, "
        f"css={len(css_bytes)}->{len(css_payload)} bytes, "
        f"js={len(js_bytes)}->{len(js_payload)} bytes"
    )


main()
