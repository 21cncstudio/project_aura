"""Keep Arduino's first STA retry subject to setAutoReconnect(false).

Generate a build-local source override; never modify managed components.
Fail configuration if the upstream implementation changes under this patch.
"""

import argparse
from pathlib import Path


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
    if source.count(BEFORE) != 1 or AFTER in source:
        raise ValueError("Arduino STA first-retry implementation changed; review the Aura overlay")
    return source.replace(BEFORE, AFTER, 1)


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
