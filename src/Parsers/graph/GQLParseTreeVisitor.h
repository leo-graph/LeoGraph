#pragma once

#include <Parsers/graph/fwd_decl.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
#pragma clang diagnostic ignored "-Wdocumentation-html"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wshadow-field"
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#include <GQLBaseVisitor.h>
#pragma clang diagnostic pop

namespace DB::OPENGQL {

class GQLParseTreeVisitor final : public GQLBaseVisitor {
 public:
  std::any visitCompositeQueryStatement(GQLParser::CompositeQueryStatementContext* context) override;
  std::any visitCompositeQueryExpression(GQLParser::CompositeQueryExpressionContext* context) override;
  std::any visitCompositeQueryPrimary(GQLParser::CompositeQueryPrimaryContext* context) override;
  std::any visitLinearQueryStatement(GQLParser::LinearQueryStatementContext* context) override;
  std::any visitAmbientLinearQueryStatement(GQLParser::AmbientLinearQueryStatementContext* context) override;
  std::any visitSimpleLinearQueryStatement(GQLParser::SimpleLinearQueryStatementContext* context) override;
  std::any visitSimpleQueryStatement(GQLParser::SimpleQueryStatementContext* context) override;
  std::any visitPrimitiveQueryStatement(GQLParser::PrimitiveQueryStatementContext* context) override;
  std::any visitMatchStatement(GQLParser::MatchStatementContext* context) override;
  std::any visitSimpleMatchStatement(GQLParser::SimpleMatchStatementContext* context) override;
  std::any visitOptionalMatchStatement(GQLParser::OptionalMatchStatementContext* context) override;
  std::any visitFilterStatement(GQLParser::FilterStatementContext* context) override;
  std::any visitWhereClause(GQLParser::WhereClauseContext* context) override;
  std::any visitGraphPatternBindingTable(GQLParser::GraphPatternBindingTableContext* context) override;
  std::any visitGraphPattern(GQLParser::GraphPatternContext* context) override;
  std::any visitGraphPatternWhereClause(GQLParser::GraphPatternWhereClauseContext* context) override;
  std::any visitPathPatternList(GQLParser::PathPatternListContext* context) override;
  std::any visitPathPattern(GQLParser::PathPatternContext* context) override;
  std::any visitPpePathTerm(GQLParser::PpePathTermContext* context) override;
  std::any visitPathTerm(GQLParser::PathTermContext* context) override;
  std::any visitPfPathPrimary(GQLParser::PfPathPrimaryContext* context) override;
  std::any visitPfQuantifiedPathPrimary(GQLParser::PfQuantifiedPathPrimaryContext* context) override;
  std::any visitPfQuestionedPathPrimary(GQLParser::PfQuestionedPathPrimaryContext* context) override;
  std::any visitPpElementPattern(GQLParser::PpElementPatternContext* context) override;
  std::any visitElementPattern(GQLParser::ElementPatternContext* context) override;
  std::any visitNodePattern(GQLParser::NodePatternContext* context) override;
  std::any visitElementPatternFiller(GQLParser::ElementPatternFillerContext* context) override;
  std::any visitElementPatternWhereClause(GQLParser::ElementPatternWhereClauseContext* context) override;
  std::any visitElementPropertySpecification(GQLParser::ElementPropertySpecificationContext* context) override;
  std::any visitPropertyKeyValuePairList(GQLParser::PropertyKeyValuePairListContext* context) override;
  std::any visitPropertyKeyValuePair(GQLParser::PropertyKeyValuePairContext* context) override;
  std::any visitEdgePattern(GQLParser::EdgePatternContext* context) override;
  std::any visitFullEdgePattern(GQLParser::FullEdgePatternContext* context) override;
  std::any visitFullEdgePointingLeft(GQLParser::FullEdgePointingLeftContext* context) override;
  std::any visitFullEdgeUndirected(GQLParser::FullEdgeUndirectedContext* context) override;
  std::any visitFullEdgePointingRight(GQLParser::FullEdgePointingRightContext* context) override;
  std::any visitFullEdgeLeftOrUndirected(GQLParser::FullEdgeLeftOrUndirectedContext* context) override;
  std::any visitFullEdgeUndirectedOrRight(GQLParser::FullEdgeUndirectedOrRightContext* context) override;
  std::any visitFullEdgeLeftOrRight(GQLParser::FullEdgeLeftOrRightContext* context) override;
  std::any visitFullEdgeAnyDirection(GQLParser::FullEdgeAnyDirectionContext* context) override;
  std::any visitAbbreviatedEdgePattern(GQLParser::AbbreviatedEdgePatternContext* context) override;
  std::any visitLabelExpressionNegation(GQLParser::LabelExpressionNegationContext* context) override;
  std::any visitLabelExpressionDisjunction(GQLParser::LabelExpressionDisjunctionContext* context) override;
  std::any visitLabelExpressionConjunction(GQLParser::LabelExpressionConjunctionContext* context) override;
  std::any visitLabelExpressionName(GQLParser::LabelExpressionNameContext* context) override;
  std::any visitLabelExpressionWildcard(GQLParser::LabelExpressionWildcardContext* context) override;
  std::any visitLabelExpressionParenthesized(GQLParser::LabelExpressionParenthesizedContext* context) override;
  std::any visitPrimitiveResultStatement(GQLParser::PrimitiveResultStatementContext* context) override;
  std::any visitReturnStatement(GQLParser::ReturnStatementContext* context) override;
  std::any visitReturnStatementBody(GQLParser::ReturnStatementBodyContext* context) override;
  std::any visitReturnItemList(GQLParser::ReturnItemListContext* context) override;
  std::any visitReturnItem(GQLParser::ReturnItemContext* context) override;
  std::any visitOrderByAndPageStatement(GQLParser::OrderByAndPageStatementContext* context) override;
  std::any visitOrderByClause(GQLParser::OrderByClauseContext* context) override;
  std::any visitSortSpecificationList(GQLParser::SortSpecificationListContext* context) override;
  std::any visitSortSpecification(GQLParser::SortSpecificationContext* context) override;
  std::any visitLimitClause(GQLParser::LimitClauseContext* context) override;
  std::any visitOffsetClause(GQLParser::OffsetClauseContext* context) override;
  std::any visitSearchCondition(GQLParser::SearchConditionContext* context) override;
  std::any visitAggregatingValueExpression(GQLParser::AggregatingValueExpressionContext* context) override;
  std::any visitConjunctiveExprAlt(GQLParser::ConjunctiveExprAltContext* context) override;
  std::any visitMultDivExprAlt(GQLParser::MultDivExprAltContext* context) override;
  std::any visitSignedExprAlt(GQLParser::SignedExprAltContext* context) override;
  std::any visitIsNotExprAlt(GQLParser::IsNotExprAltContext* context) override;
  std::any visitNotExprAlt(GQLParser::NotExprAltContext* context) override;
  std::any visitValueFunctionExprAlt(GQLParser::ValueFunctionExprAltContext* context) override;
  std::any visitConcatenationExprAlt(GQLParser::ConcatenationExprAltContext* context) override;
  std::any visitDisjunctiveExprAlt(GQLParser::DisjunctiveExprAltContext* context) override;
  std::any visitComparisonExprAlt(GQLParser::ComparisonExprAltContext* context) override;
  std::any visitPrimaryExprAlt(GQLParser::PrimaryExprAltContext* context) override;
  std::any visitAddSubtractExprAlt(GQLParser::AddSubtractExprAltContext* context) override;
  std::any visitPredicateExprAlt(GQLParser::PredicateExprAltContext* context) override;
  std::any visitValueExpressionPrimary(GQLParser::ValueExpressionPrimaryContext* context) override;
  std::any visitUnsignedValueSpecification(GQLParser::UnsignedValueSpecificationContext* context) override;
  std::any visitUnsignedLiteral(GQLParser::UnsignedLiteralContext* context) override;

 private:
  AST::Ptr makeRawTextExpr(antlr4::ParserRuleContext* context) const;
};

}  // namespace DB::OPENGQL
