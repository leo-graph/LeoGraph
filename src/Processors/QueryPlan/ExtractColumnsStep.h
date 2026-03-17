#pragma once
#include <Interpreters/ActionsDAG.h>
#include <Processors/QueryPlan/ITransformingStep.h>

namespace DB {

class ExtractColumnsStep : public ITransformingStep {
 public:
  explicit ExtractColumnsStep(SharedHeader input_header_, const NamesAndTypesList& requested_columns_);
  String getName() const override { return "ExtractColumns"; }

  void transformPipeline(QueryPipelineBuilder& pipeline, const BuildQueryPipelineSettings& settings) override;

 private:
  void updateOutputHeader() override;

  NamesAndTypesList requested_columns;
};

}  // namespace DB
