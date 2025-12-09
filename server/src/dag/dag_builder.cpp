#include "dag_builder.h"
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>
namespace taskhub::dag {
    void DagBuilder::addTask(const DagTaskSpec &spec)
    {
        _specs.push_back(spec);  
    }

    DagGraph DagBuilder::build()
    {
        DagGraph graph;

        for (const auto& spec : _specs) {
            auto node = graph.ensureNode(spec.id);
            node->setRunnerConfig(spec.runnerCfg);
        }
    
        for (const auto& spec : _specs) {
            auto node = graph.ensureNode(spec.id);
            for (const auto& depId : spec.deps) {
                node->addDependency(depId);
                auto depNode = graph.ensureNode(depId);
                depNode->addDownstream(spec.id);
            }
        }
    
        graph.recomputeIndegree();
        return graph;
    }
    
    /// \brief 验证DAG构建配置的有效性
    /// \details 检查所有依赖项是否存在以及图中是否存在循环依赖
    /// \return DagValidateResult 包含验证结果的对象，包括：
    ///         - ok: 验证是否通过
    ///         - errorMessage: 错误信息（如果有）
    ///         - cycleNodes: 形成循环的节点列表（如果有）

    DagValidateResult DagBuilder::validate() const
    {
        DagValidateResult result;

        // 为了校验方便，先建一个 id 集合
        std::unordered_set<std::string> ids;
        ids.reserve(_specs.size());
        for (const auto& spec : _specs) {
            ids.insert(spec.id.value);
        }
    
        // TODO Step 1：检查所有 deps 是否存在
        //   for spec in specs_:
        //     for dep in spec.deps:
        //        如果 ids 中找不到 dep -> result.ok=false, 写 errorMessage
        //        可以立即返回，也可以收集多个错误一起返回
        for (const auto& spec : _specs) {
            for (const auto& dep : spec.deps) {
                if (!dep.value.empty() && ids.find(dep.value) == ids.end()) {
                    result.ok = false;
                    result.errorMessage = "DagBuilder::validate()失败：未找到依赖项: " + dep.value;
                    return result;
                }
            }
        }
    
        // TODO Step 2：检查是否有环（可用 Kahn 算法）
        //
        //   简单流程：
        //     1. 构建本地邻接表 + 入度表（不要复用 DagGraph，以免逻辑耦合）
        //     2. 将入度=0 的节点入队
        //     3. 每次 pop 一个节点，把它的所有下游节点入度--，入度变 0 再入队
        //     4. 统计拓扑序列中节点数，如果 < 总节点数，则存在环
        //
        //   你也可以用 DFS + 颜色标记法（0=未访问,1=访问中,2=访问完成）
        //
        //   如检测到环，可尝试记录 cycleNodes（至少填几个节点 id 进去提示一下）
    
        // ===== 简易 Kahn 示例骨架（你可以重写） =====
        // 1. 构建图结构
        std::unordered_map<std::string, int> indegree;
        std::unordered_map<std::string, std::vector<std::string>> edges;
    
        for (const auto& spec : _specs) {
            indegree[spec.id.value] = 0;
        }
        for (const auto& spec : _specs) {
            for (const auto& dep : spec.deps) {
                edges[dep.value].push_back(spec.id.value);
                indegree[spec.id.value] += 1;
            }
        }
    
        std::queue<std::string> q;
        for (auto& [id, deg] : indegree) {
            if (deg == 0) q.push(id);
        }
    
        int visitedCount = 0;
        while (!q.empty()) {
            auto id = q.front(); q.pop();
            ++visitedCount;
            for (auto& nxt : edges[id]) {
                if (--indegree[nxt] == 0) {
                    q.push(nxt);
                }
            }
        }
    
        if (visitedCount != static_cast<int>(ids.size())) {
            result.ok = false;
            result.errorMessage = "DagBuilder::validate()失败：在DAG中检测到环";
            std::unordered_map<std::string, int> color;
            std::unordered_map<std::string, size_t> stackPos;
            std::vector<std::string> stack;
            stack.reserve(ids.size());
            color.reserve(ids.size());
            stackPos.reserve(ids.size());

            std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
                color[node] = 1;
                stackPos[node] = stack.size();
                stack.push_back(node);

                auto it = edges.find(node);
                if (it != edges.end()) {
                    for (const auto& nxt : it->second) {
                        const auto colorIt = color.find(nxt);
                        const int c = colorIt == color.end() ? 0 : colorIt->second;
                        if (c == 0) {
                            if (dfs(nxt)) return true;
                        } else if (c == 1) {
                            const size_t start = stackPos[nxt];
                            for (size_t i = start; i < stack.size(); ++i) {
                                result.cycleNodes.push_back(core::TaskId{stack[i]});
                            }
                            return true;
                        }
                    }
                }

                color[node] = 2;
                stackPos.erase(node);
                stack.pop_back();
                return false;
            };

            for (const auto& id : ids) {
                if (color.find(id) == color.end() && dfs(id)) break;
            }
        }
    
        return result;
    }
}
