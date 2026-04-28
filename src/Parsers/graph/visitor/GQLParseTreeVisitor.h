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
  std::any visitNestedQuerySpecification(GQLParser::NestedQuerySpecificationContext* context) override;
  std::any visitFocusedNestedQuerySpecification(GQLParser::FocusedNestedQuerySpecificationContext* context) override;
  std::any visitSelectStatement(GQLParser::SelectStatementContext* context) override;
  std::any visitLinearQueryStatement(GQLParser::LinearQueryStatementContext* context) override;
  std::any visitAmbientLinearQueryStatement(GQLParser::AmbientLinearQueryStatementContext* context) override;
  std::any visitSimpleLinearQueryStatement(GQLParser::SimpleLinearQueryStatementContext* context) override;
  std::any visitSimpleQueryStatement(GQLParser::SimpleQueryStatementContext* context) override;
  std::any visitCallQueryStatement(GQLParser::CallQueryStatementContext* context) override;
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
  std::any visitPpeMultisetAlternation(GQLParser::PpeMultisetAlternationContext* context) override;
  std::any visitPpePatternUnion(GQLParser::PpePatternUnionContext* context) override;
  std::any visitPathTerm(GQLParser::PathTermContext* context) override;
  std::any visitPfPathPrimary(GQLParser::PfPathPrimaryContext* context) override;
  std::any visitPfQuantifiedPathPrimary(GQLParser::PfQuantifiedPathPrimaryContext* context) override;
  std::any visitPfQuestionedPathPrimary(GQLParser::PfQuestionedPathPrimaryContext* context) override;
  std::any visitPpElementPattern(GQLParser::PpElementPatternContext* context) override;
  std::any visitPpParenthesizedPathPatternExpression(GQLParser::PpParenthesizedPathPatternExpressionContext* context) override;
  std::any visitPpSimplifiedPathPatternExpression(GQLParser::PpSimplifiedPathPatternExpressionContext* context) override;
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
  std::any visitLinearCatalogModifyingStatement(GQLParser::LinearCatalogModifyingStatementContext* context) override;
  std::any visitCreateSchemaStatement(GQLParser::CreateSchemaStatementContext* context) override;
  std::any visitDropSchemaStatement(GQLParser::DropSchemaStatementContext* context) override;
  std::any visitCreateGraphStatement(GQLParser::CreateGraphStatementContext* context) override;
  std::any visitDropGraphStatement(GQLParser::DropGraphStatementContext* context) override;
  std::any visitCreateGraphTypeStatement(GQLParser::CreateGraphTypeStatementContext* context) override;
  std::any visitDropGraphTypeStatement(GQLParser::DropGraphTypeStatementContext* context) override;
  std::any visitReturnStatement(GQLParser::ReturnStatementContext* context) override;
  std::any visitReturnStatementBody(GQLParser::ReturnStatementBodyContext* context) override;
  std::any visitReturnItemList(GQLParser::ReturnItemListContext* context) override;
  std::any visitReturnItem(GQLParser::ReturnItemContext* context) override;
  std::any visitGroupByClause(GQLParser::GroupByClauseContext* context) override;
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
  std::any visitNormalizedPredicateExprAlt(GQLParser::NormalizedPredicateExprAltContext* context) override;
  std::any visitPropertyGraphExprAlt(GQLParser::PropertyGraphExprAltContext* context) override;
  std::any visitBindingTableExprAlt(GQLParser::BindingTableExprAltContext* context) override;
  std::any visitNumericValueExpression(GQLParser::NumericValueExpressionContext* context) override;
  std::any visitParenthesizedValueExpression(GQLParser::ParenthesizedValueExpressionContext* context) override;
  std::any visitNonParenthesizedValueExpressionPrimary(GQLParser::NonParenthesizedValueExpressionPrimaryContext* context) override;
  std::any visitNonParenthesizedValueExpressionPrimarySpecialCase(
      GQLParser::NonParenthesizedValueExpressionPrimarySpecialCaseContext* context) override;
  std::any visitObjectExpressionPrimary(GQLParser::ObjectExpressionPrimaryContext* context) override;
  std::any visitCharacterStringValueExpression(GQLParser::CharacterStringValueExpressionContext* context) override;
  std::any visitByteStringValueExpression(GQLParser::ByteStringValueExpressionContext* context) override;
  std::any visitPathValueExpression(GQLParser::PathValueExpressionContext* context) override;
  std::any visitListValueExpression(GQLParser::ListValueExpressionContext* context) override;
  std::any visitDatetimeValueExpression(GQLParser::DatetimeValueExpressionContext* context) override;
  std::any visitDurationValueExpression(GQLParser::DurationValueExpressionContext* context) override;
  std::any visitNodeReferenceValueExpression(GQLParser::NodeReferenceValueExpressionContext* context) override;
  std::any visitEdgeReferenceValueExpression(GQLParser::EdgeReferenceValueExpressionContext* context) override;
  std::any visitValueQueryExpression(GQLParser::ValueQueryExpressionContext* context) override;
  std::any visitLetValueExpression(GQLParser::LetValueExpressionContext* context) override;
  std::any visitPathValueConstructor(GQLParser::PathValueConstructorContext* context) override;
  std::any visitListValueConstructorByEnumeration(GQLParser::ListValueConstructorByEnumerationContext* context) override;
  std::any visitRecordConstructor(GQLParser::RecordConstructorContext* context) override;
  std::any visitListLiteral(GQLParser::ListLiteralContext* context) override;
  std::any visitRecordLiteral(GQLParser::RecordLiteralContext* context) override;
  std::any visitAggregateFunction(GQLParser::AggregateFunctionContext* context) override;
  std::any visitValueFunction(GQLParser::ValueFunctionContext* context) override;
  std::any visitNumericValueFunction(GQLParser::NumericValueFunctionContext* context) override;
  std::any visitCharacterOrByteStringFunction(GQLParser::CharacterOrByteStringFunctionContext* context) override;
  std::any visitDatetimeValueFunction(GQLParser::DatetimeValueFunctionContext* context) override;
  std::any visitDurationValueFunction(GQLParser::DurationValueFunctionContext* context) override;
  std::any visitListValueFunction(GQLParser::ListValueFunctionContext* context) override;
  std::any visitCaseExpression(GQLParser::CaseExpressionContext* context) override;
  std::any visitCastSpecification(GQLParser::CastSpecificationContext* context) override;
  std::any visitGeneralValueSpecification(GQLParser::GeneralValueSpecificationContext* context) override;
  std::any visitDynamicParameterSpecification(GQLParser::DynamicParameterSpecificationContext* context) override;
  std::any visitGeneralLiteral(GQLParser::GeneralLiteralContext* context) override;
  std::any visitNullLiteral(GQLParser::NullLiteralContext* context) override;
  std::any visitTemporalLiteral(GQLParser::TemporalLiteralContext* context) override;
  std::any visitDateLiteral(GQLParser::DateLiteralContext* context) override;
  std::any visitTimeLiteral(GQLParser::TimeLiteralContext* context) override;
  std::any visitDatetimeLiteral(GQLParser::DatetimeLiteralContext* context) override;
  std::any visitDurationLiteral(GQLParser::DurationLiteralContext* context) override;
  std::any visitValueInitializer(GQLParser::ValueInitializerContext* context) override;
  std::any visitBooleanValueExpression(GQLParser::BooleanValueExpressionContext* context) override;
  std::any visitResultExpression(GQLParser::ResultExpressionContext* context) override;
  std::any visitCardinalityExpressionArgument(GQLParser::CardinalityExpressionArgumentContext* context) override;
  std::any visitTrimCharacterOrByteStringSource(GQLParser::TrimCharacterOrByteStringSourceContext* context) override;
  std::any visitStringLength(GQLParser::StringLengthContext* context) override;
  std::any visitIndependentValueExpression(GQLParser::IndependentValueExpressionContext* context) override;
  std::any visitForItemSource(GQLParser::ForItemSourceContext* context) override;
  std::any visitNumericValueExpressionDividend(GQLParser::NumericValueExpressionDividendContext* context) override;
  std::any visitNumericValueExpressionDivisor(GQLParser::NumericValueExpressionDivisorContext* context) override;
  std::any visitNumericValueExpressionBase(GQLParser::NumericValueExpressionBaseContext* context) override;
  std::any visitNumericValueExpressionExponent(GQLParser::NumericValueExpressionExponentContext* context) override;
  std::any visitGeneralLogarithmBase(GQLParser::GeneralLogarithmBaseContext* context) override;
  std::any visitGeneralLogarithmArgument(GQLParser::GeneralLogarithmArgumentContext* context) override;
  std::any visitValueExpressionPrimary(GQLParser::ValueExpressionPrimaryContext* context) override;
  std::any visitUnsignedValueSpecification(GQLParser::UnsignedValueSpecificationContext* context) override;
  std::any visitUnsignedLiteral(GQLParser::UnsignedLiteralContext* context) override;
  std::any visitLinearDataModifyingStatement(GQLParser::LinearDataModifyingStatementContext* context) override;

  AST::Ptr buildSubquery(GQLParser::ProcedureBodyContext* procedure_body, antlr4::ParserRuleContext* context);
};

}  // namespace DB::OPENGQL
