#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <cstring>

#include "../websocket_frame.h"
#include "../websocket_handshake.h"

using namespace ws;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) do { \
    std::cout << "TEST: " << name << " ... "; \
} while(0)

#define PASS() do { \
    std::cout << "PASS" << std::endl; \
    g_tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    std::cout << "FAIL: " << msg << std::endl; \
    g_tests_failed++; \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { FAIL(#cond " is false"); return; } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if (!((a) == (b))) { FAIL(#a " != " #b); return; } \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if ((cond)) { FAIL(#cond " is true"); return; } \
} while(0)

// ─── Frame Tests ──────────────────────────────────────────────────

static void test_encode_text_short() {
    TEST("encode text short frame");
    std::string text = "Hello";
    auto encoded = WebSocketFrameCoder::encodeText(text, false);
    ASSERT_EQ(encoded.size(), size_t(2 + 5));
    ASSERT_EQ((uint8_t)encoded[0], 0x81);
    ASSERT_EQ(encoded[1], 0x05);
    ASSERT_TRUE(std::string(encoded.begin() + 2, encoded.end()) == "Hello");
    PASS();
}

static void test_encode_text_masked() {
    TEST("encode text masked frame");
    std::string text = "Hello";
    auto encoded = WebSocketFrameCoder::encodeText(text, true);
    ASSERT_TRUE(encoded.size() >= 6);
    ASSERT_EQ((uint8_t)encoded[0], 0x81);
    ASSERT_TRUE((encoded[1] & 0x80) != 0);
    ASSERT_EQ((uint8_t)(encoded[1] & 0x7F), 0x05);
    PASS();
}

static void test_encode_binary() {
    TEST("encode binary frame");
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto encoded = WebSocketFrameCoder::encodeBinary(data, false);
    ASSERT_EQ(encoded.size(), size_t(2 + 5));
    ASSERT_EQ((uint8_t)encoded[0], 0x82);
    ASSERT_EQ(encoded[1], 0x05);
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(encoded[2 + i], data[i]);
    }
    PASS();
}

static void test_encode_ping_pong() {
    TEST("encode ping/pong frames");
    std::vector<uint8_t> payload = {0x01, 0x02};
    
    auto ping = WebSocketFrameCoder::encodePing(payload, false);
    ASSERT_EQ((uint8_t)ping[0], 0x89);
    ASSERT_EQ(ping[1], 0x02);
    
    auto pong = WebSocketFrameCoder::encodePong(payload, false);
    ASSERT_EQ((uint8_t)pong[0], 0x8A);
    ASSERT_EQ(pong[1], 0x02);
    PASS();
}

static void test_encode_close() {
    TEST("encode close frame");
    auto encoded = WebSocketFrameCoder::encodeClose(1000, "bye", false);
    ASSERT_EQ((uint8_t)encoded[0], 0x88);
    ASSERT_EQ(encoded[1], 0x05);
    uint16_t code;
    std::memcpy(&code, encoded.data() + 2, 2);
    code = ntohs(code);
    ASSERT_EQ(code, (uint16_t)1000);
    std::string reason(encoded.begin() + 4, encoded.end());
    ASSERT_EQ(reason, "bye");
    PASS();
}

static void test_parse_text_frame() {
    TEST("parse text frame");
    std::vector<uint8_t> raw = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
    WebSocketFrame frame;
    size_t consumed = 0;
    auto result = WebSocketFrameCoder::parse(raw.data(), raw.size(), frame, consumed);
    ASSERT_EQ(result, FrameParseResult::Ok);
    ASSERT_EQ(consumed, raw.size());
    ASSERT_TRUE(frame.fin);
    ASSERT_EQ(frame.opcode, OpCode::Text);
    ASSERT_FALSE(frame.masked);
    ASSERT_EQ(frame.payload.size(), (size_t)5);
    ASSERT_TRUE(std::string(frame.payload.begin(), frame.payload.end()) == "Hello");
    PASS();
}

static void test_parse_masked_frame() {
    TEST("parse masked frame");
    std::string text = "Hello World";
    auto encoded = WebSocketFrameCoder::encodeText(text, true);
    WebSocketFrame frame;
    size_t consumed = 0;
    auto result = WebSocketFrameCoder::parse(encoded.data(), encoded.size(), frame, consumed);
    ASSERT_EQ(result, FrameParseResult::Ok);
    ASSERT_EQ(frame.opcode, OpCode::Text);
    ASSERT_TRUE(frame.masked);
    ASSERT_TRUE(std::string(frame.payload.begin(), frame.payload.end()) == text);
    PASS();
}

static void test_parse_incomplete() {
    TEST("parse incomplete frame");
    std::vector<uint8_t> raw = {0x81, 0x05, 'H', 'e'};
    WebSocketFrame frame;
    size_t consumed = 0;
    auto result = WebSocketFrameCoder::parse(raw.data(), raw.size(), frame, consumed);
    ASSERT_EQ(result, FrameParseResult::Incomplete);
    PASS();
}

