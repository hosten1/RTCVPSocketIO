/**
 * STUN 协议测试程序
 * 测试 STUN 消息的创建、解析、RFC 5769 测试向量
 */

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "api/transport/stun.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/socket_address.h"

#define TEST_PASS() std::cout << "  [PASS] " << __FUNCTION__ << std::endl
#define TEST_FAIL(msg)                          \
    do {                                        \
        std::cerr << "  [FAIL] " << __FUNCTION__ \
                  << ": " << msg << std::endl;  \
        return false;                           \
    } while (0)
#define ASSERT_TRUE(cond) \
    if (!(cond)) TEST_FAIL("assertion failed: " #cond)
#define ASSERT_EQ(a, b) \
    if ((a) != (b)) TEST_FAIL("expected " #a " == " #b)

// ==================== 辅助函数 ====================

static void PrintHex(const char* label, const char* data, size_t len) {
    if (!label) return;
    std::cout << "  " << label << ": ";
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", static_cast<unsigned char>(data[i]));
    }
    std::cout << std::endl;
}

// ==================== 测试用例 ====================

// 测试 1: 创建 STUN Binding Request
bool TestCreateBindingRequest() {
    cricket::StunMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);

    // 设置 Transaction ID
    std::string tid = "123456789012";
    msg.SetTransactionID(tid);

    ASSERT_EQ(static_cast<int>(msg.type()), cricket::STUN_BINDING_REQUEST);
    ASSERT_EQ(msg.transaction_id().size(), cricket::kStunTransactionIdLength);
    ASSERT_TRUE(msg.transaction_id() == tid);

    // 序列化
    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));

    std::cout << "  Binding Request size: " << buf.Length() << " bytes" << std::endl;
    ASSERT_EQ(buf.Length(), cricket::kStunHeaderSize);
    TEST_PASS();
    return true;
}

// 测试 2: 创建带属性的 STUN 消息 (使用 IceMessage 支持 PRIORITY)
bool TestCreateMessageWithAttributes() {
    cricket::IceMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);
    msg.SetTransactionID("abcdabcdabcd");

    // 添加 SOFTWARE 属性
    auto sw = std::make_unique<cricket::StunByteStringAttribute>(
        cricket::STUN_ATTR_SOFTWARE);
    sw->CopyBytes("libwebrtc test");
    msg.AddAttribute(std::move(sw));

    // 添加 PRIORITY 属性
    auto pri = std::make_unique<cricket::StunUInt32Attribute>(
        cricket::STUN_ATTR_PRIORITY, 0x6e0001ff);
    msg.AddAttribute(std::move(pri));

    // 添加 USERNAME 属性
    auto uname = std::make_unique<cricket::StunByteStringAttribute>(
        cricket::STUN_ATTR_USERNAME);
    uname->CopyBytes("testuser");
    msg.AddAttribute(std::move(uname));

    // 序列化
    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));
    std::cout << "  Message with attributes size: " << buf.Length() << " bytes" << std::endl;

    // 解析回来
    cricket::IceMessage msg2;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(msg2.Read(&reader));

    ASSERT_EQ(static_cast<int>(msg2.type()), cricket::STUN_BINDING_REQUEST);
    ASSERT_TRUE(msg2.transaction_id() == "abcdabcdabcd");

    // 验证 SOFTWARE
    const auto* sw2 = msg2.GetByteString(cricket::STUN_ATTR_SOFTWARE);
    ASSERT_TRUE(sw2 != nullptr);
    ASSERT_TRUE(sw2->GetString() == "libwebrtc test");

    // 验证 PRIORITY
    const auto* pri2 = msg2.GetUInt32(cricket::STUN_ATTR_PRIORITY);
    ASSERT_TRUE(pri2 != nullptr);
    ASSERT_EQ(pri2->value(), 0x6e0001ff);

    // 验证 USERNAME
    const auto* uname2 = msg2.GetByteString(cricket::STUN_ATTR_USERNAME);
    ASSERT_TRUE(uname2 != nullptr);
    ASSERT_TRUE(uname2->GetString() == "testuser");

    TEST_PASS();
    return true;
}

