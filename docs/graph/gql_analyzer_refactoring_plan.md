# GQL Analyzer 架构重构计划

## 一、背景与目标

### 1.1 重构动机

当前 GQL 实现采用 **direct planner** 架构，直接从 AST 生成 QueryPlan，存在以下问题：

1. **缺少语义分析层**：没有独立的 analyze/bind 阶段，名字解析和类型推导混杂在 planning 中
2. **MatchStep 设计受限**：继承 `ISourceStep`，无法接受输入，无法支持 correlated subquery
3. **WHERE 无法下推**：WHERE 在 MatchStep 之后作为 FilterStep，无法下推到图存储层
4. **代码复用不足**：RETURN/WHERE/ORDER BY 等未充分复用 ClickHouse 能力

### 1.2 重构目标

1. **长期视角**：追求代码风格工整，参考 `InterpreterSelectQueryAnalyzer` 的 4 阶段模式
2. **最大化复用**：WHERE/RETURN/ORDER BY/LIMIT 等复用 ClickHouse 的 QueryNode 和 Step
3. **分层清晰**：AST → QueryTree → QueryPlan 三阶段分离
4. **支持优化**：WHERE 下推、pattern 优化、JOIN 重排等

### 1.3 参考架构

ClickHouse Analyzer 的 4 阶段模式：

```
SQL → Parser → AST
           ↓
    InterpreterSelectQueryAnalyzer
    ├─ normalize (规范化 AST)
    ├─ buildQueryTree (AST → QueryTree)
    ├─ QueryTreePassManager (名字解析 + 优化)
    └─ Planner (QueryTree → QueryPlan)
           ↓
    QueryPlan → QueryPipeline → Executors
```

## 二、架构设计

### 2.1 整体流程

```
GQL Query
    ↓
ParserGQLQuery → GQL AST (GQLSingleQuery/GQLCombinedQuery)
    ↓
InterpreterGQLQuery::normalize() → 规范化 AST
    ↓
GQLQueryTreeBuilder::build() → GQL QueryTree (GQLQueryNode)
    ↓
GQLQueryTreePassManager::run() → 名字解析 + 类型推导 + 优化
    ↓
GQLPlanner::buildQueryPlan() → QueryPlan (MatchStep + FilterStep + ...)
    ↓
QueryPlan::buildQueryPipeline() → QueryPipeline
    ↓
Executors → Result
```

### 2.2 GQL QueryTree 节点体系

新增 7 种 GQL 特有节点类型：

| 节点类型 | 继承关系 | 职责 |
|---------|---------|------|
| `GQLQueryNode` | 继承 `QueryNode` | GQL 查询根节点，复用 projection/where/order_by/limit |
| `GQLUseNode` | 继承 `IQueryTreeNode` | USE 子句，存储图引用 |
| `GQLMatchNode` | 继承 `IQueryTreeNode` | MATCH 子句，存储 path patterns + WHERE |
| `GQLPathPatternNode` | 继承 `IQueryTreeNode` | 路径模式，存储 node/edge 序列 |
| `GQLNodePatternNode` | 继承 `IQueryTreeNode` | 节点模式，存储 label/property/WHERE |
| `GQLEdgePatternNode` | 继承 `IQueryTreeNode` | 边模式，存储 direction/label/property/quantifier |
| `GQLQuantifiedPatternNode` | 继承 `IQueryTreeNode` | 量化路径模式（可选，用于 `{1,5}` 等） |

**关键设计**：`GQLQueryNode` 继承 `QueryNode`，复用 17 个 child 索引：
- `projection_child_index` → RETURN
- `where_child_index` → WHERE
- `order_by_child_index` → ORDER BY
- `limit_child_index` → LIMIT
- 新增 `use_child_index` → USE
- 新增 `matches_child_index` → MATCH 列表

### 2.3 节点类定义

#### GQLQueryNode

