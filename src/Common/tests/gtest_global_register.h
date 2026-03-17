#pragma once

#include <AggregateFunctions/registerAggregateFunctions.h>
#include <Formats/registerFormats.h>
#include <Functions/registerFunctions.h>

inline void tryRegisterAggregateFunctions() {
  static struct Register {
    Register() { DB::registerAggregateFunctions(); }
  } registered;
}

inline void tryRegisterFunctions() {
  static struct Register {
    Register() { DB::registerFunctions(); }
  } registered;
}

inline void tryRegisterFormats() {
  static struct Register {
    Register() { DB::registerFormats(); }
  } registered;
}
