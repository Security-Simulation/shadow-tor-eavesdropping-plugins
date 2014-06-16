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

# TODO : topology and tracking

import xml.etree.ElementTree as ET;
import xml.dom.minidom as MD;
import random;

DEFAULT_TOR_ARGS = "1024 --quiet --Address ${NODEID} --Nickname ${NODEID} --DataDirectory ./data/${NODEID} --GeoIPFile ~/.shadow/share/geoip --BandwidthRate 1024000 --BandwidthBurst 1024000 --ControlPort 9051";
DEFAULT_TORCTL_ARGS = "localhost 9051 STREAM,CIRC,CIRC_MINOR,ORCONN,BW,STREAM_BW,CIRC_BW,CONN_BW,BUILDTIMEOUT_SET,CLIENTS_SEEN,GUARD,CELL_STATS,TB_EMPTY";

class ScallionNetwork(object):

    root = None;
    killElement = None;
    serverpool = [];

    def __init__(self, entrytag="shadow", killtime = 0):
        self.root = ET.Element(entrytag);

        if (killtime != 0):
            self.killElement = ET.SubElement(self.root, "kill");
            self.killElement.set("time", str(killtime));

    def __str__(self):
        rough_string = ET.tostring(self.root, 'utf-8')
        reparsed = MD.parseString(rough_string)
        return reparsed.toprettyxml(indent="\t")

    def add_plugin(self, name, path="~/.shadow/plugins/libshadow-plugin-"):
        plugin = ET.SubElement(self.root, "plugin");
        plugin.set("id", name);
        plugin.set("path", path + name + ".so");

    def setup_tor(self, node, argv0, start_time=1, tor_id="tor", torctl_id="torctl"):
        # tor
        tor_app = ET.SubElement(node, "application");
        tor_app.set("plugin", tor_id);
        tor_app.set("time", str(start_time));
        tor_app.set("arguments", argv0 + ' ' +
                    DEFAULT_TOR_ARGS + ' ' +
                    "-f ./" + node.get("id") + ".torrc");

        # torctl
        torctl_app = ET.SubElement(node, "application");
        torctl_app.set("plugin", torctl_id);
        torctl_app.set("time", str(start_time + 1));
        torctl_app.set("arguments", argv0 + ' ' +
                       DEFAULT_TORCTL_ARGS + ' ' +
                       "-f ./" + node.get("id") + ".torrc");

    def add_tor_control_node(self, id_str, argv0,
                            torctl_id="torctl",
                            quantity = 1,
                            start_time = 1,
                            bandwidthdown=10240,
                            bandwidthup=10240):
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);
        node.set("bandiwidthdown", str(bandwidthdown));
        node.set("bandiwidthup", str(bandwidthup));

        if (quantity > 1):
            node.set("quantity", str(quantity));

        self.setup_tor(node, argv0, start_time=start_time);


    def add_tor_4autority(self):
        self.add_tor_control_node("4autority", "dirauth")

    def add_tor_relay(self, quantity = 1):
        self.add_tor_control_node("relay", "relay", quantity = quantity)

    def add_tor_exit(self, quantity = 1):
        self.add_tor_control_node("exit", "exitrelay", quantity = quantity)

    def add_client(self, id_str, plugin, argv, quantity=1, tor=0,
                            start_time=1000, tracked = 0):
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);

        # realign, otherwise the program will start too soon...
        if (start_time < 200):
            start_time = 200;

        if (tor):
            self.setup_tor(node, "client", start_time=start_time - 100);

        if (tracked):
# TODO here we will implement the tracker.
#            self.setup_tor_tracker_proxy(node, start_time=start_time-1);
            pass

        ctl_app = ET.SubElement(node, "application");
        ctl_app.set("plugin", plugin);
        ctl_app.set("time", str(start_time));
        ctl_app.set("arguments", argv);

    def add_server(self, id_str, plugin, argv, quantity=1, tor=0,
                            start_time=10, tracked = 0):
        self.serverpool.append("id_str")
        node = ET.SubElement(self.root, "node");
        node.set("id", id_str);

        if (tor):
            # hidden service
            self.setup_tor(node, "client", start_time=start_time - 100);

        if (tracked):
# TODO here we will implement the tracker.
#            self.setup_tor_tracker_proxy(node, start_time=start_time-1);
            pass

        ctl_app = ET.SubElement(node, "application");
        ctl_app.set("plugin", plugin);
        ctl_app.set("time", str(start_time));
        ctl_app.set("arguments", argv);

if __name__ == '__main__':
    from sys import argv;
    sn = ScallionNetwork(killtime = 18000);

    sn.add_plugin("filetransfer");
    sn.add_plugin("scallion");
    sn.add_plugin("torctl");

    # tor structure
    sn.add_tor_4autority();
    sn.add_tor_relay(quantity = 3);
    sn.add_tor_exit(quantity = 2);

    sn.add_server("fileserver", "filetransfer",
                  "server 80 ~/.shadow/share", tor = 1, tracked = 1);

    sn.add_client("fileclient", "filetransfer", "client single "
                  + random.choice(sn.serverpool) +
                  " 80 localhost 9000 10 /1MiB.urnd", tor = 1, tracked = 1);

    print sn