// 测试 3: STUN Binding Response
bool TestCreateBindingResponse() {
    cricket::StunMessage response;
    response.SetType(cricket::STUN_BINDING_RESPONSE);
    response.SetTransactionID("resp123456789");

    // 添加 XOR-MAPPED-ADDRESS
    rtc::SocketAddress addr("192.168.1.100", 3478);
    auto xaddr = std::make_unique<cricket::StunXorAddressAttribute>(
        cricket::STUN_ATTR_XOR_MAPPED_ADDRESS, addr);
    response.AddAttribute(std::move(xaddr));

    // 序列化
    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(response.Write(&buf));

    // 解析
    cricket::StunMessage parsed;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(parsed.Read(&reader));

    ASSERT_EQ(static_cast<int>(parsed.type()), cricket::STUN_BINDING_RESPONSE);

    const auto* xaddr2 = parsed.GetAddress(cricket::STUN_ATTR_XOR_MAPPED_ADDRESS);
    ASSERT_TRUE(xaddr2 != nullptr);
    std::cout << "  XOR-MAPPED-ADDRESS: " << xaddr2->ipaddr().ToString()
              << ":" << xaddr2->port() << std::endl;

    TEST_PASS();
    return true;
}

// 测试 4: MAPPED-ADDRESS 属性
bool TestMappedAddress() {
    cricket::StunMessage msg;
    msg.SetType(cricket::STUN_BINDING_RESPONSE);
    msg.SetTransactionID("mapped0000000");

    rtc::SocketAddress addr("10.0.0.1", 8080);
    auto maddr = std::make_unique<cricket::StunAddressAttribute>(
        cricket::STUN_ATTR_MAPPED_ADDRESS, addr);
    msg.AddAttribute(std::move(maddr));

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));

    cricket::StunMessage parsed;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(parsed.Read(&reader));

    const auto* maddr2 = parsed.GetAddress(cricket::STUN_ATTR_MAPPED_ADDRESS);
    ASSERT_TRUE(maddr2 != nullptr);
    ASSERT_EQ(maddr2->family(), cricket::STUN_ADDRESS_IPV4);
    ASSERT_EQ(maddr2->port(), 8080);
    ASSERT_TRUE(maddr2->ipaddr().ToString() == "10.0.0.1");

    TEST_PASS();
    return true;
}

// 测试 5: ERROR-CODE 属性
bool TestErrorCode() {
    cricket::StunMessage err;
    err.SetType(cricket::STUN_BINDING_ERROR_RESPONSE);
    err.SetTransactionID("error000000000");

    auto ec = std::make_unique<cricket::StunErrorCodeAttribute>(
        cricket::STUN_ATTR_ERROR_CODE,
        cricket::STUN_ERROR_UNAUTHORIZED,
        "Unauthorized");
    err.AddAttribute(std::move(ec));

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(err.Write(&buf));

    cricket::StunMessage parsed;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(parsed.Read(&reader));

    const auto* ec2 = parsed.GetErrorCode();
    ASSERT_TRUE(ec2 != nullptr);
    ASSERT_EQ(ec2->eclass(), 4);
    ASSERT_EQ(ec2->number(), 1);
    ASSERT_TRUE(ec2->reason() == "Unauthorized");

    TEST_PASS();
    return true;
}

// 测试 6: UNKNOWN-ATTRIBUTES 属性
bool TestUnknownAttributes() {
    cricket::StunMessage err;
    err.SetType(cricket::STUN_BINDING_ERROR_RESPONSE);
    err.SetTransactionID("unknown0000000");

    auto ua = cricket::StunAttribute::CreateUnknownAttributes();
    auto* ua_list = static_cast<cricket::StunUInt16ListAttribute*>(ua.get());
    ua_list->AddType(0x1234);
    ua_list->AddType(0x5678);
    err.AddAttribute(std::move(ua));

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(err.Write(&buf));

    cricket::StunMessage parsed;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(parsed.Read(&reader));

    const auto* ua2 = parsed.GetUnknownAttributes();
    ASSERT_TRUE(ua2 != nullptr);
    ASSERT_EQ(ua2->Size(), 2);

    TEST_PASS();
    return true;
}

// 测试 7: 使用已知字节序列解析 STUN 消息
bool TestParseRawBytes() {
    // 一个简单的 STUN Binding Request 字节序列
    // Type=0x0001, Length=0x0000, Magic Cookie=0x2112A442,
    // Transaction ID = "0123456789ab"
    const unsigned char raw[] = {
        0x00, 0x01, 0x00, 0x00,  // type + length
        0x21, 0x12, 0xA4, 0x42,  // magic cookie
        '0', '1', '2', '3',      // transaction id
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
    };

    cricket::StunMessage msg;
    rtc::ByteBufferReader reader(reinterpret_cast<const char*>(raw), sizeof(raw));
    ASSERT_TRUE(msg.Read(&reader));

    ASSERT_EQ(static_cast<int>(msg.type()), cricket::STUN_BINDING_REQUEST);
    ASSERT_EQ(msg.length(), 0);
    std::string expected_tid = "0123456789ab";
    ASSERT_TRUE(msg.transaction_id() == expected_tid);

    TEST_PASS();
    return true;
}

