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
 **/

#include "face-controller.hpp"

#include "common.hpp"
#ifdef NS3_NLSR_SIM
#include "nlsr-logger.hpp"
#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/channel.h>
#include <ns3/node-list.h>
#include <ns3/point-to-point-net-device.h>
#include "model/ndn-l3-protocol.hpp"
#include "apps/ndn-nlsr-app.hpp"
#include "adjacent.hpp"
#else
#include "logger.hpp"
#endif

namespace nlsr {
namespace util {

INIT_LOGGER("FaceController");

using ndn::util::FaceUri;

const ndn::time::seconds FaceController::TIME_ALLOWED_FOR_CANONIZATION = ndn::time::seconds(4);

void
FaceController::createFace(const std::string& request,
                           const CommandSuccessCallback& onSuccess,
                           const CommandFailureCallback& onFailure)
{
  FaceUri uri(request);

  _LOG_TRACE("Converting " << uri << " to canonical form");
  uri.canonize(bind(&FaceController::onCanonizeSuccess, this, _1, onSuccess, onFailure, uri),
               bind(&FaceController::onCanonizeFailure, this, _1, onSuccess, onFailure, uri),
               m_ioService, TIME_ALLOWED_FOR_CANONIZATION);
}

void
FaceController::createFaceInNfd(const FaceUri& uri,
                                const CommandSuccessCallback& onSuccess,
                                const CommandFailureCallback& onFailure)
{
  ndn::nfd::ControlParameters faceParameters;
  faceParameters.setUri(uri.toString());

#ifdef NS3_NLSR_SIM
  uint32_t faceId = getFaceFromP2PLink(uri.toString());
  faceParameters.setFaceId(faceId);
  onSuccess(faceParameters);
  _LOG_DEBUG("Creating Face in NFD with face-uri: " << uri);
#else
  _LOG_DEBUG("Creating Face in NFD with face-uri: " << uri);
  m_controller.start<ndn::nfd::FaceCreateCommand>(faceParameters, onSuccess, onFailure);
#endif
}

void
FaceController::onCanonizeSuccess(const FaceUri& uri,
                                  const CommandSuccessCallback& onSuccess,
                                  const CommandFailureCallback& onFailure,
                                  const FaceUri& request)
{
  _LOG_DEBUG("Converted " << request << " to canonical form: " << uri);

  createFaceInNfd(uri, onSuccess, onFailure);
}

void
FaceController::onCanonizeFailure(const std::string& reason,
                                  const CommandSuccessCallback& onSuccess,
                                  const CommandFailureCallback& onFailure,
                                  const FaceUri& request)
{
  _LOG_WARN("Could not convert " << request << " to canonical form: " << reason);
  onFailure(CANONIZE_ERROR_CODE, "Could not canonize face-uri: " + request.toString());
}

#ifdef NS3_NLSR_SIM
uint32_t
FaceController::getFaceFromP2PLink(std::string faceUri)
{
  ns3::Ptr<ns3::Node> thisNode;
  ns3::Ptr<ns3::Node> adjNode;
  ns3::Ptr<ns3::ndn::L3Protocol> ndnStack;
  ns3::PointToPointNetDevice *netDevice;
  uint32_t faceId = 0;
  uint32_t adjNodeId = 0;
  std::string simName = "";

  thisNode = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  _LOG_DEBUG("@ THIS node is: " << thisNode->GetId());
  ns3::Ptr<ns3::ndn::NlsrApp> nlsrApp = thisNode->GetApplication(0)->GetObject<ns3::ndn::NlsrApp> ();
  NS_ASSERT (nlsrApp != 0);

  Adjacent *adj = nlsrApp->GetNlsr().getAdjacencyList().findAdjacent(faceUri);
  if (adj != 0) {
    simName = adj->getSimulatedName();
    _LOG_DEBUG("@ ADJC node is: " << simName);
    if (nlsrApp->GetNode(simName) != NULL) {
      adjNode = nlsrApp->GetNode(simName);
      adjNodeId = adjNode->GetId();
      _LOG_DEBUG("@ ADJC node is: " << adjNode->GetId());
    }
  }

  ndnStack = thisNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT(ndnStack != nullptr);

  for (uint32_t deviceId = 0; deviceId < thisNode->GetNDevices(); deviceId++) {
    netDevice = dynamic_cast<ns3::PointToPointNetDevice*>(&(*(thisNode->GetDevice(deviceId))));
    if (netDevice == NULL)
      continue;

    ns3::Ptr<ns3::Channel> channel = netDevice->GetChannel();
    if (channel == 0)
      continue;

    _LOG_DEBUG("@ channel node is: " << channel->GetDevice(0)->GetNode()->GetId());
    _LOG_DEBUG("@ channel node is: " << channel->GetDevice(1)->GetNode()->GetId());
    if (channel->GetDevice(0)->GetNode()->GetId() == adjNodeId ||
        channel->GetDevice(1)->GetNode()->GetId() == adjNodeId) {
      faceId = ndnStack->getFaceByNetDevice(netDevice)->getId();
      _LOG_DEBUG("@ ADJC node (" << adjNodeId << ")" << " face ID: (" << faceId << ")");
      return faceId;
    }
  }

  return faceId;
}
#endif

} // namespace util
} // namespace nlsr
