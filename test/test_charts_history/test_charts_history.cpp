#include <unity.h>
#include <atomic>
#include <thread>
#include <math.h>
#include "ArduinoMock.h"
#include "TimeMock.h"
#include "core/ChartsRuntimeState.h"
#include "modules/ChartsHistory.h"
#include "modules/StorageManager.h"
#include "web/WebChartsApiUtils.h"

constexpr uint32_t base_epoch = 1767225600; // UTC hour boundary
constexpr auto TEMP = ChartsHistory::METRIC_TEMPERATURE;
constexpr auto CO2 = ChartsHistory::METRIC_CO2;
constexpr auto GAS = ChartsHistory::METRIC_OPTIONAL_GAS;
constexpr uint16_t temp_mask = ChartsHistory::metricBit(TEMP);
void at(uint32_t s) { setMillis(s * 1000); setNowEpoch(base_epoch + s); }
void setUp() {
    at(0); ChartsHistory::setNowEpochFn(&mockNow);
    StorageManager::setTestForceSaveFailure(false);
    ChartsRuntimeState::setSnapshotCopyHook(nullptr);
    FreeRtosSemaphoreMock::resetBlockedTakeHook();
}
void tearDown() { StorageManager::setTestForceSaveFailure(false); ChartsHistory::setNowEpochFn(nullptr); }
SensorData temp(float v) { SensorData d; d.temperature=v; d.temp_valid=true; return d; }
void observe(ChartsHistory &h, StorageManager &s, uint32_t second, float value, uint16_t mask=temp_mask) {
    at(second); h.update(temp(value),s,false,true,mask);
}
ChartsHistory::Entry entry(ChartsHistory &h, uint16_t n=0) {
    ChartsHistory::Entry e; TEST_ASSERT_TRUE(h.entryFromOldest(n,e)); return e;
}
class View : public WebChartsApiUtils::HistoryView {
public:
    explicit View(const ChartsRuntimeState::Snapshot &s): s_(s) {}
    uint16_t count() const override { return s_.count(); }
    uint32_t latestEpoch() const override { return s_.latestEpoch(); }
    bool latestMetric(ChartsHistory::Metric m,float &v) const override { return s_.latestMetric(m,v); }
    bool entryFromOldest(uint16_t n,ChartsHistory::Entry &e) const override { return s_.entryFromOldest(n,e); }
    bool metricValueFromOldest(uint16_t n,ChartsHistory::Metric m,float &v,bool &b) const override { return s_.metricValueFromOldest(n,m,v,b); }
private: const ChartsRuntimeState::Snapshot &s_;
};
void test_summary_counts_new_equal_values_and_preserves_peak() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    observe(h,s,0,20); observe(h,s,10,20); observe(h,s,20,80);
    for(uint32_t n=21;n<300;++n) observe(h,s,n,999,0); // repeated cached data is not acquired
    observe(h,s,300,10);
    TEST_ASSERT_EQUAL(1,h.count()); const auto e=entry(h);
    TEST_ASSERT_FLOAT_WITHIN(.001,40,e.values[TEMP]);
    TEST_ASSERT_FLOAT_WITHIN(.001,20,e.minimum[TEMP]); TEST_ASSERT_FLOAT_WITHIN(.001,80,e.maximum[TEMP]);
    TEST_ASSERT_EQUAL(3,e.samples[TEMP]); TEST_ASSERT_EQUAL(0,e.first_second[TEMP]); TEST_ASSERT_EQUAL(20,e.last_second[TEMP]);
    TEST_ASSERT_TRUE(e.persisted); TEST_ASSERT_EQUAL(base_epoch,e.start_epoch);
    observe(h,s,600,0,0); TEST_ASSERT_FLOAT_WITHIN(.001,10,entry(h,1).values[TEMP]);
}
void test_validity_warmup_and_metric_counts_are_independent() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    SensorData d=temp(20); d.co2_valid=true; d.co2=700; d.voc_valid=true; d.voc_index=40;
    h.update(d,s,true,true,0x3fff);
    at(10); d.temperature=NAN; d.co2=900; h.update(d,s,false,true,ChartsHistory::metricBit(CO2)|temp_mask);
    at(300); h.update(d,s,false,true,0);
    auto e=entry(h); TEST_ASSERT_EQUAL(1,e.samples[TEMP]); TEST_ASSERT_EQUAL(2,e.samples[CO2]);
    TEST_ASSERT_FLOAT_WITHIN(.001,800,e.values[CO2]); TEST_ASSERT_EQUAL(0,e.samples[ChartsHistory::METRIC_VOC]);
}
void test_gaps_are_not_pressure_measurements() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    SensorData d=temp(20); d.pressure_valid=true; d.pressure=1000;
    h.update(d,s,false,true,0x3fff); at(1200); h.update(d,s,false,true,0);
    TEST_ASSERT_EQUAL(4,h.count());
    for(int n=1;n<4;++n) { auto e=entry(h,n); TEST_ASSERT_EQUAL(ChartsHistory::Gap,e.kind); TEST_ASSERT_EQUAL(0,e.valid_mask); }
}
void test_save_retry_and_reboot_do_not_claim_unsaved_records() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s); observe(h,s,0,22);
    StorageManager::setTestForceSaveFailure(true); observe(h,s,300,0,0);
    TEST_ASSERT_FALSE(entry(h).persisted); ChartsHistory reboot; reboot.load(s); TEST_ASSERT_EQUAL(0,reboot.count());
    StorageManager::setTestForceSaveFailure(false); observe(h,s,311,0,0);
    TEST_ASSERT_TRUE(entry(h).persisted); reboot.load(s); TEST_ASSERT_EQUAL(1,reboot.count());
    TEST_ASSERT_FLOAT_WITHIN(.001,22,entry(reboot).values[TEMP]);
    at(312); reboot.update(temp(24),s,false,true,temp_mask); observe(reboot,s,600,0,0);
    TEST_ASSERT_EQUAL(2,reboot.count()); TEST_ASSERT_EQUAL(1,entry(reboot,1).samples[TEMP]);
    TEST_ASSERT_EQUAL(12,entry(reboot,1).first_second[TEMP]);
}
void test_retention_ring_and_blocks_restore_last_day() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    for(uint32_t n=0;n<=300;++n) observe(h,s,n*300,static_cast<float>(n));
    TEST_ASSERT_EQUAL(288,h.count()); TEST_ASSERT_EQUAL(base_epoch+12*300,entry(h).start_epoch);
    ChartsHistory r; r.load(s); TEST_ASSERT_EQUAL(288,r.count());
    TEST_ASSERT_EQUAL(base_epoch+299*300,r.latestEpoch()); TEST_ASSERT_EQUAL(base_epoch+12*300,entry(r).start_epoch);
    TEST_ASSERT_FLOAT_WITHIN(.001,299,entry(r,287).values[TEMP]);
}
void test_clock_rollback_never_rewrites_completed_summary() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    observe(h,s,0,20); observe(h,s,300,30); observe(h,s,310,40);
    observe(h,s,250,999); TEST_ASSERT_EQUAL(1,h.count());
    observe(h,s,315,50); observe(h,s,600,0,0);
    TEST_ASSERT_EQUAL(2,h.count()); TEST_ASSERT_FLOAT_WITHIN(.001,20,entry(h).values[TEMP]);
    TEST_ASSERT_FLOAT_WITHIN(.001,50,entry(h,1).values[TEMP]);
}
void test_untrusted_time_keeps_ram_history_out_of_export() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    h.update(temp(20),s,false,false,temp_mask); at(300); h.update(temp(30),s,false,false,temp_mask);
    TEST_ASSERT_EQUAL(1,h.count()); TEST_ASSERT_EQUAL(0,entry(h).start_epoch); TEST_ASSERT_FALSE(entry(h).persisted);
    at(301); h.update(temp(40),s,false,true,temp_mask); observe(h,s,600,0,0);
    TEST_ASSERT_EQUAL(1,h.count()); TEST_ASSERT_EQUAL(base_epoch+300,entry(h).start_epoch);
}
struct Legacy {
    uint32_t magic=0x43524849; uint16_t version=2,reserved=0; uint8_t gas=0,reserved2=0;
    uint32_t epoch=base_epoch-10; uint16_t index=1,count=1;
    uint16_t masks[288]{}; float values[14][288]{};
};
void test_legacy_snapshot_is_preserved_without_invented_statistics() {
    StorageManager s; s.begin(); Legacy old; old.masks[0]=temp_mask; old.values[TEMP][0]=19;
    TEST_ASSERT_TRUE(s.saveBlobAtomic(StorageManager::kChartsPath,&old,sizeof(old)));
    ChartsHistory h; h.load(s); auto e=entry(h);
    TEST_ASSERT_EQUAL(ChartsHistory::LegacySnapshot,e.kind); TEST_ASSERT_EQUAL(old.epoch,e.start_epoch);
    TEST_ASSERT_FLOAT_WITHIN(.001,19,e.values[TEMP]); TEST_ASSERT_EQUAL(0,e.samples[TEMP]);
    h.update(temp(500),s,false,false,temp_mask); TEST_ASSERT_EQUAL(1,h.count());
    observe(h,s,0,20); observe(h,s,300,0,0); TEST_ASSERT_EQUAL(2,h.count());
    ChartsHistory r; r.load(s); TEST_ASSERT_EQUAL(2,r.count());
}
void test_optional_gas_keeps_record_identity_and_filters_current_chart() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s); SensorData d;
    d.optional_gas_sensor_present=d.optional_gas_valid=true; d.optional_gas_type=1; d.optional_gas_ppm=12;
    h.update(d,s,false,true,ChartsHistory::metricBit(GAS)); at(300); h.update(d,s,false,true,0);
    d.optional_gas_type=5; d.optional_gas_ppm=.1; at(301); h.update(d,s,false,true,ChartsHistory::metricBit(GAS));
    at(600); h.update(d,s,false,true,0);
    auto old=entry(h); TEST_ASSERT_EQUAL(1,old.optional_gas_type); TEST_ASSERT_EQUAL(1,old.samples[GAS]);
    float v; bool valid; h.metricValueFromOldest(0,GAS,v,valid); TEST_ASSERT_FALSE(valid);
    h.metricValueFromOldest(1,GAS,v,valid); TEST_ASSERT_TRUE(valid); TEST_ASSERT_FLOAT_WITHIN(.001,.1,v);
}
void test_clear_removes_legacy_and_segment_history() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    observe(h,s,0,20); observe(h,s,300,30); h.clear(s);
    TEST_ASSERT_EQUAL(0,h.count()); ChartsHistory r; r.load(s); TEST_ASSERT_EQUAL(0,r.count());
}
void test_chart_and_bounded_export_share_exact_summary_values() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    for(uint32_t n=0;n<=12;++n) observe(h,s,n*300,20+n);
    ChartsRuntimeState runtime; runtime.update(h); auto snap=runtime.copySnapshot(); TEST_ASSERT_NOT_NULL(snap.get());
    View view(*snap); ArduinoJson::JsonDocument chart, batch;
    WebChartsApiUtils::fillJson(chart.to<ArduinoJson::JsonObject>(),view,"1h","core","ppm",true);
    WebChartsApiUtils::fillHistoryJson(batch.to<ArduinoJson::JsonObject>(),view);
    TEST_ASSERT_EQUAL(8,batch["records"].size()); TEST_ASSERT_TRUE(batch["has_more"].as<bool>());
    TEST_ASSERT_FLOAT_WITHIN(.001,20,batch["records"][0]["metrics"]["1"]["mean"].as<float>());
    TEST_ASSERT_FLOAT_WITHIN(.001,20,chart["series"][1]["values"][0].as<float>());
    TEST_ASSERT_EQUAL(1,chart["series"][1]["count"][0].as<int>());
    auto cursor=batch["next_after"].as<uint32_t>(); batch.clear();
    WebChartsApiUtils::fillHistoryJson(batch.to<ArduinoJson::JsonObject>(),view,cursor);
    TEST_ASSERT_EQUAL(4,batch["records"].size()); TEST_ASSERT_FALSE(batch["has_more"].as<bool>());
}
std::atomic<bool> copying{false}, waiting{false};
void blocked(SemaphoreHandle_t) { waiting=true; }
void copying_hook() { copying=true; while(!waiting.load()) std::this_thread::yield(); }
void test_snapshot_remains_one_generation_during_concurrent_update() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s); observe(h,s,0,20); observe(h,s,300,30);
    ChartsRuntimeState runtime; runtime.update(h); observe(h,s,600,0,0);
    copying=false; waiting=false; FreeRtosSemaphoreMock::setBlockedTakeHook(&blocked);
    ChartsRuntimeState::setSnapshotCopyHook(&copying_hook);
    std::thread writer([&] { while(!copying.load()) std::this_thread::yield(); runtime.update(h); });
    auto old=runtime.copySnapshot(); writer.join();
    ChartsRuntimeState::setSnapshotCopyHook(nullptr); FreeRtosSemaphoreMock::resetBlockedTakeHook();
    TEST_ASSERT_EQUAL(1,old->count()); auto current=runtime.copySnapshot(); TEST_ASSERT_EQUAL(2,current->count());
    ChartsHistory::Entry e; old->entryFromOldest(0,e); TEST_ASSERT_FLOAT_WITHIN(.001,20,e.values[TEMP]);
}

