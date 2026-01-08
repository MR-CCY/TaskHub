#pragma once

#include "execution_strategy.h"

namespace taskhub::runner {

class DagExecutionStrategy : public IExecutionStrategy {
public:
    core::TaskResult execute(core::ExecutionContext& ctx) override;
};

} // namespace taskhub::runner
