/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define MS_CLASS "webrtc::PacedSender"
// #define MS_LOG_DEV_LEVEL 3

#include "modules/pacing/paced_sender.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "system_wrappers/include/field_trial.h" // webrtc::field_trial.
#ifdef USE_MEDIASOUP_ClASS
#include "Logger.hpp"
#include "RTC/RtpPacket.hpp"
#endif

#include <absl/memory/memory.h>
#include <algorithm>
#include <utility>

namespace webrtc {
namespace {
// Time limit in milliseconds between packet bursts.
const int64_t kDefaultMinPacketLimitMs = 5;
const int64_t kCongestedPacketIntervalMs = 500;
const int64_t kPausedProcessIntervalMs = kCongestedPacketIntervalMs;
const int64_t kMaxElapsedTimeMs = 2000;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int64_t kMaxIntervalTimeMs = 30;

}  // namespace
const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(Clock* clock,
                         PacketRouter* packet_router,
                         const WebRtcKeyValueConfig* field_trials)
    : clock_(clock),
      packet_router_(packet_router),
      fallback_field_trials_(
          !field_trials ? absl::make_unique<FieldTrialBasedConfig>() : nullptr),
      field_trials_(field_trials ? field_trials : fallback_field_trials_.get()),
      min_packet_limit_ms_("", kDefaultMinPacketLimitMs),
      last_timestamp_ms_(clock_->TimeInMilliseconds()),
      paused_(false),
      media_budget_(0),
      padding_budget_(0),
      prober_(*field_trials_),
      probing_send_failure_(false),
      pacing_bitrate_kbps_(0),
      time_last_process_us_(clock_->TimeInMilliseconds()),
      first_sent_packet_ms_(-1),
      packet_counter_(0),
      account_for_audio_(false) {
  ParseFieldTrial({&min_packet_limit_ms_},
                  webrtc::field_trial::FindFullName("WebRTC-Pacer-MinPacketLimitMs"));
  UpdateBudgetWithElapsedTime(min_packet_limit_ms_);
}

void PacedSender::CreateProbeCluster(int bitrate_bps, int cluster_id) {
  // TODO: REMOVE
  // MS_DEBUG_DEV("---- bitrate_bps:%d, cluster_id:%d", bitrate_bps, cluster_id);

  prober_.CreateProbeCluster(bitrate_bps,TimeMilliseconds(), cluster_id);
}

void PacedSender::Pause() {
  if (!paused_)
#ifdef USE_MEDIASOUP_ClASS
    MS_DEBUG_DEV("paused");
#else
    ;
#endif

  paused_ = true;
}

void PacedSender::Resume() {
  if (paused_)
#ifdef USE_MEDIASOUP_ClASS
    MS_DEBUG_DEV("resumed");
#else
    ;
#endif
    

  paused_ = false;
}

void PacedSender::SetCongestionWindow(int64_t congestion_window_bytes) {
  congestion_window_bytes_ = congestion_window_bytes;
}

void PacedSender::UpdateOutstandingData(int64_t outstanding_bytes) {
  outstanding_bytes_ = outstanding_bytes;
}

bool PacedSender::Congested() const {
  if (congestion_window_bytes_ == kNoCongestionWindow)
    return false;
  return outstanding_bytes_ >= congestion_window_bytes_;
}

int64_t PacedSender::TimeMilliseconds() const {
  int64_t time_ms = clock_->TimeInMilliseconds();
  if (time_ms < last_timestamp_ms_) {
//    RTC_LOG(LS_WARNING)
//        << "Non-monotonic clock behavior observed. Previous timestamp: "
//        << last_timestamp_ms_ << ", new timestamp: " << time_ms;
    RTC_DCHECK_GE(time_ms, last_timestamp_ms_);
    time_ms = last_timestamp_ms_;
  }
  last_timestamp_ms_ = time_ms;
  return time_ms;
}
void PacedSender::SetProbingEnabled(bool enabled) {
#ifdef USE_MEDIASOUP_ClASS
  // RTC_CHECK_EQ(0, packet_counter_);
  MS_ASSERT(packet_counter_ == 0, "packet counter must be 0");
#endif
  prober_.SetEnabled(enabled);
}

void PacedSender::SetPacingRates(uint32_t pacing_rate_bps,
                                 uint32_t padding_rate_bps) {
#ifdef USE_MEDIASOUP_ClASS
  // RTC_DCHECK(pacing_rate_bps > 0);
  MS_ASSERT(pacing_rate_bps > 0, "pacing rate must be > 0");
#endif
  pacing_bitrate_kbps_ = pacing_rate_bps / 1000;
  padding_budget_.set_target_rate_kbps(padding_rate_bps / 1000);

  // TODO: REMOVE
  // MS_DEBUG_DEV("[pacer_updated pacing_kbps:%" PRIu32 ", padding_budget_kbps:%" PRIu32 "]",
  //              pacing_bitrate_kbps_,
  //              padding_rate_bps / 1000);
}

void PacedSender::InsertPacket(size_t bytes) {
#ifdef USE_MEDIASOUP_ClASS
  // RTC_DCHECK(pacing_bitrate_kbps_ > 0)
  //     << "SetPacingRate must be called before InsertPacket.";
  MS_ASSERT(pacing_bitrate_kbps_ > 0, "SetPacingRates() must be called before InsertPacket()");
#endif

  prober_.OnIncomingPacket(bytes);

  packet_counter_++;

  // MS_NOTE: Since we don't send media packets within ::Process(),
  // we use this callback to acknowledge sent packets.
  OnPacketSent(bytes);
}

void PacedSender::SetAccountForAudioPackets(bool account_for_audio) {
  account_for_audio_ = account_for_audio;
}

int64_t PacedSender::TimeUntilNextProcess() {
  int64_t elapsed_time_us =
    clock_->TimeInMicroseconds() - time_last_process_us_;
  int64_t elapsed_time_ms = (elapsed_time_us + 500) / 1000;
  // When paused we wake up every 500 ms to send a padding packet to ensure
  // we won't get stuck in the paused state due to no feedback being received.
  if (paused_)
    return std::max<int64_t>(kPausedProcessIntervalMs - elapsed_time_ms, 0);

  if (prober_.IsProbing()) {
    int64_t ret = prober_.TimeUntilNextProbe(TimeMilliseconds());
    if (ret > 0 || (ret == 0 && !probing_send_failure_))
      return ret;
  }
  return std::max<int64_t>(min_packet_limit_ms_ - elapsed_time_ms, 0);
}

int64_t PacedSender::UpdateTimeAndGetElapsedMs(int64_t now_us) {
  int64_t elapsed_time_ms = (now_us - time_last_process_us_ + 500) / 1000;
  time_last_process_us_ = now_us;
  if (elapsed_time_ms > kMaxElapsedTimeMs) {
#ifdef USE_MEDIASOUP_ClASS
    MS_WARN_TAG(bwe, "elapsed time (%" PRIi64 " ms) longer than expected,"
                     " limiting to %" PRIi64 " ms",
                        elapsed_time_ms,
                        kMaxElapsedTimeMs);
#endif
    elapsed_time_ms = kMaxElapsedTimeMs;
  }
  return elapsed_time_ms;
}

void PacedSender::Process() {
  int64_t now_us = clock_->TimeInMicroseconds();
  int64_t elapsed_time_ms = UpdateTimeAndGetElapsedMs(now_us);

  if (paused_)
    return;

  if (elapsed_time_ms > 0) {
    int target_bitrate_kbps = pacing_bitrate_kbps_;
    media_budget_.set_target_rate_kbps(target_bitrate_kbps);
    UpdateBudgetWithElapsedTime(elapsed_time_ms);
  }

  if (!prober_.IsProbing())
    return;

  PacedPacketInfo pacing_info;
  absl::optional<size_t> recommended_probe_size;

  pacing_info = prober_.CurrentCluster();
  recommended_probe_size = prober_.RecommendedMinProbeSize();

  size_t bytes_sent = 0;
#ifdef USE_MEDIASOUP_ClASS
  // MS_NOTE: Let's not use a useless vector.
  RTC::RtpPacket* padding_packet{ nullptr };
#endif

  // Check if we should send padding.
  while (true)
  {
    size_t padding_bytes_to_add =
      PaddingBytesToAdd(recommended_probe_size, bytes_sent);

    if (padding_bytes_to_add == 0)
      break;
#ifdef USE_MEDIASOUP_ClASS
    // TODO: REMOVE
    // MS_DEBUG_DEV(
    //   "[recommended_probe_size:%zu, padding_bytes_to_add:%zu]",
    //   *recommended_probe_size, padding_bytes_to_add);

    padding_packet =
      packet_router_->GeneratePadding(padding_bytes_to_add);

    // TODO: REMOVE.
    // MS_DEBUG_DEV("sending padding packet [size:%zu]", padding_packet->GetSize());

    packet_router_->SendPacket(padding_packet, pacing_info);
    bytes_sent += padding_packet->GetSize();
#endif


    if (recommended_probe_size && bytes_sent > *recommended_probe_size)
      break;
  }

  if (bytes_sent != 0)
  {
    auto now =  clock_->TimeInMicroseconds();
    OnPaddingSent(now, bytes_sent);
    prober_.ProbeSent((now + 500) / 1000, bytes_sent);
  }
}

size_t PacedSender::PaddingBytesToAdd(
    absl::optional<size_t> recommended_probe_size,
    size_t bytes_sent) {

  // Don't add padding if congested, even if requested for probing.
  if (Congested()) {
    return 0;
  }

  // MS_NOTE: This does not apply to mediasoup.
  // We can not send padding unless a normal packet has first been sent. If we
  // do, timestamps get messed up.
  // if (packet_counter_ == 0) {
  //   return 0;
  // }

  if (recommended_probe_size) {
    if (*recommended_probe_size > bytes_sent) {
      return *recommended_probe_size - bytes_sent;
    }
    return 0;
  }

  return padding_budget_.bytes_remaining();
}

void PacedSender::OnPacketSent(size_t size) {
  if (first_sent_packet_ms_ == -1)
      first_sent_packet_ms_ = TimeMilliseconds();

  // Update media bytes sent.
  UpdateBudgetWithBytesSent(size);
//  last_send_time_us_ = clock_->TimeInMicroseconds();
}

PacedPacketInfo PacedSender::GetPacingInfo() {
  PacedPacketInfo pacing_info;

  if (prober_.IsProbing()) {
    pacing_info = prober_.CurrentCluster();
  }

  return pacing_info;
}

void PacedSender::OnPaddingSent(int64_t now, size_t bytes_sent) {
  if (bytes_sent > 0) {
    UpdateBudgetWithBytesSent(bytes_sent);
  }
}

void PacedSender::UpdateBudgetWithElapsedTime(int64_t delta_time_ms) {
  delta_time_ms = std::min(kMaxIntervalTimeMs, delta_time_ms);
  media_budget_.IncreaseBudget(delta_time_ms);
  padding_budget_.IncreaseBudget(delta_time_ms);
}

void PacedSender::UpdateBudgetWithBytesSent(size_t bytes_sent) {
  outstanding_bytes_ += bytes_sent;
  media_budget_.UseBudget(bytes_sent);
  padding_budget_.UseBudget(bytes_sent);
}

}  // namespace webrtc
