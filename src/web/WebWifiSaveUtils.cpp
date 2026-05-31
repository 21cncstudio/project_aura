// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "web/WebWifiSaveUtils.h"

#include <ctype.h>
#include <string.h>

#include "web/WebInputValidation.h"

namespace WebWifiSaveUtils {

namespace {

constexpr size_t kEnterpriseFieldMaxBytes = 64;

String trim_copy(const String &value) {
    const char *raw = value.c_str();
    if (!raw) {
        return String();
    }

    const char *start = raw;
    while (*start != '\0' && isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    const char *end = raw + strlen(raw);
    while (end > start && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    String out;
    while (start < end) {
        out += *start++;
    }
    return out;
}

String lower_copy(const String &value) {
    String trimmed = trim_copy(value);
    String out;
    out.reserve(trimmed.length());
    const char *raw = trimmed.c_str();
    if (!raw) {
        return out;
    }
    for (; *raw != '\0'; ++raw) {
        out += static_cast<char>(tolower(static_cast<unsigned char>(*raw)));
    }
    return out;
}

String normalize_pem_text(const String &value) {
    String normalized;
    normalized.reserve(value.length());

    const char *raw = value.c_str();
    if (!raw) {
        return normalized;
    }

    for (size_t i = 0; raw[i] != '\0'; ++i) {
        if (raw[i] == '\r') {
            if (raw[i + 1] == '\n') {
                continue;
            }
            normalized += '\n';
        } else {
            normalized += raw[i];
        }
    }

    return trim_copy(normalized);
}

bool is_known_auth_mode(const String &value) {
    const String normalized = lower_copy(value);
    return normalized.length() == 0 || normalized == "personal" || normalized == "enterprise";
}

bool is_known_eap_method(const String &value) {
    const String normalized = lower_copy(value);
    return normalized.length() == 0 ||
           normalized == "peap" ||
           normalized == "ttls" ||
           normalized == "tls";
}

bool is_known_ttls_phase2(const String &value) {
    const String normalized = lower_copy(value);
    return normalized.length() == 0 ||
           normalized == "mschapv2" ||
           normalized == "pap" ||
           normalized == "chap" ||
           normalized == "mschap" ||
           normalized == "eap";
}

bool isEnterpriseFieldValid(const String &value) {
    return value.length() <= kEnterpriseFieldMaxBytes &&
           !WebInputValidation::hasControlChars(value);
}

bool looksEncryptedPrivateKey(const String &value) {
    const char *raw = value.c_str();
    if (!raw) {
        return false;
    }
    return strstr(raw, "BEGIN ENCRYPTED PRIVATE KEY") != nullptr ||
           strstr(raw, "BEGIN PKCS12") != nullptr ||
           strstr(raw, "Proc-Type: 4,ENCRYPTED") != nullptr;
}

void fail(ParseResult &result, const char *message) {
    result.success = false;
    result.status_code = 400;
    result.error_message = message;
}

} // namespace

ParseResult parseSaveInput(const SaveInput &input) {
    ParseResult result{};

    if (input.ssid.length() == 0) {
        fail(result, "SSID required");
        return result;
    }

    if (!WebInputValidation::isWifiSsidValid(input.ssid, WebInputValidation::kWifiSsidMaxBytes)) {
        fail(result, "SSID must be 1-32 bytes");
        return result;
    }

    if (!is_known_auth_mode(input.auth_mode)) {
        fail(result, "Unknown WiFi auth mode");
        return result;
    }

    if (!is_known_eap_method(input.eap_method)) {
        fail(result, "Unknown EAP method");
        return result;
    }

    if (!is_known_ttls_phase2(input.ttls_phase2)) {
        fail(result, "Unknown TTLS phase 2 method");
        return result;
    }

    const Config::WifiAuthMode auth_mode = Config::parseWifiAuthMode(input.auth_mode);
    const Config::WifiEapMethod eap_method = Config::parseWifiEapMethod(input.eap_method);
    const Config::WifiTtlsPhase2 ttls_phase2 = Config::parseWifiTtlsPhase2(input.ttls_phase2);

    Config::WifiSettings settings{};
    settings.ssid = input.ssid;
    settings.enabled = true;
    settings.auth_mode = auth_mode;
    settings.eap_method = eap_method;
    settings.ttls_phase2 = ttls_phase2;

    if (auth_mode == Config::WifiAuthMode::Personal) {
        if (WebInputValidation::hasControlChars(input.pass)) {
            fail(result, "Password contains unsupported control characters");
            return result;
        }
        settings.pass = input.pass;
        result.success = true;
        result.status_code = 200;
        result.update.settings = settings;
        return result;
    }

    settings.identity = input.identity;
    settings.username = input.username;
    settings.enterprise_password = input.enterprise_password;
    settings.ca_cert_pem = normalize_pem_text(input.ca_cert_pem);
    settings.client_cert_pem = normalize_pem_text(input.client_cert_pem);
    settings.client_key_pem = normalize_pem_text(input.client_key_pem);

    if (!isEnterpriseFieldValid(settings.identity) ||
        !isEnterpriseFieldValid(settings.username) ||
        !isEnterpriseFieldValid(settings.enterprise_password)) {
        fail(result, "Enterprise identity, username, and password must be 64 bytes or less");
        return result;
    }

    if (eap_method == Config::WifiEapMethod::Peap ||
        eap_method == Config::WifiEapMethod::Ttls) {
        if (settings.username.length() == 0 || settings.enterprise_password.length() == 0) {
            fail(result, "Enterprise username and password required");
            return result;
        }
        if (settings.identity.length() == 0) {
            settings.identity = settings.username;
        }
    } else if (eap_method == Config::WifiEapMethod::Tls) {
        if (settings.identity.length() == 0) {
            fail(result, "TLS identity required");
            return result;
        }
        if (settings.client_cert_pem.length() == 0 || settings.client_key_pem.length() == 0) {
            fail(result, "TLS client certificate and private key required");
            return result;
        }
        if (looksEncryptedPrivateKey(settings.client_key_pem)) {
            fail(result, "Encrypted private keys and PKCS12/PFX files are not supported");
            return result;
        }
    }

    result.success = true;
    result.status_code = 200;
    result.update.settings = settings;
    return result;
}

ParseResult parseSaveInput(const String &ssid, const String &pass) {
    SaveInput input{};
    input.ssid = ssid;
    input.pass = pass;
    input.auth_mode = "personal";
    return parseSaveInput(input);
}

} // namespace WebWifiSaveUtils
