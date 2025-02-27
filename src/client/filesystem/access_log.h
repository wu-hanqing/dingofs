/*
 *  Copyright (c) 2023 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * Project: Dingofs
 * Created Date: 2023-03-17
 * Author: Jingli Chen (Wine93)
 */

#include <butil/time.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "dingofs/metaserver.pb.h"
#include "client/common/config.h"

#ifndef DINGOFS_SRC_CLIENT_FILESYSTEM_ACCESS_LOG_H_
#define DINGOFS_SRC_CLIENT_FILESYSTEM_ACCESS_LOG_H_

namespace dingofs {
namespace client {
namespace common {

DECLARE_bool(access_logging);

}
namespace filesystem {

using ::absl::StrFormat;
using ::dingofs::client::common::FLAGS_access_logging;
using MessageHandler = std::function<std::string()>;

static std::shared_ptr<spdlog::logger> Logger;

bool InitAccessLog(const std::string& prefix) {
  std::string filename = StrFormat("%s/access_%d.log", prefix, getpid());
  Logger = spdlog::daily_logger_mt("fuse_access", filename, 0, 0);
  spdlog::flush_every(std::chrono::seconds(1));
  return true;
}

struct AccessLogGuard {
  explicit AccessLogGuard(MessageHandler handler)
      : enable(FLAGS_access_logging), handler(handler) {
    if (!enable) {
      return;
    }

    timer.start();
  }

  ~AccessLogGuard() {
    if (!enable) {
      return;
    }

    timer.stop();
    Logger->info("{0} <{1:.6f}>", handler(), timer.u_elapsed() / 1e6);
  }

  bool enable;
  MessageHandler handler;
  butil::Timer timer;
};

}  // namespace filesystem
}  // namespace client
}  // namespace dingofs

#endif  // DINGOFS_SRC_CLIENT_FILESYSTEM_ACCESS_LOG_H_
