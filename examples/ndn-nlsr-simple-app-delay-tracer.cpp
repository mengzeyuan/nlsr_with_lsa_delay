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

// ndn-nlsr-simple-app-delay-tracer.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/log.h"

#include "utils/topology/nlsr-conf-reader.hpp"

// ZhangYu 2019-6-2
#include "ns3/ndnSIM/helper/ndn-global-routing-helper.hpp"

namespace ns3 {

//可以运行，但my-nlsr-app-delay-trace.txt中无内容

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

  // Install NLSR app on all nodes.
  // NS_LOG_INFO ("Installing NLSR application on " << nodes.size() << " nodes");
  ndn::AppHelper nlsrHelper ("ns3::ndn::NlsrApp");
  nlsrHelper.Install(nodes);

  // Install NDN stack on all nodes
  // NS_LOG_INFO ("Installing NDN stack on " << nodes.size() << " nodes");
  ndn::StackHelper ndnHelper;
  ndnHelper.InstallAll();

  // ncc策略
  ndn::StrategyChoiceHelper::InstallAll("/", "ndn:/localhost/nfd/strategy/ncc");
 
  // Initialize the NLSR app on nodes.
  nlsrConfReader.InitializeNlsr();

  //ymz
  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue("1")); // 1 interest a second
  consumerHelper.Install(nodes.Get(1));                        // first node
  
  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(6)); // third node

  Simulator::Stop (Seconds (120.0));

  ndn::AppDelayTracer::InstallAll(("my-nlsr-app-delay-trace.txt"));
  
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

