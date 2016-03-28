"tropeaproxy": a Shadow plug-in
=========================

This plug-in provides a simple TCP proxy. Also, `tropeaproxy` (abbreviated `tproxy`) whispers the hostname and 
the timestamp of each incoming connection to an external logger server.

`tproxy` born with the aim to simulate and analyse timing attacks against Tor. 
In particular `tproxy` can be placed on both client and server sides in order to let 
the logger server collect timing information about the TCP connections.


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
The example uses the hello plugin as a TCP client/server and the loggerserver plugin as 
the server that collects timing data.

After running the above, check the following files for process output:

  + `shadow.data/hosts/helloclient/stdout-tproxy-1000.log`
  + `shadow.data/hosts/helloserver/stdout-tproxy-1000.log`
  + `shadow.data/hosts/loggerserver/stdout-loggerserver-1000.log`
  + `connections.log`

The stdout log files should contain debug prints about the TCP connection initialized by helloclient 
and about the "Hello" and "World" forwarded messages. 
The `connections.log` file
should contain two strings, the first one representing the time of when helloclient connects to `tproxy` client side and
the second one representing the time when `tproxy` server side connects to helloserver.
Those strings should have the following format:
```
tproxyside;hostname;timestamp
```
where
  + `tproxyside` may be `c`(client) or `s`(server)
  + `hostname` is the network name of the machine where `tproxy` whose sent this message is running
  + `timestamp` the time when `tproxy` build this message just before sending it to the logger server.

Please see also the `example-tor.xml`, a more complex example which may be run in Shadow with the shdow-tor-plugin.

TODO...


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
