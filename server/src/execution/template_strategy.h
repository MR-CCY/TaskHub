#pragma once

#include "execution_strategy.h"

namespace taskhub::runner {

class TemplateExecutionStrategy : public IExecutionStrategy {
public:
    core::TaskResult execute(core::ExecutionContext& ctx) override;
};

} // namespace taskhub::runner
