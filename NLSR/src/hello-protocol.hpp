/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016,  The University of Memphis,
 *                           Regents of the University of California,
 *                           Arizona Board of Regents.
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
 **/

#ifndef NLSR_HELLO_PROTOCOL_HPP
#define NLSR_HELLO_PROTOCOL_HPP

#include "test-access-control.hpp"
#include "../../apps/ndn-consumer.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/management/nfd-control-parameters.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/lexical_cast.hpp>

#ifdef NS3_NLSR_SIM
#include "utils/tracers/ndn-nlsr-tracer.hpp"
#endif

using namespace std;  //ymz

namespace nlsr {

class Nlsr;

class HelloProtocol
{
public:
  HelloProtocol(Nlsr& nlsr, ndn::Scheduler& scheduler)
    : m_nlsr(nlsr)
    , m_scheduler(scheduler)
#ifdef NS3_NLSR_SIM
    , m_tracer(ns3::ndn::NlsrTracer::Instance())
#endif
  {
    m_outInterest = 0;
    m_inData = 0;
    m_timedOutInterest = 0;
    m_inInterest = 0;
    m_outData = 0;

    m_instanceId = string("Instance " + boost::lexical_cast<string>(m_instanceCounter++) + " ");  //ymz
  }

  void
  scheduleInterest(uint32_t seconds);

  void 
  scheduleInterest_ymz(uint32_t seconds);

  void
  expressInterest(const ndn::Name& interestNamePrefix, uint32_t seconds);

  void
  sendScheduledInterest(uint32_t seconds);

  void
  processInterest(const ndn::Name& name, const ndn::Interest& interest);

  void
  registerAdjacentPrefixes();

private:
  void
  processInterestTimedOut(const ndn::Interest& interest);

  void
  onContent(const ndn::Interest& interest, const ndn::Data& data);

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  void
  onContentValidated(const ndn::shared_ptr<const ndn::Data>& data);

private:
  void
  onContentValidationFailed(const ndn::shared_ptr<const ndn::Data>& data,
                            const std::string& msg);

  void
  onRegistrationFailure(uint32_t code, const std::string& error,
                        const ndn::Name& name);

  void
  onRegistrationSuccess1(const ndn::nfd::ControlParameters& commandSuccessResult,
                        const ndn::Name& neighbor, const ndn::time::milliseconds& timeout);

  void
  onRegistrationSuccess(const ndn::nfd::ControlParameters& commandSuccessResult,
                        const ndn::Name& neighbor, const ndn::time::milliseconds& timeout);

  void
  registerPrefixes(const ndn::Name& adjName, const std::string& faceUri,
                   double linkCost, const ndn::time::milliseconds& timeout);
  void
  registerPrefixes1(const ndn::Name& adjName, const std::string& faceUri,
                   double linkCost, const ndn::time::milliseconds& timeout);
private:
private:
  Nlsr& m_nlsr;
  ndn::Scheduler& m_scheduler;

  static const std::string INFO_COMPONENT;
  static const std::string NLSR_COMPONENT;

  std::string m_instanceId;
  static int m_instanceCounter;  //ymz


#ifdef NS3_NLSR_SIM
  ns3::ndn::NlsrTracer &m_tracer;
  long m_outInterest;
  long m_inData;
  long m_timedOutInterest;
  long m_inInterest;
  long m_outData;
#endif
};

} //namespace nlsr

#endif // NLSR_HELLO_PROTOCOL_HPP
