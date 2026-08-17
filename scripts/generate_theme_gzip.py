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
SOURCE_HEADER = PROJECT_DIR / "src" / "web" / "WebTemplates.h"
OUT_INC = PROJECT_DIR / "src" / "web" / "generated" / "WebTemplatesThemeGzip.inc"

START_MARKER = 'static const char kThemePageTemplate[] PROGMEM = R"HTML(\n'
END_MARKER = ')HTML";'

SHELL = ShellConfig(
    shell_boot_html="""
<div id="shellBoot" class="shell-boot" aria-live="polite">
  <div class="shell-boot-card">
    <div class="shell-kicker">Project Aura</div>
    <div class="shell-title">Theme Studio</div>
    <div class="shell-sub">Loading current colors and controls...</div>
    <div class="shell-preview">
      <div class="shell-preview-card"></div>
      <div class="shell-preview-meta">
        <div class="shell-line shell-line-strong"></div>
        <div class="shell-line"></div>
        <div class="shell-line"></div>
      </div>
    </div>
  </div>
</div>
""".strip(),
    shell_critical_css="""
:root{color-scheme:dark;--bg:#04080b;--surface:#11181d;--surface-2:#1c282d;--border:rgba(169,199,196,.12);--text:#f9fafb;--subtle:#9ca3af;--teal:#2bb4bd}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
body{min-height:100vh}
body.shell-loading .main-container,body.shell-loading .toast-notification,body.shell-loading .aura{display:none}
.app-viewport{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:18px}
.shell-boot{display:none;width:100%;max-width:920px}
body.shell-loading .shell-boot{display:block}
.shell-boot-card{border:1px solid var(--border);border-radius:28px;padding:24px;background:rgba(17,24,29,.96);box-shadow:inset 0 1px 0 rgba(226,250,245,.04),0 10px 26px rgba(0,0,0,.18)}
.shell-kicker{font-size:11px;font-weight:800;letter-spacing:.14em;text-transform:uppercase;color:var(--subtle)}
.shell-title{margin-top:10px;font-size:30px;font-weight:800;letter-spacing:-.04em}
.shell-sub{margin-top:6px;color:var(--subtle);font-size:14px}
.shell-preview{display:grid;grid-template-columns:minmax(240px,320px) 1fr;gap:18px;margin-top:22px;align-items:center}
.shell-preview-card{min-height:220px;border-radius:22px;border:1px solid var(--border);background:linear-gradient(135deg,rgba(43,180,189,.5),rgba(7,16,20,.94) 58%,rgba(28,40,45,.94));position:relative;overflow:hidden}
.shell-preview-card::after,.shell-line{content:"";display:block;background:linear-gradient(110deg,rgba(17,24,29,.94) 8%,rgba(28,40,45,.82) 18%,rgba(17,24,29,.94) 33%);background-size:200% 100%;animation:shell-shimmer 1.2s linear infinite}
.shell-preview-card::after{position:absolute;inset:18px;border-radius:18px}
.shell-preview-meta{display:flex;flex-direction:column;gap:12px}
.shell-line{height:16px;border-radius:999px}
.shell-line-strong{height:22px;width:68%}
@keyframes shell-shimmer{to{background-position-x:-200%}}
@media (max-width:760px){.app-viewport{padding:14px}.shell-boot-card{padding:20px;border-radius:24px}.shell-title{font-size:25px}.shell-preview{grid-template-columns:1fr}.shell-preview-card{min-height:180px}}
""".strip(),
    insert_anchor='<div class="main-container">',
)

SYMBOLS = SplitSymbols(
    version_symbol="kThemeAssetVersion",
    css_path_symbol="kThemeStylesCssPath",
    js_path_symbol="kThemeAppJsPath",
    shell_symbol="kThemeShellHtmlGzip",
    css_symbol="kThemeStylesCssGzip",
    js_symbol="kThemeAppJsGzip",
)


def main() -> None:
    html = extract_template(SOURCE_HEADER, START_MARKER, END_MARKER, "Theme")
    shell_html, css_text, js_text = split_assets(
        html,
        CSS_PATH_PLACEHOLDER,
        JS_PATH_PLACEHOLDER,
        SHELL,
        "Theme",
    )
    version_token = make_version_token(
        get_app_version(env),
        "theme\n" + shell_html + "\n" + css_text + "\n" + js_text,
    )
    css_path = f"/assets/theme/styles.{version_token}.css"
    js_path = f"/assets/theme/app.{version_token}.js"
    shell_html = apply_asset_paths(shell_html, css_path, js_path)

    shell_bytes = shell_html.encode("utf-8")
    css_bytes = css_text.encode("utf-8")
    js_bytes = js_text.encode("utf-8")

    shell_payload = gzip_bytes(shell_bytes)
    css_payload = gzip_bytes(css_bytes)
    js_payload = gzip_bytes(js_bytes)
    inc_content = render_split_inc(
        "scripts/generate_theme_gzip.py",
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
        "[theme-gzip] "
        f"{status}: shell={len(shell_bytes)}->{len(shell_payload)} bytes, "
        f"css={len(css_bytes)}->{len(css_payload)} bytes, "
        f"js={len(js_bytes)}->{len(js_payload)} bytes"
    )


main()
