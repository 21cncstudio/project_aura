// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiController.h"

#include <ctype.h>
#include <string.h>

#include "config/AppConfig.h"
#include "core/Logger.h"
#include "ui/ui.h"

namespace {

constexpr uint32_t kAuraLinkLinkedHoldMs = 900;
constexpr const char *kAuraLinkComingSoonQrText = "Coming soon";

const char *claim_status_text(AuraLinkManager::ClaimStatus status) {
    switch (status) {
        case AuraLinkManager::ClaimStatus::Pending:
            return "Activating...";
        case AuraLinkManager::ClaimStatus::Success:
            return "Linked.";
        case AuraLinkManager::ClaimStatus::InvalidCode:
            return "Invalid code.";
        case AuraLinkManager::ClaimStatus::Expired:
            return "Code expired.";
        case AuraLinkManager::ClaimStatus::AlreadyUsed:
            return "Already used.";
        case AuraLinkManager::ClaimStatus::ServerUnreachable:
            return "Server unreachable.";
        case AuraLinkManager::ClaimStatus::NotConfigured:
            return "Link server is not configured.";
        case AuraLinkManager::ClaimStatus::Idle:
        default:
            return nullptr;
    }
}

bool is_claim_error(AuraLinkManager::ClaimStatus status) {
    switch (status) {
        case AuraLinkManager::ClaimStatus::InvalidCode:
        case AuraLinkManager::ClaimStatus::Expired:
        case AuraLinkManager::ClaimStatus::AlreadyUsed:
        case AuraLinkManager::ClaimStatus::ServerUnreachable:
            return true;
        default:
            return false;
    }
}

void format_interval(uint32_t seconds, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (seconds == 0) {
        snprintf(out, out_size, "-");
    } else if (seconds % 60 == 0) {
        snprintf(out, out_size, "%lu min", static_cast<unsigned long>(seconds / 60));
    } else {
        snprintf(out, out_size, "%lu sec", static_cast<unsigned long>(seconds));
    }
}

void format_last_upload(uint32_t now_ms, uint32_t last_ms, char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return;
    }
    if (last_ms == 0) {
        snprintf(out, out_size, "Never");
        return;
    }
    const uint32_t age_s = (now_ms - last_ms) / 1000UL;
    if (age_s < 60) {
        snprintf(out, out_size, "Just now");
    } else if (age_s < 3600) {
        snprintf(out, out_size, "%lu min ago", static_cast<unsigned long>(age_s / 60));
    } else {
        snprintf(out, out_size, "%lu h ago", static_cast<unsigned long>(age_s / 3600));
    }
}

} // namespace