```cpp
class GQLQueryNode : public QueryNode
{
public:
    // 继承 QueryNode 的所有接口
    // 新增 GQL 特有的 child 索引
    static constexpr size_t use_child_index = 17;
    static constexpr size_t matches_child_index = 18;
    
    // USE 子句访问
    QueryTreeNodePtr & getUse() { return children[use_child_index]; }
    const QueryTreeNodePtr & getUse() const { return children[use_child_index]; }
    
    // MATCH 列表访问（ListNode 包含多个 GQLMatchNode）
    QueryTreeNodePtr & getMatches() { return children[matches_child_index]; }
    const QueryTreeNodePtr & getMatches() const { return children[matches_child_index]; }
};
```

#### GQLUseNode

```cpp
class GQLUseNode : public IQueryTreeNode
{
public:
    // 图引用（可以是 identifier 或 table function）
    QueryTreeNodePtr graph_expression;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_USE; }
};
```

#### GQLMatchNode

```cpp
class GQLMatchNode : public IQueryTreeNode
{
public:
    // OPTIONAL 标记
    bool is_optional = false;
    
    // 路径模式列表（ListNode 包含多个 GQLPathPatternNode）
    QueryTreeNodePtr path_patterns;
    
    // MATCH 级别的 WHERE（可选）
    QueryTreeNodePtr where_expression;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_MATCH; }
};
```

#### GQLPathPatternNode

```cpp
class GQLPathPatternNode : public IQueryTreeNode
{
public:
    // 路径变量名（可选）
    std::optional<String> path_variable;
    
    // 元素序列（ListNode 包含 GQLNodePatternNode 和 GQLEdgePatternNode 交替）
    QueryTreeNodePtr elements;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PATH_PATTERN; }
};
```

#### GQLNodePatternNode

```cpp
class GQLNodePatternNode : public IQueryTreeNode
{
public:
    // 节点变量名（可选）
    std::optional<String> variable;
    
    // 标签表达式（可选，可以是 identifier 或 label expression）
    QueryTreeNodePtr label_expression;
    
    // 属性约束（可选，WHERE 表达式）
    QueryTreeNodePtr where_expression;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_NODE_PATTERN; }
};
```

#### GQLEdgePatternNode

```cpp
class GQLEdgePatternNode : public IQueryTreeNode
{
public:
    enum class Direction { Left, Right, Undirected };
    
    // 边变量名（可选）
    std::optional<String> variable;
    
    // 方向
    Direction direction = Direction::Right;
    
    // 标签表达式（可选）
    QueryTreeNodePtr label_expression;
    
    // 属性约束（可选）
    QueryTreeNodePtr where_expression;
    
    // 量化器（可选，GQLQuantifiedPatternNode）
    QueryTreeNodePtr quantifier;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_EDGE_PATTERN; }
};
```

#### GQLQuantifiedPatternNode

```cpp
class GQLQuantifiedPatternNode : public IQueryTreeNode
{
public:
    // 最小重复次数
    size_t min_count = 1;
    
    // 最大重复次数（std::nullopt 表示无限）
    std::optional<size_t> max_count;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_QUANTIFIED_PATTERN; }
};
```

## 三、GQLQueryTreeBuilder 设计

### 3.1 入口函数

```cpp
// src/Analyzer/GQLQueryTreeBuilder.h
namespace DB::GQL
{
    QueryTreeNodePtr buildQueryTree(const ASTPtr & query, const ContextPtr & context);
}
```

### 3.2 构建流程

```
buildQueryTree(GQLSingleQuery)
  ├─ 创建 GQLQueryNode
  ├─ 遍历 clauses
  │   ├─ GQLUseClause → buildUseNode → 设置 GQLQueryNode::use
  │   ├─ GQLMatchClause → buildMatchNode → 添加到 GQLQueryNode::matches
  │   ├─ GQLWhereClause → buildExpression → 设置 GQLQueryNode::where
  │   ├─ GQLReturnClause → buildProjection → 设置 GQLQueryNode::projection
  │   ├─ GQLOrderByClause → buildOrderBy → 设置 GQLQueryNode::order_by
  │   └─ GQLLimitClause → buildLimit → 设置 GQLQueryNode::limit
  └─ 返回 GQLQueryNode

buildQueryTree(GQLCombinedQuery)
  ├─ 创建 UnionNode
  ├─ 递归构建每个子查询
  └─ 设置 union_mode（UNION_ALL/UNION_DISTINCT/EXCEPT/INTERSECT）
```

