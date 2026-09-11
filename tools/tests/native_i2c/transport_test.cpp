#include "AuraI2c.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
struct FakeBus {int port;std::vector<FakeDevice*> devices;};
struct FakeDevice {FakeBus *bus;int address;uint32_t frequency;};
bool fail_lock=false;
static int result=ESP_OK, fail_add=ESP_OK, fail_remove=ESP_OK;
static int created=0, destroyed=0, added=0, removed=0, operations=0;
static int last_port=-1,last_address=-1,last_timeout=0;
static std::vector<uint8_t> payload;
static std::vector<i2c_master_bus_config_t> bus_configs;
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c,FakeBus **out) {bus_configs.push_back(*c);*out=new FakeBus{c->i2c_port,{}};++created;return ESP_OK;}
esp_err_t i2c_del_master_bus(FakeBus *b) {if(!b->devices.empty())return ESP_ERR_INVALID_STATE;delete b;++destroyed;return ESP_OK;}
esp_err_t i2c_master_bus_add_device(FakeBus *b,const i2c_device_config_t *c,FakeDevice **out) {if(fail_add)return fail_add;*out=new FakeDevice{b,c->device_address,c->scl_speed_hz};b->devices.push_back(*out);++added;return ESP_OK;}
esp_err_t i2c_master_bus_rm_device(FakeDevice *d) {if(fail_remove)return fail_remove;auto &v=d->bus->devices;v.erase(std::find(v.begin(),v.end(),d));delete d;++removed;return ESP_OK;}
static esp_err_t op(FakeDevice *d,int timeout) {assert(d->frequency==100000);last_port=d->bus->port;last_address=d->address;last_timeout=timeout;++operations;return result;}
esp_err_t i2c_master_probe(FakeBus *b,uint16_t addr,int timeout) {last_port=b->port;last_address=addr;last_timeout=timeout;++operations;return result;}
esp_err_t i2c_master_transmit(FakeDevice *d,const uint8_t *tx,size_t len,int timeout) {payload.assign(tx,tx+len);return op(d,timeout);}
esp_err_t i2c_master_receive(FakeDevice *d,uint8_t *rx,size_t len,int timeout) {std::fill(rx,rx+len,0x42);return op(d,timeout);}
esp_err_t i2c_master_transmit_receive(FakeDevice *d,const uint8_t *tx,size_t len,uint8_t *rx,size_t rxlen,int timeout) {payload.assign(tx,tx+len);std::fill(rx,rx+rxlen,0x91);return op(d,timeout);}
esp_err_t i2c_master_multi_buffer_transmit(FakeDevice *d,i2c_master_transmit_multi_buffer_info_t *b,size_t n,int timeout) {payload.clear();for(size_t i=0;i<n;++i)payload.insert(payload.end(),b[i].write_buffer,b[i].write_buffer+b[i].buffer_size);return op(d,timeout);}
int main() {
    const aura_i2c_host_config_t panel{8,9,true,true,100000},sensor{44,6,false,false,100000};
    uint8_t tx[]={0x12,0x34},params[]={0xAB,0xCD,0xEF},rx[3]{};
    assert(aura_i2c_write(0,0x14,tx,2,50)==ESP_ERR_INVALID_STATE);
    assert(aura_i2c_start(0,&panel)==ESP_OK);
    assert(aura_i2c_start(0,&panel)==ESP_ERR_INVALID_STATE);
    assert(aura_i2c_start(1,&sensor)==ESP_OK);
    assert(created==2&&bus_configs[0].sda_io_num==8&&bus_configs[1].sda_io_num==44);
    assert(bus_configs[0].flags.enable_internal_pullup&&!bus_configs[1].flags.enable_internal_pullup);
    assert(aura_i2c_write(0,0x14,tx,2,50)==ESP_OK);
    assert(aura_i2c_write(1,0x14,tx,2,25)==ESP_OK);
    assert(last_port==1&&last_address==0x14&&last_timeout==25&&added==2);
    assert(aura_i2c_write(0,0x14,tx,2,50)==ESP_OK&&added==2);
    assert(aura_i2c_write_pair(1,0x6B,tx,2,params,3,40)==ESP_OK);
    assert((payload==std::vector<uint8_t>{0x12,0x34,0xAB,0xCD,0xEF}));
    const int before=operations;
    assert(aura_i2c_write_read(0,0x14,tx,2,rx,3,50)==ESP_OK);
    assert(operations==before+1&&rx[0]==0x91); // combined repeated-START transaction
    result=ESP_ERR_NOT_FOUND;
    const int devices_before_probe=added;
    assert(aura_i2c_probe(1,0x6A,10)==ESP_FAIL&&added==devices_before_probe);
    result=ESP_ERR_INVALID_RESPONSE;assert(aura_i2c_read(1,0x14,rx,3,50)==ESP_FAIL);
    result=ESP_ERR_TIMEOUT;assert(aura_i2c_read(1,0x14,rx,3,50)==ESP_ERR_TIMEOUT);
    result=ESP_OK;
    fail_lock=true;assert(aura_i2c_probe(0,0x14,50)==ESP_ERR_TIMEOUT);fail_lock=false;
    fail_add=ESP_ERR_NO_MEM;assert(aura_i2c_write(0,0x38,tx,2,50)==ESP_ERR_NO_MEM);fail_add=0;
    assert(aura_i2c_write(0,0x38,tx,2,50)==ESP_OK);
    fail_remove=ESP_FAIL;assert(aura_i2c_stop(0)==ESP_FAIL);fail_remove=0;
    assert(aura_i2c_stop(0)==ESP_OK&&destroyed==1&&aura_i2c_bus(0)==nullptr);
    assert(aura_i2c_write(1,0x14,tx,2,50)==ESP_OK); // other port survives
    assert(aura_i2c_start(0,&panel)==ESP_OK);
    const int add_before_restart=added;
    assert(aura_i2c_write(0,0x14,tx,2,50)==ESP_OK&&added==add_before_restart+1);
    assert(aura_i2c_write(-1,0x14,tx,2,50)==ESP_ERR_INVALID_ARG);
    assert(aura_i2c_probe(0,128,50)==ESP_ERR_INVALID_ARG);
    assert(aura_i2c_write_read(0,0x14,nullptr,1,rx,3,50)==ESP_ERR_INVALID_ARG);
    assert(aura_i2c_stop(0)==ESP_OK&&aura_i2c_stop(1)==ESP_OK);
    assert(created==destroyed&&added==removed);
    std::puts("native I2C: routing, reuse, repeated START, command framing, NACK/timeout, failure cleanup and restart passed");
}
