# DeviceAccess-OPC-UA-Backend

This backend uses the opcua client from open62541. The version of open62541 is the same as the one used in the ConstrolsystemAdapter-OPC-UA.

**REMARK:**
Use the same version as used in the ConstrolsystemAdapter-OPC-UA, because a chimeraTK server should be able to use the backend. With different open62541 versions 
the same symbols will probably appear in the Server (coming from ConstrolsystemAdapter-OPC-UA) and the backend library (libDeviceAcces-OPC-UA-Backend.so)! 
This results in unpredictable behaviour and Segfaults that are hard to find.

Martin already tried to use a DOOCS version for the DOOCS backend that is independent of the DOOCS version used in the ControlSystem adapter, which failed due to the reasons mentioned above. The only difference here is, that in case of DOOCS C++ is used and the open62541 stack is written in C. 


Things to consider:

- Write test against a server that is implemented using the same stack version
- The whole test should be a server that can change it's PV and also check if the backend changed some PV
- A second stage test could be against a ControlsystemAdapter-OPC-UA server -> this should be optional and not needed in the first place
- The backend should only depend on DeviceAccess and not on other ChimeraTK packages
- Test the backend also against other Server (in case of problems don't solve problems that are based on the open62541 stack)


## Supported data types

So far only `float`, `double`, `int32` and `uint` are handled by the backend. Other types are to be added on request. Casting of e.g. `double` to `float`  is not done and instead an exception is thrown. On the other hand, conversion from `float` to `double` or `uint` to `int32` is done without notification.