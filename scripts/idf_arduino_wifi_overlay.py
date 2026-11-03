"""Keep Arduino's first STA retry and failure status subject to auto-reconnect.

Generate a build-local source override; never modify managed components.
Fail configuration if the upstream implementation changes under this patch.
"""

import argparse
from pathlib import Path


STATUS_BEFORE = """    } else if ((reason == WIFI_REASON_AUTH_FAIL) && !first_connect) {
"""
STATUS_AFTER = """    } else if ((reason == WIFI_REASON_AUTH_FAIL) && (!first_connect || !_sta_network_if->getAutoReconnect())) {
"""


BEFORE = """    } else if (first_connect) {               //Retry once for all failure reasons
      first_connect = false;
      DoReconnect = true;
      log_d("WiFi Reconnect Running");
"""
AFTER = """    } else if (first_connect) {               //Retry once only when auto-reconnect is enabled
      first_connect = false;
      DoReconnect = _sta_network_if->getAutoReconnect();
      if (DoReconnect) {
        log_d("WiFi Reconnect Running");
      }
"""


def adapt(source: str) -> str:
    if (source.count(BEFORE) != 1 or AFTER in source or
            source.count(STATUS_BEFORE) != 1 or STATUS_AFTER in source):
        raise ValueError("Arduino STA first-retry implementation changed; review the Aura overlay")
    # Without the hidden first retry, AUTH_FAIL is terminal on the first attempt
    # too. Let the application owner schedule its bounded retry immediately.
    # With auto-reconnect enabled preserve upstream's first-attempt behavior.
    return source.replace(BEFORE, AFTER, 1).replace(STATUS_BEFORE, STATUS_AFTER, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--destination", required=True, type=Path)
    args = parser.parse_args()
    result = adapt(args.source.read_text(encoding="utf-8"))
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    if not args.destination.exists() or args.destination.read_text(encoding="utf-8") != result:
        args.destination.write_text(result, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
