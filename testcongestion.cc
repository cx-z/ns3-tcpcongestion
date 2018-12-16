/* 
   n0         n4
     \        /
      n1----n3
     /        \
   n2          n5
 */
#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FifthScriptExample");

class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void 
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
}

static void
RxDrop (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}

int 
main (int argc, char *argv[])
{
  //设置拥塞控制算法
  std::string tcpModel ("ns3::TcpBic");
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpModel));

  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  NodeContainer nodes;
  nodes.Create (6);
  
  NodeContainer n0n1 = NodeContainer(nodes.Get(0),nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1),nodes.Get(2));
  NodeContainer n1n3 = NodeContainer(nodes.Get(1),nodes.Get(3));
  NodeContainer n3n4 = NodeContainer(nodes.Get(3),nodes.Get(4));
  NodeContainer n3n5 = NodeContainer(nodes.Get(3),nodes.Get(5));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 =p2p.Install(n0n1);
  NetDeviceContainer d1d2 =p2p.Install(n1n2);
  NetDeviceContainer d1d3 =p2p.Install(n1n3);
  NetDeviceContainer d3d4 =p2p.Install(n3n4);
  NetDeviceContainer d3d5 =p2p.Install(n3n5);

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00000));
  d0d1.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  d1d2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  d1d3.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  d3d4.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  d3d5.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase("10.1.2.0","255.255.255.0");
  Ipv4InterfaceContainer i2i1=address.Assign(d1d2);

  address.SetBase("10.1.3.0","255.255.255.0");
  address.Assign(d1d3);

  address.SetBase("10.1.4.0","255.255.255.0");
  Ipv4InterfaceContainer i3i4=address.Assign(d3d4);

  address.SetBase("10.1.5.0","255.255.255.0");
  Ipv4InterfaceContainer i3i5 = address.Assign(d3d5);


  uint16_t sinkPort = 8080;
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));

  Address sink1Address (InetSocketAddress (i0i1.GetAddress (0), sinkPort));
  ApplicationContainer sinkApp1 = packetSinkHelper.Install (nodes.Get (0));
  sinkApp1.Start (Seconds (0.));
  sinkApp1.Stop (Seconds (20.));

  Address sink2Address (InetSocketAddress (i2i1.GetAddress (1), sinkPort));
  ApplicationContainer sinkApp2 = packetSinkHelper.Install (nodes.Get (2));
  sinkApp2.Start (Seconds (0.));
  sinkApp2.Stop (Seconds (20.));

  
  Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket (nodes.Get (5), TcpSocketFactory::GetTypeId ());
  //输出窗口大小
  //ns3TcpSocket1->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  
  Ptr<MyApp> app1 = CreateObject<MyApp> ();
  app1->Setup (ns3TcpSocket1, sink1Address, 1024, 1000, DataRate ("1Mbps"));
  nodes.Get (5)->AddApplication (app1);
  app1->SetStartTime (Seconds (1.));
  app1->SetStopTime (Seconds (10.));

  Ptr<Socket> ns3TcpSocket2 = Socket::CreateSocket (nodes.Get (4), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket2->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app2->Setup (ns3TcpSocket2, sink2Address, 1024, 2000, DataRate ("1Mbps"));
  nodes.Get (4)->AddApplication (app2);
  app2->SetStartTime (Seconds (1.));
  app2->SetStopTime (Seconds (20.));

  //输出丢包
  d0d1.Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));//收节点0
  d1d2.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));//收节点2
  //nodes.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));//汇集节点1

  Simulator::Stop (Seconds (20));
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

