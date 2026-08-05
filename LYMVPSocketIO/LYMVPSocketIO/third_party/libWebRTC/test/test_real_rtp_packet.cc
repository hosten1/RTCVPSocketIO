#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cassert>

#include "modules/rtp_rtcp/source/rtp_pt_manipulator_impl.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/ulpfec_header_reader_writer.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "rtc_base/copy_on_write_buffer.h"

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return {};
    }
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

int main() {
    // 从 test/ 目录或项目根目录运行
    std::vector<uint8_t> raw_packet = ReadFile("ulpfec_rtp_packet.rtp");
    if (raw_packet.empty()) {
        raw_packet = ReadFile("test/ulpfec_rtp_packet.rtp");
    }
    if (raw_packet.empty()) {
        raw_packet = ReadFile("../test/ulpfec_rtp_packet.rtp");
    }
      if (raw_packet.empty()) {
        raw_packet = ReadFile("../../test/ulpfec_rtp_packet.rtp");
    }
    if (raw_packet.empty()) {
        return 1;
    }
    std::cout << "Raw packet size: " << raw_packet.size() << " bytes" << std::endl;

    // 定位 RTP 数据（跳过以太网、IP、UDP 头）
    size_t ip_offset = 14;
    if (raw_packet.size() < ip_offset + 1) {
        std::cerr << "Packet too short" << std::endl;
        return 1;
    }
    uint8_t ip_ihl = raw_packet[ip_offset] & 0x0F;
    size_t udp_offset = ip_offset + ip_ihl * 4;
    if (raw_packet.size() < udp_offset + 8) {
        std::cerr << "Packet too short for UDP" << std::endl;
        return 1;
    }
    uint16_t udp_len = (raw_packet[udp_offset + 4] << 8) | raw_packet[udp_offset + 5];
    size_t rtp_offset = udp_offset + 8;
    size_t rtp_size = udp_len - 8;
    if (rtp_offset + rtp_size > raw_packet.size()) {
        std::cerr << "RTP out of bounds" << std::endl;
        return 1;
    }

    // 解析 RTP 包
    rtc::CopyOnWriteBuffer rtp_buffer;
    rtp_buffer.AppendData(raw_packet.data() + rtp_offset, rtp_size);
    webrtc::RtpPacket packet;
    if (!packet.Parse(rtp_buffer)) {
        std::cerr << "Failed to parse RTP packet" << std::endl;
        return 1;
    }

    // 配置旧 SDP（只需要 RED 和 ULPFEC）
    webrtc::SdpMediaDescription old_sdp;
    old_sdp.rtpmap[124] = "red/90000";
    old_sdp.rtpmap[123] = "ulpfec/90000";
    webrtc::RtpPtManipulatorImpl manip;
    manip.ConfigureSdp(old_sdp);

    // 只修改 RED 和 ULPFEC 的 PT
    std::vector<webrtc::PtMappingRule> mappings = {
        {124, 104},
        {123, 106}
    };

    if (!manip.ModifyPtValues(&packet, mappings)) {
        std::cerr << "Modification failed" << std::endl;
        return 1;
    }

    // 直接检查修改后的外层 PT 和内层 PT
    uint8_t outer_pt = packet.PayloadType();
    uint8_t inner_pt = 0;
    if (packet.payload_size() >= 1) {
        inner_pt = packet.payload()[0] & 0x7F;
    }
    std::cout << "Modified outer PT: " << static_cast<int>(outer_pt) << std::endl;
    std::cout << "Modified inner PT: " << static_cast<int>(inner_pt) << std::endl;

    // 验证是否修改为目标值
    if (outer_pt != 104 || inner_pt != 106) {
        std::cerr << "PT modification failed: expected (104,106) but got ("
                  << static_cast<int>(outer_pt) << "," << static_cast<int>(inner_pt) << ")" << std::endl;
        return 1;
    }

    // 使用 WebRTC 原生 UlpfecHeaderReader 验证 ULPFEC 头的完整性
    std::cout << "\n--- WebRTC native ULPFEC header check ---" << std::endl;
    const uint8_t* ulpfec_start = packet.payload().data() + 1;
    size_t ulpfec_len = packet.payload_size() - 1;
    if (ulpfec_len < 10) {
        std::cerr << "ULPFEC payload too short (<10 bytes)" << std::endl;
        return 1;
    }

    rtc::scoped_refptr<webrtc::ForwardErrorCorrection::Packet> fec_packet(
        new webrtc::ForwardErrorCorrection::Packet());
    memcpy(fec_packet->data, ulpfec_start, ulpfec_len);
    fec_packet->length = ulpfec_len;
    webrtc::ForwardErrorCorrection::ReceivedFecPacket received_fec;
    received_fec.pkt = fec_packet;
    received_fec.ssrc = packet.Ssrc();
    webrtc::UlpfecHeaderReader reader;
    bool native_ok = reader.ReadFecHeader(&received_fec);
    if (native_ok) {
        std::cout << "UlpfecHeaderReader::ReadFecHeader SUCCESS" << std::endl;
        std::cout << "  fec_header_size=" << received_fec.fec_header_size
                  << ", protected_ssrc=" << received_fec.protected_ssrc
                  << ", seq_num_base=" << received_fec.seq_num_base
                  << ", protection_length=" << received_fec.protection_length << std::endl;
    } else {
        std::cout << "UlpfecHeaderReader::ReadFecHeader FAIL" << std::endl;
        std::cout << "First 10 bytes of ULPFEC data: ";
        for (size_t i = 0; i < 10 && i < ulpfec_len; ++i) {
            printf("%02x ", ulpfec_start[i]);
        }
        std::cout << std::endl;
        return 1;
    }

    std::cout << "\nOverall test: PASS (PT modified and ULPFEC header intact)" << std::endl;
    return 0;
}