// 测试 8: FINGERPRINT 属性
bool TestFingerprint() {
    cricket::StunMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);
    msg.SetTransactionID("fingerprint0");

    ASSERT_TRUE(msg.AddFingerprint());

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));

    // 验证 FINGERPRINT
    ASSERT_TRUE(cricket::StunMessage::ValidateFingerprint(buf.Data(), buf.Length()));

    // 修改一个字节，验证 FAIL
    std::vector<char> corrupted(buf.Data(), buf.Data() + buf.Length());
    corrupted[4]++;  // 破坏 length 字段
    ASSERT_TRUE(!cricket::StunMessage::ValidateFingerprint(
        corrupted.data(), corrupted.size()));

    TEST_PASS();
    return true;
}

// 测试 9: MESSAGE-INTEGRITY 属性
bool TestMessageIntegrity() {
    cricket::StunMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);
    msg.SetTransactionID("integrity0000");

    std::string password = "testpassword";
    ASSERT_TRUE(msg.AddMessageIntegrity(password));

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));

    // 验证
    ASSERT_TRUE(cricket::StunMessage::ValidateMessageIntegrity(
        buf.Data(), buf.Length(), password));

    // 错误密码
    ASSERT_TRUE(!cricket::StunMessage::ValidateMessageIntegrity(
        buf.Data(), buf.Length(), "wrongpassword"));

    TEST_PASS();
    return true;
}

// 测试 10: RFC 5769 测试向量 - Sample Request
bool TestRfc5769SampleRequest() {
    // RFC 5769 Section 2.1 Sample Request
    static const unsigned char kRfc5769SampleRequest[] = {
        0x00, 0x01, 0x00, 0x58,   // Request type and message length
        0x21, 0x12, 0xa4, 0x42,   // Magic cookie
        0xb7, 0xe7, 0xa7, 0x01,   // }
        0xbc, 0x34, 0xd6, 0x86,   // }  Transaction ID
        0xfa, 0x87, 0xdf, 0xae,   // }
        0x80, 0x22, 0x00, 0x10,   // SOFTWARE attribute header
        0x53, 0x54, 0x55, 0x4e,   // "STUN"
        0x20, 0x74, 0x65, 0x73,   // " test"
        0x74, 0x20, 0x63, 0x6c,   // " cl"
        0x69, 0x65, 0x6e, 0x74,   // "ient"
        0x00, 0x24, 0x00, 0x04,   // PRIORITY attribute header
        0x6e, 0x00, 0x01, 0xff,   // ICE priority value
        0x80, 0x29, 0x00, 0x08,   // ICE-CONTROLLED attribute header
        0x93, 0x2f, 0xf9, 0xb1,   // }
        0x51, 0x26, 0x3b, 0x36,   // }  Tie breaker
        0x00, 0x06, 0x00, 0x09,   // USERNAME attribute header
        0x65, 0x76, 0x74, 0x6a,   // "evtj"
        0x3a, 0x68, 0x36, 0x76,   // ":h6v"
        0x59, 0x20, 0x20, 0x20,   // "Y" + padding
        0x00, 0x08, 0x00, 0x14,   // MESSAGE-INTEGRITY attribute header
        0x9a, 0xea, 0xa7, 0x0c,   // }
        0xbf, 0xd8, 0xcb, 0x56,   // }
        0x78, 0x1e, 0xf2, 0xb5,   // }  HMAC-SHA1
        0xb2, 0xd3, 0xf2, 0x49,   // }
        0xc1, 0xb5, 0x71, 0xa2,   // }
        0x80, 0x28, 0x00, 0x04,   // FINGERPRINT attribute header
        0xe5, 0x7a, 0x3b, 0xcf    // CRC32 fingerprint
    };

    cricket::IceMessage msg;
    rtc::ByteBufferReader reader(
        reinterpret_cast<const char*>(kRfc5769SampleRequest),
        sizeof(kRfc5769SampleRequest));
    ASSERT_TRUE(msg.Read(&reader));

    ASSERT_EQ(static_cast<int>(msg.type()), cricket::STUN_BINDING_REQUEST);

    // 验证 Transaction ID
    unsigned char expected_tid[] = {0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34,
                                    0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae};
    ASSERT_EQ(msg.transaction_id().size(), cricket::kStunTransactionIdLength);
    ASSERT_TRUE(memcmp(msg.transaction_id().c_str(), expected_tid,
                       cricket::kStunTransactionIdLength) == 0);

    // 验证 SOFTWARE
    const auto* sw = msg.GetByteString(cricket::STUN_ATTR_SOFTWARE);
    ASSERT_TRUE(sw != nullptr);
    ASSERT_TRUE(sw->GetString() == "STUN test client");

    // 验证 USERNAME
    const auto* uname = msg.GetByteString(cricket::STUN_ATTR_USERNAME);
    ASSERT_TRUE(uname != nullptr);
    ASSERT_TRUE(uname->GetString() == "evtj:h6vY");

    // 验证 PRIORITY
    const auto* pri = msg.GetUInt32(cricket::STUN_ATTR_PRIORITY);
    ASSERT_TRUE(pri != nullptr);
    ASSERT_EQ(pri->value(), 0x6e0001ff);

    // 验证 Fingerprint
    ASSERT_TRUE(cricket::StunMessage::ValidateFingerprint(
        reinterpret_cast<const char*>(kRfc5769SampleRequest),
        sizeof(kRfc5769SampleRequest)));

    // 验证 Message Integrity
    ASSERT_TRUE(cricket::StunMessage::ValidateMessageIntegrity(
        reinterpret_cast<const char*>(kRfc5769SampleRequest),
        sizeof(kRfc5769SampleRequest),
        "VOkJxbRl1RmTxUk/WvJxBt"));

    TEST_PASS();
    return true;
}

