#include <Columns/ColumnNothing.h>
#include <DataTypes/DataTypeFactory.h>
#include <DataTypes/DataTypeNothing.h>
#include <DataTypes/Serializations/SerializationNothing.h>

namespace DB {

MutableColumnPtr DataTypeNothing::createColumn() const { return ColumnNothing::create(0); }

bool DataTypeNothing::equals(const IDataType &rhs) const { return typeid(rhs) == typeid(*this); }

SerializationPtr DataTypeNothing::doGetSerialization(const SerializationInfoSettings &) const {
  return std::make_shared<SerializationNothing>();
}

void registerDataTypeNothing(DataTypeFactory &factory) {
  factory.registerSimpleDataType("Nothing", [] { return DataTypePtr(std::make_shared<DataTypeNothing>()); });
}

}  // namespace DB
