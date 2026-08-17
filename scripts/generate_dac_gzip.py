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
OUT_INC = PROJECT_DIR / "src" / "web" / "generated" / "WebTemplatesDacGzip.inc"

START_MARKER = 'static const char kDacPageTemplate[] PROGMEM = R"HTML(\n'
END_MARKER = ')HTML";'

SHELL = ShellConfig(
    shell_boot_html="""
  <div id="shellBoot" class="shell-boot" aria-live="polite">
  <div class="shell-boot-card">
    <div class="shell-head">
      <div>
        <div class="shell-kicker">Project Aura</div>
        <div class="shell-title">Fan Control</div>
        <div class="shell-sub">Loading fan state and automation controls...</div>
      </div>
      <div class="shell-chip">Live</div>
    </div>
    <div class="shell-stat-grid">
      <div class="shell-stat"></div>
      <div class="shell-stat"></div>
    </div>
    <div class="shell-panel-grid">
      <div class="shell-panel shell-panel-tall"></div>
      <div class="shell-panel"></div>
    </div>
  </div>
</div>
""".strip(),
    shell_critical_css="""
:root{color-scheme:dark;--bg:#04080b;--surface:#11181d;--surface-2:#1c282d;--border:rgba(169,199,196,.12);--text:#f9fafb;--subtle:#9ca3af}
*{box-sizing:border-box}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
body{padding:16px;display:flex;justify-content:center}
body.shell-loading .container{display:none}
.shell-boot{display:none;width:100%;max-width:560px}
body.shell-loading .shell-boot{display:block}
.shell-boot-card{width:100%;border:1px solid var(--border);border-radius:18px;padding:16px;background:rgba(17,24,29,.96);box-shadow:inset 0 1px 0 rgba(226,250,245,.04),0 10px 26px rgba(0,0,0,.18)}
.shell-head{display:flex;align-items:flex-start;justify-content:space-between;gap:16px}
.shell-kicker{font-size:10px;font-weight:800;letter-spacing:.14em;text-transform:uppercase;color:var(--subtle)}
.shell-title{margin-top:8px;font-size:27px;font-weight:800;letter-spacing:-.04em}
.shell-sub{margin-top:4px;font-size:13px;color:var(--subtle)}
.shell-chip{padding:8px 12px;border-radius:999px;border:1px solid rgba(111,201,143,.3);background:rgba(7,16,20,.72);color:#6fc98f;font-size:11px;font-weight:800;text-transform:uppercase;letter-spacing:.08em}
.shell-stat-grid,.shell-panel-grid{display:grid;gap:10px;margin-top:14px}
.shell-stat-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
.shell-panel-grid{grid-template-columns:1fr}
.shell-stat,.shell-panel{border-radius:14px;border:1px solid var(--border);background:linear-gradient(110deg,rgba(7,16,20,.96) 8%,rgba(28,40,45,.82) 18%,rgba(7,16,20,.96) 33%);background-size:200% 100%;animation:shell-shimmer 1.2s linear infinite}
.shell-stat{height:86px}
.shell-panel{height:154px}
.shell-panel-tall{height:202px}
@keyframes shell-shimmer{to{background-position-x:-200%}}
@media (max-width:480px){body{padding:12px}.shell-title{font-size:23px}.shell-head{flex-direction:column}.shell-stat-grid{grid-template-columns:1fr}}
""".strip(),
    insert_anchor='<div class="container">',
)

SYMBOLS = SplitSymbols(
    version_symbol="kDacAssetVersion",
    css_path_symbol="kDacStylesCssPath",
    js_path_symbol="kDacAppJsPath",
    shell_symbol="kDacShellHtmlGzip",
    css_symbol="kDacStylesCssGzip",
    js_symbol="kDacAppJsGzip",
)


def main() -> None:
    html = extract_template(SOURCE_HEADER, START_MARKER, END_MARKER, "DAC")
    shell_html, css_text, js_text = split_assets(
        html,
        CSS_PATH_PLACEHOLDER,
        JS_PATH_PLACEHOLDER,
        SHELL,
        "DAC",
    )
    version_token = make_version_token(
        get_app_version(env),
        "dac\n" + shell_html + "\n" + css_text + "\n" + js_text,
    )
    css_path = f"/assets/dac/styles.{version_token}.css"
    js_path = f"/assets/dac/app.{version_token}.js"
    shell_html = apply_asset_paths(shell_html, css_path, js_path)

    shell_bytes = shell_html.encode("utf-8")
    css_bytes = css_text.encode("utf-8")
    js_bytes = js_text.encode("utf-8")

    shell_payload = gzip_bytes(shell_bytes)
    css_payload = gzip_bytes(css_bytes)
    js_payload = gzip_bytes(js_bytes)
    inc_content = render_split_inc(
        "scripts/generate_dac_gzip.py",
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
        "[dac-gzip] "
        f"{status}: shell={len(shell_bytes)}->{len(shell_payload)} bytes, "
        f"css={len(css_bytes)}->{len(css_payload)} bytes, "
        f"js={len(js_bytes)}->{len(js_payload)} bytes"
    )


main()