// 测试 11: RFC 5769 测试向量 - Sample IPv4 Response
bool TestRfc5769SampleResponse() {
    static const unsigned char kRfc5769SampleResponse[] = {
        0x01, 0x01, 0x00, 0x3c,  // Response type and message length
        0x21, 0x12, 0xa4, 0x42,  // Magic cookie
        0xb7, 0xe7, 0xa7, 0x01,  // }
        0xbc, 0x34, 0xd6, 0x86,  // }  Transaction ID
        0xfa, 0x87, 0xdf, 0xae,  // }
        0x80, 0x22, 0x00, 0x0b,  // SOFTWARE attribute header
        0x74, 0x65, 0x73, 0x74,  // "test"
        0x20, 0x76, 0x65, 0x63,  // " vec"
        0x74, 0x6f, 0x72, 0x20,  // "tor "
        0x00, 0x20, 0x00, 0x08,  // XOR-MAPPED-ADDRESS attribute header
        0x00, 0x01, 0xa1, 0x47,  // Address family (IPv4) and xor'd mapped port
        0xe1, 0x12, 0xa6, 0x43,  // Xor'd mapped IPv4 address
        0x00, 0x08, 0x00, 0x14,  // MESSAGE-INTEGRITY attribute header
        0x2b, 0x91, 0xf5, 0x99,  // }
        0xfd, 0x9e, 0x90, 0xc3,  // }
        0x8c, 0x74, 0x89, 0xf9,  // }  HMAC-SHA1
        0x2a, 0xf9, 0xba, 0x53,  // }
        0xf0, 0x6b, 0xe7, 0xd7,  // }
        0x80, 0x28, 0x00, 0x04,  // FINGERPRINT attribute header
        0xc0, 0x7d, 0x4c, 0x96   // CRC32 fingerprint
    };

    cricket::StunMessage msg;
    rtc::ByteBufferReader reader(
        reinterpret_cast<const char*>(kRfc5769SampleResponse),
        sizeof(kRfc5769SampleResponse));
    ASSERT_TRUE(msg.Read(&reader));

    ASSERT_EQ(static_cast<int>(msg.type()), cricket::STUN_BINDING_RESPONSE);

    // 验证 SOFTWARE
    const auto* sw = msg.GetByteString(cricket::STUN_ATTR_SOFTWARE);
    ASSERT_TRUE(sw != nullptr);
    ASSERT_TRUE(sw->GetString() == "test vector");

    // 验证 XOR-MAPPED-ADDRESS
    const auto* xaddr = msg.GetAddress(cricket::STUN_ATTR_XOR_MAPPED_ADDRESS);
    ASSERT_TRUE(xaddr != nullptr);
    ASSERT_EQ(xaddr->family(), cricket::STUN_ADDRESS_IPV4);
    ASSERT_EQ(xaddr->port(), 32853);
    ASSERT_TRUE(xaddr->ipaddr().ToString() == "192.0.2.1");

    // 验证 Fingerprint
    ASSERT_TRUE(cricket::StunMessage::ValidateFingerprint(
        reinterpret_cast<const char*>(kRfc5769SampleResponse),
        sizeof(kRfc5769SampleResponse)));

    // 验证 Message Integrity
    ASSERT_TRUE(cricket::StunMessage::ValidateMessageIntegrity(
        reinterpret_cast<const char*>(kRfc5769SampleResponse),
        sizeof(kRfc5769SampleResponse),
        "VOkJxbRl1RmTxUk/WvJxBt"));

    TEST_PASS();
    return true;
}

