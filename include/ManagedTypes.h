// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <open62541/types.h>

#include <string>
/*
 * ManagedTypes.h
 *
 *  Created on: Jan 28, 2026
 *      Author: Klaus Zenker (HZDR)
 */
namespace ChimeraTK {
  struct ManagedVariant {
    ManagedVariant();
    ~ManagedVariant();

    UA_Variant* var;
  };

  /**
   * Wrapper class for the UA_DataValue.
   *
   * It was introduced in order to clear memory from the FutureQueue that
   * is used by the OpcUABackendRegisterAccessor.
   * By default the UA_DataValue will not be cleared on destruction.
   * In the backend a ManagedDataValue is always created from a UA_DataValue
   * and after moved to the FutureQueue. Moving will not enable
   * clearing the UA_DataValue on destruction of the moved object.
   * So after the last move use clearDataOnDestruction().
   * If the assignment operator is used clearing the UA_DataValue on destruction is
   * enabled.
   */
  class ManagedDataValue {
   public:
    ManagedDataValue();

    /**
     * Does not take ownership of the internal UA_DataVale - will not be cleared.
     * This method is used to create a temporary object before moving it to the accessors
     * FutureQueue, when a second accessor is added for a Node that has already an accessor.
     *
     */
    ManagedDataValue(const ManagedDataValue& other);

    /**
     * Takes ownership by making sure the internal UA_DataVale will be cleared.
     */
    ManagedDataValue(const ManagedDataValue&& other) noexcept;

    /**
     * Does not take ownership of the internal UA_DataVale - will not be cleared.
     *
     * This method is used to create a temporary object before moving it to the accessors
     * FutureQueue
     */
    explicit ManagedDataValue(UA_DataValue* data);
    ~ManagedDataValue();
    [[nodiscard]] bool hasValue() const { return _val.hasValue; };

    /**
     * Moves ownership to the new ManagedDataValue.
     * The internal UA_DataVale of 'other' will not be cleared.
     */
    ManagedDataValue& operator=(ManagedDataValue&& other) noexcept;

    /**
     * Copy Variant. Takes the ownership of the Variant - the internal  UA_DataValue will be cleared.
     */
    void copyVariant(const UA_Variant& src, const std::string& dataRange);
    [[nodiscard]] void* getValue() const { return _val.value.data; }
    [[nodiscard]] UA_Variant* getVariant() { return &_val.value; }
    [[nodiscard]] UA_DateTime getSourceTime() const { return _val.sourceTimestamp; }
    [[nodiscard]] UA_StatusCode getStatus() const { return _val.status; }
    /**
     * Force clearing the UA_DataValue on destruction.
     */
    void clearDataOnDestruction() { _clearData = true; };

   private:
    UA_DataValue _val{};    ///< Data to be managed by this wrapper
    bool _clearData{false}; ///< If true the UA_DataValue _val is cleared on destruction
    void prepare();
  };
} // namespace ChimeraTK