### 3.3 MATCH 构建（复用现有 PatternBinder）

```cpp
QueryTreeNodePtr buildMatchNode(const GQLMatchClause & clause, const ContextPtr & context)
{
    auto match_node = std::make_shared<GQLMatchNode>();
    match_node->is_optional = clause.is_optional;
    
    // 复用现有 PatternBinder 提取 binding 信息
    PatternBinder binder;
    auto bound_patterns = binder.bindMatchClause(clause);
    
    // 转换为 QueryTree 节点
    auto path_list = std::make_shared<ListNode>();
    for (const auto & pattern : bound_patterns)
        path_list->getNodes().push_back(buildPathPattern(pattern));
    
    match_node->path_patterns = path_list;
    
    if (clause.where)
        match_node->where_expression = buildExpression(clause.where, context);
    
    return match_node;
}
```

### 3.4 表达式构建（复用 ClickHouse）

```cpp
// WHERE/RETURN 中的表达式直接复用 ClickHouse 的 buildExpression
QueryTreeNodePtr buildExpression(const ASTPtr & ast, const ContextPtr & context)
{
    // 复用 src/Analyzer/QueryTreeBuilder.cpp 的 buildExpression
    return DB::buildExpression(ast, context);
}
```

### 3.5 Combined Query 处理

```cpp
QueryTreeNodePtr buildQueryTree(const GQLCombinedQuery & query, const ContextPtr & context)
{
    auto union_node = std::make_shared<UnionNode>();
    
    // 递归构建子查询
    for (const auto & child : query.queries)
        union_node->getQueries().push_back(buildQueryTree(child, context));
    
    // 设置 union mode
    auto mode = getCombinedPlanMode(query);
    union_node->setUnionMode(convertToUnionMode(mode));
    union_node->setDistinct(needsDistinct(mode));
    
    return union_node;
}
```

## 四、GQLQueryTreePassManager 设计

### 4.1 必需 Pass

| Pass 名称 | 职责 | 优先级 |
|----------|------|--------|
| `GQLQueryAnalysisPass` | 名字解析、类型推导、作用域管理 | 最高（必须第一个运行） |
| `GQLPatternNormalizationPass` | 规范化 pattern（展开量化器、合并连续节点） | 高 |
| `GQLWhereExtractionPass` | 提取 MATCH WHERE 到顶层 WHERE | 中 |

### 4.2 可选优化 Pass

| Pass 名称 | 职责 | 触发条件 |
|----------|------|---------|
| `GQLPatternSimplificationPass` | 简化 pattern（去除冗余约束） | 启用优化 |
| `GQLPredicatePushdownPass` | 下推谓词到 pattern | 启用优化 |
| `GQLMatchReorderingPass` | 重排 MATCH 顺序（基于选择性） | 启用优化 |

### 4.3 Pass 执行顺序

```cpp
void GQLQueryTreePassManager::run(QueryTreeNodePtr & query_tree)
{
    // 1. 必需：名字解析和类型推导
    GQLQueryAnalysisPass().run(query_tree, context);
    
    // 2. 必需：pattern 规范化
    GQLPatternNormalizationPass().run(query_tree, context);
    
    if (!options.ignore_optimizations)
    {
        // 3. 可选：pattern 简化
        GQLPatternSimplificationPass().run(query_tree, context);
        
        // 4. 可选：谓词下推
        GQLPredicatePushdownPass().run(query_tree, context);
        
        // 5. 可选：MATCH 重排
        GQLMatchReorderingPass().run(query_tree, context);
    }
    
    // 6. WHERE 提取（必须在优化后）
    GQLWhereExtractionPass().run(query_tree, context);
}
```

