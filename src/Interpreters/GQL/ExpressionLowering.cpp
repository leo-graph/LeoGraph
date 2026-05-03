#include <Interpreters/GQL/ExpressionLowering.h>

#include <Common/Exception.h>
#include <Common/StringUtils.h>
#include <Core/Field.h>
#include <DataTypes/DataTypeNullable.h>
#include <DataTypes/DataTypeNothing.h>
#include <DataTypes/DataTypeString.h>
#include <DataTypes/DataTypesNumber.h>
#include <Functions/FunctionFactory.h>
#include <IO/ReadBufferFromString.h>
#include <IO/ReadHelpers.h>

#include <Poco/String.h>

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{
namespace
{

using GQLExpr = OPENGQL::AST::GQLExpr;

String kindToString(GQLExpr::Kind kind)
{
    switch (kind)
    {
        case GQLExpr::Kind::Identifier:
            return "Identifier";
        case GQLExpr::Kind::Literal:
            return "Literal";
        case GQLExpr::Kind::Property:
            return "Property";
        case GQLExpr::Kind::UnaryOp:
            return "UnaryOp";
        case GQLExpr::Kind::BinaryOp:
            return "BinaryOp";
        case GQLExpr::Kind::FunctionCall:
            return "FunctionCall";
        case GQLExpr::Kind::Cast:
            return "Cast";
        case GQLExpr::Kind::DurationBetween:
            return "DurationBetween";
        case GQLExpr::Kind::TrimString:
            return "TrimString";
        case GQLExpr::Kind::ExprList:
            return "ExprList";
        case GQLExpr::Kind::ValueQuery:
            return "ValueQuery";
        case GQLExpr::Kind::LetExpr:
            return "LetExpr";
        case GQLExpr::Kind::PathConstructor:
            return "PathConstructor";
        case GQLExpr::Kind::DynamicParameter:
            return "DynamicParameter";
        case GQLExpr::Kind::SpecialValue:
            return "SpecialValue";
        case GQLExpr::Kind::TemporalLiteral:
            return "TemporalLiteral";
        case GQLExpr::Kind::DurationLiteral:
            return "DurationLiteral";
        case GQLExpr::Kind::VariableExpression:
            return "VariableExpression";
        case GQLExpr::Kind::GraphExpression:
            return "GraphExpression";
        case GQLExpr::Kind::BindingTableExpression:
            return "BindingTableExpression";
    }

    return "Unknown";
}

[[noreturn]] void throwUnsupportedKind(const GQLExpr & expr)
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL expression kind: {}", kindToString(expr.kind));
}

[[noreturn]] void throwUnsupportedShape(const GQLExpr & expr, const String & reason)
{
    throw Exception(
        ErrorCodes::NOT_IMPLEMENTED,
        "Unsupported GQL {} expression: {}",
        kindToString(expr.kind),
        reason);
}

String normalizedOperator(String op)
{
    trim(op);
    return Poco::toUpper(op);
}

const GQLExpr & getChildExpression(const GQLExpr & expr, size_t index)
{
    if (index >= expr.children.size() || !expr.children[index])
        throwUnsupportedShape(expr, fmt::format("missing child {}", index));

    const auto * child = expr.children[index]->as<GQLExpr>();
    if (!child)
        throwUnsupportedShape(expr, fmt::format("child {} is not a GQL expression", index));

    return *child;
}

const ActionsDAG::Node & addConstant(ActionsDAG & dag, DataTypePtr type, Field value, String name)
{
    ColumnWithTypeAndName column;
    column.type = std::move(type);
    column.name = std::move(name);
    column.column = column.type->createColumnConst(1, value);
    return dag.addColumn(std::move(column));
}

String parseStringLiteral(const String & text)
{
    ReadBufferFromString in(text);
    String value;

    if (text.starts_with('"'))
        readDoubleQuotedStringWithSQLStyle(value, in);
    else
        readQuotedStringWithSQLStyle(value, in);

    assertEOF(in);
    return value;
}

const ActionsDAG::Node & lowerLiteral(const GQLExpr & expr, ActionsDAG & dag)
{
    String text = expr.text;
    trim(text);
    const String upper_text = Poco::toUpper(text);

    if (upper_text == "TRUE")
        return addConstant(dag, std::make_shared<DataTypeUInt8>(), UInt64(1), expr.text);

    if (upper_text == "FALSE")
        return addConstant(dag, std::make_shared<DataTypeUInt8>(), UInt64(0), expr.text);

    if (upper_text == "NULL")
        return addConstant(
            dag,
            std::make_shared<DataTypeNullable>(std::make_shared<DataTypeNothing>()),
            Null(),
            expr.text);

    if (text.size() >= 2 && ((text.front() == '\'' && text.back() == '\'') || (text.front() == '"' && text.back() == '"')))
        return addConstant(dag, std::make_shared<DataTypeString>(), parseStringLiteral(text), expr.text);

    try
    {
        if (text.find_first_of(".eE") != String::npos)
            return addConstant(dag, std::make_shared<DataTypeFloat64>(), parseFromString<Float64>(text), expr.text);

        if (text.starts_with('-'))
            return addConstant(dag, std::make_shared<DataTypeInt64>(), parseFromString<Int64>(text), expr.text);

        return addConstant(dag, std::make_shared<DataTypeUInt64>(), parseFromString<UInt64>(text), expr.text);
    }
    catch (...)
    {
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL literal: {}", expr.text);
    }
}

const ActionsDAG::Node & lowerSpecialValue(const GQLExpr & expr, ActionsDAG & dag)
{
    const String text = normalizedOperator(expr.text);

    if (text == "TRUE")
        return addConstant(dag, std::make_shared<DataTypeUInt8>(), UInt64(1), expr.text);

    if (text == "FALSE")
        return addConstant(dag, std::make_shared<DataTypeUInt8>(), UInt64(0), expr.text);

    if (text == "NULL")
        return addConstant(
            dag,
            std::make_shared<DataTypeNullable>(std::make_shared<DataTypeNothing>()),
            Null(),
            expr.text);

    if (text == "UNKNOWN")
        return addConstant(
            dag,
            std::make_shared<DataTypeNullable>(std::make_shared<DataTypeUInt8>()),
            Null(),
            expr.text);

    throwUnsupportedShape(expr, fmt::format("special value '{}' is not supported", expr.text));
}

const ActionsDAG::Node & addFunction(
    ActionsDAG & dag,
    ContextPtr context,
    const String & function_name,
    ActionsDAG::NodeRawConstPtrs children)
{
    auto function = FunctionFactory::instance().get(function_name, context);
    return dag.addFunction(function, std::move(children), {});
}

const ActionsDAG::Node & lowerProperty(const GQLExpr & expr, ActionsDAG & dag, ContextPtr context)
{
    const auto & base = lowerExpression(getChildExpression(expr, 0), dag, context);
    const auto & property = addConstant(dag, std::make_shared<DataTypeString>(), expr.text, expr.text);

    /// A future refactor might prefer `getSubcolumn` for struct types.
    return addFunction(dag, context, "tupleElement", {&base, &property});
}

const char * getUnaryFunctionName(const String & op)
{
    if (op == "NOT")
        return "not";

    if (op == "-")
        return "negate";

    return nullptr;
}

const ActionsDAG::Node & lowerUnaryOp(const GQLExpr & expr, ActionsDAG & dag, ContextPtr context)
{
    const auto * function_name = getUnaryFunctionName(normalizedOperator(expr.text));
    if (!function_name)
        throwUnsupportedShape(expr, fmt::format("operator '{}' is not supported", expr.text));

    const auto & operand = lowerExpression(getChildExpression(expr, 0), dag, context);
    return addFunction(dag, context, function_name, {&operand});
}

const char * getBinaryFunctionName(const String & op)
{
    if (op == "=")
        return "equals";
    if (op == "<>" || op == "!=")
        return "notEquals";
    if (op == ">")
        return "greater";
    if (op == ">=")
        return "greaterOrEquals";
    if (op == "<")
        return "less";
    if (op == "<=")
        return "lessOrEquals";
    if (op == "+")
        return "plus";
    if (op == "-")
        return "minus";
    if (op == "*")
        return "multiply";
    if (op == "/")
        return "divide";
    if (op == "AND")
        return "and";
    if (op == "OR")
        return "or";

    return nullptr;
}

bool isNullSpecialValue(const GQLExpr & expr)
{
    return expr.kind == GQLExpr::Kind::SpecialValue && normalizedOperator(expr.text) == "NULL";
}

const ActionsDAG::Node & lowerIsPredicate(const GQLExpr & expr, ActionsDAG & dag, ContextPtr context, bool negative)
{
    const auto & left_expr = getChildExpression(expr, 0);
    const auto & right_expr = getChildExpression(expr, 1);

    if (!isNullSpecialValue(right_expr))
        throwUnsupportedShape(expr, fmt::format("operator '{}' only supports NULL right operand", expr.text));

    const auto & left = lowerExpression(left_expr, dag, context);
    return addFunction(dag, context, negative ? "isNotNull" : "isNull", {&left});
}

const ActionsDAG::Node & lowerBinaryOp(const GQLExpr & expr, ActionsDAG & dag, ContextPtr context)
{
    const String op = normalizedOperator(expr.text);
    if (op == "IS")
        return lowerIsPredicate(expr, dag, context, false);
    if (op == "IS NOT")
        return lowerIsPredicate(expr, dag, context, true);

    const auto * function_name = getBinaryFunctionName(op);
    if (!function_name)
        throwUnsupportedShape(expr, fmt::format("operator '{}' is not supported", expr.text));

    const auto & left = lowerExpression(getChildExpression(expr, 0), dag, context);
    const auto & right = lowerExpression(getChildExpression(expr, 1), dag, context);
    return addFunction(dag, context, function_name, {&left, &right});
}

}

