#pragma once
#include "log_record.h"

namespace taskhub::core {

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void consume(const LogRecord& rec) = 0;
};

} // namespace taskhub::core