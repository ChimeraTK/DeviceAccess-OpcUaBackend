// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

#include <iostream>
#include <memory>

struct Test {
  UA_DataValue _val;
  void transfer(UA_DataValue&& val, bool clear = false) {
    std::cout << "Data moved." << std::endl;
    std::cout << "Pointer: " << &val.value << "\t" << &val.value.data << " received" << std::endl;
    UA_DataValue_clear(&_val);
    UA_DataValue_copy(&val, &_val);
    if(clear) {
      UA_DataValue_clear(&val);
    }
  }
  void transfer(UA_DataValue& val) {
    std::cout << "Pointer: " << &val.value << "\t" << &val.value.data << " received" << std::endl;
    UA_DataValue_clear(&_val);
    UA_DataValue_copy(&val, &_val);
  }
  void print() {
    UA_UInt32* tmp = (UA_UInt32*)(_val.value.data);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Data : %u", *tmp);
  }

  Test() { UA_DataValue_init(&_val); }

  ~Test() { UA_DataValue_clear(&_val); }
};

void testPointer() {
  Test t;
  for(size_t i = 2; i < 10; i++) {
    UA_DataValue* val = UA_DataValue_new();
    UA_DataValue_init(val);
    UA_UInt32* j = UA_UInt32_new();
    *j = i;
    // here the address of j is taken. Don't use &i, because UA_DataValue_delete will try to delete the Address of i
    UA_Variant_setScalar(&val->value, j, &UA_TYPES[UA_TYPES_INT32]);
    t.transfer(std::move(*val));
    t.transfer(*val);
    UA_UInt32* tmp = (UA_UInt32*)(val->value.data);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Data : %u", *tmp);
    UA_DataValue_delete(val);
    //      UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
    //                              "Data : %u", *tmp);
    t.print();
  }
}

void test() {
  Test t1, t2;
  for(UA_UInt32 i = 2; i < 10; i++) {
    UA_DataValue val;
    UA_DataValue_init(&val);
    UA_Variant_setScalar(&val.value, &i, &UA_TYPES[UA_TYPES_INT32]);

    std::cout << "Pointer: " << &val.value << "\t" << &val.value.data << " send" << std::endl;
    t1.transfer(std::move(val));
    t2.transfer(std::move(val));
    //    t.transfer(val);

    t1.print();
    t2.print();
    UA_UInt32* tmp = (UA_UInt32*)(val.value.data);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Data : %u", *tmp);
  }
}

void testCopy() {
  Test t1, t2;
  for(UA_UInt32 i = 2; i < 10; i++) {
    UA_DataValue val;
    UA_DataValue_init(&val);
    UA_Variant_setScalar(&val.value, &i, &UA_TYPES[UA_TYPES_INT32]);
    std::cout << "Pointer: " << &val.value << "\t" << &val.value.data << " send" << std::endl;
    UA_DataValue valCopy1, valCopy2;
    UA_DataValue_copy(&val, &valCopy1);
    UA_DataValue_copy(&val, &valCopy2);
    t1.transfer(std::move(valCopy1), true);
    t1.print();
    t2.transfer(std::move(valCopy2), true);
    t2.print();
    UA_UInt32* tmp = (UA_UInt32*)(val.value.data);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Data : %u", *tmp);
  }
}

void testCopyTwice() {}

int main() {
  // valgrind --leak-check=full ./memoryTest
  test();
  testPointer();
  testCopy();
}
