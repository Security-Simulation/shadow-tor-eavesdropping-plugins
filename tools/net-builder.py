#!/usr/bin/env python2

###############################################################################
# Scallion random network creator                                             #
#                                                                             #
# Copyright (C) 2014                                                          #
# Davide Berardi, Matteo Martelli                                             #
#                                                                             #
# This program is free software; you can redistribute it and/or               #
# modify it under the terms of the GNU General Public License                 #
# as published by the Free Software Foundation; either version 2              #
# of the License, or any later version.                                       #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU General Public License for more details.                                #
#                                                                             #
# You should have received a copy of the GNU General Public License           #
# along with this program; if not, write to the Free Software                 #
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,  #
# USA.                                                                        #
###############################################################################

# TODO : truly random topology, code-polishing

import xml.etree.ElementTree as ET;
import xml.dom.minidom as MD;
import random;

DEFAULT_TOR_ARGS = "1024 --quiet --Address ${NODEID} --Nickname ${NODEID} --DataDirectory ./data/${NODEID} --GeoIPFile ~/.shadow/share/geoip --BandwidthRate 1024000 --BandwidthBurst 1024000 --ControlPort 9051";
DEFAULT_TORCTL_ARGS = "localhost 9051 STREAM,CIRC,CIRC_MINOR,ORCONN,BW,STREAM_BW,CIRC_BW,CONN_BW,BUILDTIMEOUT_SET,CLIENTS_SEEN,GUARD,CELL_STATS,TB_EMPTY";
DEFAULT_AUTOSYS_ARGS = "localhost:10000 localhost:9000 evil_analyzer:12345"
DEFAULT_AUTOSYS_SERVER_ARGS = "any:80 localhost:8080 evil_analyzer:12345 server"

default_topology_path = "~/.shadow/share/topology.graphml.xml"

geocodes_pool = [
             "SD", "NI", "LK", "VG", "NC", "OM", "ME", "MC", "AO", "HT", "LT", "TR",
             "UY", "GA", "LU", "ML", "MY", "BA", "KZ", "MD", "SG", "JP", "US", "MX",
             "CN", "CL", "RU", "VI", "ES", "NL", "TN", "GM", "ZW", "DK", "TW", "PK",
             "CR", "BG", "EC", "AU", "SA", "PG", "HN", "HR", "BN", "BS", "UZ", "JO",
             "GI", "AI", "QA", "BZ", "TZ", "KH", "AL", "IR", "RO", "CZ", "PT", "GR",
             "NZ", "NO", "CO", "TH", "AR", "RW", "CD", "KG", "AD", "LA", "GU", "PY",
             "MA", "KY", "SR", "SN", "BW", "MW", "CY", "NG", "LV", "UA", "FI", "HU",
             "MO", "DM", "HK", "SK", "JM", "GD", "MU", "AF", "SV", "GL", "BJ", "KW",
             "YE", "FO", "BF", "PL", "FR", "KR", "AT", "DE", "CA", "CH", "BE", "VN",
             "IN", "IS", "MN", "MK", "MM", "KE", "SI", "GE", "GG", "CV", "NP", "CI",
             "NA", "BR", "GB", "IE", "ZA", "UG", "EG", "IT", "SE", "TJ", "IL", "PR",
             "AE", "BH", "BY", "BT", "GT", "ID", "DO", "PH", "LB", "TT", "MQ", "AG",
             "MT", "BD", "KN", "RE", "BM", "JE", "BO", "AN", "MR", "LY", "LI", "AM",
             "TC", "MG", "BB", "FJ", "SZ", "SM", "IM", "AZ", "DZ", "AX", "GP", "MV",
             "AW", "ZM", "LS", "MZ", "LC", "PE", "PS", "RS", "VE", "EE", "SC", "PA",
             "GH", "IQ", "SY" ];

random_pool = [];

