<?xml version="1.0" encoding="UTF-8"?>
<simconf>
  <project EXPORT="discard">[APPS_DIR]/mrm</project>
  <project EXPORT="discard">[APPS_DIR]/mspsim</project>
  <project EXPORT="discard">[APPS_DIR]/avrora</project>
  <project EXPORT="discard">[APPS_DIR]/serial_socket</project>
  <project EXPORT="discard">[APPS_DIR]/collect-view</project>
  <project EXPORT="discard">[APPS_DIR]/powertracker</project>
  <simulation>
    <title>BMRF-test-z1</title>
    <randomseed>16796</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>50.0</transmitting_range>
      <interference_range>60.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.mspmote.Z1MoteType
      <identifier>z11</identifier>
      <description>udp-sink</description>
      <source EXPORT="discard">[CONTIKI_DIR]/Projects/rpl-density-udp/udp-sink.c</source>
      <commands EXPORT="discard">make udp-sink.z1 TARGET=z1</commands>
      <firmware EXPORT="copy">[CONTIKI_DIR]/Projects/rpl-density-udp/udp-sink.z1</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.RimeAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspButton</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDefaultSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspLED</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
    </motetype>
    <motetype>
      org.contikios.cooja.mspmote.Z1MoteType
      <identifier>z12</identifier>
      <description>udp-sender</description>
      <source EXPORT="discard">[CONTIKI_DIR]/Projects/rpl-density-udp/udp-sender.c</source>
      <commands EXPORT="discard">make udp-sender.z1 TARGET=z1</commands>
      <firmware EXPORT="copy">[CONTIKI_DIR]/Projects/rpl-density-udp/udp-sender.z1</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.RimeAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspButton</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDefaultSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspLED</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
    </motetype>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>9.907877676718702</x>
        <y>9.808289552234818</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>1</id>
      </interface_config>
      <motetype_identifier>z11</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>13.581661007915525</x>
        <y>4.315433492347074</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>2</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>1.9020459735346762</x>
        <y>14.51537407245684</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>3</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>12.49493967338325</x>
        <y>0.9029111957965141</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>4</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>12.860582487958872</x>
        <y>10.675959328055253</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>5</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>2.2243335185364677</x>
        <y>6.6772054813169905</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>6</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>19.34630091152746</x>
        <y>9.10706278114154</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>7</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>9.288459763129195</x>
        <y>9.106305241629762</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>8</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>9.574787934251198</x>
        <y>13.450924096831532</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>9</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>2.3583212342980486</x>
        <y>16.76878270117335</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>10</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>7.252703601212993</x>
        <y>11.002927918088412</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>11</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>5.3851794332559555</x>
        <y>15.4980177212825</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>12</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>9.746075511000626</x>
        <y>18.76539769236291</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>13</id>
      </interface_config>
      <motetype_identifier>z12</motetype_identifier>
    </mote>
  </simulation>
  <plugin>
    org.contikios.cooja.plugins.SimControl
    <width>280</width>
    <z>1</z>
    <height>160</height>
    <location_x>24</location_x>
    <location_y>616</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <viewport>1.7669007568023336 0.0 0.0 1.7669007568023336 -87.14323835061148 -96.78499285587132</viewport>
    </plugin_config>
    <width>370</width>
    <z>4</z>
    <height>262</height>
    <location_x>1</location_x>
    <location_y>1</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <width>687</width>
    <z>2</z>
    <height>822</height>
    <location_x>1209</location_x>
    <location_y>37</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.RadioLogger
    <plugin_config>
      <split>495</split>
      <formatted_time />
      <showdups>true</showdups>
      <hidenodests>false</hidenodests>
    </plugin_config>
    <width>1875</width>
    <z>0</z>
    <height>1033</height>
    <location_x>0</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    PowerTracker
    <width>629</width>
    <z>3</z>
    <height>523</height>
    <location_x>-10</location_x>
    <location_y>37</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>//import Java Package to JavaScript&#xD;
