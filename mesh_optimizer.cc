#include "ns3/core-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>

using namespace ns3;

int g_outSock = 0;
int g_inSock = 0;
struct sockaddr_in g_sendAddr, g_recvAddr;

void SetupSockets() {
    g_outSock = socket(AF_INET, SOCK_DGRAM, 0);
    g_sendAddr.sin_family = AF_INET;
    g_sendAddr.sin_port = htons(5555);
    inet_pton(AF_INET, "127.0.0.1", &g_sendAddr.sin_addr);

    g_inSock = socket(AF_INET, SOCK_DGRAM, 0);
    g_recvAddr.sin_family = AF_INET;
    g_recvAddr.sin_addr.s_addr = INADDR_ANY;
    g_recvAddr.sin_port = htons(5556);
    bind(g_inSock, (struct sockaddr *)&g_recvAddr, sizeof(g_recvAddr));
    fcntl(g_inSock, F_SETFL, O_NONBLOCK);
}

void TracePhy (std::string context, Ptr<const Packet> packet, uint16_t channelFreqMhz, 
              WifiTxVector txVector, MpduInfo mpduInfo, SignalNoiseDbm signalNoise, uint16_t staId)
{
    std::string sub = context.substr (10);
    size_t pos = sub.find ("/");
    std::string nodeID = sub.substr (0, pos);
    
    // Python'a Durumu Bildir
    std::string msg = nodeID + "," + std::to_string(signalNoise.signal);
    sendto(g_outSock, msg.c_str(), msg.length(), 0, (struct sockaddr *)&g_sendAddr, sizeof(g_sendAddr));

    // Python'dan Karar Oku
    char buffer[1024];
    int len = recv(g_inSock, buffer, sizeof(buffer) - 1, 0);
    if (len > 0) {
        buffer[len] = '\0';        
        std::cout << "--- [AI KARARI] Hedef Dugum: " << buffer << " uzerinden rota guncellendi! ---" << std::endl;
    }
}

int main (int argc, char *argv[])
{
    SetupSockets();
    uint32_t nNodes = 9;
    NodeContainer nodes;
    nodes.Create (nNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "DeltaX", DoubleValue (30.0), 
                                   "DeltaY", DoubleValue (30.0),
                                   "GridWidth", UintegerValue (3));
    
    // Hareketli model: Sinirlar NetAnim'de daha iyi gorunmesi icin ayarlandi
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=4.0]"));
    mobility.Install (nodes);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    MeshHelper mesh = MeshHelper::Default ();
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    NetDeviceContainer meshDevices = mesh.Install (phy, nodes);

    InternetStackHelper internet;
    internet.Install (nodes);
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (8));
    serverApps.Start (Seconds (1.0));

    UdpEchoClientHelper echoClient (interfaces.GetAddress (8), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (10000));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));

    Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", 
                     MakeCallback (&TracePhy));

    AnimationInterface anim ("mesh-rl-dynamic.xml");
    // Dugumlerin NetAnim'de renkli ve net gorunmesi icin
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Dugum " + std::to_string(i));
    }

    std::cout << "Simulasyon Basliyor... Terminali ve NetAnim'i izleyin." << std::endl;
    Simulator::Stop (Seconds (40.0));
    Simulator::Run ();
    
    close(g_outSock);
    close(g_inSock);
    Simulator::Destroy ();
    return 0;
}