class ScallionNetwork(object):

    root = None;
    killElement = None;
    serverpool = [];

    def __init__(self, entrytag="shadow", killtime = 0, random_topology = 0,
                 no_geohint = 0,
                 topology_path = default_topology_path):
        self.root = ET.Element(entrytag);

        if (killtime != 0):
            self.killElement = ET.SubElement(self.root, "kill");
            self.killElement.set("time", str(killtime));

        self.random_topology = random_topology;
        self.no_geohint = no_geohint;

        self.topology = ET.SubElement(self.root, "topology");
        self.setup_topology(path = topology_path); 

    def __str__(self):
        rough_string = ET.tostring(self.root, 'utf-8')
        reparsed = MD.parseString(rough_string)
        return reparsed.toprettyxml(indent="\t")

    def add_plugin(self, name, path="~/.shadow/plugins/libshadow-plugin-"):
        plugin = ET.SubElement(self.root, "plugin");
        plugin.set("id", name);
        plugin.set("path", path + name + ".so");


    # Setup the topology for the net
    # if random is == 0 then the world (default) graph is used
    def setup_topology(self, path=default_topology_path):
        if (not self.no_geohint):
            if (not self.random_topology):
                self.topology.set("path", path);
                self.geocodes_pool = geocodes_pool;
            else:
                self.geocodes_pool = random_pool;

    def setup_tor(self, node, argv0, start_time=1, tor_id="scallion", torctl_id="torctl", v3bandwidthsfile = ""):
        # tor
        tor_app = ET.SubElement(node, "application");
        tor_app.set("plugin", tor_id);
        tor_app.set("time", str(start_time));

        argv = argv0 + ' ' + DEFAULT_TOR_ARGS + ' ' + "-f ./" + argv0 + ".torrc";

        if (v3bandwidthsfile != ""):
            argv = argv + " --V3BandwidthsFile " + v3bandwidthsfile;

        tor_app.set("arguments", argv);

        # torctl
        torctl_app = ET.SubElement(node, "application");
        torctl_app.set("plugin", torctl_id);
        torctl_app.set("time", str(start_time + 1));
        torctl_app.set("arguments", DEFAULT_TORCTL_ARGS);

    def add_tor_control_node(self, id_str, argv0,
                            torctl_id="torctl",
                            quantity = 1,
                            start_time = 1,
                            geocodehint = "Random",
                            bandwidthdown=10240,
                            bandwidthup=10240,
                            v3bandwidthsfile = ""):
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);
        node.set("bandwidthdown", str(bandwidthdown));
        node.set("bandwidthup", str(bandwidthup));

        if (quantity > 1):
            node.set("quantity", str(quantity));

        self.set_geocode(node, geocodehint);

        self.setup_tor(node, argv0, start_time=start_time, v3bandwidthsfile = v3bandwidthsfile);


    def add_tor_4authority(self, name="4authority"):
        self.add_tor_control_node(name, "dirauth", v3bandwidthsfile = "./data/${NODEID}/dirauth.v3bw")

    def add_tor_relay(self, quantity = 1, name="relay"):
        self.add_tor_control_node(name, "relay", quantity = quantity)

    def add_tor_exit(self, quantity = 1, name="exit"):
        self.add_tor_control_node(name, "exitrelay", quantity = quantity)

    def add_client(self, id_str, plugin, argv, quantity=1, tor=0, geocodehint = "Random",
                            start_time=1000, tracked = 0):
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);

        # realign, otherwise the program will start too soon...
        if (start_time < 200):
            start_time = 200;

        if (tor):
            self.setup_tor(node, "client", start_time=start_time - 100);

        if (tracked):
            self.setup_tor_tracker_proxy(node, start_time=start_time-1, autosys_id="autosys");

        ctl_app = ET.SubElement(node, "application");
        ctl_app.set("plugin", plugin);
        ctl_app.set("time", str(start_time));
        ctl_app.set("arguments", argv);
        self.set_geocode(node, geocodehint);

    def add_server(self, id_str, plugin, argv, quantity=1, tor=0,
                            geocodehint = "Random",
                            start_time=10, tracked = 0):
        self.serverpool.append(id_str)
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);

        if (tor):
            # hidden service
            self.setup_tor(node, "client", start_time=start_time - 100);

        if (tracked):
            self.setup_tor_tracker_proxy(node, start_time=start_time-1, autosys_id="autosys", server=1);

        ctl_app = ET.SubElement(node, "application");
        ctl_app.set("plugin", plugin);
        ctl_app.set("time", str(start_time));
        ctl_app.set("arguments", argv);
        self.set_geocode(node, geocodehint);

    def setup_tor_tracker_proxy(self, node, start_time = 500, autosys_id="autosys", server=0):
        autosys_app = ET.SubElement(node, "application");
        autosys_app.set("plugin", autosys_id);
        autosys_app.set("time", str(start_time));

	if (not server):
	        argv = DEFAULT_AUTOSYS_ARGS;
	else:
            argv = DEFAULT_AUTOSYS_SERVER_ARGS;

        autosys_app.set("arguments", argv);

    def set_geocode(self, node, geocodehint = "Random"):
        if (not self.no_geohint):
            if (geocodehint == "Random"):
                geocodehint = random.choice(self.geocodes_pool);

            if (geocodehint != ""):
                node.set("geocodehint", geocodehint);
