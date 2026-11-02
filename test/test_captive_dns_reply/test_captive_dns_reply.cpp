#include <unity.h>
#include <array>
#include <vector>
#include <cstring>
#include "core/CaptiveDnsReply.h"
namespace {
const uint8_t ip[4] = {192,168,4,1};
std::vector<uint8_t> query(uint16_t type=1) {
    return {0x12,0x34,0x01,0x20,0,1,0,0,0,0,0,0,3,'w','w','w',4,'t','e','s','t',0,
            uint8_t(type>>8),uint8_t(type),0,1};
}
size_t build(const std::vector<uint8_t>& q, uint8_t *out, size_t cap=512) {
    return CaptiveDnsReply::build(q.data(),q.size(),out,cap,ip);
}
}
void setUp() {}
void tearDown() {}
void test_a_answer_has_ap_address_zero_ttl_and_no_authenticated_or_recursive_claim() {
    auto q=query(); uint8_t out[512]{}; const size_t n=build(q,out);
    TEST_ASSERT_EQUAL_UINT(q.size()+16,n);
    const uint8_t header[]={0x12,0x34,0x85,0,0,1,0,1,0,0,0,0};
    TEST_ASSERT_EQUAL_MEMORY(header,out,sizeof(header));
    const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};
    TEST_ASSERT_EQUAL_MEMORY(answer,out+q.size(),sizeof(answer));
    TEST_ASSERT_EQUAL_MEMORY(q.data()+12,out+12,q.size()-12);
}
void test_aaaa_and_https_queries_get_noerror_without_bogus_a_answer() {
    for(uint16_t type : {28,65,15}) {
        auto q=query(type);uint8_t out[512]{};
        TEST_ASSERT_EQUAL_UINT(q.size(),build(q,out));
        TEST_ASSERT_EQUAL_UINT8(0,out[7]);TEST_ASSERT_EQUAL_UINT8(0,out[3]);
    }
}
void test_any_query_gets_one_a_record() {
    auto q=query(255);uint8_t out[512]{};TEST_ASSERT_EQUAL_UINT(q.size()+16,build(q,out));
    TEST_ASSERT_EQUAL_UINT8(1,out[q.size()+3]);
}
void test_every_truncated_question_is_rejected_without_writing() {
    auto q=query();uint8_t out[512];
    for(size_t n=0;n<q.size();++n) {
        memset(out,0xa5,sizeof(out));
        TEST_ASSERT_EQUAL_UINT(0,CaptiveDnsReply::build(q.data(),n,out,sizeof(out),ip));
        for(auto b:out)TEST_ASSERT_EQUAL_UINT8(0xa5,b);
    }
}
void test_compression_bad_label_and_missing_terminator_are_rejected() {
    uint8_t out[512]{};
    for(uint8_t label:{uint8_t(64),uint8_t(0xc0),uint8_t(0xff)}) {
        auto q=query();q[12]=label;TEST_ASSERT_EQUAL_UINT(0,build(q,out));
    }
    auto q=query();q[21]=63;TEST_ASSERT_EQUAL_UINT(0,build(q,out));
}
void test_question_count_response_opcode_and_class_are_validated() {
    uint8_t out[512]{};
    for(auto pair: {std::array<unsigned,2>{2,0x81},{2,0x09},{5,0},{5,2},{7,1},{9,1},{25,3}}) {
        auto q=query();q[pair[0]]=pair[1];TEST_ASSERT_EQUAL_UINT(0,build(q,out));
    }
}
void test_edns_is_stripped_and_in_place_reply_is_supported() {
    auto q=query();q[11]=1;q.insert(q.end(),{0,0,41,4,208,0,0,0,0,0,0});
    uint8_t buffer[512]{};memcpy(buffer,q.data(),q.size());
    TEST_ASSERT_EQUAL_UINT(42,CaptiveDnsReply::build(buffer,q.size(),buffer,sizeof(buffer),ip));
    TEST_ASSERT_EQUAL_UINT8(0,buffer[11]);TEST_ASSERT_EQUAL_MEMORY(ip,buffer+38,4);
}
void test_capacity_overflow_and_oversized_input_do_not_write() {
    auto q=query();uint8_t out[512];memset(out,0x5a,sizeof(out));
    TEST_ASSERT_EQUAL_UINT(0,build(q,out,q.size()+15));
    for(auto b:out)TEST_ASSERT_EQUAL_UINT8(0x5a,b);
    q.resize(513);TEST_ASSERT_EQUAL_UINT(0,build(q,out));
    TEST_ASSERT_EQUAL_UINT(0,CaptiveDnsReply::build(nullptr,26,out,512,ip));
    TEST_ASSERT_EQUAL_UINT(0,CaptiveDnsReply::build(q.data(),26,nullptr,512,ip));
}
void test_maximum_name_is_accepted_but_overlong_name_is_not() {
    auto q=query();q.resize(12);q[3]=0;
    for(int size:{63,63,63,61}) {q.push_back(size);q.insert(q.end(),size,'a');}
    q.insert(q.end(),{0,0,1,0,1});uint8_t out[512]{};
    TEST_ASSERT_EQUAL_UINT(q.size()+16,build(q,out));
    q[204]=63;q.insert(q.begin()+266,2,'a');TEST_ASSERT_EQUAL_UINT(0,build(q,out));
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_answer_has_ap_address_zero_ttl_and_no_authenticated_or_recursive_claim);
    RUN_TEST(test_aaaa_and_https_queries_get_noerror_without_bogus_a_answer);
    RUN_TEST(test_any_query_gets_one_a_record);
    RUN_TEST(test_every_truncated_question_is_rejected_without_writing);
    RUN_TEST(test_compression_bad_label_and_missing_terminator_are_rejected);
    RUN_TEST(test_question_count_response_opcode_and_class_are_validated);
    RUN_TEST(test_edns_is_stripped_and_in_place_reply_is_supported);
    RUN_TEST(test_capacity_overflow_and_oversized_input_do_not_write);
    RUN_TEST(test_maximum_name_is_accepted_but_overlong_name_is_not);
    return UNITY_END();
}
