"loggerserver": a Shadow plug-in
=========================

This plug-in simply acts as an UDP server: it listens for incoming UDP packets 
on a specified port and prints them on a log file.


copyright holders
-----------------
Davide Berardi, Matteo Martelli.

licensing deviations
--------------------

No deviations from LICENSE.

last known working version
--------------------------

This plug-in was last tested and known to work with `Shadow v1.11.1-11-gf349fe5 2016-03-11`

usage
-----

Please see the `example.xml`, which may be run in Shadow

```bash
shadow example.xml
```

After running the above, check the following files for process output:

  + `shadow.data/hosts/client1/stdout-loggerserver-1000.log`
  + `loggerserver.log`

The first one should contains the debug prints of the logger server application, while the second one 
should contains the payload of the two messages sent by the two clients.

A binary version of the code is available for usage outside of Shadow.
Run the program `loggerserver` providing a bind hostname, a bind port and a logfile:

```bash
loggerserver any:50000 loggerserver.log
```
Then you can test it with any software tool which can send UDP packets to a specified host and port.

```bash
nc -u localhost 50000
```
At the end of the test, logerserver.log should contain the payloads of all the UDP packets loggerserver received.