## 五、GQLPlanner 设计

### 5.1 继承策略

```cpp
// src/Planner/GQLPlanner.h
class GQLPlanner
{
public:
    GQLPlanner(const QueryTreeNodePtr & query_tree, const SelectQueryOptions & options, ContextPtr context);
    
    void buildQueryPlanIfNeeded();
    QueryPlan & getQueryPlan() { return query_plan; }
    
private:
    void buildPlanForGQLQuery(const GQLQueryNode & query_node);
    void buildPlanForUnion(const UnionNode & union_node);
    
    QueryTreeNodePtr query_tree;
    SelectQueryOptions options;
    ContextPtr context;
    QueryPlan query_plan;
};
```

**不继承 ClickHouse 的 `Planner`**，因为：
1. GQL 的 source 构建逻辑完全不同（MATCH vs FROM）
2. 避免继承带来的复杂性
3. 但可以复用 `Planner` 的 helper 函数（如 `buildPrewhereActions`）

### 5.2 MATCH 处理

```cpp
void GQLPlanner::buildPlanForGQLQuery(const GQLQueryNode & query_node)
{
    // 1. 处理 USE（设置 active graph）
    if (query_node.getUse())
        processUseClause(query_node.getUse());
    
    // 2. 构建 MATCH source
    if (query_node.getMatches())
    {
        auto match_spec = buildMatchSpec(query_node.getMatches(), query_node.getWhere());
        auto match_step = std::make_unique<MatchStep>(std::move(match_spec), source_factory);
        query_plan.addStep(std::move(match_step));
    }
    
    // 3. 复用 ClickHouse 的 projection/order_by/limit 处理
    if (query_node.getProjection())
        buildProjection(query_node.getProjection());
    
    if (query_node.getOrderBy())
        buildOrderBy(query_node.getOrderBy());
    
    if (query_node.getLimit())
        buildLimit(query_node.getLimit());
}
```

### 5.3 WHERE 下推策略

```cpp
MatchSpec buildMatchSpec(const QueryTreeNodePtr & matches, const QueryTreeNodePtr & where)
{
    MatchSpec spec;
    
    // 1. 从 GQLMatchNode 列表构建 clause specs
    for (const auto & match : matches->as<ListNode>()->getNodes())
    {
        auto & match_node = match->as<GQLMatchNode>();
        spec.clauses.push_back(buildClauseSpec(match_node));
    }
    
    // 2. WHERE 下推分析
    if (where)
    {
        auto predicates = extractPredicates(where);
        for (const auto & pred : predicates)
        {
            if (canPushdownToMatch(pred))
            {
                // 下推到 MatchSpec
                spec.filter_predicates.push_back(pred);
            }
            else
            {
                // 保留在 QueryPlan 的 FilterStep
                spec.post_filter_predicates.push_back(pred);
            }
        }
    }
    
    return spec;
}
```

**下推规则**：
- 可下推：`n.id = 1`、`e.weight > 10`（直接引用 pattern 变量）
- 不可下推：`n.id = m.id`（跨 pattern 引用）、`count(*) > 5`（聚合函数）

### 5.4 其他子句处理（复用 ClickHouse）

```cpp
void GQLPlanner::buildProjection(const QueryTreeNodePtr & projection)
{
    // 复用 ClickHouse Planner 的 buildProjection
    auto actions = buildProjectionActions(projection, context);
    auto step = std::make_unique<ExpressionStep>(query_plan.getCurrentHeader(), std::move(actions));
    query_plan.addStep(std::move(step));
}

void GQLPlanner::buildOrderBy(const QueryTreeNodePtr & order_by)
{
    // 复用 ClickHouse Planner 的 buildOrderBy
    auto sort_description = buildSortDescription(order_by, context);
    auto step = std::make_unique<SortingStep>(query_plan.getCurrentHeader(), sort_description);
    query_plan.addStep(std::move(step));
}
```

### 5.5 新 MatchStep 设计

