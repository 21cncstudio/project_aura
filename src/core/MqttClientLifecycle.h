// SPDX-FileCopyrightText: 2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace MqttClientLifecycle {

// Destroy an owned client without losing the only handle when the transport
// refuses teardown. A later poll can retry the same client; a replacement must
// not be created until this returns true.
template <typename Handle, typename Destroy>
bool destroyOwned(Handle &client, Destroy destroy) {
    if (client == nullptr) {
        return true;
    }
    if (!destroy(client)) {
        return false;
    }
    client = nullptr;
    return true;
}

} // namespace MqttClientLifecycle
