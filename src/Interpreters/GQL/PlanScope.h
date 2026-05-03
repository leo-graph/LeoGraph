#pragma once

#include <Core/NamesAndTypes.h>
#include <base/types.h>

#include <vector>

namespace DB
{
class Block;
}

namespace DB::GQL
{

enum class BindingKind : UInt8
{
    Source,
    Projection,
};

struct PlanBinding
{
    String name;
    DataTypePtr type;
    BindingKind kind = BindingKind::Source;
};

class PlanScope final
{
public:
    void replaceWithHeader(const Block & header, BindingKind kind);

    bool hasBinding(const String & name) const;
    const PlanBinding * tryGetBinding(const String & name) const;
    const std::vector<PlanBinding> & getBindings() const { return bindings; }

private:
    void setBinding(String name, DataTypePtr type, BindingKind kind);

    std::vector<PlanBinding> bindings;
};

}
