# GQL Analyzer 架构重构计划

## 一、背景与目标

### 1.1 重构动机

当前 GQL 实现采用 **direct planner** 架构，直接从 AST 生成 QueryPlan，存在以下问题：

1. **缺少语义分析层**：没有独立的 analyze/bind 阶段，名字解析和类型推导混杂在 planning 中
2. **WHERE 下推通道未完全利用**：当前 `MATCH ... WHERE ...` 已被 `PatternBinder` 提取并放入 `MatchSpec::where_clause`，但 graph storage reader 尚未接入，无法真正消费这些谓词做索引过滤
3. **代码复用不足**：RETURN/WHERE/ORDER BY 等已复用 ClickHouse 的 Step（FilterStep/ExpressionStep/SortingStep/LimitStep），但未通过 QueryTree 统一表达
4. **缺少中间 IR**：直接从 AST 到 QueryPlan，缺少语义层表示，难以做跨 clause 优化（如 MATCH 重排、谓词下推分析）

### 1.2 重构目标

1. **长期视角**：追求代码风格工整，参考 `InterpreterSelectQueryAnalyzer` 的 4 阶段模式
2. **最大化复用**：继续复用 ClickHouse 的 Step（FilterStep/ExpressionStep/SortingStep/LimitStep/ActionsDAG），通过 QueryTree 统一表达语义
3. **分层清晰**：AST → QueryTree → QueryPlan 三阶段分离
4. **支持优化**：WHERE 下推分析、pattern 优化、MATCH 重排等

### 1.3 关键约束

1. **QueryNode 是 final 类**：无法继承扩展，需要独立设计 GQL QueryTree 节点
2. **ClickHouse helper 多为 private**：`QueryTreeBuilder::buildExpression`、`Planner` 的很多 helper 都是 cpp 内部函数，无法直接调用
3. **MatchStep 当前是 ISourceStep**：改为 `ITransformingStep` 不足以支持 correlated subquery，需要明确的 Apply 语义
4. **现有语义丰富**：当前 `MatchSpec` 已包含 match_mode、KEEP、YIELD、optional、path prefix、path alternation、compound edge direction、properties、predicates 等，新节点体系必须覆盖

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

**核心设计决策**：由于 `QueryNode` 是 `final` 类且 `children_size` 固定为 17，无法通过继承扩展。采用**独立节点 + 组合**策略：

1. **GQL 特有节点独立设计**：USE/MATCH/pattern 等作为独立的 `IQueryTreeNode` 子类
2. **复用 QueryNode 处理 RETURN/WHERE/ORDER BY/LIMIT**：这些子句通过 `QueryNode` 的现有 child 索引表达
3. **通过 ListNode 组织 GQL 子句**：USE/MATCH 列表作为 `QueryNode` 的 JOIN 表达式或自定义扩展字段

新增节点类型（至少 12 种，覆盖现有 MatchSpec 语义）：

| 节点类型 | 继承关系 | 职责 |
|---------|---------|------|
| `GQLUseNode` | 继承 `IQueryTreeNode` | USE 子句，存储图引用 |
| `GQLMatchNode` | 继承 `IQueryTreeNode` | MATCH 子句，存储 match_mode + path patterns + WHERE |
| `GQLPathPatternNode` | 继承 `IQueryTreeNode` | 路径模式，存储 path_variable + node/edge 序列 |
| `GQLNodePatternNode` | 继承 `IQueryTreeNode` | 节点模式，存储 variable + label + properties + WHERE |
| `GQLEdgePatternNode` | 继承 `IQueryTreeNode` | 边模式，存储 variable + direction + label + properties + WHERE + quantifier |
| `GQLQuantifiedPatternNode` | 继承 `IQueryTreeNode` | 量化路径模式（`{1,5}` 等） |
| `GQLKeepNode` | 继承 `IQueryTreeNode` | KEEP 子句，存储保留的绑定变量 |
| `GQLYieldNode` | 继承 `IQueryTreeNode` | YIELD 子句，存储输出的路径变量 |
| `GQLPathPrefixNode` | 继承 `IQueryTreeNode` | 路径前缀（`p = (n)-[e]->(m)`） |
| `GQLPathAlternationNode` | 继承 `IQueryTreeNode` | 路径选择（`(n)-[:A\|:B]->(m)`） |
| `GQLPropertyNode` | 继承 `IQueryTreeNode` | 属性约束（`{id: 1, name: "Alice"}`） |
| `GQLEdgeDirectionNode` | 继承 `IQueryTreeNode` | 复合边方向（left/right/undirected/left_or_undirected 等） |

