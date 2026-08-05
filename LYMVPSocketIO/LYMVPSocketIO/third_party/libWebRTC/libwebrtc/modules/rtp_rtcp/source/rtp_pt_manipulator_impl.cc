#include "modules/rtp_rtcp/source/rtp_pt_manipulator_impl.h"

#include <cstring>
#include <map>
#include <set>

#include "rtc_base/checks.h"

namespace webrtc {

// WebRTC 中 RED 头只有 1 字节（F=0, PT），不是完整的 RFC 2198 多块结构。
// RED 负载的第一个字节是内层包的 Payload Type（低 7 位）。
static constexpr size_t kRedHeaderSize = 1;
// ULPFEC Level 0 Header 大小（RFC 5109）
static constexpr size_t kUlpfecLevel0HeaderSize = 10;

// === SdpMediaDescription 实现 ===
std::string SdpMediaDescription::GetCodecName(uint8_t pt) const {
  auto it = rtpmap.find(pt);
  if (it == rtpmap.end()) return "";
  const std::string& full = it->second;
  size_t slash = full.find('/');
  if (slash != std::string::npos) {
    return full.substr(0, slash);
  }
  return full;
}

bool SdpMediaDescription::IsCodec(uint8_t pt, const std::string& codec_name) const {
  std::string name = GetCodecName(pt);
  return !name.empty() && name == codec_name;
}

// === RtpPayloadTypes 实现 ===
void RtpPayloadTypes::Clear() {
  red_pts.clear();
  ulpfec_pts.clear();
  video_pts.clear();
  rtx_pts.clear();
}

bool RtpPayloadTypes::HasAny() const {
  return !red_pts.empty() || !ulpfec_pts.empty() || !video_pts.empty() || !rtx_pts.empty();
}

std::string RtpPayloadTypes::ToString() const {
  std::string out;
  auto append = [&](const char* name, const std::vector<uint8_t>& pts) {
    if (!pts.empty()) {
      out += name;
      out += "=";
      for (size_t i = 0; i < pts.size(); ++i) {
        out += std::to_string(pts[i]);
        if (i + 1 < pts.size()) out += ",";
      }
      out += " ";
    }
  };
  append("RED", red_pts);
  append("ULPFEC", ulpfec_pts);
  append("VIDEO", video_pts);
  append("RTX", rtx_pts);
  if (!out.empty()) out.pop_back();
  return out;
}

// === RtpPtManipulatorImpl 实现 ===
RtpPtManipulatorImpl::RtpPtManipulatorImpl() = default;
RtpPtManipulatorImpl::~RtpPtManipulatorImpl() = default;

void RtpPtManipulatorImpl::ConfigureSdp(const SdpMediaDescription& sdp) {
  red_pts_.clear();
  ulpfec_pts_.clear();
  video_pts_.clear();
  rtx_pts_.clear();

  // 常见视频编码名称列表（可根据需要扩展）
  static const std::set<std::string> kVideoCodecs = {
      "VP8", "VP9", "H264", "H265", "AV1", "VP8/90000", "VP9/90000"
  };

  for (const auto& kv : sdp.rtpmap) {
    std::string name = sdp.GetCodecName(kv.first);
    if (name == "red") {
      red_pts_.insert(kv.first);
    } else if (name == "ulpfec") {
      ulpfec_pts_.insert(kv.first);
    } else if (name == "rtx") {
      rtx_pts_.insert(kv.first);
    } else if (kVideoCodecs.find(name) != kVideoCodecs.end()) {
      video_pts_.insert(kv.first);
    }
  }
}

RtpPtManipulatorImpl::CodecType RtpPtManipulatorImpl::GetCodecType(uint8_t pt) const {
  if (red_pts_.find(pt) != red_pts_.end()) return kCodecRed;
  if (ulpfec_pts_.find(pt) != ulpfec_pts_.end()) return kCodecUlpfec;
  if (video_pts_.find(pt) != video_pts_.end()) return kCodecVideo;
  if (rtx_pts_.find(pt) != rtx_pts_.end()) return kCodecRtx;
  return kCodecUnknown;
}

bool RtpPtManipulatorImpl::IsRedPacket(uint8_t pt) const {
  return red_pts_.find(pt) != red_pts_.end();
}
bool RtpPtManipulatorImpl::IsUlpfecPacket(uint8_t pt) const {
  return ulpfec_pts_.find(pt) != ulpfec_pts_.end();
}
bool RtpPtManipulatorImpl::IsVideoPacket(uint8_t pt) const {
  return video_pts_.find(pt) != video_pts_.end();
}
bool RtpPtManipulatorImpl::IsRtxPacket(uint8_t pt) const {
  return rtx_pts_.find(pt) != rtx_pts_.end();
}

absl::optional<uint8_t> RtpPtManipulatorImpl::GetUlpfecPtRecovery(const RtpPacket& packet) const {
  uint8_t pt = packet.PayloadType();
  if (!IsUlpfecPacket(pt)) return absl::nullopt;
  if (packet.payload_size() < kUlpfecLevel0HeaderSize) return absl::nullopt;
  uint8_t pt_recovery = packet.payload()[1] & 0x7F;
  return pt_recovery;
}

RtpPayloadTypes RtpPtManipulatorImpl::ParsePtValues(const RtpPacket& packet) const {
  RtpPayloadTypes result;
  uint8_t outer_pt = packet.PayloadType();
  CodecType outer_type = GetCodecType(outer_pt);

  if (outer_type == kCodecRed) {
    result.red_pts.push_back(outer_pt);
    // RED 包：负载的第一个字节是内层 PT
    if (packet.payload_size() >= kRedHeaderSize) {
      uint8_t inner_pt = packet.payload()[0] & 0x7F;
      CodecType inner_type = GetCodecType(inner_pt);
      if (inner_type == kCodecUlpfec) {
        result.ulpfec_pts.push_back(inner_pt);
      } else if (inner_type == kCodecVideo) {
        result.video_pts.push_back(inner_pt);
      } else if (inner_type == kCodecRtx) {
        result.rtx_pts.push_back(inner_pt);
      }
    }
  } else if (outer_type == kCodecUlpfec) {
    result.ulpfec_pts.push_back(outer_pt);
  } else if (outer_type == kCodecVideo) {
    result.video_pts.push_back(outer_pt);
  } else if (outer_type == kCodecRtx) {
    result.rtx_pts.push_back(outer_pt);
  }
  return result;
}

bool RtpPtManipulatorImpl::ModifyPtValues(RtpPacket* packet,
                                          const std::vector<PtMappingRule>& mappings) {
  if (!packet) return false;
  uint8_t current_pt = packet->PayloadType();
  CodecType outer_type = GetCodecType(current_pt);

  // 构建映射表
  std::map<uint8_t, uint8_t> pt_map;
  for (const auto& m : mappings) {
    pt_map[m.old_pt] = m.new_pt;
  }

  // 非 RED 包：直接修改外层 PT
  if (outer_type != kCodecRed) {
    if (outer_type != kCodecUnknown) {
      auto it = pt_map.find(current_pt);
      if (it != pt_map.end()) {
        packet->SetPayloadType(it->second);
      }
    }
    return true;
  }

  // RED 包：需要修改外层 PT 和 RED 负载的第一个字节
  // 1. 修改外层 RTP PT
  auto it = pt_map.find(current_pt);
  if (it != pt_map.end()) {
    packet->SetPayloadType(it->second);
  }

  // 2. 修改内层 PT（RED 负载第一个字节）
  if (packet->payload_size() >= kRedHeaderSize) {
    rtc::CopyOnWriteBuffer buffer = packet->Buffer();
    uint8_t* payload_ptr = buffer.data() + packet->headers_size();
    uint8_t inner_pt = payload_ptr[0] & 0x7F;
    auto it2 = pt_map.find(inner_pt);
    if (it2 != pt_map.end()) {
      payload_ptr[0] = (payload_ptr[0] & 0x80) | (it2->second & 0x7F);
    }
    if (!packet->Parse(buffer)) {
      return false;
    }
  }
  return true;
}

bool RtpPtManipulatorImpl::ModifyPtValuesSimple(RtpPacket* packet,
                                                const RtpPayloadTypes& new_pt_values) {
  if (!packet) return false;
  uint8_t current_pt = packet->PayloadType();
  CodecType outer_type = GetCodecType(current_pt);

  std::vector<PtMappingRule> mappings;

  auto map_outer = [&](uint8_t old_pt, const std::vector<uint8_t>& new_pts) {
    if (!new_pts.empty()) {
      mappings.push_back({old_pt, new_pts[0]});
    }
  };
  if (outer_type == kCodecRed && !new_pt_values.red_pts.empty()) {
    map_outer(current_pt, new_pt_values.red_pts);
  } else if (outer_type == kCodecUlpfec && !new_pt_values.ulpfec_pts.empty()) {
    map_outer(current_pt, new_pt_values.ulpfec_pts);
  } else if (outer_type == kCodecVideo && !new_pt_values.video_pts.empty()) {
    map_outer(current_pt, new_pt_values.video_pts);
  } else if (outer_type == kCodecRtx && !new_pt_values.rtx_pts.empty()) {
    map_outer(current_pt, new_pt_values.rtx_pts);
  }

  // 如果是 RED 包，还需要添加内层 PT 的映射
  if (outer_type == kCodecRed && packet->payload_size() >= kRedHeaderSize) {
    uint8_t inner_pt = packet->payload()[0] & 0x7F;
    CodecType inner_type = GetCodecType(inner_pt);
    if (inner_type == kCodecUlpfec && !new_pt_values.ulpfec_pts.empty()) {
      mappings.push_back({inner_pt, new_pt_values.ulpfec_pts[0]});
    } else if (inner_type == kCodecVideo && !new_pt_values.video_pts.empty()) {
      mappings.push_back({inner_pt, new_pt_values.video_pts[0]});
    } else if (inner_type == kCodecRtx && !new_pt_values.rtx_pts.empty()) {
      mappings.push_back({inner_pt, new_pt_values.rtx_pts[0]});
    }
  }

  return ModifyPtValues(packet, mappings);
}

bool RtpPtManipulatorImpl::VerifyModification(const RtpPacket& packet,
                                              const std::vector<PtMappingRule>& expected_mappings) const {
  RtpPayloadTypes parsed = ParsePtValues(packet);
  std::set<uint8_t> actual_pts;
  auto add = [&](const std::vector<uint8_t>& pts) {
    for (uint8_t pt : pts) actual_pts.insert(pt);
  };
  add(parsed.red_pts);
  add(parsed.ulpfec_pts);
  add(parsed.video_pts);
  add(parsed.rtx_pts);

  std::set<uint8_t> expected_new_pts;
  std::set<uint8_t> expected_old_pts;
  for (const auto& m : expected_mappings) {
    expected_new_pts.insert(m.new_pt);
    expected_old_pts.insert(m.old_pt);
  }

  for (uint8_t pt : expected_new_pts) {
    if (actual_pts.find(pt) == actual_pts.end()) return false;
  }
  for (uint8_t pt : actual_pts) {
    if (expected_old_pts.find(pt) != expected_old_pts.end()) {
      bool is_unchanged = false;
      for (const auto& m : expected_mappings) {
        if (m.old_pt == pt && m.new_pt == pt) {
          is_unchanged = true;
          break;
        }
      }
      if (!is_unchanged) return false;
    }
  }
  return true;
}

}  // namespace webrtc