# DeviceAccess-OPC-UA-Backend

This backend uses the opcua client from open62541. The version of open62541 is the same as the one used in the ConstrolsystemAdapter-OPC-UA. 
`port` is a required parameter in the device mapping file and the syntax in the device mapping file is as following:
       
      # using target name 
      test (opcua:localhost?port=16664)
      # using target ip
      test (opcua:127.0.0.1?port=16664)

If authentification should be used when connecting to a server use optional device mapping parameters `username` and `password`.

Furthermore, there is a parameter called `publishingInterval`, which is relevant for asynchronous reading. It defines the smallest update time of data to the backend.
Changes that happen faster than the publishing interval will not be seen by the backend. The unit of the publishing interval is ms and in case no publishing interval is given 
a publishing of 500ms is used. 

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

### Behaviour after server restart

During the development, the reconnection after a connection error was tested. This was done by killing the server the backend was connected to. After, the server was restarted. On the backend side the client read server data very fast for 5 times and the result was always 0.
The 5 subsequent reads happen, because there is a full que of read triggers. The que was filled during the server was down by the PeriodicTrigger module that was used in the test. After the client recovered from the error state this que was emptied first. After, peridic triggers from the PeriodicTrigger module followed as expected and correct values were read from the server. 

The read value of 0 did not result e.g. from the client connection being not fully set up. But this is the initial register value after starting a ChimeraTK server. Thus, the opc server was opened but the application did not reach 
the first mainLoop during the first backend reads. So value of 0 read from the server via the backend is correct. 

### Considerations for testing the backend

- Write test against a server that is implemented using the same stack version
- The whole test should be a server that can change it's PV and also check if the backend changed some PV
- A second stage test could be against a ControlsystemAdapter-OPC-UA server -> this should be optional and not needed in the first place
- The backend should only depend on DeviceAccess and not on other ChimeraTK packages
- Test the backend also against other Server (in case of problems don't solve problems that are based on the open62541 stack)