**关键变更**：从 `ISourceStep` 改为 `ITransformingStep`，支持接受输入。

```cpp
// src/Processors/QueryPlan/Graph/MatchStep.h
class MatchStep final : public ITransformingStep
{
public:
    MatchStep(
        const Header & input_header_,
        MatchSpec match_spec_,
        MatchSourceFactoryPtr source_factory_);
    
    String getName() const override { return "GraphMatch"; }
    
    void transformPipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;
    
private:
    MatchSpec match_spec;
    MatchSourceFactoryPtr source_factory;
};
```

**优势**：
1. 支持 correlated subquery（可以从外层接收绑定变量）
2. 支持 MATCH 后接 MATCH（pipeline 串联）
3. 与 ClickHouse 其他 step 一致

## 六、InterpreterGQLQuery 重构

### 6.1 四阶段实现

```cpp
// src/Interpreters/InterpreterGQLQuery.h
class InterpreterGQLQuery : public IInterpreter
{
public:
    InterpreterGQLQuery(ASTPtr query_ptr_, ContextPtr context_, GQL::PlanEnvironment environment_);
    
    BlockIO execute() override;
    QueryPlan & getQueryPlan();
    
private:
    // 阶段 1：规范化
    ASTPtr normalizeQuery(const ASTPtr & query);
    
    // 阶段 2：构建 QueryTree
    QueryTreeNodePtr buildQueryTree();
    
    // 阶段 3：运行 passes
    void runQueryTreePasses();
    
    // 阶段 4：构建 QueryPlan
    void buildQueryPlan();
    
    ASTPtr query_ptr;
    ContextMutablePtr context;
    GQL::PlanEnvironment environment;
    
    QueryTreeNodePtr query_tree;
    std::unique_ptr<GQL::GQLPlanner> planner;
};
```

### 6.2 execute 实现

```cpp
BlockIO InterpreterGQLQuery::execute()
{
    // 1. 规范化
    auto normalized_query = normalizeQuery(query_ptr);
    
    // 2. 构建 QueryTree
    query_tree = GQL::buildQueryTree(normalized_query, context);
    
    // 3. 运行 passes
    GQL::GQLQueryTreePassManager pass_manager(context);
    pass_manager.run(query_tree);
    
    // 4. 构建 QueryPlan
    planner = std::make_unique<GQL::GQLPlanner>(query_tree, SelectQueryOptions{}, context);
    planner->buildQueryPlanIfNeeded();
    
    // 5. 构建 QueryPipeline
    auto & query_plan = planner->getQueryPlan();
    auto builder = query_plan.buildQueryPipeline(
        QueryPlanOptimizationSettings(context),
        BuildQueryPipelineSettings(context));
    
    BlockIO result;
    result.pipeline = QueryPipelineBuilder::getPipeline(std::move(*builder));
    return result;
}
```

## 七、实施路线图

### 7.1 阶段划分

| 阶段 | 目标 | 关键文件 | 预计工作量 |
|------|------|---------|-----------|
| **Phase 1**: QueryTree 节点定义 | 定义 7 种 GQL 节点类型 | `src/Analyzer/QueryTree.h`<br>`src/Analyzer/GQL/GQLQueryTreeNodes.h` | 2-3 天 |
| **Phase 2**: GQLQueryTreeBuilder | 实现 AST → QueryTree 转换 | `src/Analyzer/GQL/GQLQueryTreeBuilder.cpp` | 3-4 天 |
| **Phase 3**: GQLQueryAnalysisPass | 实现名字解析和类型推导 | `src/Analyzer/Passes/GQLQueryAnalysisPass.cpp` | 4-5 天 |
| **Phase 4**: GQLPlanner | 实现 QueryTree → QueryPlan | `src/Planner/GQL/GQLPlanner.cpp` | 3-4 天 |
| **Phase 5**: MatchStep 重构 | 改为 ITransformingStep | `src/Processors/QueryPlan/Graph/MatchStep.cpp` | 2-3 天 |
| **Phase 6**: InterpreterGQLQuery 重构 | 实现 4 阶段模式 | `src/Interpreters/InterpreterGQLQuery.cpp` | 2-3 天 |
| **Phase 7**: 优化 Pass | 实现 WHERE 下推等优化 | `src/Analyzer/Passes/GQL*.cpp` | 3-4 天 |
| **Phase 8**: 测试和文档 | 端到端测试、文档更新 | `tests/queries/`<br>`docs/` | 3-4 天 |