static void test_roundtrip_text() {
    TEST("roundtrip text frames");
    std::vector<std::string> test_cases = {
        "",
        "Hello",
        "Hello, World!",
        std::string(125, 'A'),
        std::string(1000, 'B'),
        std::string(70000, 'C'),
    };
    
    for (const auto& text : test_cases) {
        auto encoded = WebSocketFrameCoder::encodeText(text, true);
        WebSocketFrame frame;
        size_t consumed = 0;
        auto result = WebSocketFrameCoder::parse(encoded.data(), encoded.size(), frame, consumed);
        if (result != FrameParseResult::Ok) {
            FAIL("parse failed for length " + std::to_string(text.size()));
            return;
        }
        if (consumed != encoded.size()) {
            FAIL("consumed != size for length " + std::to_string(text.size()));
            return;
        }
        if (std::string(frame.payload.begin(), frame.payload.end()) != text) {
            FAIL("payload mismatch for length " + std::to_string(text.size()));
            return;
        }
    }
    PASS();
}

static void test_roundtrip_binary() {
    TEST("roundtrip binary frames");
    std::vector<uint8_t> data(65536);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = i & 0xFF;
    }
    
    auto encoded = WebSocketFrameCoder::encodeBinary(data, true);
    WebSocketFrame frame;
    size_t consumed = 0;
    auto result = WebSocketFrameCoder::parse(encoded.data(), encoded.size(), frame, consumed);
    ASSERT_EQ(result, FrameParseResult::Ok);
    ASSERT_EQ(frame.opcode, OpCode::Binary);
    ASSERT_EQ(frame.payload.size(), data.size());
    ASSERT_TRUE(frame.payload == data);
    PASS();
}

// ─── Handshake Tests ──────────────────────────────────────────────

static void test_generate_key() {
    TEST("generate websocket key");
    auto key1 = WebSocketHandshake::generateKey();
    auto key2 = WebSocketHandshake::generateKey();
    ASSERT_TRUE(!key1.empty());
    ASSERT_TRUE(!key2.empty());
    ASSERT_TRUE(key1 != key2);
    ASSERT_EQ(key1.size(), (size_t)24);
    PASS();
}

static void test_build_request() {
    TEST("build handshake request");
    HandshakeRequest req;
    req.host = "example.com";
    req.port = 8080;
    req.path = "/ws";
    req.key = "dGhlIHNhbXBsZSBub25jZQ==";
    req.protocols = {"chat", "superchat"};
    req.extra_headers["User-Agent"] = "TestAgent";
    
    auto request = WebSocketHandshake::buildRequest(req);
    
    ASSERT_TRUE(request.find("GET /ws HTTP/1.1") == 0);
    ASSERT_TRUE(request.find("Host: example.com:8080") != std::string::npos);
    ASSERT_TRUE(request.find("Upgrade: websocket") != std::string::npos);
    ASSERT_TRUE(request.find("Connection: Upgrade") != std::string::npos);
    ASSERT_TRUE(request.find("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==") != std::string::npos);
    ASSERT_TRUE(request.find("Sec-WebSocket-Version: 13") != std::string::npos);
    ASSERT_TRUE(request.find("Sec-WebSocket-Protocol: chat, superchat") != std::string::npos);
    ASSERT_TRUE(request.find("User-Agent: TestAgent") != std::string::npos);
    ASSERT_TRUE(request.find("\r\n\r\n") != std::string::npos);
    PASS();
}

static void test_parse_response() {
    TEST("parse handshake response");
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        "Sec-WebSocket-Protocol: chat\r\n"
        "\r\n";
    
    auto resp = WebSocketHandshake::parseResponse(response);
    ASSERT_TRUE(resp.success);
    ASSERT_EQ(resp.status_code, 101);
    ASSERT_EQ(resp.headers["upgrade"], "websocket");
    ASSERT_EQ(resp.headers["connection"], "Upgrade");
    ASSERT_EQ(resp.accept_key, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    PASS();
}

static void test_verify_accept() {
    TEST("verify accept key");
    std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string server_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    
    ASSERT_TRUE(WebSocketHandshake::verifyAccept(client_key, server_accept));
    ASSERT_FALSE(WebSocketHandshake::verifyAccept(client_key, "wrongkey"));
    PASS();
}

static void test_parse_response_error_status() {
    TEST("parse 400 response");
    std::string response = 
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    
    auto resp = WebSocketHandshake::parseResponse(response);
    ASSERT_FALSE(resp.success);
    ASSERT_EQ(resp.status_code, 400);
    PASS();
}

// ─── Main ─────────────────────────────────────────────────────────

int main() {
    std::cout << "=== WebSocket Unit Tests ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "--- Frame Encoding ---" << std::endl;
    test_encode_text_short();
    test_encode_text_masked();
    test_encode_binary();
    test_encode_ping_pong();
    test_encode_close();
    
    std::cout << std::endl << "--- Frame Parsing ---" << std::endl;
    test_parse_text_frame();
    test_parse_masked_frame();
    test_parse_incomplete();
    
    std::cout << std::endl << "--- Roundtrip ---" << std::endl;
    test_roundtrip_text();
    test_roundtrip_binary();
    
    std::cout << std::endl << "--- Handshake ---" << std::endl;
    test_generate_key();
    test_build_request();
    test_parse_response();
    test_verify_accept();
    test_parse_response_error_status();
    
    std::cout << std::endl << "=== Results ===" << std::endl;
    std::cout << "Passed: " << g_tests_passed << std::endl;
    std::cout << "Failed: " << g_tests_failed << std::endl;
    
    if (g_tests_failed > 0) {
        std::cout << "SOME TESTS FAILED!" << std::endl;
        return 1;
    }
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
