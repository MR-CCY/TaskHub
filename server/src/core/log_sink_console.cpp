// log_sink_console.cpp
#include "log_sink_console.h"
#include <iostream>
#include "log_formatter.h"
namespace taskhub::core {


void ConsoleLogSink::consume(const LogRecord& rec) {
    const std::string line =LogFormatter::instance().formatLine(rec);
    if(rec.level >= LogLevel::Error){
        std::cerr << line << "\n";
    }else{ 
        std::cout << line << "\n";
    }
}
}