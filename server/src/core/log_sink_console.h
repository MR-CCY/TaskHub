// log_sink_console.h
#pragma once
#include "log_sink.h"

namespace taskhub::core {

class ConsoleLogSink : public ILogSink {
public:
    void consume(const LogRecord& rec) override;
};

}