// 测试 12: 检测非 STUN 包 (RTCP 包)
bool TestRejectNonStunPacket() {
    // RTCP 包数据
    static const unsigned char kRtcpPacket[] = {
        0x80, 0xc8, 0x00, 0x06, 0x00, 0x00, 0x00, 0x55,
        0xce, 0xa5, 0x18, 0x3a, 0x39, 0xcc, 0x7d, 0x09,
        0x23, 0xed, 0x19, 0x07, 0x00, 0x00, 0x01, 0x56,
        0x00, 0x03, 0x73, 0x50,
    };

    cricket::StunMessage msg;
    rtc::ByteBufferReader reader(
        reinterpret_cast<const char*>(kRtcpPacket), sizeof(kRtcpPacket));
    ASSERT_TRUE(!msg.Read(&reader));

    TEST_PASS();
    return true;
}

// 测试 13: 消息克隆 (使用 IceMessage 支持 PRIORITY)
bool TestCloneMessage() {
    cricket::IceMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);
    msg.SetTransactionID("clone0000000");

    auto sw = std::make_unique<cricket::StunByteStringAttribute>(
        cricket::STUN_ATTR_SOFTWARE);
    sw->CopyBytes("clone test");
    msg.AddAttribute(std::move(sw));

    auto pri = std::make_unique<cricket::StunUInt32Attribute>(
        cricket::STUN_ATTR_PRIORITY, 12345);
    msg.AddAttribute(std::move(pri));

    auto cloned = msg.Clone();
    ASSERT_TRUE(cloned != nullptr);

    ASSERT_EQ(static_cast<int>(cloned->type()), cricket::STUN_BINDING_REQUEST);
    ASSERT_TRUE(cloned->transaction_id() == "clone0000000");

    const auto* sw2 = cloned->GetByteString(cricket::STUN_ATTR_SOFTWARE);
    ASSERT_TRUE(sw2 != nullptr);
    ASSERT_TRUE(sw2->GetString() == "clone test");

    const auto* pri2 = cloned->GetUInt32(cricket::STUN_ATTR_PRIORITY);
    ASSERT_TRUE(pri2 != nullptr);
    ASSERT_EQ(pri2->value(), 12345);

    TEST_PASS();
    return true;
}

// 测试 14: 无效长度 STUN 消息
bool TestInvalidLengthMessages() {
    // 当声明的长度与缓冲区实际长度不匹配时，Read 应返回 false
    static const unsigned char kMsgWithZeroLength[] = {
        0x00, 0x01, 0x00, 0x00,  // length = 0 (但后面有属性数据)
        0x21, 0x12, 0xA4, 0x42,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
        0x00, 0x20, 0x00, 0x08,  // XOR-MAPPED-ADDRESS (8 bytes)
        0x00, 0x01, 0x21, 0x1F,
        0x21, 0x12, 0xA4, 0x53,
    };

    cricket::StunMessage msg;
    rtc::ByteBufferReader reader(
        reinterpret_cast<const char*>(kMsgWithZeroLength),
        sizeof(kMsgWithZeroLength));
    // 长度与缓冲区不匹配，应拒绝
    ASSERT_TRUE(!msg.Read(&reader));

    // 正确长度的消息应该能解析
    static const unsigned char kValidMsg[] = {
        0x00, 0x01, 0x00, 0x00,  // length = 0
        0x21, 0x12, 0xA4, 0x42,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
    };

    cricket::StunMessage msg2;
    rtc::ByteBufferReader reader2(
        reinterpret_cast<const char*>(kValidMsg), sizeof(kValidMsg));
    ASSERT_TRUE(msg2.Read(&reader2));
    ASSERT_EQ(msg2.length(), 0);

    TEST_PASS();
    return true;
}