var version = java.lang.System.getProperty("java.version");&#xD;
if (version.startsWith("1.8.0")) {&#xD;
    load("nashorn:mozilla_compat.js");&#xD;
}&#xD;
importClass(java.util.ArrayList);&#xD;
var v = new ArrayList();&#xD;
importPackage(java.io);&#xD;
&#xD;
simulation_time = 7200000;&#xD;
&#xD;
TIMEOUT(7200000, timeout_func()); /* milliseconds */&#xD;
&#xD;
outs_count = new Array(12);&#xD;
outs = new Array(256);&#xD;
i = 0;&#xD;
while (i &lt; 256) {&#xD;
    outs[i]=new Array(12+1);&#xD;
    i++;&#xD;
}&#xD;
total_time = 0;&#xD;
total_ins = 0;&#xD;
&#xD;
pdr = 0;&#xD;
frame_tx = 0;&#xD;
msg_tx = 0;&#xD;
rx_time = 0;&#xD;
tx_time = 0;&#xD;
lpm_time = 0;&#xD;
cpu_time = 0;&#xD;
total_subscribed = 0;&#xD;
total_nodes = 12;&#xD;
&#xD;
fr = new File("raw_results.csv");&#xD;
fd = new File("delay_results.csv");&#xD;
fs = new File("summary_results.csv");&#xD;
fdup = new File("duplicates_results.csv");&#xD;
frawdup = new File("duplicates_raw_results.csv");&#xD;
//Use this if using the GUI &#xD;
// f = new File("log.txt");&#xD;
// tflog = new FileWriter(f);&#xD;
tfraw = new FileWriter(fr);&#xD;
tfdelay = new FileWriter(fd);&#xD;
tfsummary = new FileWriter(fs);&#xD;
tfduplicates = new FileWriter(fdup);&#xD;
tfraw_duplicates = new FileWriter(frawdup);&#xD;
timeout_func = function simulationEnd() {&#xD;
  /* Extract PowerTracker statistics */&#xD;
  plugin = mote.getSimulation().getCooja().getStartedPlugin("PowerTracker");&#xD;
  highest_node=0;&#xD;
  if (plugin != null) {&#xD;
      for (i=2; i&lt;= total_nodes+1; i++){&#xD;
      if (outs_count[i]&gt;0){&#xD;
          tfsummary.write(outs_count[i] + ";");&#xD;
          total_subscribed += outs_count[i];&#xD;
          highest_node=i;&#xD;
      } else{&#xD;
          tfsummary.write("0;");&#xD;
      }&#xD;
      log.log("outs_count["+i+"] = "+outs_count[i]+ " total_subscribed = " + total_subscribed + "\n");&#xD;
  }&#xD;
  log.log("total_nodes:"+total_nodes+"\n");&#xD;
    stats = plugin.radioStatistics();&#xD;
    splited_stats = stats.split(/\r\n|\n|\r/);&#xD;
    splited_on = splited_stats[0].split(" ");&#xD;
    splited_tx = splited_stats[1].split(" ");&#xD;
    splited_rx = splited_stats[2].split(" ");&#xD;
    splited_int = splited_stats[3].split(" ");&#xD;
    idle_listening = (splited_on[2]-splited_tx[2]-splited_rx[2])/(1000000*51);&#xD;
    //log.log("PowerTracker: Extracted statistics:\n"&#xD;
    //  + "AVG ON " + splited_on[2]/(1000000*51) + "s " + splited_on[4] + "%" +"\n"&#xD;
    //  + "AVG TX " + splited_tx[2]/(1000000*51) + "s " + splited_tx[4] + "%" + "\n"&#xD;
    //  + "AVG RX " + splited_rx[2]/(1000000*51) + "s " + splited_rx[4] + "%" + "\n"&#xD;
    //  + "AVG IDLE LISTENING " + idle_listening + "s " + idle_listening*100/(simulation_time/1000) + "%" + "\n"&#xD;
    //  + "AVG INT " + splited_int[2]/(1000000*51) + "s " + splited_int[4] + "%" + "\n"&#xD;
    //  + "\n");&#xD;
    splited_on_real_time = 0;&#xD;
    splited_tx_real_time = 0;&#xD;
    splited_rx_real_time = 0;&#xD;
    splited_int_real_time = 0;&#xD;
    splited_on_real_pcent = 0;&#xD;
    splited_tx_real_pcent = 0;&#xD;
    splited_rx_real_pcent = 0;&#xD;
    splited_int_real_pcent = 0;&#xD;
    for(i=2; i &lt;= total_nodes+1; i++){&#xD;
        splited_on_real_temp = splited_stats[10 + ((i-2)*5)].split(" ");&#xD;
        splited_tx_real_temp = splited_stats[10 + ((i-2)*5)+1].split(" ");&#xD;
        splited_rx_real_temp = splited_stats[10 + ((i-2)*5)+2].split(" ");&#xD;
        splited_int_real_temp = splited_stats[10 + ((i-2)*5)+3].split(" ");&#xD;
        //log.log("splited_on_real_time: " + splited_on_real_time + "+" + parseFloat(splited_on_real_temp[2]));&#xD;
        splited_on_real_time += parseFloat(splited_on_real_temp[2]);&#xD;
        //log.log("=" + splited_on_real_time + "\n");&#xD;
        //log.log("splited_tx_real_time: " + splited_tx_real_time + "+" + parseFloat(splited_tx_real_temp[2]));&#xD;
        splited_tx_real_time += parseFloat(splited_tx_real_temp[2]);&#xD;
        //log.log("=" + splited_tx_real_time + "\n");&#xD;
        //log.log("splited_rx_real_time: " + splited_rx_real_time + "+" + parseFloat(splited_rx_real_temp[2]));&#xD;
        splited_rx_real_time += parseFloat(splited_rx_real_temp[2]);&#xD;
        //log.log("=" + splited_rx_real_time + "\n");&#xD;
        //log.log("splited_int_real_time: " + splited_int_real_time + "+" + parseFloat(splited_int_real_temp[2]));&#xD;
        splited_int_real_time += parseFloat(splited_int_real_temp[2]);&#xD;
        //log.log("=" + splited_int_real_time + "\n");&#xD;
        //log.log("splited_on_real_pcent: " + splited_on_real_pcent + "+" + parseFloat(splited_on_real_temp[4]));&#xD;
        splited_on_real_pcent += parseFloat(splited_on_real_temp[4]);&#xD;
        //log.log("=" + splited_on_real_pcent + "\n");&#xD;
        //log.log("splited_tx_real_pcent: " + splited_tx_real_pcent + "+" + parseFloat(splited_tx_real_temp[4]));&#xD;
        splited_tx_real_pcent += parseFloat(splited_tx_real_temp[4]);&#xD;
        //log.log("=" + splited_tx_real_pcent + "\n");&#xD;
        //log.log("splited_rx_real_pcent: " + splited_rx_real_pcent + "+" + parseFloat(splited_rx_real_temp[4]));&#xD;
        splited_rx_real_pcent += parseFloat(splited_rx_real_temp[4]);&#xD;
        //log.log("=" + splited_rx_real_pcent + "\n");&#xD;
        //log.log("splited_int_real_pcent: " + splited_int_real_pcent + "+" + parseFloat(splited_int_real_temp[4]));&#xD;
        splited_int_real_pcent += parseFloat(splited_int_real_temp[4]);&#xD;
        //log.log("=" + splited_int_real_pcent + "\n");&#xD;
    }&#xD;
    idle_listening_real_time = (splited_on_real_time - splited_tx_real_time - splited_rx_real_time);&#xD;
    idle_listening_real_pcent = (splited_on_real_pcent - splited_tx_real_pcent - splited_rx_real_pcent);&#xD;
    tfsummary.write(&#xD;
      (splited_on_real_time/total_nodes).toFixed(0) + ";" + (splited_on_real_pcent/total_nodes).toFixed(3) +";"&#xD;
      + (splited_tx_real_time/total_nodes).toFixed(0) + ";" + (splited_tx_real_pcent/total_nodes).toFixed(3) + ";"&#xD;
      + (splited_rx_real_time/total_nodes).toFixed(0) + ";" + (splited_rx_real_pcent/total_nodes).toFixed(3) + ";"&#xD;
      + (idle_listening_real_time/total_nodes).toFixed(0) + ";" + (idle_listening_real_pcent/total_nodes).toFixed(3) + ";"&#xD;
      + (splited_int_real_time/total_nodes).toFixed(0) + ";" + (splited_int_real_pcent/total_nodes).toFixed(3));&#xD;
    log.log("PowerTracker: Extracted statistics REAL:\n"&#xD;
    + "AVG ON " + (splited_on_real_time/total_nodes).toFixed(0) + "s " + (splited_on_real_pcent/total_nodes).toFixed(3) + "%" +"\n"&#xD;
    + "AVG TX " + (splited_tx_real_time/total_nodes).toFixed(0) + "s " + (splited_tx_real_pcent/total_nodes).toFixed(3) + "%" + "\n"&#xD;
    + "AVG RX " + (splited_rx_real_time/total_nodes).toFixed(0) + "s " + (splited_rx_real_pcent/total_nodes).toFixed(3) + "%" + "\n"&#xD;
    + "AVG IDLE LISTENING " + (idle_listening_real_time/total_nodes).toFixed(0) + "s " + (idle_listening_real_pcent/total_nodes).toFixed(3) + "%" + "\n"&#xD;
    + "AVG INT " + (splited_int_real_time/total_nodes).toFixed(0) + "s " + (splited_int_real_pcent/total_nodes).toFixed(3) + "%" + "\n"&#xD;
    + "\n");&#xD;
    //tfsummary.write("PowerTracker: Extracted statistics:\n"&#xD;
    //+ "AVG ON " + splited_on[2]/(1000000*51) + "s " + splited_on[4] + "%" +"\n"&#xD;
    //+ "AVG TX " + splited_tx[2]/(1000000*51) + "s " + splited_tx[4] + "%" + "\n"&#xD;
    //+ "AVG RX " + splited_rx[2]/(1000000*51) + "s " + splited_rx[4] + "%" + "\n"&#xD;
    //+ "AVG IDLE LISTENING " + idle_listening + "s " + idle_listening*100/(simulation_time/1000) + "%" + "\n"&#xD;
    //+ "AVG INT " + splited_int[2]/(1000000*51) + "s " + splited_int[4] + "%" + "\n"&#xD;
    //+ "\n");&#xD;
  } else {&#xD;
    //log.log("No PowerTracker plugin\n");&#xD;
    tfsummary.write("No PowerTracker plugin\n");&#xD;
  }&#xD;
  //log.log("Average end-to-end delay: " + (total_time/total_ins) +"\n");&#xD;
  //log.log("Packet delivery ratio: " + (pdr/total_subscribed) + "\n");&#xD;
  //log.log("Frame transmission: " + (frame_tx/total_nodes) + "\n");&#xD;
  //log.log("Packet transmission: " + (msg_tx/total_nodes) + "\n");&#xD;
  //log.log("Rx time: " + (rx_time/total_nodes) + " = " + ((rx_time/total_nodes)/32768) + " s" + "\n");&#xD;
  //log.log("Tx time: " + (tx_time/total_nodes) + " = " + ((tx_time/total_nodes)/32768) + " s" + "\n");&#xD;
  //log.log("LPM time: " + (lpm_time/total_nodes) + " = " + ((lpm_time/total_nodes)/32768) + " s" + "\n");&#xD;
  //log.log("CPU time: " + (cpu_time/total_nodes) + " = " + ((cpu_time/total_nodes)/32768) + " s" + "\n");&#xD;
&#xD;
  tfsummary.write(";" + (total_time/total_ins).toFixed(5) +";"&#xD;
    + total_ins + ";" + total_subscribed + ";" + (total_ins/total_subscribed).toFixed(3) + ";"&#xD;
    + (frame_tx/total_nodes).toFixed(3) + ";"&#xD;
    + (msg_tx/total_nodes).toFixed(3) + ";"&#xD;
    + (rx_time/total_nodes).toFixed(3) + ";" + ((rx_time/total_nodes)/32768).toFixed(3) + ";"&#xD;
    + (tx_time/total_nodes).toFixed(3) + ";" + ((tx_time/total_nodes)/32768).toFixed(3) + ";"&#xD;
    + (lpm_time/total_nodes).toFixed(3) + ";" + ((lpm_time/total_nodes)/32768).toFixed(3) + ";"&#xD;
    + (cpu_time/total_nodes).toFixed(3) + ";" + ((cpu_time/total_nodes)/32768).toFixed(3) + "\n");&#xD;
    &#xD;
  try {&#xD;
    plugin = mote.getSimulation().getCooja().getStartedPlugin("org.contikios.cooja.plugins.RadioLogger");&#xD;
    if (plugin != null) {&#xD;
      //log.log("Found radio logger, saving to file packets.dat\n");&#xD;
      plugin.saveConnectionsToFile("packets.dat");&#xD;
    } else {&#xD;
     log.log("No radio logger started!\n");&#xD;
    }&#xD;
  }&#xD;
  catch(e){}&#xD;
  //tflog.close();&#xD;
  tfraw.close();&#xD;
  tfdelay.close();&#xD;
  tfsummary.close();&#xD;
  tfduplicates.close();&#xD;
  tfraw_duplicates.close();&#xD;
  log.testOK();&#xD;
}&#xD;
&#xD;
while(true){&#xD;
  YIELD();&#xD;
  time_msg = sim.getSimulationTimeMillis();&#xD;
  time_msg_seconds = time_msg/1000;&#xD;
  message = msg.split(";");&#xD;
  //log.log(""+message[0]+"\n");&#xD;
  if (message.length == 3 || message.length == 4) {&#xD;
    if (message[0]=="Out") {&#xD;
      outs[parseInt(message[1])][id] = time_msg;&#xD;
      //log.log("New out time\n");&#xD;
      //log.log("outs["+parseInt(message[1])+"] = "+outs[parseInt(message[1])]+" = "+time_msg+"\n");&#xD;
      outs_count[id] = parseInt(message[2]);&#xD;
      //log.log("outs_count["+id+"] = "+outs_count[id]+" = "+outs_count[id]+"\n");&#xD;
    } else if (message[0]=="In") {&#xD;
      total_time = total_time + (time_msg - outs[parseInt(message[1])][parseInt(message[2])]);&#xD;
      total_ins++;&#xD;
      //log.log("Delay: "+time_msg+" + "+outs[parseInt(message[1])]+" = "+(time_msg - outs[parseInt(message[1])])+"\n");&#xD;
      //log.log("test INS");&#xD;
      //log.log(""+parseInt(message[1])+";"+id+";"+(time_msg - outs[parseInt(message[1])])+"\n");&#xD;
      tfdelay.write(""+parseInt(message[2])+";"+parseInt(message[1])+";"+(time_msg - outs[parseInt(message[1])][parseInt(message[2])])+";"+parseInt(message[3])+"\n");&#xD;
      //log.log(""+parseInt(message[1])+";"+parseInt(message[2])+";"+(time_msg - outs[parseInt(message[1])][parseInt(message[2])])+"\n");&#xD;
    }else if (message[0]=="Duplicates") {&#xD;
      tfduplicates.write(""+id+";"+parseInt(message[1])+"\n");&#xD;
    }else if (message[0]=="Received duplicate") {&#xD;
      tfraw_duplicates.write((time_msg_seconds/60|0) + ":" + (time_msg_seconds % 60).toFixed(3) + ";ID:" + id + ";" + msg + "\n");&#xD;
    }&#xD;
  } else if (message.length == 7) {&#xD;
    //frame_tx += parseInt(message[0]);&#xD;
    msg_tx += parseInt(message[0]);&#xD;
    rx_time += parseInt(message[1]);&#xD;
    tx_time += parseInt(message[2]);&#xD;
    lpm_time += parseInt(message[3]);&#xD;
    cpu_time += parseInt(message[4]);&#xD;
    //tfraw.write((time_msg_seconds/60|0) + ":" + (time_msg_seconds % 60).toFixed(3) + ";ID:" + id + ";" + id + ";" + msg + "\n");&#xD;
    tfraw.write(id + ";" + msg + "\n");&#xD;
    log.log(id + ";" + msg + "\n");&#xD;
  }&#xD;
}</script>
      <active>true</active>
    </plugin_config>
    <width>1236</width>
    <z>0</z>
    <height>900</height>
    <location_x>479</location_x>
    <location_y>87</location_y>
  </plugin>
</simconf>

