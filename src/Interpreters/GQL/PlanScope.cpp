#include <Interpreters/GQL/PlanScope.h>

#include <Core/Block.h>

#include <algorithm>
#include <utility>

namespace DB::GQL
{

void PlanScope::replaceWithHeader(const Block & header, BindingKind kind)
{
    bindings.clear();
    bindings.reserve(header.columns());

    for (size_t i = 0; i < header.columns(); ++i)
    {
        const auto & column = header.getByPosition(i);
        setBinding(column.name, column.type, kind);
    }
}

bool PlanScope::hasBinding(const String & name) const
{
    return tryGetBinding(name) != nullptr;
}

const PlanBinding * PlanScope::tryGetBinding(const String & name) const
{
    const auto it = std::find_if(bindings.begin(), bindings.end(), [&](const auto & binding)
    {
        return binding.name == name;
    });

    if (it == bindings.end())
        return nullptr;

    return &*it;
}

void PlanScope::setBinding(String name, DataTypePtr type, BindingKind kind)
{
    if (name.empty())
        return;

    const auto it = std::find_if(bindings.begin(), bindings.end(), [&](const auto & binding)
    {
        return binding.name == name;
    });

    if (it != bindings.end())
    {
        it->type = std::move(type);
        it->kind = kind;
        return;
    }

    bindings.push_back(PlanBinding{
        .name = std::move(name),
        .type = std::move(type),
        .kind = kind,
    });
}

}
