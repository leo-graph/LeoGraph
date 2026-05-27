#include <Analyzer/GQL/GQLQueryTreeBuilder.h>

#include <Analyzer/GQL/GQLCombinedQueryNode.h>
#include <Analyzer/GQL/GQLEdgePatternNode.h>
#include <Analyzer/GQL/GQLFilterNode.h>
#include <Analyzer/GQL/GQLKeepNode.h>
#include <Analyzer/GQL/GQLLabelExpressionNode.h>
#include <Analyzer/GQL/GQLLinearQueryNode.h>
#include <Analyzer/GQL/GQLMatchNode.h>
#include <Analyzer/GQL/GQLNodePatternNode.h>
#include <Analyzer/GQL/GQLOrderByNode.h>
#include <Analyzer/GQL/GQLPageNode.h>
#include <Analyzer/GQL/GQLPathPatternNode.h>
#include <Analyzer/GQL/GQLReturnNode.h>
#include <Analyzer/GQL/GQLYieldNode.h>
#include <Analyzer/ListNode.h>
#include <Common/Exception.h>
#include <Parsers/graph/GraphAST.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace GQL
{

namespace
{

namespace GAST = DB::OPENGQL::AST;

/** Internal implementation class for building GQL QueryTree.
 *
 * This class traverses the GQL Parser AST and constructs corresponding
 * QueryTree nodes. It maintains context during the traversal.
 */
class GQLQueryTreeBuilderImpl
{
public:
    explicit GQLQueryTreeBuilderImpl(ContextPtr context_) : context(std::move(context_)) { }

    QueryTreeNodePtr build(const IAST & query)
    {
        if (const auto * single = query.as<GAST::GQLSingleQuery>())
            return buildLinearQuery(*single);

        if (const auto * combined = query.as<GAST::GQLCombinedQuery>())
            return buildCombinedQuery(*combined);

        throw Exception(ErrorCodes::LOGICAL_ERROR, "Unsupported GQL query root: {}", query.getID(' '));
    }

private:
    QueryTreeNodePtr buildLinearQuery(const GAST::GQLSingleQuery & query)
    {
        auto linear_node = std::make_shared<GQLLinearQueryNode>();
        auto steps_list = std::make_shared<ListNode>();

        // Convert each clause to a QueryTree step
        for (const auto & clause : query.clauses)
        {
            if (auto step = buildClause(clause))
                steps_list->getNodes().push_back(step);
        }

        linear_node->getStepsNode() = std::move(steps_list);
        return linear_node;
    }

    QueryTreeNodePtr buildCombinedQuery(const GAST::GQLCombinedQuery & query)
    {
        auto combined_node = std::make_shared<GQLCombinedQueryNode>();
        auto queries_list = std::make_shared<ListNode>();

        // Convert each subquery
        for (const auto & subquery : query.queries)
        {
            if (!subquery)
                throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined query has null subquery");

            queries_list->getNodes().push_back(build(*subquery));
        }

        combined_node->getQueriesNode() = std::move(queries_list);

        // Convert operators
        std::vector<GQLCombinedQueryNode::CombinedOperator> operators;
        operators.reserve(query.operators.size());

        for (auto op : query.operators)
            operators.push_back(convertOperator(op));

        combined_node->setOperators(std::move(operators));

        return combined_node;
    }

    QueryTreeNodePtr buildClause(const ASTPtr & clause)
    {
        if (!clause)
            return nullptr;

        if (const auto * match = clause->as<GAST::GQLMatchClause>())
            return buildMatchClause(*match);

        if (const auto * where = clause->as<GAST::GQLWhereClause>())
            return buildFilterClause(*where);

        if (const auto * ret = clause->as<GAST::GQLReturnClause>())
            return buildReturnClause(*ret);

        if (const auto * order_by = clause->as<GAST::GQLOrderByClause>())
            return buildOrderByClause(*order_by);

        if (const auto * page = clause->as<GAST::GQLPageClause>())
            return buildPageClause(*page);

        // TODO: Add support for other clause types:
        // - GQLSelectClause
        // - GQLLetClause
        // - GQLForClause
        // - GQLCallClauseBase (inline/named)
        // - GQLFinishClause
        // - etc.

        throw Exception(
            ErrorCodes::NOT_IMPLEMENTED, "GQL clause not yet supported in QueryTree builder: {}", clause->getID(' '));
    }

    QueryTreeNodePtr buildMatchClause(const GAST::GQLMatchClause & match)
    {
        auto match_node = std::make_shared<GQLMatchNode>();

        match_node->setOptional(match.optional);
        match_node->setMatchMode(match.match_mode);

        // Convert path patterns
        auto patterns_list = std::make_shared<ListNode>();
        for (const auto & pattern : match.path_patterns)
        {
            if (auto pattern_node = buildPathPattern(pattern))
                patterns_list->getNodes().push_back(pattern_node);
        }
        match_node->getPathPatternsNode() = std::move(patterns_list);

        // Convert WHERE predicate
        if (match.where)
        {
            // TODO: Implement expression conversion
            // For now, we'll leave it null and implement expression building later
            // match_node->getWhere() = buildExpression(match.where);
        }

        // Convert KEEP clause
        if (match.keep_clause)
        {
            // TODO: Implement KEEP clause conversion
            // match_node->getKeep() = buildKeepClause(*match.keep_clause);
        }

        // Convert YIELD items
        if (!match.yield_items.empty())
        {
            // TODO: Implement YIELD conversion
            // auto yield_node = buildYieldClause(match.yield_items);
            // match_node->getYield() = yield_node;
        }

        return match_node;
    }

    QueryTreeNodePtr buildPathPattern(const ASTPtr & pattern)
    {
        if (!pattern)
            return nullptr;

        // TODO: Implement full path pattern conversion
        // For now, create a placeholder node
        auto path_node = std::make_shared<GQLPathPatternNode>();

        // The actual implementation should handle:
        // - GQLPathPattern -> GQLPathPatternNode
        // - GQLNodePattern -> GQLNodePatternNode
        // - GQLEdgePattern -> GQLEdgePatternNode
        // - GQLQuantifier -> GQLQuantifierNode
        // - GQLLabelExpression -> GQLLabelExpressionNode
        // - etc.

        return path_node;
    }

    QueryTreeNodePtr buildFilterClause(const GAST::GQLWhereClause & where)
    {
        auto filter_node = std::make_shared<GQLFilterNode>();

        // TODO: Convert predicate expression
        // filter_node->getPredicate() = buildExpression(where.predicate);

        return filter_node;
    }

    QueryTreeNodePtr buildReturnClause(const GAST::GQLReturnClause & ret)
    {
        auto return_node = std::make_shared<GQLReturnNode>();

        // TODO: Convert return items
        // auto items_list = std::make_shared<ListNode>();
        // for (const auto & item : ret.items)
        //     items_list->getNodes().push_back(buildReturnItem(item));
        // return_node->getItemsNode() = std::move(items_list);

        return return_node;
    }

    QueryTreeNodePtr buildOrderByClause(const GAST::GQLOrderByClause & order_by)
    {
        auto order_by_node = std::make_shared<GQLOrderByNode>();

        // TODO: Convert order by items
        // auto items_list = std::make_shared<ListNode>();
        // for (const auto & item : order_by.items)
        //     items_list->getNodes().push_back(buildOrderByItem(item));
        // order_by_node->getItemsNode() = std::move(items_list);

        return order_by_node;
    }

    QueryTreeNodePtr buildPageClause(const GAST::GQLPageClause & page)
    {
        auto page_node = std::make_shared<GQLPageNode>();

        // TODO: Convert LIMIT and OFFSET expressions
        // if (page.limit)
        //     page_node->getLimit() = buildExpression(page.limit);
        // if (page.offset)
        //     page_node->getOffset() = buildExpression(page.offset);

        return page_node;
    }

    GQLCombinedQueryNode::CombinedOperator convertOperator(GAST::CombinedQueryOperator op)
    {
        switch (op)
        {
            case GAST::CombinedQueryOperator::UnionAll:
                return GQLCombinedQueryNode::CombinedOperator::UNION_ALL;
            case GAST::CombinedQueryOperator::UnionDistinct:
                return GQLCombinedQueryNode::CombinedOperator::UNION_DISTINCT;
            case GAST::CombinedQueryOperator::ExceptAll:
                return GQLCombinedQueryNode::CombinedOperator::EXCEPT_ALL;
            case GAST::CombinedQueryOperator::ExceptDistinct:
                return GQLCombinedQueryNode::CombinedOperator::EXCEPT_DISTINCT;
            case GAST::CombinedQueryOperator::IntersectAll:
                return GQLCombinedQueryNode::CombinedOperator::INTERSECT_ALL;
            case GAST::CombinedQueryOperator::IntersectDistinct:
                return GQLCombinedQueryNode::CombinedOperator::INTERSECT_DISTINCT;
        }

        throw Exception(ErrorCodes::LOGICAL_ERROR, "Unknown GQL combined query operator");
    }

    ContextPtr context;
};

} // anonymous namespace

QueryTreeNodePtr buildGQLQueryTree(const IAST & query, ContextPtr context)
{
    GQLQueryTreeBuilderImpl builder(std::move(context));
    return builder.build(query);
}

} // namespace GQL

} // namespace DB