**关键设计**：
- **不继承 QueryNode**，避免与 ClickHouse 核心类冲突
- **通过 QueryNode 的 JOIN 表达式或自定义字段关联 GQL 节点**
- **每个节点覆盖现有 MatchSpec 的对应字段**

### 2.3 节点类定义

所有 GQL 节点均继承 `IQueryTreeNode`，不继承 `QueryNode`（因为 `QueryNode` 是 final）。

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
    // match_mode (TRAIL/WALK/SIMPLE/ACYCLIC)
    MatchMode match_mode = MatchMode::WALK;
    
    // OPTIONAL 标记
    bool is_optional = false;
    
    // 路径模式列表（ListNode 包含多个 GQLPathPatternNode）
    QueryTreeNodePtr path_patterns;
    
    // MATCH 级别的 WHERE（可选）
    QueryTreeNodePtr where_expression;
    
    // KEEP 子句（可选，GQLKeepNode）
    QueryTreeNodePtr keep_clause;
    
    // YIELD 子句（可选，GQLYieldNode）
    QueryTreeNodePtr yield_clause;
    
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
    
    // 属性约束（可选，GQLPropertyNode 列表）
    QueryTreeNodePtr properties;
    
    // WHERE 表达式（可选）
    QueryTreeNodePtr where_expression;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_NODE_PATTERN; }
};
```

#### GQLEdgePatternNode

```cpp
class GQLEdgePatternNode : public IQueryTreeNode
{
public:
    // 边变量名（可选）
    std::optional<String> variable;
    
    // 方向（GQLEdgeDirectionNode）
    QueryTreeNodePtr direction;
    
    // 标签表达式（可选）
    QueryTreeNodePtr label_expression;
    
    // 属性约束（可选，GQLPropertyNode 列表）
    QueryTreeNodePtr properties;
    