**总计**：22-30 天

### 7.2 依赖关系

```
Phase 1 (节点定义)
    ↓
Phase 2 (Builder) ← 依赖 Phase 1
    ↓
Phase 3 (Analysis Pass) ← 依赖 Phase 1, 2
    ↓
Phase 4 (Planner) ← 依赖 Phase 1, 2, 3
    ↓
Phase 5 (MatchStep) ← 依赖 Phase 4
    ↓
Phase 6 (Interpreter) ← 依赖 Phase 2, 3, 4
    ↓
Phase 7 (优化 Pass) ← 依赖 Phase 3, 4
    ↓
Phase 8 (测试) ← 依赖所有前序阶段
```

### 7.3 优先级

**高优先级**（必须完成）：
- Phase 1-6：核心架构重构
- Phase 8：基础测试

**中优先级**（重要但可延后）：
- Phase 7：优化 Pass（WHERE 下推、pattern 简化）

**低优先级**（可选）：
- 高级优化（MATCH 重排、JOIN 重排）
- 性能调优

## 八、风险评估

### 8.1 高风险项

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| **QueryTree 节点类型不足** | 无法表达所有 GQL 语义 | Phase 1 完成后进行全面语义覆盖测试 |
| **名字解析复杂度** | GQLQueryAnalysisPass 实现困难 | 参考 ClickHouse QueryAnalysisPass，复用作用域管理逻辑 |
| **WHERE 下推规则不完善** | 性能回退或语义错误 | 保守下推策略，优先保证正确性 |
| **MatchStep 改造影响现有功能** | 破坏已有 MATCH 执行 | 保留旧 MatchStep 作为 fallback，逐步迁移 |

### 8.2 中风险项

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| **与现有代码集成困难** | 重构周期延长 | 增量迁移，保持向后兼容 |
| **测试覆盖不足** | 引入回归 bug | Phase 8 补充端到端测试 |
| **文档更新滞后** | 维护困难 | 每个 Phase 完成后更新对应文档 |

## 九、验证策略

### 9.1 单元测试

每个 Phase 完成后添加单元测试：

```cpp
// Phase 1: 节点定义测试
TEST(GQLQueryTreeNodes, GQLQueryNodeBasic)
{
    auto query_node = std::make_shared<GQLQueryNode>();
    EXPECT_EQ(query_node->getNodeType(), QueryTreeNodeType::GQL_QUERY);
}

// Phase 2: Builder 测试
TEST(GQLQueryTreeBuilder, SimpleMatch)
{
    auto ast = parseGQL("MATCH (n) RETURN n");
    auto tree = GQL::buildQueryTree(ast, context);
    EXPECT_NE(tree->as<GQLQueryNode>(), nullptr);
}

// Phase 3: Analysis Pass 测试
TEST(GQLQueryAnalysisPass, NameResolution)
{
    auto tree = buildQueryTree("MATCH (n:Person) WHERE n.age > 18 RETURN n.name");
    GQLQueryAnalysisPass pass;
    pass.run(tree, context);
    // 验证 n.age 和 n.name 已解析
}
```

### 9.2 集成测试

```sql
-- tests/queries/0_stateless/03500_gql_analyzer_basic.sql
-- 基础 MATCH + RETURN
MATCH (n:Person) RETURN n.name, n.age;

-- WHERE 下推
MATCH (n:Person) WHERE n.age > 18 RETURN n.name;

-- ORDER BY + LIMIT
MATCH (n:Person) RETURN n.name ORDER BY n.age DESC LIMIT 10;

-- UNION
MATCH (n:Person) RETURN n.name
UNION ALL
MATCH (m:Company) RETURN m.name;

-- OPTIONAL MATCH
MATCH (n:Person)
OPTIONAL MATCH (n)-[r:WORKS_AT]->(c:Company)
RETURN n.name, c.name;
```