const ActionsDAG::Node & lowerExpression(const GQLExpr & expr, ActionsDAG & dag, ContextPtr context)
{
    switch (expr.kind)
    {
        case GQLExpr::Kind::Identifier:
        {
            if (const auto * node = dag.tryFindInOutputs(expr.text))
                return *node;

            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL identifier '{}' is not available in ActionsDAG outputs", expr.text);
        }
        case GQLExpr::Kind::Literal:
            return lowerLiteral(expr, dag);
        case GQLExpr::Kind::Property:
            return lowerProperty(expr, dag, context);
        case GQLExpr::Kind::UnaryOp:
            return lowerUnaryOp(expr, dag, context);
        case GQLExpr::Kind::BinaryOp:
            return lowerBinaryOp(expr, dag, context);
        case GQLExpr::Kind::SpecialValue:
            return lowerSpecialValue(expr, dag);
        case GQLExpr::Kind::FunctionCall:
        case GQLExpr::Kind::Cast:
        case GQLExpr::Kind::DurationBetween:
        case GQLExpr::Kind::TrimString:
        case GQLExpr::Kind::ExprList:
        case GQLExpr::Kind::ValueQuery:
        case GQLExpr::Kind::LetExpr:
        case GQLExpr::Kind::PathConstructor:
        case GQLExpr::Kind::DynamicParameter:
        case GQLExpr::Kind::TemporalLiteral:
        case GQLExpr::Kind::DurationLiteral:
        case GQLExpr::Kind::VariableExpression:
        case GQLExpr::Kind::GraphExpression:
        case GQLExpr::Kind::BindingTableExpression:
            throwUnsupportedKind(expr);
    }

    throwUnsupportedKind(expr);
}

}
