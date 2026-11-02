#pragma once
class NetworkCommandQueue { public: enum class Type { RequestWifiScanStart, RequestWifiScanStop };
    bool enqueue(Type) { return true; } bool publishWifiScanRequest(Type) { return true; } };