void test_export_waits_for_durable_save_and_does_not_advance_cursor() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    observe(h,s,0,20); StorageManager::setTestForceSaveFailure(true); observe(h,s,300,0,0);
    ChartsRuntimeState runtime; runtime.update(h); auto snapshot=runtime.copySnapshot(); View view(*snapshot);
    ArduinoJson::JsonDocument batch; WebChartsApiUtils::fillHistoryJson(batch.to<ArduinoJson::JsonObject>(),view);
    TEST_ASSERT_EQUAL(0,batch["records"].size()); TEST_ASSERT_EQUAL(0,batch["next_after"].as<uint32_t>());
    TEST_ASSERT_TRUE(batch["has_more"].as<bool>());
    StorageManager::setTestForceSaveFailure(false); observe(h,s,311,0,0); runtime.update(h);
    auto saved=runtime.copySnapshot(); View saved_view(*saved); batch.clear();
    WebChartsApiUtils::fillHistoryJson(batch.to<ArduinoJson::JsonObject>(),saved_view);
    TEST_ASSERT_EQUAL(1,batch["records"].size());
}
void test_retention_boundary_stays_stable_between_completed_intervals() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    for(uint32_t n=0;n<=288;++n) observe(h,s,n*300,20);
    observe(h,s,288*300+1,20); TEST_ASSERT_EQUAL(288,h.count());
    TEST_ASSERT_EQUAL(base_epoch,entry(h).start_epoch);
}
void test_clear_all_removes_segment_files_too() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    observe(h,s,0,20); observe(h,s,300,20); s.clearAll();
    ChartsHistory reboot; reboot.load(s); TEST_ASSERT_EQUAL(0,reboot.count());
}
void test_corrupt_block_is_not_exported_as_valid_measurements() {
    StorageManager s; s.begin(); ChartsHistory h; h.load(s);
    observe(h,s,0,20); observe(h,s,300,20);
    char path[32]; StorageManager::chartsSegmentPath((base_epoch/3600)%StorageManager::kChartsSegmentCount,path,sizeof(path));
    const uint8_t garbage[3]={1,2,3}; TEST_ASSERT_TRUE(s.saveBlobAtomic(path,garbage,sizeof(garbage)));
    ChartsHistory reboot; reboot.load(s); TEST_ASSERT_EQUAL(0,reboot.count());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_export_waits_for_durable_save_and_does_not_advance_cursor);
    RUN_TEST(test_retention_boundary_stays_stable_between_completed_intervals);
    RUN_TEST(test_clear_all_removes_segment_files_too);
    RUN_TEST(test_corrupt_block_is_not_exported_as_valid_measurements);
    RUN_TEST(test_summary_counts_new_equal_values_and_preserves_peak);
    RUN_TEST(test_validity_warmup_and_metric_counts_are_independent);
    RUN_TEST(test_gaps_are_not_pressure_measurements);
    RUN_TEST(test_save_retry_and_reboot_do_not_claim_unsaved_records);
    RUN_TEST(test_retention_ring_and_blocks_restore_last_day);
    RUN_TEST(test_clock_rollback_never_rewrites_completed_summary);
    RUN_TEST(test_untrusted_time_keeps_ram_history_out_of_export);
    RUN_TEST(test_legacy_snapshot_is_preserved_without_invented_statistics);
    RUN_TEST(test_optional_gas_keeps_record_identity_and_filters_current_chart);
    RUN_TEST(test_clear_removes_legacy_and_segment_history);
    RUN_TEST(test_chart_and_bounded_export_share_exact_summary_values);
    RUN_TEST(test_snapshot_remains_one_generation_during_concurrent_update);
    return UNITY_END();
}
