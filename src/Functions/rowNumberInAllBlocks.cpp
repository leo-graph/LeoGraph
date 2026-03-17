#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <Functions/FunctionFactory.h>
#include <Functions/IFunction.h>
#include <atomic>

namespace DB {
namespace {

/** Incremental number of row within all columns passed to this function. */
class FunctionRowNumberInAllBlocks : public IFunction {
 private:
  mutable std::atomic<size_t> rows{0};

 public:
  static constexpr auto name = "rowNumberInAllBlocks";
  static FunctionPtr create(ContextPtr) { return std::make_shared<FunctionRowNumberInAllBlocks>(); }

  /// Get the name of the function.
  String getName() const override { return name; }

  bool isStateful() const override { return true; }

  size_t getNumberOfArguments() const override { return 0; }

  bool isDeterministic() const override { return false; }

  bool isDeterministicInScopeOfQuery() const override { return false; }

  bool isSuitableForShortCircuitArgumentsExecution(const DataTypesWithConstInfo & /*arguments*/) const override { return false; }

  DataTypePtr getReturnTypeImpl(const DataTypes & /*arguments*/) const override { return std::make_shared<DataTypeUInt64>(); }

  ColumnPtr executeImplDryRun(const ColumnsWithTypeAndName &, const DataTypePtr &, size_t input_rows_count) const override {
    return ColumnUInt64::create(input_rows_count);
  }

  ColumnPtr executeImpl(const ColumnsWithTypeAndName &, const DataTypePtr &, size_t input_rows_count) const override {
    size_t current_row_number = rows.fetch_add(input_rows_count);

    auto column = ColumnUInt64::create();
    auto &data = column->getData();
    data.resize(input_rows_count);
    for (size_t i = 0; i < input_rows_count; ++i) data[i] = current_row_number + i;

    return column;
  }
};

}  // namespace

REGISTER_FUNCTION(RowNumberInAllBlocks) {
  FunctionDocumentation::Description description = R"(
Returns a unique row number for each row processed.
    )";
  FunctionDocumentation::Syntax syntax = "rowNumberInAllBlocks()";
  FunctionDocumentation::Arguments arguments = {};
  FunctionDocumentation::ReturnedValue returned_value = {"Returns the ordinal number of the row in the data block starting from `0`.",
                                                         {"UInt64"}};
  FunctionDocumentation::Examples examples = {{"Usage example",
                                               R"(
SELECT rowNumberInAllBlocks()
FROM
(
    SELECT *
    FROM system.numbers_mt
    LIMIT 10
)
SETTINGS max_block_size = 2
            )",
                                               R"(
┌─rowNumberInAllBlocks()─┐
│                      0 │
│                      1 │
└────────────────────────┘
┌─rowNumberInAllBlocks()─┐
│                      4 │
│                      5 │
└────────────────────────┘
┌─rowNumberInAllBlocks()─┐
│                      2 │
│                      3 │
└────────────────────────┘
┌─rowNumberInAllBlocks()─┐
│                      6 │
│                      7 │
└────────────────────────┘
┌─rowNumberInAllBlocks()─┐
│                      8 │
│                      9 │
└────────────────────────┘
            )"}};
  FunctionDocumentation::IntroducedIn introduced_in = {1, 1};
  FunctionDocumentation::Category category = FunctionDocumentation::Category::Other;
  FunctionDocumentation documentation = {description, syntax, arguments, {}, returned_value, examples, introduced_in, category};

  factory.registerFunction<FunctionRowNumberInAllBlocks>(documentation);
}

}  // namespace DB
