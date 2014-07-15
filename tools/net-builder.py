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

# TODO : truly random topology, tracking, user interface and code-polishing

import xml.etree.ElementTree as ET;
import xml.dom.minidom as MD;
import random;

DEFAULT_TOR_ARGS = "1024 --quiet --Address ${NODEID} --Nickname ${NODEID} --DataDirectory ./data/${NODEID} --GeoIPFile ~/.shadow/share/geoip --BandwidthRate 1024000 --BandwidthBurst 1024000 --ControlPort 9051";
DEFAULT_TORCTL_ARGS = "localhost 9051 STREAM,CIRC,CIRC_MINOR,ORCONN,BW,STREAM_BW,CIRC_BW,CONN_BW,BUILDTIMEOUT_SET,CLIENTS_SEEN,GUARD,CELL_STATS,TB_EMPTY";

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

        argv = argv0 + ' ' + DEFAULT_TOR_ARGS + ' ' + "-f ./" + node.get("id") + ".torrc";

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


    def add_tor_4autority(self):
        self.add_tor_control_node("4autority", "dirauth", v3bandwidthsfile = "./data/${NODEID}/dirauth.v3bw")

    def add_tor_relay(self, quantity = 1):
        self.add_tor_control_node("relay", "relay", quantity = quantity)

    def add_tor_exit(self, quantity = 1):
        self.add_tor_control_node("exit", "exitrelay", quantity = quantity)

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
            self.setup_tor_tracker_proxy(node, start_time=start_time-1);

        ctl_app = ET.SubElement(node, "application");
        ctl_app.set("plugin", plugin);
        ctl_app.set("time", str(start_time));
        ctl_app.set("arguments", argv);
        self.set_geocode(node, geocodehint);

    def add_server(self, id_str, plugin, argv, quantity=1, tor=0,
                            geocodehint = "Random",
                            start_time=10, tracked = 0):
        self.serverpool.append("id_str")
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);

        if (tor):
            # hidden service
            self.setup_tor(node, "client", start_time=start_time - 100);

        if (tracked):
            self.setup_tor_tracker_proxy(node, start_time=start_time-1);

        ctl_app = ET.SubElement(node, "application");
        ctl_app.set("plugin", plugin);
        ctl_app.set("time", str(start_time));
        ctl_app.set("arguments", argv);
        self.set_geocode(node, geocodehint);

    def setup_tor_tracker_proxy(self, node, start_time = 500):
        pass

    def set_geocode(self, node, geocodehint = "Random"):
        if (not self.no_geohint):
            if (geocodehint == "Random"):
                geocodehint = random.choice(self.geocodes_pool);

            if (geocodehint != ""):
                node.set("geocodehint", geocodehint);
if __name__ == '__main__':
    from sys import argv;

    random_topology = 0;
    if (len(argv) > 1):
        random_topology = int(argv[1]);

    sn = ScallionNetwork(killtime = 1800, random_topology = random_topology, no_geohint = 0);

    sn.add_plugin("filetransfer");
    sn.add_plugin("scallion");
    sn.add_plugin("torctl");

    # tor structure
    sn.add_tor_4autority();
    sn.add_tor_relay(quantity = 3);
    sn.add_tor_exit(quantity = 2);

    sn.add_server("fileserver", "filetransfer",
                  "server 80 ~/.shadow/share", tor = 0, tracked = 1);

    sn.add_client("fileclient", "filetransfer", "client single "
                  + random.choice(sn.serverpool) +
                  " 80 localhost 9000 10 /1MiB.urnd", tor = 1, tracked = 1);

    print sn
