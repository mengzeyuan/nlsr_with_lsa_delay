/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

// ndn-nlsr-simple.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/log.h"

#include "utils/tracers/ndn-nlsr-tracer.hpp"

#include "utils/topology/nlsr-conf-reader.hpp"

// for LinkStatusControl::FailLinks and LinkStatusControl::UpLinks
#include "ns3/ndnSIM/helper/ndn-link-control-helper.hpp"

// ZhangYu 2019-6-2
#include "ns3/ndnSIM/helper/ndn-global-routing-helper.hpp"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NdnNlsrSimple");

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +---------+     1Mbps      +---------+
 *      | router1  | <------------> | router2 | <------------> | router3 |
 *      +----------+         10ms   +---------+          10ms  +---------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-nlsr-simple
 */

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Creating nodes
  NodeContainer nodes;

  // Build the NLSR network topology from nlsr.conf
  ndn::NlsrConfReader nlsrConfReader("src/ndnSIM/examples/ndn-nlsr-conf/nlsr_sim.conf", 15);
  nodes = nlsrConfReader.Read();

  ns3::ndn::NlsrTracer &tracer = ndn::NlsrTracer::Instance();
  std::string prefix = std::to_string(nodes.size());
  tracer.InitializeTracer(prefix);

  // Install NLSR app on all nodes.
  NS_LOG_INFO ("Installing NLSR application on " << nodes.size() << " nodes");
  ndn::AppHelper nlsrHelper ("ns3::ndn::NlsrApp");
  nlsrHelper.Install(nodes);

  // Install NDN stack on all nodes
  NS_LOG_INFO ("Installing NDN stack on " << nodes.size() << " nodes");
  ndn::StackHelper ndnHelper;
  ndnHelper.InstallAll();

  // multicast可以实现向多个接口转发兴趣包
  //ndn::StrategyChoiceHelper::InstallAll("/", "ndn:/localhost/nfd/strategy/multicast");
  // BestRouteStrategy2测试（链路中断后可以切换路径）
  //ndn::StrategyChoiceHelper::InstallAll("/", "ndn:/localhost/nfd/strategy/best-route");
  // ncc策略测试
  ndn::StrategyChoiceHelper::InstallAll("/", "ndn:/localhost/nfd/strategy/ncc");
 
  //ymz
  // Installing global routing interface on all nodes
  // ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  // ndnGlobalRoutingHelper.InstallAll();

  // Initialize the NLSR app on nodes.
  nlsrConfReader.InitializeNlsr();

  //ymz
  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue("1")); // 1 interest a second
  consumerHelper.Install(nodes.Get(0));                        // first node
  
  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(3)); // third node

  //ymz
  // Calculate and install FIBs
  // ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();

  // 节点1 DOWN:40s UP:50s
  Simulator::Schedule(Seconds(40.0), ndn::LinkControlHelper::FailLink, nodes.Get(0), nodes.Get(3));
  Simulator::Schedule(Seconds(80.0), ndn::LinkControlHelper::UpLink, nodes.Get(0), nodes.Get(3));
  Simulator::Schedule(Seconds(40.0), ndn::LinkControlHelper::FailLink, nodes.Get(0), nodes.Get(3));
  Simulator::Schedule(Seconds(80.0), ndn::LinkControlHelper::UpLink, nodes.Get(0), nodes.Get(3));

  Simulator::Stop (Seconds (200.0));

  //ndn::L3RateTracer::InstallAll ((prefix + "-nlsr-l3-rate-trace.txt"), Seconds (1));
  ndn::L3RateTracer::InstallAll (("my-nlsr-l3-rate-trace.txt"), Seconds (10));   //OK
  //L2RateTracer::InstallAll (("my-nlsr-l2-rate-trace.txt"));     //OK（这个是看丢包情况）
  //ndn::AppDelayTracer::InstallAll(("my-nlsr-app-delays-trace.txt"));
  
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}

