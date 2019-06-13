# DeviceAccess-OPC-UA-Backend

This backend uses the opcua client from open62541. The version of open62541 is the same as the one used in the ConstrolsystemAdapter-OPC-UA. 
`port` is a required parameter in the device mapping file and the syntax in the device mapping file is as following:
       
      # using target name 
      test (opcua:localhost?port=16664)
      # using target ip
      test (opcua:127.0.0.1?port=16664)

If authentification should be used when connecting to a server use optional device mapping parameters `username` and `password`.

The backend can be used in two different ways:

- Automatic catalog creation via browsing the server (only works for ChimeraTK OPC-UA servers) &rarr; use `test (opcua:localhost?port=16664)` in the device mapping file
- Manual catalog creation via map file &rarr; use `test (opcua:localhost?port=16664&map=mymap.map)` in the device mapping file

## Map file based catalog creation

This options is useful when connecting to servers with many process variables. No browsing is done in that case and therefor no load is put on the target server.
The map file syntax is as following:

    #Sting node id       Namespace id
    /dir/var1            1
    #Numeric node id     Namespace id
    123                  1
    #New Name   Sting node id      Namespace id
    myname1     /dir/var1          1
    #New name   Numeric node id    Namespace id
    myname2     123                1 

In case a numeric id is used and no new name is given it will appear with the register path: /numeric/id.

## Technical details

**REMARK:**
Use the same version as used in the ConstrolsystemAdapter-OPC-UA, because a chimeraTK server should be able to use the backend. With different open62541 versions 
the same symbols will probably appear in the Server (coming from ConstrolsystemAdapter-OPC-UA) and the backend library (libDeviceAcces-OPC-UA-Backend.so)! 
This results in unpredictable behaviour and Segfaults that are hard to find.

It was already seen when using a DOOCS version for the DOOCS backend that is independent of the DOOCS version used in the ControlSystem adapter. That failed due to the reasons mentioned above. The only difference here is, that in case of DOOCS C++ is used and the open62541 stack is written in C. 


Things to consider:

- Write test against a server that is implemented using the same stack version
- The whole test should be a server that can change it's PV and also check if the backend changed some PV
- A second stage test could be against a ControlsystemAdapter-OPC-UA server -> this should be optional and not needed in the first place
- The backend should only depend on DeviceAccess and not on other ChimeraTK packages
- Test the backend also against other Server (in case of problems don't solve problems that are based on the open62541 stack)
