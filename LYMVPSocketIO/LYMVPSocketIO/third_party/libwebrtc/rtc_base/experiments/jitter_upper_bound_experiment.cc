/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/jitter_upper_bound_experiment.h"

#include <stdio.h>

#include <string>
#ifdef USE_MEDIASOUP_ClASS
#include "Logger.hpp"
#else
//#include "rtc_base/logging.h"
#endif

#include "system_wrappers/include/field_trial.h"

namespace webrtc {

const char JitterUpperBoundExperiment::kJitterUpperBoundExperimentName[] =
    "WebRTC-JitterUpperBound";

absl::optional<double> JitterUpperBoundExperiment::GetUpperBoundSigmas() {
  if (!field_trial::IsEnabled(kJitterUpperBoundExperimentName)) {
    return absl::nullopt;
  }
  const std::string group =
      webrtc::field_trial::FindFullName(kJitterUpperBoundExperimentName);

  double upper_bound_sigmas;
  if (sscanf(group.c_str(), "Enabled-%lf", &upper_bound_sigmas) != 1) {
#ifndef USE_MEDIASOUP_ClASS
//    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
#else
      MS_DEBUG_TAG(bwe, "Invalid number of parameters provided.");
#endif
    return absl::nullopt;
  }

  if (upper_bound_sigmas < 0) {
#ifndef USE_MEDIASOUP_ClASS
//    RTC_LOG(LS_WARNING) << "Invalid jitter upper bound sigmas, must be >= 0.0: "
//                        << upper_bound_sigmas;
#else
      MS_DEBUG_TAG(bwe, "Invalid jitter upper bound sigmas, must be >= 0.0: %lf",upper_bound_sigmas);
#endif
    return absl::nullopt;
  }

  return upper_bound_sigmas;
}

}  // namespace webrtc
