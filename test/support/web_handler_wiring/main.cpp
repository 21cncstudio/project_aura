// Exercise the production entry points, not copied forwarding wrappers.
#include <cstdio>
#include "web/WebHandlers.h"
#include "web/WebHandlersSupport.h"
#include "web/WebMqttHandlers.h"
#include "web/WebWifiHandlers.h"
class ConnectivityRuntime {}; // Opaque boundary: this test never calls the runtime.
class Request final : public WebRequest {
public:
 bool hasArg(const char *) const override {return false;}
 String arg(const char *) const override {return {};}
 String uri() const override {return {};}
 void sendHeader(const char *,const String &,bool=false) override {}
 void send(int,const char *,const String &) override {}
 void send(int,const char *,const char *) override {}
 bool clientConnected() const override {return true;}
 void setUploadDeadlineMs(uint32_t) override {}
 void clearUploadDeadline() override {}
 void rejectUpload() override {}
 bool uploadRejected() const override {return false;}
 size_t pendingRequestBodyBytes() const override {return 0;}
 size_t drainPendingRequestBody(size_t,uint32_t) override {return 0;}
 void stopClient() override {}
 bool beginStreamResponse(int,const char *,size_t,bool=false) override {return true;}
 int32_t writeStreamChunk(const uint8_t *,size_t n,int &) override {return n;}
 bool waitUntilWritable(uint16_t,int &) override {return true;}
 void endStreamResponse() override {}
 void streamRequestInfo(WebStreamRequestInfo &) const override {}
 WebUpload upload() override {return {};}
};
static Request request;
static WebUiBridge ui;
static ConnectivityRuntime connectivity;
static WebHandlerContext ctx;
static WebHandlerContext *context_ptr=&ctx;
static WebDeferredActionsState deferred;
static unsigned random_calls=0,mqtt_calls=0,wifi_calls=0,response_records=0,failures=0;
static uint32_t mqtt_delay=0;
static uint64_t last_receipt=0;
uint32_t esp_random() { return 0xaabbcc00U + ++random_calls; }
namespace WebHandlersSupport {
WebHandlerContext *context() { return context_ptr; }
bool isOtaBusy() { return false; }
WebDeferredActionsState &deferredActions() { return deferred; }
WebResponseUtils::StreamContext responseContext() { return {}; }
}
namespace WebResponseUtils { void recordSimpleResponseFailure(WebRequest &,const StreamContext &) {++response_records;} }
namespace WebMqttHandlers {
void handleSave(WebHandlerContext &,bool,uint32_t delay,WebDeferredActionsState &state,const WebResponseUtils::StreamContext &) {
 ++mqtt_calls; mqtt_delay=delay; state.scheduleMqttSync(1000,delay);
}
}
namespace WebWifiHandlers {
void handleSave(WebHandlerContext &,bool,uint64_t receipt,WebDeferredActionsState &,const WebResponseUtils::StreamContext &) {
 ++wifi_calls;last_receipt=receipt;
}
}
static void check(bool ok,const char *text) { printf("[%s] %s\n",ok?"PASS":"FAIL",text);if(!ok)++failures; }
int main() {
 ctx.server=&request;ctx.web_ui_bridge=&ui;ctx.connectivity_runtime=&connectivity;
 mqtt_handle_save();
 check(mqtt_calls==1 && random_calls==0,"MQTT save does not consume Wi-Fi receipt randomness");
 check(mqtt_delay==200,"MQTT save forwards its 200 ms delay");
 check(!deferred.pollDue(1199).mqtt_sync,"MQTT remains deferred before 200 ms");
 check(deferred.pollDue(1200).mqtt_sync,"MQTT sync becomes due after 200 ms");
 wifi_handle_save();uint64_t first=last_receipt;wifi_handle_save();
 check(wifi_calls==2 && first && last_receipt && first!=last_receipt,"Wi-Fi entry point supplies a fresh nonzero receipt per save");
 check(random_calls==4,"Wi-Fi receipt samples both halves for each save");
 mqtt_handle_save();check(mqtt_delay==200 && random_calls==4,"Wi-Fi receipt generation cannot change later MQTT scheduling");
 check(response_records==4,"production entry points forward simple-response diagnostics after each handler");
 context_ptr=nullptr;mqtt_handle_save();wifi_handle_save();
 check(mqtt_calls==2 && wifi_calls==2 && random_calls==4 && response_records==4,"missing context invokes neither downstream handler");
 return failures?1:0;
}