if __name__ == '__main__':
    from sys import argv;
    import getopt, sys, random;

    random.seed();

    authorityn = 2;
    relaysn = 20;
    exitsn = 5;
    clientsn = 100;
    serversn = 30;
    density = 'average';

    client_track_prob = 1.0;
    server_track_prob = 1.0;

    random_topology = 0;
    try:
    	opts, args = getopt.getopt(sys.argv[1:], "hD:r:e:c:s:C:S:a:t",
				   ["help", "density=","relays=","exits=","clients=","servers=",
				    "clientprob=", "serverprob=", "authorityes=",
  				    "random"]);
    except:
    	print sys.argv[0] + " --help --density=[slow,average,fast] --relays=N --exits=N --clients=N --servers=N --serverprob=float --clientprob=float --random";
    	sys.exit(2);
    for o, a in opts:
    	if o in ("-h", "--help"):
    		print sys.argv[0] + " --help --density=[slow,average,fast] --relays=N --exits=N --clients=N --servers=N --serverprob=float --clientprob=float";
    		sys.exit(2);
    	if o in ("-r", "--relays"):
    		relaysn = int(a);
    	if o in ("-e", "--exits"):
    		exitsn = int(a);
    	if o in ("-c", "--clients"):
    		clientsn = int(a);
    	if o in ("-C", "--clientprob"):
    		client_track_prob = float(a);
    	if o in ("-a", "--authorityes"):
    		authorityn = int(a);

    	if o in ("-s", "--servers"):
    		serversn = int(a);
    	if o in ("-S", "--serverprob"):
    		server_track_prob = float(a);
    	if o in ("-D", "--density"):
    		density = a;

	if o in ("-t", "--random"):
		randomtopology = 1

    sn = ScallionNetwork(killtime = 18000, random_topology = random_topology, no_geohint = 0);

    sn.add_plugin("simpletcp");
    sn.add_plugin("scallion");
    sn.add_plugin("torctl");
    sn.add_plugin("autosys");
    sn.add_plugin("analyzer");

    sn.add_client("evil_analyzer", "analyzer", "any:12345 ./data/analyzer_trace.log", start_time=600);
    # tor structure
    for i in range(authorityn):
            sn.add_tor_4authority(name="4uthority" + str(i+1));

    for i in range(relaysn):
	    sn.add_tor_relay(name="relay" + str(i));
    for i in range(exitsn):
	    sn.add_tor_exit(name="exit" + str(i));

    for i in range(serversn):
	    tracked = 0;
	    r = random.randint(0, 100);
	    if (float(r)/100.0 < float(server_track_prob)):
		tracked = 1;

	    if (tracked):
	    	    sn.add_server("server" + str(i), "simpletcp",
        	          "server any:8080 ./simpletcp", tor = 0, tracked = tracked);
	    else:
	    	    sn.add_server("server" + str(i), "simpletcp",
        	          "server any:80 ./simpletcp", tor = 0, tracked = tracked);

    for i in range(clientsn):
	    tracked = 0;
	    r = random.randint(0, 100);
	    if (float(r)/100.0 < client_track_prob):
		tracked = 1;

# gaussian-cut distribution
# GIUSTIFICA STA ROBA NELLA RELAZIONE
# SLOW
	    if (density == 'slow'):
		    randtime = int(random.gauss(800,40));
		    randtime_end = int(random.gauss(2000,40));
# AVERAGE
	    if (density == 'average'):
		    randtime = int(random.gauss(80,40));
		    randtime_end = int(random.gauss(1000,40));
# FAST
	    if (density == 'fast'):
		    randtime = int(random.gauss(20,40));
		    randtime_end = int(random.gauss(100,40));
	    
            if (tracked):
	    	sn.add_client("client" + str(i), "simpletcp", "client " +
			"localhost:10000 "
			 + random.choice(sn.serverpool) +
			 ":80 " + str(int(random.uniform(1,100))) +
			 " " + str(randtime) + "," + 
			 str(randtime + randtime_end),
			 tor = 1, start_time = 1400+i*2,
			 tracked = tracked);
            else:
	    	sn.add_client("client" + str(i), "simpletcp", "client " +
			"localhost:9000 "
			 + random.choice(sn.serverpool) +
			 ":80 " + str(random.randint(1,100)) +
			 " " + str(randtime) + "," + 
			 str(randtime + randtime_end),
			 tor = 1, start_time = 1000+i*2,
			 tracked = tracked);

    print sn
