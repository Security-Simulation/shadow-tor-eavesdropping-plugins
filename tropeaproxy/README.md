"tropeaproxy": a Shadow plug-in
=========================

This plug-in provides a simple TCP proxy. Also, `tropeaproxy` (abbreviated `tproxy`) whispers the hostname and 
the timestamp of each incoming connection to an external logger server.

`tproxy` born with the aim to simulate and analyse timing attacks against Tor. 
In particular `tproxy` can be placed on both client and server sides in order to let 
the logger server collect timing information about the TCP connections.


copyright holde.s
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

The latter shows an example of how tproxy can be used in a simulated Tor network with 2 clients, 2 servers, 5 Tor relays, 2 Tor exit node and a logger server.

In particular, the tproxy plugin runs only on 1 server and on 2 clients.

This example wants to show how a timing attack simulation against Tor can be afforded,
in fact, once the simulation ends, one can look at the logger server output file in order to analyse 
the timing relations about the logged connections.

A binary version of the code is available for usage outside of Shadow.
Run the program `tproxy` providing a bind hostname and port, a forward address and port and an address and port for the logger server.
Also, the optional `server` argument can be provided to inform tproxy that it's running on a server. This one will just affect the log messages.
Thus, in order to test that the proxy is working run on a proxy host:
```bash
tproxy any:5000 serverhost:5000 loggerserver:12345
```
Then you can test it with any software tool which can act as a TCP client and as a TCP server.
On the server host:
```bash
nc -lp 5000
```
and on the client host:
```bash
nc proxyhost 5000 
```
At the end of the test, the logger server log file should contain the connection timing data and tproxy should have correctly forwarded 
all the TCP packets.
