#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Enhanced WLAN Handoff");

int main(int argc, char *argv[]) {

    uint64_t payloadSize = 2048;
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "Payload", payloadSize);
    cmd.AddValue("simulationTime", "Simulation Time", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer deviceNode;
    deviceNode.Create(2);
    NodeContainer apNode;
    apNode.Create(10);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper();

    phy.SetChannel(channel.Create());

    Ssid ssid = Ssid("Public_WiFi");
    
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);    // WiFi 5
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));
    
    WifiMacHelper deviceMac;
    deviceMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer userDevice = wifi.Install(phy, deviceMac, deviceNode);
    NetDeviceContainer publicAp = wifi.Install(phy, apMac, apNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(deviceNode);

    Ipv4AddressHelper addr;
    addr.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer deviceInterface = addr.Assign(userDevice);
    Ipv4InterfaceContainer apInterface = addr.Assign(publicAp);

    MobilityHelper mobil;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();

    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    posAlloc->Add(Vector(50.0, 0.0, 0.0));
    mobil.SetPositionAllocator(posAlloc);
    // mobil.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobil.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                            "Bounds",
                            RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobil.Install(apNode);
    mobil.Install(deviceNode);

    UdpServerHelper server(100);
    ApplicationContainer serverApp = server.Install(deviceNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0));

    UdpClientHelper client(deviceInterface.GetAddress(0), 100);
    client.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
    client.SetAttribute("Interval", TimeValue(Time("0.00002")));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = client.Install(apNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime + 1.0));

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();
    Simulator::Destroy();

    uint32_t totalPacketsRecv = DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
    double throughput = totalPacketsRecv * payloadSize * 8 / (simulationTime * 1000000.0);
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    return 0;
}