### 9.3 性能测试

对比重构前后的性能：

```bash
# 重构前
clickhouse-benchmark --query "MATCH (n:Person) WHERE n.age > 18 RETURN n.name" --iterations 1000

# 重构后
clickhouse-benchmark --query "MATCH (n:Person) WHERE n.age > 18 RETURN n.name" --iterations 1000
```

**预期结果**：
- WHERE 下推后，性能提升 20-50%
- 其他查询性能持平或略有提升

### 9.4 回归测试

运行现有所有 GQL 测试：

```bash
python3 -m pytest tests/integration/test_gql/ -v
```

**通过标准**：所有现有测试必须通过。

## 十、关键文件清单

### 10.1 新增文件

```
src/Analyzer/GQL/
├── GQLQueryTreeNodes.h          # 7 种 GQL 节点定义
├── GQLQueryTreeBuilder.h        # AST → QueryTree
├── GQLQueryTreeBuilder.cpp
└── Passes/
    ├── GQLQueryAnalysisPass.h   # 名字解析 + 类型推导
    ├── GQLQueryAnalysisPass.cpp
    ├── GQLPatternNormalizationPass.h
    ├── GQLPatternNormalizationPass.cpp
    ├── GQLWhereExtractionPass.h
    └── GQLWhereExtractionPass.cpp

src/Planner/GQL/
├── GQLPlanner.h                 # QueryTree → QueryPlan
└── GQLPlanner.cpp

tests/queries/0_stateless/
├── 03500_gql_analyzer_basic.sql
├── 03500_gql_analyzer_basic.reference
├── 03501_gql_where_pushdown.sql
└── 03501_gql_where_pushdown.reference
```

### 10.2 修改文件

```
src/Analyzer/QueryTree.h         # 添加 7 种 GQL 节点类型枚举
src/Interpreters/InterpreterGQLQuery.h
src/Interpreters/InterpreterGQLQuery.cpp
src/Processors/QueryPlan/Graph/MatchStep.h
src/Processors/QueryPlan/Graph/MatchStep.cpp
docs/graph/gql_runtime_flow.md  # 更新架构文档
```

### 10.3 可能删除的文件

```
src/Interpreters/GQL/
├── GQLPlanBuilder.cpp           # 功能被 GQLPlanner 替代
├── ClauseSequencePlanner.cpp    # 功能被 GQLQueryTreeBuilder 替代
├── SourcePlanner.cpp            # 功能被 GQLPlanner 替代
└── PostSourceClausePlanner.cpp  # 功能被 GQLPlanner 替代
```

**注意**：删除前需确认没有其他依赖。

## 十一、总结

本重构计划的核心目标是：

1. **长期视角**：建立清晰的 AST → QueryTree → QueryPlan 三层架构
2. **最大化复用**：WHERE/RETURN/ORDER BY/LIMIT 等复用 ClickHouse 能力
3. **支持优化**：WHERE 下推、pattern 优化等
4. **可扩展性**：为未来的 correlated subquery、JOIN 重排等预留空间

**关键设计决策**：
- `GQLQueryNode` 继承 `QueryNode`，复用 17 个 child 索引
- `GQLUseNode` 独立节点，明确表达 USE 语义
- `MatchStep` 改为 `ITransformingStep`，支持接受输入
- 表达式构建直接复用 ClickHouse 的 `buildExpression`

**实施建议**：
- 按 Phase 1-8 顺序实施，每个 Phase 完成后进行测试
- 保持向后兼容，逐步迁移现有代码
- 优先保证正确性，性能优化可延后

---

> 文档生成时间: 2026-05-19
> 基于 PR #14 现状分析与 ClickHouse Analyzer 架构参考

