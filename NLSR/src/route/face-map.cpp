/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014  University of Memphis,
 *                     Regents of the University of California
 *
 * This file is part of NLSR (Named-data Link State Routing).
 * See AUTHORS.md for complete list of NLSR authors and contributors.
 *
 * NLSR is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NLSR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NLSR, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \author A K M Mahmudul Hoque <ahoque1@memphis.edu>
 *
 **/
#include <iostream>
#include <list>
#include <utility>

#include "common.hpp"
#ifdef NS3_NLSR_SIM
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/names.h>
#include "model/ndn-l3-protocol.hpp"
#include "apps/ndn-nlsr-app.hpp"
#include "nlsr-logger.hpp"
#else
#include "logger.hpp"
#endif
#include "face-map.hpp"

namespace nlsr {

INIT_LOGGER("FaceMap");

void
FaceMap::writeLog()
{
  _LOG_DEBUG("------- Face Map-----------");
  for(std::list<FaceMapEntry>::iterator it = m_table.begin();
      it != m_table.end(); ++it) {
    _LOG_DEBUG("Face Map Entry (FaceUri: " << (*it).getFaceUri() << " Face Id: "
               << (*it).getFaceId() << ")");
#if 0
#ifdef NS3_NLSR_SIM
  ns3::Ptr<ns3::Node> thisNode;
  thisNode = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  _LOG_DEBUG("------- FIB Table of Node: " << ns3::Names::FindName(thisNode) << " -----------");
  ns3::Ptr<ns3::ndn::NlsrApp> nlsrApp = thisNode->GetApplication(0)->GetObject<ns3::ndn::NlsrApp> ();
  nlsrApp->GetNlsr().getFib().writeLog();
#endif
#endif
  }
}

} // namespace NLSR
