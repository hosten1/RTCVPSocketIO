#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PT_MANIPULATOR_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PT_MANIPULATOR_IMPL_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

struct SdpMediaDescription {
  std::string media_type;
  uint16_t port;
  std::string protocol;
  std::vector<uint8_t> payload_types;
  std::map<uint8_t, std::string> rtpmap;
  std::string address;

  std::string GetCodecName(uint8_t pt) const;
  bool IsCodec(uint8_t pt, const std::string& codec_name) const;
};

// 支持多 PT 映射（每种 codec 可以有多个 PT）
struct RtpPayloadTypes {
  std::vector<uint8_t> red_pts;
  std::vector<uint8_t> ulpfec_pts;
  std::vector<uint8_t> video_pts;   // 统一存储所有视频编码（VP8, VP9, H264 等）
  std::vector<uint8_t> rtx_pts;

  void Clear();
  bool HasAny() const;
  std::string ToString() const;
};

// 单次修改的映射规则
struct PtMappingRule {
  uint8_t old_pt;
  uint8_t new_pt;
};

class RtpPtManipulatorImpl {
 public:
  RtpPtManipulatorImpl();
  ~RtpPtManipulatorImpl();

  void ConfigureSdp(const SdpMediaDescription& sdp);

  // 解析包中的所有 PT（RTP 头 + RED 块）
  RtpPayloadTypes ParsePtValues(const RtpPacket& packet) const;

  // 通用修改：根据映射规则修改包中的 PT（零拷贝，不重新解析）
  bool ModifyPtValues(RtpPacket* packet, const std::vector<PtMappingRule>& mappings);

  // 便捷版本：使用新的 PT 值（只匹配每种类型的第一个 PT）
  bool ModifyPtValuesSimple(RtpPacket* packet, const RtpPayloadTypes& new_pt_values);

  // 严格验证：检查修改后的包中是否只包含期望的新 PT，且不包含任何旧 PT（除非未映射）
  bool VerifyModification(const RtpPacket& packet,
                          const std::vector<PtMappingRule>& expected_mappings) const;

  // 调试功能：解析 ULPFEC 包头中的 PT recovery 字段（仅用于日志，不能作为媒体类型）
  // 返回 PT recovery 值，如果包不是 ULPFEC 则返回 nullopt。
  absl::optional<uint8_t> GetUlpfecPtRecovery(const RtpPacket& packet) const;

 private:
  enum CodecType { kCodecRed, kCodecUlpfec, kCodecVideo, kCodecRtx, kCodecUnknown };
  CodecType GetCodecType(uint8_t pt) const;

  bool IsRedPacket(uint8_t pt) const;
  bool IsUlpfecPacket(uint8_t pt) const;
  bool IsVideoPacket(uint8_t pt) const;
  bool IsRtxPacket(uint8_t pt) const;

  // 缓存的 PT 集合（支持多 PT）
  std::set<uint8_t> red_pts_;
  std::set<uint8_t> ulpfec_pts_;
  std::set<uint8_t> video_pts_;   // 存储所有视频 codec 的 PT
  std::set<uint8_t> rtx_pts_;
};

}  // namespace webrtc

#endif