// 测试 15: IceMessage
bool TestIceMessage() {
    cricket::IceMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);
    msg.SetTransactionID("ice0000000000");

    // 添加 PRIORITY
    auto pri = std::make_unique<cricket::StunUInt32Attribute>(
        cricket::STUN_ATTR_PRIORITY, 0x7e0001ff);
    msg.AddAttribute(std::move(pri));

    // 添加 ICE-CONTROLLING
    auto ctrl = std::make_unique<cricket::StunUInt64Attribute>(
        cricket::STUN_ATTR_ICE_CONTROLLING, 0x932ff9b151263b36ull);
    msg.AddAttribute(std::move(ctrl));

    // 添加 USE-CANDIDATE
    auto uc = std::make_unique<cricket::StunByteStringAttribute>(
        cricket::STUN_ATTR_USE_CANDIDATE);
    msg.AddAttribute(std::move(uc));

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));
    std::cout << "  ICE message size: " << buf.Length() << " bytes" << std::endl;

    cricket::IceMessage parsed;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(parsed.Read(&reader));

    const auto* pri2 = parsed.GetUInt32(cricket::STUN_ATTR_PRIORITY);
    ASSERT_TRUE(pri2 != nullptr);
    ASSERT_EQ(pri2->value(), 0x7e0001ff);

    const auto* ctrl2 = parsed.GetUInt64(cricket::STUN_ATTR_ICE_CONTROLLING);
    ASSERT_TRUE(ctrl2 != nullptr);
    ASSERT_EQ(ctrl2->value(), 0x932ff9b151263b36ull);

    TEST_PASS();
    return true;
}

// 测试 16: 修改 Magic Cookie
bool TestCustomMagicCookie() {
    cricket::StunMessage msg;
    msg.SetType(cricket::STUN_BINDING_REQUEST);
    msg.SetTransactionID("magic0000000");

    // 使用 legacy cookie (RFC 3489)
    msg.SetStunMagicCookie(0);

    rtc::ByteBufferWriter buf;
    ASSERT_TRUE(msg.Write(&buf));

    // 验证 legacy 模式
    cricket::StunMessage parsed;
    rtc::ByteBufferReader reader(buf.Data(), buf.Length());
    ASSERT_TRUE(parsed.Read(&reader));

    ASSERT_TRUE(parsed.IsLegacy());
    ASSERT_EQ(parsed.transaction_id().size(),
              cricket::kStunLegacyTransactionIdLength);

    TEST_PASS();
    return true;
}

// ==================== 主函数 ====================

int main() {
    std::cout << "=== STUN Protocol Tests ===" << std::endl << std::endl;

    struct TestCase {
        const char* name;
        bool (*func)();
    };

    TestCase tests[] = {
        {"Create Binding Request", TestCreateBindingRequest},
        {"Create Message with Attributes", TestCreateMessageWithAttributes},
        {"Create Binding Response", TestCreateBindingResponse},
        {"Mapped Address", TestMappedAddress},
        {"Error Code", TestErrorCode},
        {"Unknown Attributes", TestUnknownAttributes},
        {"Parse Raw Bytes", TestParseRawBytes},
        {"Fingerprint", TestFingerprint},
        {"Message Integrity", TestMessageIntegrity},
        {"RFC 5769 Sample Request", TestRfc5769SampleRequest},
        {"RFC 5769 Sample Response", TestRfc5769SampleResponse},
        {"Reject Non-STUN Packet", TestRejectNonStunPacket},
        {"Clone Message", TestCloneMessage},
        {"Invalid Length Messages", TestInvalidLengthMessages},
        {"ICE Message", TestIceMessage},
        {"Custom Magic Cookie", TestCustomMagicCookie},
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        std::cout << "Test: " << test.name << std::endl;
        if (test.func()) {
            passed++;
        } else {
            failed++;
        }
    }

    std::cout << std::endl;
    std::cout << "=== Results: " << passed << " passed, "
              << failed << " failed ===" << std::endl;

    return failed > 0 ? 1 : 0;
}