    // WHERE 表达式（可选）
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

#### GQLKeepNode

```cpp
class GQLKeepNode : public IQueryTreeNode
{
public:
    // 保留的绑定变量列表（ListNode 包含 IdentifierNode）
    QueryTreeNodePtr variables;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_KEEP; }
};
```

#### GQLYieldNode

```cpp
class GQLYieldNode : public IQueryTreeNode
{
public:
    // 输出的路径变量列表（ListNode 包含 IdentifierNode）
    QueryTreeNodePtr path_variables;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_YIELD; }
};
```

#### GQLPathPrefixNode

```cpp
class GQLPathPrefixNode : public IQueryTreeNode
{
public:
    // 路径变量名
    String path_variable;
    
    // 路径模式（GQLPathPatternNode）
    QueryTreeNodePtr path_pattern;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PATH_PREFIX; }
};
```

#### GQLPathAlternationNode

```cpp
class GQLPathAlternationNode : public IQueryTreeNode
{
public:
    // 标签选择列表（ListNode 包含 IdentifierNode）
    QueryTreeNodePtr label_alternatives;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PATH_ALTERNATION; }
};
```

#### GQLPropertyNode

```cpp
class GQLPropertyNode : public IQueryTreeNode
{
public:
    // 属性名
    String property_name;
    
    // 属性值表达式
    QueryTreeNodePtr value_expression;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PROPERTY; }
};
```

#### GQLEdgeDirectionNode

```cpp
class GQLEdgeDirectionNode : public IQueryTreeNode
{
public:
    enum class Direction
    {
        Left,              // <-
        Right,             // ->
        Undirected,        // -
        LeftOrUndirected,  // <-|-
        RightOrUndirected, // ->|-
        Any                // <->
    };
    
    Direction direction = Direction::Right;
    
    QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_EDGE_DIRECTION; }
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

**关键约束**：由于 `QueryNode` 是 final 类，无法创建 `GQLQueryNode`。采用以下策略：

1. **顶层使用 QueryNode**：用于表达 RETURN/WHERE/ORDER BY/LIMIT
2. **GQL 子句作为扩展字段**：USE/MATCH 通过 QueryNode 的 JOIN 表达式或自定义机制关联
3. **或者使用独立的 GQL 根节点**：创建 `GQLRootNode` 包含 QueryNode + GQL 特有子句

```
buildQueryTree(GQLSingleQuery)
  ├─ 创建 QueryNode（用于 RETURN/WHERE/ORDER BY/LIMIT）
  ├─ 创建 GQLRootNode（包装 QueryNode + GQL 子句）
  ├─ 遍历 clauses
  │   ├─ GQLUseClause → buildUseNode → 设置 GQLRootNode::use
  │   ├─ GQLMatchClause → buildMatchNode → 添加到 GQLRootNode::matches
  │   ├─ GQLWhereClause → buildExpression → 设置 QueryNode::where
  │   ├─ GQLReturnClause → buildProjection → 设置 QueryNode::projection
  │   ├─ GQLOrderByClause → buildOrderBy → 设置 QueryNode::order_by
  │   └─ GQLLimitClause → buildLimit → 设置 QueryNode::limit
  └─ 返回 GQLRootNode

buildQueryTree(GQLCombinedQuery)
  ├─ 创建 UnionNode
  ├─ 递归构建每个子查询
  └─ 设置 union_mode（UNION_ALL/UNION_DISTINCT/EXCEPT/INTERSECT）
```

### 3.3 MATCH 构建

```cpp
QueryTreeNodePtr buildMatchNode(const GQLMatchClause & clause, const ContextPtr & context)
{
    auto match_node = std::make_shared<GQLMatchNode>();
    match_node->match_mode = clause.match_mode;
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
        match_node->where_expression = buildExpressionTree(clause.where, context);
    
    if (clause.keep)
        match_node->keep_clause = buildKeepNode(clause.keep);
    
    if (clause.yield)
        match_node->yield_clause = buildYieldNode(clause.yield);
    
    return match_node;
}
```

### 3.4 表达式构建

**约束**：`QueryTreeBuilder::buildExpression` 是 private helper，无法直接调用。

**解决方案**：
1. **使用公开入口**：调用 `DB::buildQueryTree(ast, context)` 构建完整 QueryTree，然后提取表达式部分
2. **自行实现 helper**：参考 `QueryTreeBuilder.cpp` 实现 GQL 专用的表达式构建函数
3. **直接构造节点**：对于简单表达式，直接构造 `FunctionNode`/`ColumnNode`/`ConstantNode`

```cpp
// 方案 1：使用公开入口（适用于复杂表达式）
QueryTreeNodePtr buildExpressionTree(const ASTPtr & ast, const ContextPtr & context)
{
    // 包装成 SELECT 查询，利用 buildQueryTree 解析
    auto select_query = std::make_shared<ASTSelectQuery>();
    select_query->setExpression(ASTSelectQuery::Expression::SELECT, ast);
    
    auto query_tree = DB::buildQueryTree(select_query, context);
    return query_tree->as<QueryNode>()->getProjection();
}

// 方案 2：自行实现（适用于 GQL 特有表达式）
QueryTreeNodePtr buildGQLExpression(const ASTPtr & ast, const ContextPtr & context)
{
    if (auto * identifier = ast->as<ASTIdentifier>())
        return std::make_shared<ColumnNode>(identifier->name(), context);
    
    if (auto * literal = ast->as<ASTLiteral>())
        return std::make_shared<ConstantNode>(literal->value);
    
    if (auto * function = ast->as<ASTFunction>())
    {
        auto function_node = std::make_shared<FunctionNode>(function->name);
        for (const auto & arg : function->arguments->children)
            function_node->getArguments().push_back(buildGQLExpression(arg, context));
        return function_node;
    }
    
    throw Exception(ErrorCodes::LOGICAL_ERROR, "Unsupported expression type");
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
void GQLPlanner::buildPlanForGQLQuery(const GQLRootNode & root_node)
{
    // 1. 处理 USE（设置 active graph）
    if (root_node.getUse())
        processUseClause(root_node.getUse());
    
    // 2. 构建 MATCH source
    if (root_node.getMatches())
    {
        auto match_spec = buildMatchSpec(root_node.getMatches(), root_node.getQueryNode()->getWhere());
        auto match_step = std::make_unique<MatchStep>(std::move(match_spec), source_factory);
        query_plan.addStep(std::move(match_step));
    }
    
    // 3. 直接构造 Step（不通过 Planner helper）
    auto & query_node = root_node.getQueryNode();
    
    if (query_node->getProjection())
        buildProjectionStep(query_node->getProjection());
    
    if (query_node->getOrderBy())
        buildOrderByStep(query_node->getOrderBy());
    
    if (query_node->getLimit())
        buildLimitStep(query_node->getLimit());
}
```

### 5.3 WHERE 下推策略

**当前状态**：WHERE 下推通道已存在但未完全利用
- `PatternBinder` 已提取 `MATCH ... WHERE ...` 到 `MatchSpec::where_clause`
- `MatchSourceFactory::createReader` 接收 `MatchSpec` 但尚未消费 `where_clause`
- 当前通过追加 `FilterStep` 保证语义正确

**重构目标**：在 QueryTree 阶段分析谓词下推可行性

```cpp
MatchSpec buildMatchSpec(const QueryTreeNodePtr & matches, const QueryTreeNodePtr & where)
{
    MatchSpec spec;
    
    // 1. 从 GQLMatchNode 列表构建 clause specs
    for (const auto & match : matches->as<ListNode>()->getNodes())
    {
        auto & match_node = match->as<GQLMatchNode>();
        auto clause_spec = buildClauseSpec(match_node);
        
        // 将 MATCH 级别的 WHERE 放入 clause_spec
        if (match_node.where_expression)
            clause_spec.where_clause = match_node.where_expression;
        
        spec.clauses.push_back(clause_spec);
    }
    
    // 2. WHERE 下推分析（顶层 WHERE）
    if (where)
    {
        auto predicates = extractPredicates(where);
        for (const auto & pred : predicates)
        {
            if (canPushdownToMatch(pred))
            {
                // 标记为可下推，等待 graph storage reader 实现
                spec.pushdown_predicates.push_back(pred);
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
- 可下推：`n.id = 1`、`e.weight > 10`（直接引用单个 pattern 变量的属性）
- 不可下推：`n.id = m.id`（跨 pattern 引用）、`count(*) > 5`（聚合函数）、`n.id IN (SELECT ...)`（子查询）

**实施路径**：
1. Phase 1-4：在 QueryTree 中标记可下推谓词
2. Phase 5：修改 `MatchSourceFactory` 和 reader 实现，消费 `MatchSpec::pushdown_predicates`
3. Phase 7：优化 Pass 进一步分析下推机会

### 5.4 其他子句处理

**约束**：ClickHouse `Planner` 的很多 helper 是 private 或 cpp 内部函数，无法直接调用。

**解决方案**：直接构造 Step，参考现有 GQL 实现。

```cpp
void GQLPlanner::buildProjectionStep(const QueryTreeNodePtr & projection)
{
    // 直接构造 ExpressionStep（当前 GQL 已在使用）
    auto actions_dag = std::make_shared<ActionsDAG>();
    
    // 从 projection QueryTree 构建 ActionsDAG
    for (const auto & expr : projection->as<ListNode>()->getNodes())
    {
        buildActionsDAGFromExpression(expr, actions_dag);
    }
    
    auto step = std::make_unique<ExpressionStep>(
        query_plan.getCurrentHeader(),
        std::move(actions_dag));
    query_plan.addStep(std::move(step));
}

void GQLPlanner::buildOrderByStep(const QueryTreeNodePtr & order_by)
{
    // 直接构造 SortingStep（当前 GQL 已在使用）
    SortDescription sort_description;
    
    for (const auto & sort_node : order_by->as<ListNode>()->getNodes())
    {
        auto & sort_column = sort_node->as<SortNode>();
        sort_description.emplace_back(
            sort_column.getExpression()->as<ColumnNode>()->getColumnName(),
            sort_column.getSortDirection());
    }
    
    auto step = std::make_unique<SortingStep>(
        query_plan.getCurrentHeader(),
        sort_description);
    query_plan.addStep(std::move(step));
}

void GQLPlanner::buildLimitStep(const QueryTreeNodePtr & limit)
{
    // 直接构造 LimitStep（当前 GQL 已在使用）
    auto & limit_node = limit->as<LimitNode>();
    
    auto step = std::make_unique<LimitStep>(
        query_plan.getCurrentHeader(),
        limit_node.getLimit(),
        limit_node.getOffset());
    query_plan.addStep(std::move(step));
}
```

**复用现状**：当前 GQL 已在使用 `FilterStep`/`ExpressionStep`/`SortingStep`/`LimitStep`/`ActionsDAG`，重构后继续保持这种模式。

### 5.5 MatchStep 设计考量

**当前状态**：`MatchStep` 继承 `ISourceStep`，作为 pipeline 的 source boundary。

**改为 ITransformingStep 的局限性**：
- 仅改基类不足以支持 correlated subquery
- Correlated nested scan 需要明确的 **Apply 语义**：
  1. 外层每行绑定变量传入内层
  2. 内层执行 nested scan
  3. 结果合并回外层

**正确方向**：
1. **短期**：保持 `ISourceStep`，优先完成 QueryTree 重构
2. **中期**：实现 `ApplyStep` + `ApplyPlanner`，支持 correlated subquery
3. **长期**：考虑 `MatchStep` 是否需要改为 `ITransformingStep`，取决于 Apply 语义的实现方式

**当前 MatchStep 保持不变**：

```cpp
// src/Processors/QueryPlan/Graph/MatchStep.h
class MatchStep final : public ISourceStep
{
public:
    MatchStep(
        MatchSpec match_spec_,
        MatchSourceFactoryPtr source_factory_);
    
    String getName() const override { return "GraphMatch"; }
    
    void initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;
    
private:
    MatchSpec match_spec;
    MatchSourceFactoryPtr source_factory;
};
```

**未来 ApplyStep 设计**（Phase 7 之后）：

```cpp
class ApplyStep final : public ITransformingStep
{
public:
    ApplyStep(
        const Header & input_header_,
        QueryPlanPtr subquery_plan_,
        Names correlation_names_);
    
    String getName() const override { return "Apply"; }
    
    void transformPipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;
    
private:
    QueryPlanPtr subquery_plan;
    Names correlation_names;
};
```

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
| **Phase 1**: QueryTree 节点定义 | 定义 12 种 GQL 节点类型 | `src/Analyzer/QueryTree.h`<br>`src/Analyzer/GQL/GQLQueryTreeNodes.h` | 3-4 天 |
| **Phase 2**: GQLQueryTreeBuilder | 实现 AST → QueryTree 转换 | `src/Analyzer/GQL/GQLQueryTreeBuilder.cpp` | 4-5 天 |
| **Phase 3**: GQLQueryAnalysisPass | 实现名字解析和类型推导 | `src/Analyzer/Passes/GQLQueryAnalysisPass.cpp` | 5-6 天 |
| **Phase 4**: GQLPlanner | 实现 QueryTree → QueryPlan | `src/Planner/GQL/GQLPlanner.cpp` | 4-5 天 |
| **Phase 5**: InterpreterGQLQuery 重构 | 实现 4 阶段模式 | `src/Interpreters/InterpreterGQLQuery.cpp` | 2-3 天 |
| **Phase 6**: 优化 Pass | 实现 WHERE 下推分析等优化 | `src/Analyzer/Passes/GQL*.cpp` | 3-4 天 |
| **Phase 7**: 测试和文档 | 端到端测试、文档更新 | `tests/queries/`<br>`docs/` | 3-4 天 |
| **Phase 8**: MatchStep 改造（可选） | 评估是否需要改为 ITransformingStep | `src/Processors/QueryPlan/Graph/MatchStep.cpp` | 2-3 天 |

**总计**：26-34 天

**注意**：Phase 5 不再包含 MatchStep 重构，因为改为 `ITransformingStep` 不足以支持 correlated subquery，需要更复杂的 Apply 语义。

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
Phase 5 (Interpreter) ← 依赖 Phase 2, 3, 4
    ↓
Phase 6 (优化 Pass) ← 依赖 Phase 3, 4
    ↓
Phase 7 (测试) ← 依赖所有前序阶段
    ↓
Phase 8 (MatchStep 改造，可选) ← 依赖 Phase 4
```

### 7.3 优先级

**高优先级**（必须完成）：
- Phase 1-5：核心架构重构
- Phase 7：基础测试

**中优先级**（重要但可延后）：
- Phase 6：优化 Pass（WHERE 下推分析、pattern 简化）

**低优先级**（可选）：
- Phase 8：MatchStep 改造（需先明确 Apply 语义）
- 高级优化（MATCH 重排、JOIN 重排）
- 性能调优

## 八、风险评估

### 8.1 高风险项

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| **QueryTree 节点类型不足** | 无法表达所有 GQL 语义 | Phase 1 完成后进行全面语义覆盖测试，确保 12 种节点覆盖现有 MatchSpec |
| **QueryNode 无法继承** | 需要设计 GQLRootNode 包装 | 采用组合模式，GQLRootNode 包含 QueryNode + GQL 子句 |
| **ClickHouse helper 无法调用** | 需自行实现表达式构建和 Step 构造 | 参考现有 GQL 实现，直接构造 ActionsDAG 和 Step |
| **名字解析复杂度** | GQLQueryAnalysisPass 实现困难 | 参考 ClickHouse QueryAnalysisPass，复用作用域管理逻辑 |
| **WHERE 下推执行侧缺失** | 分析完成但无法真正下推 | Phase 6 标记可下推谓词，Phase 8 后实现 reader 消费 |

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
TEST(GQLQueryTreeNodes, GQLMatchNodeBasic)
{
    auto match_node = std::make_shared<GQLMatchNode>();
    EXPECT_EQ(match_node->getNodeType(), QueryTreeNodeType::GQL_MATCH);
}

// Phase 2: Builder 测试
TEST(GQLQueryTreeBuilder, SimpleMatch)
{
    auto ast = parseGQL("MATCH (n) RETURN n");
    auto tree = GQL::buildQueryTree(ast, context);
    EXPECT_NE(tree->as<GQLRootNode>(), nullptr);
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
- WHERE 下推分析完成后，标记可下推谓词（实际下推需 Phase 8 后实现 reader 消费）
- 其他查询性能持平或略有提升（得益于 QueryTree 优化）

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
├── GQLQueryTreeNodes.h          # 12 种 GQL 节点定义
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
src/Analyzer/QueryTree.h         # 添加 12 种 GQL 节点类型枚举
src/Interpreters/InterpreterGQLQuery.h
src/Interpreters/InterpreterGQLQuery.cpp
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
2. **最大化复用**：继续复用 ClickHouse 的 Step（FilterStep/ExpressionStep/SortingStep/LimitStep/ActionsDAG），通过 QueryTree 统一表达语义
3. **支持优化**：WHERE 下推分析、pattern 优化等
4. **可扩展性**：为未来的 correlated subquery、JOIN 重排等预留空间

**关键设计决策**：
- **不继承 QueryNode**（因为是 final 类），采用 GQLRootNode 包装 QueryNode + GQL 子句
- **12 种独立 GQL 节点**，覆盖现有 MatchSpec 的所有语义（match_mode、KEEP、YIELD、path prefix、path alternation、properties、compound edge direction 等）
- **自行实现表达式构建和 Step 构造**（因为 ClickHouse helper 多为 private）
- **MatchStep 保持 ISourceStep**（改为 ITransformingStep 不足以支持 correlated subquery，需明确 Apply 语义）
- **WHERE 下推分析与执行分离**（Phase 6 完成分析，Phase 8 后实现 reader 消费）

**实施建议**：
- 按 Phase 1-7 顺序实施，每个 Phase 完成后进行测试
- Phase 8（MatchStep 改造）可选，需先明确 Apply 语义
- 保持向后兼容，逐步迁移现有代码
- 优先保证正确性，性能优化可延后

**与原计划的主要差异**：
1. 节点数量从 7 种增加到 12 种，覆盖现有语义
2. 不继承 QueryNode，改用组合模式
3. 不直接调用 ClickHouse private helper，自行实现或直接构造
4. MatchStep 不改为 ITransformingStep，保持现状
5. WHERE 下推分为分析阶段（Phase 6）和执行阶段（Phase 8 后）

---

> 文档生成时间: 2026-05-20
> 基于 GPT 技术反馈修正后的架构设计