void UiController::update_aura_link_ui() {
    if (!objects.page_aura_aq_link) {
        return;
    }

    const uint32_t now = millis();
    if (aura_link_close_modal_at_ms_ != 0 &&
        static_cast<int32_t>(now - aura_link_close_modal_at_ms_) >= 0) {
        aura_link_close_modal_at_ms_ = 0;
        set_aura_link_modal(AuraLinkModal::None);
        auraLinkManager.clearClaimStatus();
    }

    const AuraLinkManager::Snapshot snapshot = auraLinkManager.snapshot();
    const bool linked = snapshot.linked;
    const bool pending = snapshot.claim_status == AuraLinkManager::ClaimStatus::Pending;
    const bool claim_error = is_claim_error(snapshot.claim_status);
    lv_color_t status_color = color_yellow();
    if (pending) {
        status_color = color_blue();
    } else if (claim_error) {
        status_color = color_red();
    } else if (linked && !snapshot.upload_paused) {
        status_color = color_green();
    }

    if (objects.label_aura_aq_link_status_value) {
        const char *status = "Off";
        if (!snapshot.configured) {
            status = "Not configured";
        } else if (pending) {
            status = "Activating...";
        } else if (linked && snapshot.upload_paused) {
            status = "Upload paused";
        } else if (linked) {
            status = "Linked";
        }
        safe_label_set_text(objects.label_aura_aq_link_status_value, status);
    }

    if (objects.container_aura_aq_link_status) {
        apply_toggle_style(objects.container_aura_aq_link_status);
        set_chip_color(objects.container_aura_aq_link_status, status_color);
        if (linked && !snapshot.upload_paused && !claim_error) {
            lv_obj_add_state(objects.container_aura_aq_link_status, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.container_aura_aq_link_status, LV_STATE_CHECKED);
        }
    }

    if (objects.label_aura_aq_link_last_upload_value) {
        char buf[32];
        format_last_upload(now, snapshot.last_upload_success_ms, buf, sizeof(buf));
        safe_label_set_text(objects.label_aura_aq_link_last_upload_value, buf);
    }

    if (objects.label_aura_aq_link_upload_interval_value) {
        char buf[24];
        format_interval(snapshot.upload_interval_seconds, buf, sizeof(buf));
        safe_label_set_text(objects.label_aura_aq_link_upload_interval_value, buf);
    }

    const bool can_activate = snapshot.configured && !linked && !pending;
    const bool can_reset = linked && !pending;
    set_button_enabled(objects.btn_aura_aq_link_activate, can_activate);
    set_button_enabled(objects.label_btn_aura_aq_link_activate, can_activate);
    set_button_enabled(objects.btn_aura_aq_link_reset, can_reset);
    set_button_enabled(objects.label_btn_aura_aq_link_reset, can_reset);

    if (objects.qrcode_aura_aq_link_portal) {
        lv_obj_clear_flag(objects.qrcode_aura_aq_link_portal, LV_OBJ_FLAG_HIDDEN);
        update_qrcode_if_needed(objects.qrcode_aura_aq_link_portal,
                                kAuraLinkComingSoonQrText,
                                aura_link_qr_cache_,
                                sizeof(aura_link_qr_cache_));
    }

    if (aura_link_modal_ == AuraLinkModal::Pairing) {
        update_aura_link_pairing_ui();
    }

    aura_link_last_ui_update_ms_ = now;
}

void UiController::set_aura_link_modal(AuraLinkModal modal) {
    aura_link_modal_ = modal;
    const bool pairing_visible = modal == AuraLinkModal::Pairing;
    const bool reset_visible = modal == AuraLinkModal::Reset;
    set_visible(objects.container_aura_aq_link_main, modal == AuraLinkModal::None);
    set_visible(objects.container_aura_aq_link_pairing_modal, pairing_visible);
    set_visible(objects.container_aura_aq_link_reset_modal, reset_visible);
    if (objects.container_aura_aq_link_pairing_modal && pairing_visible) {
        lv_obj_add_flag(objects.container_aura_aq_link_pairing_modal, LV_OBJ_FLAG_CLICKABLE);
    }
    if (objects.container_aura_aq_link_reset_modal && reset_visible) {
        lv_obj_add_flag(objects.container_aura_aq_link_reset_modal, LV_OBJ_FLAG_CLICKABLE);
    }
}

void UiController::reset_aura_link_pairing_code() {
    memset(aura_link_pairing_code_, 0, sizeof(aura_link_pairing_code_));
    aura_link_pairing_len_ = 0;
    aura_link_close_modal_at_ms_ = 0;
    auraLinkManager.clearClaimStatus();
    aura_link_last_claim_status_ = AuraLinkManager::ClaimStatus::Idle;
    aura_link_claim_was_pending_ = false;
    update_aura_link_pairing_ui();
}

void UiController::update_aura_link_pairing_ui() {
    const AuraLinkManager::Snapshot snapshot = auraLinkManager.snapshot();
    const bool pending = snapshot.claim_status == AuraLinkManager::ClaimStatus::Pending;

    if (objects.label_aura_aq_link_pairing_code) {
        char code_display[7] = "------";
        for (uint8_t i = 0; i < aura_link_pairing_len_ && i < 6; ++i) {
            code_display[i] = aura_link_pairing_code_[i];
        }
        safe_label_set_text(objects.label_aura_aq_link_pairing_code, code_display);
    }

    const char *status = claim_status_text(snapshot.claim_status);
    if (!status) {
        status = !snapshot.configured
                     ? "Link server is not configured."
                     : (aura_link_pairing_len_ == 6
                     ? "Ready to activate."
                     : (aura_link_pairing_len_ == 0
                            ? "Enter the 6-digit pairing code."
                            : "Enter 6 digits to continue."));
    }
    if (objects.label_aura_aq_link_pairing_status) {
        safe_label_set_text(objects.label_aura_aq_link_pairing_status, status);
    }

    set_button_enabled(objects.btn_aura_aq_link_pairing_activate,
                       snapshot.configured && aura_link_pairing_len_ == 6 && !pending);
    set_button_enabled(objects.btn_aura_aq_link_pairing_cancel, !pending);
    set_button_enabled(objects.btnmatrix_aura_aq_link_pairing_keypad, !pending);

    if (aura_link_claim_was_pending_ &&
        snapshot.claim_status == AuraLinkManager::ClaimStatus::Success &&
        aura_link_close_modal_at_ms_ == 0) {
        aura_link_close_modal_at_ms_ = millis() + kAuraLinkLinkedHoldMs;
    }
    aura_link_claim_was_pending_ = pending;
    aura_link_last_claim_status_ = snapshot.claim_status;
}

