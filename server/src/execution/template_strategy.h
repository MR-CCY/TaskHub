#pragma once

#include "execution_strategy.h"

namespace taskhub::runner {

class TemplateExecutionStrategy : public IExecutionStrategy {
public:
    core::TaskResult execute(const core::TaskConfig& cfg,
                             std::atomic_bool* cancelFlag,
                             Deadline deadline) override;
};

} // namespace taskhub::runner
