#pragma once
#include "log_record.h"

namespace taskhub::core {

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void onLog(const LogRecord& r) = 0;
};

} // namespace taskhub::core