void UiController::append_aura_link_pairing_digit(char digit) {
    if (!isdigit(static_cast<unsigned char>(digit)) || aura_link_pairing_len_ >= 6) {
        return;
    }
    aura_link_pairing_code_[aura_link_pairing_len_++] = digit;
    aura_link_pairing_code_[aura_link_pairing_len_] = '\0';
    auraLinkManager.clearClaimStatus();
    update_aura_link_pairing_ui();
}

void UiController::delete_aura_link_pairing_digit() {
    if (aura_link_pairing_len_ == 0) {
        return;
    }
    aura_link_pairing_code_[--aura_link_pairing_len_] = '\0';
    auraLinkManager.clearClaimStatus();
    update_aura_link_pairing_ui();
}

void UiController::start_aura_link_claim() {
    if (aura_link_pairing_len_ != 6) {
        update_aura_link_pairing_ui();
        return;
    }
    if (!auraLinkManager.claim(aura_link_pairing_code_)) {
        update_aura_link_pairing_ui();
        return;
    }
    aura_link_claim_was_pending_ = true;
    update_aura_link_pairing_ui();
}

void UiController::on_aura_aq_link_settings_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    auraLinkManager.clearClaimStatus();
    set_aura_link_modal(AuraLinkModal::None);
    pending_screen_id = SCREEN_ID_PAGE_AURA_AQ_LINK;
}

void UiController::on_aura_aq_link_back_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    set_aura_link_modal(AuraLinkModal::None);
    pending_screen_id = SCREEN_ID_PAGE_SETTINGS;
}

void UiController::on_aura_aq_link_activate_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    reset_aura_link_pairing_code();
    set_aura_link_modal(AuraLinkModal::Pairing);
}

void UiController::on_aura_aq_link_reset_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    set_aura_link_modal(AuraLinkModal::Reset);
}

void UiController::on_aura_aq_link_pairing_activate_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    start_aura_link_claim();
}

void UiController::on_aura_aq_link_pairing_cancel_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (auraLinkManager.snapshot().claim_status == AuraLinkManager::ClaimStatus::Pending) {
        return;
    }
    set_aura_link_modal(AuraLinkModal::None);
    reset_aura_link_pairing_code();
}

void UiController::on_aura_aq_link_reset_confirm_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    auraLinkManager.reset();
    set_aura_link_modal(AuraLinkModal::None);
    update_aura_link_ui();
}

void UiController::on_aura_aq_link_reset_cancel_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    set_aura_link_modal(AuraLinkModal::None);
}

void UiController::on_aura_aq_link_keypad_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (auraLinkManager.snapshot().claim_status == AuraLinkManager::ClaimStatus::Pending) {
        return;
    }
    lv_obj_t *matrix = lv_event_get_target(e);
    const uint16_t selected = lv_btnmatrix_get_selected_btn(matrix);
    const char *text = lv_btnmatrix_get_btn_text(matrix, selected);
    if (!text || text[0] == '\0') {
        return;
    }
    if (isdigit(static_cast<unsigned char>(text[0]))) {
        append_aura_link_pairing_digit(text[0]);
        return;
    }
    if (text[0] == 'C' || text[0] == 'c') {
        reset_aura_link_pairing_code();
        return;
    }
    delete_aura_link_pairing_digit();
}
