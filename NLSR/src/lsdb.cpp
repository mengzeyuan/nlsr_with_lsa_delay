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

#include "lsdb.hpp"

#include "nlsr.hpp"
#include "publisher/segment-publisher.hpp"
#include "utility/name-helper.hpp"

#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#ifdef NS3_NLSR_SIM
#include "nlsr-logger.hpp"
#else
#include "logger.hpp"
#endif

#define _LOG_DEBUG_YMZ(v) NS_LOG_UNCOND(m_instanceId << " " << v)  //ymz

namespace nlsr {

int Lsdb::m_instanceCounter = 0;  //ymz

INIT_LOGGER("Lsdb");

class LsaContentPublisher : public SegmentPublisher<ndn::Face>
{
public:
  LsaContentPublisher(ndn::Face& face,
                      ndn::KeyChain& keyChain,
                      const ndn::time::milliseconds& freshnessPeriod,
                      const std::string& content)
    : SegmentPublisher(face, keyChain, freshnessPeriod)
    , m_content(content)
  {
  }

  virtual size_t
  generate(ndn::EncodingBuffer& outBuffer) {
    size_t totalLength = 0;
    totalLength += outBuffer.prependByteArray(reinterpret_cast<const uint8_t*>(m_content.c_str()),
                                              m_content.size());
    return totalLength;
  }

private:
  const std::string m_content;
};

const ndn::Name::Component Lsdb::NAME_COMPONENT = ndn::Name::Component("lsdb");
const ndn::time::seconds Lsdb::GRACE_PERIOD = ndn::time::seconds(10);
const steady_clock::TimePoint Lsdb::DEFAULT_LSA_RETRIEVAL_DEADLINE = steady_clock::TimePoint::min();
size_t intTypeLoc = 5;

Lsdb::Lsdb(Nlsr& nlsr, ndn::Scheduler& scheduler, SyncLogicHandler& sync)
  : m_nlsr(nlsr)
  , m_scheduler(scheduler)
  , m_sync(sync)
  , m_lsaRefreshTime(0)
  , m_adjLsaBuildInterval(ADJ_LSA_BUILD_INTERVAL_DEFAULT)
#ifdef NS3_NLSR_SIM
  , m_tracer(ns3::ndn::NlsrTracer::Instance())
{
  //| Search and replace refactor?
  m_outNlsaInterest = 0;
  m_outLlsaInterest = 0;
  m_outClsaInterest = 0;

  m_inNlsaInterest = 0;
  m_inLlsaInterest = 0;
  m_inClsaInterest = 0;

  m_outNlsaData = 0;
  m_outLlsaData = 0;
  m_outClsaData = 0;

  m_inNlsaData = 0;
  m_inLlsaData = 0;
  m_inClsaData = 0;

  m_timedoutNlsaInterest = 0;
  m_timedoutLlsaInterest = 0;
  m_timedoutClsaInterest = 0;

  m_instanceId = string("Instance " + boost::lexical_cast<string>(m_instanceCounter++) + " ");  //ymz
}
#else
  {
  }
#endif

void
Lsdb::onFetchLsaError(uint32_t errorCode,
                      const std::string& msg,
                      ndn::Name& interestName,
                      uint32_t retransmitNo,
                      const ndn::time::steady_clock::TimePoint& deadline,
                      ndn::Name lsaName,
                      uint64_t seqNo)
{
  _LOG_DEBUG("Failed to fetch LSA: " << lsaName << ", Error code: " << errorCode
                                                << ", Message: " << msg);

  if (ndn::time::steady_clock::now() < deadline) {
    SequenceNumberMap::const_iterator it = m_highestSeqNo.find(lsaName);

    if (it != m_highestSeqNo.end() && it->second == seqNo) {
      // If the SegmentFetcher failed due to an Interest timeout, it is safe to re-express
      // immediately since at the least the LSA Interest lifetime has elapsed.
      // Otherwise, it is necessary to delay the Interest re-expression to prevent
      // the potential for constant Interest flooding.
      ndn::time::seconds delay = m_nlsr.getConfParameter().getLsaInterestLifetime();

      if (errorCode == ndn::util::SegmentFetcher::ErrorCode::INTEREST_TIMEOUT) {
        delay = ndn::time::seconds(0);
      }

      m_scheduler.scheduleEvent(delay, std::bind(&Lsdb::expressInterest, this,
                                                 interestName, retransmitNo + 1, deadline));
    }
  }
}

void
Lsdb::afterFetchLsa(const ndn::ConstBufferPtr& bufferPtr, ndn::Name& interestName)
{
  shared_ptr<ndn::Data> data = make_shared<ndn::Data>(ndn::Name(interestName));
  data->setContent(bufferPtr);

  _LOG_DEBUG("Received data for LSA(name): " << data->getName());

  ndn::Name lsaName = interestName.getSubName(0, interestName.size()-1);
  uint64_t seqNo = interestName[-1].toNumber();

  if (m_highestSeqNo.find(lsaName) == m_highestSeqNo.end()) {
    m_highestSeqNo[lsaName] = seqNo;
  }
  else if (seqNo > m_highestSeqNo[lsaName]) {
    m_highestSeqNo[lsaName] = seqNo;
  }
  else if (seqNo < m_highestSeqNo[lsaName]) {
    return;
  }

  onContentValidated(data);
}

void
Lsdb::cancelScheduleLsaExpiringEvent(ndn::EventId eid)
{
  m_scheduler.cancelEvent(eid);
}

static bool
nameLsaCompareByKey(const NameLsa& nlsa1, const ndn::Name& key)
{
  return nlsa1.getKey() == key;
}

bool
Lsdb::buildAndInstallOwnNameLsa()
{
  NameLsa nameLsa(m_nlsr.getConfParameter().getRouterPrefix(),
                  m_nlsr.getSequencingManager().getNameLsaSeq() + 1,
                  getLsaExpirationTimePoint(),
                  m_nlsr.getNamePrefixList());
  m_nlsr.getSequencingManager().increaseNameLsaSeq();
  return installNameLsa(nameLsa);
}

NameLsa*
Lsdb::findNameLsa(const ndn::Name& key)
{
  std::list<NameLsa>::iterator it = std::find_if(m_nameLsdb.begin(),
                                                 m_nameLsdb.end(),
                                                 bind(nameLsaCompareByKey, _1, key));
  if (it != m_nameLsdb.end()) {
    return &(*it);
  }
  return 0;
}

bool
Lsdb::isNameLsaNew(const ndn::Name& key, uint64_t seqNo)
{
  NameLsa* nameLsaCheck = findNameLsa(key);
  if (nameLsaCheck != 0) {
    if (nameLsaCheck->getLsSeqNo() < seqNo) {
      return true;
    }
    else {
      return false;
    }
  }
  return true;
}

ndn::EventId
Lsdb::scheduleNameLsaExpiration(const ndn::Name& key, int seqNo,
                                const ndn::time::seconds& expTime)
{
  return m_scheduler.scheduleEvent(expTime + GRACE_PERIOD,
                                   ndn::bind(&Lsdb::exprireOrRefreshNameLsa, this, key, seqNo));
}

bool
Lsdb::installNameLsa(NameLsa& nlsa)
{
  ndn::time::seconds timeToExpire = m_lsaRefreshTime;
  NameLsa* chkNameLsa = findNameLsa(nlsa.getKey());
  if (chkNameLsa == 0) {
    addNameLsa(nlsa);
    _LOG_DEBUG("New Name LSA");
    _LOG_DEBUG("Adding Name Lsa");
    nlsa.writeLog();

    if (nlsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
      m_nlsr.getNamePrefixTable().addEntry(nlsa.getOrigRouter(),
                                           nlsa.getOrigRouter());
      std::list<ndn::Name> nameList = nlsa.getNpl().getNameList();
      for (std::list<ndn::Name>::iterator it = nameList.begin(); it != nameList.end();
           it++) {
        if ((*it) != m_nlsr.getConfParameter().getRouterPrefix()) {
          m_nlsr.getNamePrefixTable().addEntry((*it), nlsa.getOrigRouter());
        }
      }
    }
    if (nlsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
      ndn::time::system_clock::Duration duration = nlsa.getExpirationTimePoint() -
                                                   ndn::time::system_clock::now();
      timeToExpire = ndn::time::duration_cast<ndn::time::seconds>(duration);
    }
    nlsa.setExpiringEventId(scheduleNameLsaExpiration(nlsa.getKey(),
                                                      nlsa.getLsSeqNo(),
                                                      timeToExpire));
  }
  else {
    if (chkNameLsa->getLsSeqNo() < nlsa.getLsSeqNo()) {
      _LOG_DEBUG("Updated Name LSA. Updating LSDB");
      _LOG_DEBUG("Deleting Name Lsa");
      chkNameLsa->writeLog();
      chkNameLsa->setLsSeqNo(nlsa.getLsSeqNo());
      chkNameLsa->setExpirationTimePoint(nlsa.getExpirationTimePoint());
      chkNameLsa->getNpl().sort();
      nlsa.getNpl().sort();
      std::list<ndn::Name> nameToAdd;
      std::set_difference(nlsa.getNpl().getNameList().begin(),
                          nlsa.getNpl().getNameList().end(),
                          chkNameLsa->getNpl().getNameList().begin(),
                          chkNameLsa->getNpl().getNameList().end(),
                          std::inserter(nameToAdd, nameToAdd.begin()));
      for (std::list<ndn::Name>::iterator it = nameToAdd.begin();
           it != nameToAdd.end(); ++it) {
        chkNameLsa->addName((*it));
        if (nlsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
          if ((*it) != m_nlsr.getConfParameter().getRouterPrefix()) {
            m_nlsr.getNamePrefixTable().addEntry((*it), nlsa.getOrigRouter());
          }
        }
      }

      chkNameLsa->getNpl().sort();

      std::list<ndn::Name> nameToRemove;
      std::set_difference(chkNameLsa->getNpl().getNameList().begin(),
                          chkNameLsa->getNpl().getNameList().end(),
                          nlsa.getNpl().getNameList().begin(),
                          nlsa.getNpl().getNameList().end(),
                          std::inserter(nameToRemove, nameToRemove.begin()));
      for (std::list<ndn::Name>::iterator it = nameToRemove.begin();
           it != nameToRemove.end(); ++it) {
        chkNameLsa->removeName((*it));
        if (nlsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
          if ((*it) != m_nlsr.getConfParameter().getRouterPrefix()) {
            m_nlsr.getNamePrefixTable().removeEntry((*it), nlsa.getOrigRouter());
          }
        }
      }
      if (nlsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
        ndn::time::system_clock::Duration duration = nlsa.getExpirationTimePoint() -
                                                     ndn::time::system_clock::now();
        timeToExpire = ndn::time::duration_cast<ndn::time::seconds>(duration);
      }
      cancelScheduleLsaExpiringEvent(chkNameLsa->getExpiringEventId());
      chkNameLsa->setExpiringEventId(scheduleNameLsaExpiration(nlsa.getKey(),
                                                               nlsa.getLsSeqNo(),
                                                               timeToExpire));
      _LOG_DEBUG("Adding Name Lsa");
      chkNameLsa->writeLog();
    }
  }
  return true;
}

bool
Lsdb::addNameLsa(NameLsa& nlsa)
{
  std::list<NameLsa>::iterator it = std::find_if(m_nameLsdb.begin(),
                                                 m_nameLsdb.end(),
                                                 bind(nameLsaCompareByKey, _1,
                                                      nlsa.getKey()));
  if (it == m_nameLsdb.end()) {
    m_nameLsdb.push_back(nlsa);
    return true;
  }
  return false;
}

bool
Lsdb::removeNameLsa(const ndn::Name& key)
{
  std::list<NameLsa>::iterator it = std::find_if(m_nameLsdb.begin(),
                                                 m_nameLsdb.end(),
                                                 ndn::bind(nameLsaCompareByKey, _1, key));
  if (it != m_nameLsdb.end()) {
    _LOG_DEBUG("Deleting Name Lsa");
    (*it).writeLog();
    if ((*it).getOrigRouter() !=
        m_nlsr.getConfParameter().getRouterPrefix()) {
      m_nlsr.getNamePrefixTable().removeEntry((*it).getOrigRouter(),
                                              (*it).getOrigRouter());
      for (std::list<ndn::Name>::iterator nit = (*it).getNpl().getNameList().begin();
           nit != (*it).getNpl().getNameList().end(); ++nit) {
        if ((*nit) != m_nlsr.getConfParameter().getRouterPrefix()) {
          m_nlsr.getNamePrefixTable().removeEntry((*nit), (*it).getOrigRouter());
        }
      }
    }
    m_nameLsdb.erase(it);
    return true;
  }
  return false;
}

bool
Lsdb::doesNameLsaExist(const ndn::Name& key)
{
  std::list<NameLsa>::iterator it = std::find_if(m_nameLsdb.begin(),
                                                 m_nameLsdb.end(),
                                                 ndn::bind(nameLsaCompareByKey, _1, key));
  if (it == m_nameLsdb.end()) {
    return false;
  }
  return true;
}

void
Lsdb::writeNameLsdbLog()
{
  _LOG_DEBUG("---------------Name LSDB-------------------");
  for (std::list<NameLsa>::iterator it = m_nameLsdb.begin();
       it != m_nameLsdb.end() ; it++) {
    (*it).writeLog();
  }
}

const std::list<NameLsa>&
Lsdb::getNameLsdb()
{
  return m_nameLsdb;
}

// Cor LSA and LSDB related Functions start here

static bool
corLsaCompareByKey(const CoordinateLsa& clsa, const ndn::Name& key)
{
  return clsa.getKey() == key;
}

bool
Lsdb::buildAndInstallOwnCoordinateLsa()
{
  CoordinateLsa corLsa(m_nlsr.getConfParameter().getRouterPrefix(),
                       m_nlsr.getSequencingManager().getCorLsaSeq() + 1,
                       getLsaExpirationTimePoint(),
                       m_nlsr.getConfParameter().getCorR(),
                       m_nlsr.getConfParameter().getCorTheta());

  // Sync coordinate LSAs if using HR or HR dry run.
  if (m_nlsr.getConfParameter().getHyperbolicState() != HYPERBOLIC_STATE_OFF) {
    m_nlsr.getSequencingManager().increaseCorLsaSeq();
    m_sync.publishRoutingUpdate();
  }

  installCoordinateLsa(corLsa);

  return true;
}

CoordinateLsa*
Lsdb::findCoordinateLsa(const ndn::Name& key)
{
  std::list<CoordinateLsa>::iterator it = std::find_if(m_corLsdb.begin(),
                                                       m_corLsdb.end(),
                                                       ndn::bind(corLsaCompareByKey, _1, key));
  if (it != m_corLsdb.end()) {
    return &(*it);
  }
  return 0;
}

bool
Lsdb::isCoordinateLsaNew(const ndn::Name& key, uint64_t seqNo)
{
  CoordinateLsa* clsa = findCoordinateLsa(key);
  if (clsa != 0) {
    if (clsa->getLsSeqNo() < seqNo) {
      return true;
    }
    else {
      return false;
    }
  }
  return true;
}

ndn::EventId
Lsdb::scheduleCoordinateLsaExpiration(const ndn::Name& key, int seqNo,
                                      const ndn::time::seconds& expTime)
{
  return m_scheduler.scheduleEvent(expTime + GRACE_PERIOD,
                                   ndn::bind(&Lsdb::exprireOrRefreshCoordinateLsa,
                                             this, key, seqNo));
}

bool
Lsdb::installCoordinateLsa(CoordinateLsa& clsa)
{
  ndn::time::seconds timeToExpire = m_lsaRefreshTime;
  CoordinateLsa* chkCorLsa = findCoordinateLsa(clsa.getKey());
  if (chkCorLsa == 0) {
    _LOG_DEBUG("New Coordinate LSA. Adding to LSDB");
    _LOG_DEBUG("Adding Coordinate Lsa");
    clsa.writeLog();
    addCoordinateLsa(clsa);

    // Register the LSA's origin router prefix
    if (clsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
      m_nlsr.getNamePrefixTable().addEntry(clsa.getOrigRouter(),
                                           clsa.getOrigRouter());
    }
    if (m_nlsr.getConfParameter().getHyperbolicState() != HYPERBOLIC_STATE_OFF) {
      m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
    }
    if (clsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
      ndn::time::system_clock::Duration duration = clsa.getExpirationTimePoint() -
                                                   ndn::time::system_clock::now();
      timeToExpire = ndn::time::duration_cast<ndn::time::seconds>(duration);
    }
    scheduleCoordinateLsaExpiration(clsa.getKey(),
                                    clsa.getLsSeqNo(), timeToExpire);
  }
  else {
    if (chkCorLsa->getLsSeqNo() < clsa.getLsSeqNo()) {
      _LOG_DEBUG("Updated Coordinate LSA. Updating LSDB");
      _LOG_DEBUG("Deleting Coordinate Lsa");
      chkCorLsa->writeLog();
      chkCorLsa->setLsSeqNo(clsa.getLsSeqNo());
      chkCorLsa->setExpirationTimePoint(clsa.getExpirationTimePoint());
      if (!chkCorLsa->isEqualContent(clsa)) {
        chkCorLsa->setCorRadius(clsa.getCorRadius());
        chkCorLsa->setCorTheta(clsa.getCorTheta());
        if (m_nlsr.getConfParameter().getHyperbolicState() >= HYPERBOLIC_STATE_ON) {
          m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
        }
      }
      if (clsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
        ndn::time::system_clock::Duration duration = clsa.getExpirationTimePoint() -
                                                     ndn::time::system_clock::now();
        timeToExpire = ndn::time::duration_cast<ndn::time::seconds>(duration);
      }
      cancelScheduleLsaExpiringEvent(chkCorLsa->getExpiringEventId());
      chkCorLsa->setExpiringEventId(scheduleCoordinateLsaExpiration(clsa.getKey(),
                                                                    clsa.getLsSeqNo(),
                                                                    timeToExpire));
      _LOG_DEBUG("Adding Coordinate Lsa");
      chkCorLsa->writeLog();
    }
  }
  return true;
}

bool
Lsdb::addCoordinateLsa(CoordinateLsa& clsa)
{
  std::list<CoordinateLsa>::iterator it = std::find_if(m_corLsdb.begin(),
                                                       m_corLsdb.end(),
                                                       ndn::bind(corLsaCompareByKey, _1,
                                                                 clsa.getKey()));
  if (it == m_corLsdb.end()) {
    m_corLsdb.push_back(clsa);
    return true;
  }
  return false;
}

bool
Lsdb::removeCoordinateLsa(const ndn::Name& key)
{
  std::list<CoordinateLsa>::iterator it = std::find_if(m_corLsdb.begin(),
                                                       m_corLsdb.end(),
                                                       ndn::bind(corLsaCompareByKey,
                                                                 _1, key));
  if (it != m_corLsdb.end()) {
    _LOG_DEBUG("Deleting Coordinate Lsa");
    it->writeLog();

    if (it->getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
      m_nlsr.getNamePrefixTable().removeEntry(it->getOrigRouter(), it->getOrigRouter());
    }

    m_corLsdb.erase(it);
    return true;
  }
  return false;
}

bool
Lsdb::doesCoordinateLsaExist(const ndn::Name& key)
{
  std::list<CoordinateLsa>::iterator it = std::find_if(m_corLsdb.begin(),
                                                       m_corLsdb.end(),
                                                       ndn::bind(corLsaCompareByKey,
                                                                 _1, key));
  if (it == m_corLsdb.end()) {
    return false;
  }
  return true;
}

void
Lsdb::writeCorLsdbLog()
{
  _LOG_DEBUG("---------------Cor LSDB-------------------");
  for (std::list<CoordinateLsa>::iterator it = m_corLsdb.begin();
       it != m_corLsdb.end() ; it++) {
    (*it).writeLog();
  }
}

const std::list<CoordinateLsa>&
Lsdb::getCoordinateLsdb()
{
  return m_corLsdb;
}

// Adj LSA and LSDB related function starts here

static bool
adjLsaCompareByKey(AdjLsa& alsa, const ndn::Name& key)
{
  return alsa.getKey() == key;
}

void
Lsdb::scheduleAdjLsaBuild()
{
  m_nlsr.incrementAdjBuildCount();

  if (m_nlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_ON) {
    // Don't build adjacency LSAs in hyperbolic routing
    return;
  }

  if (m_nlsr.getIsBuildAdjLsaSheduled() == false) {
    _LOG_DEBUG("Scheduling Adjacency LSA build in " << m_adjLsaBuildInterval);  //ymz

    m_scheduler.scheduleEvent(m_adjLsaBuildInterval, ndn::bind(&Lsdb::buildAdjLsa, this));
    m_nlsr.setIsBuildAdjLsaSheduled(true);
  }
}

void
Lsdb::buildAdjLsa()
{
  _LOG_TRACE("Lsdb::buildAdjLsa called");

  m_nlsr.setIsBuildAdjLsaSheduled(false);

  if (m_nlsr.getAdjacencyList().isAdjLsaBuildable(m_nlsr.getConfParameter().getInterestRetryNumber())) {

    int adjBuildCount = m_nlsr.getAdjBuildCount();

    // Is the adjacency LSA build still necessary, or has another build
    // fulfilled this request?
    if (adjBuildCount > 0) {
      if (m_nlsr.getAdjacencyList().getNumOfActiveNeighbor() > 0) {
        //_LOG_DEBUG_YMZ("Building and installing own Adj LSA");
        buildAndInstallOwnAdjLsa();
      }
      else {
        //_LOG_DEBUG_YMZ("Removing own Adj LSA; no ACTIVE neighbors");
        ndn::Name key = m_nlsr.getConfParameter().getRouterPrefix();
        key.append(AdjLsa::TYPE_STRING);

        removeAdjLsa(key);

        m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
      }

      // Since more adjacency LSA builds may have been scheduled while this build
      // was in progress, decrease the build count by the number of scheduled
      // builds at the beginning of this build.
      m_nlsr.setAdjBuildCount(m_nlsr.getAdjBuildCount() - adjBuildCount);
    }
  }
}


bool
Lsdb::addAdjLsa(AdjLsa& alsa)
{
  std::list<AdjLsa>::iterator it = std::find_if(m_adjLsdb.begin(),
                                                m_adjLsdb.end(),
                                                bind(adjLsaCompareByKey, _1,
                                                     alsa.getKey()));
  if (it == m_adjLsdb.end()) {
    m_adjLsdb.push_back(alsa);
    return true;
  }
  return false;
}

AdjLsa*
Lsdb::findAdjLsa(const ndn::Name& key)
{
  std::list<AdjLsa>::iterator it = std::find_if(m_adjLsdb.begin(),
                                                m_adjLsdb.end(),
                                                bind(adjLsaCompareByKey, _1, key));
  if (it != m_adjLsdb.end()) {
    return &(*it);
  }
  return 0;
}


bool
Lsdb::isAdjLsaNew(const ndn::Name& key, uint64_t seqNo)
{
  AdjLsa*  adjLsaCheck = findAdjLsa(key);
  if (adjLsaCheck != 0) {
    if (adjLsaCheck->getLsSeqNo() < seqNo) {
      return true;
    }
    else {
      return false;
    }
  }
  return true;
}


ndn::EventId
Lsdb::scheduleAdjLsaExpiration(const ndn::Name& key, int seqNo,
                               const ndn::time::seconds& expTime)
{
  return m_scheduler.scheduleEvent(expTime + GRACE_PERIOD,
                                   ndn::bind(&Lsdb::exprireOrRefreshAdjLsa, this, key, seqNo));
}

bool
Lsdb::installAdjLsa(AdjLsa& alsa)
{
  ndn::time::seconds timeToExpire = m_lsaRefreshTime;
  AdjLsa* chkAdjLsa = findAdjLsa(alsa.getKey());
  if (chkAdjLsa == 0) {  //新节点上线？
    //_LOG_DEBUG_YMZ("New Adj LSA. Adding to LSDB");
    _LOG_DEBUG("Adding Adj Lsa");
    alsa.writeLog();
    addAdjLsa(alsa);
    alsa.addNptEntries(m_nlsr);
    m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
    if (alsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
      ndn::time::system_clock::Duration duration = alsa.getExpirationTimePoint() -
                                                   ndn::time::system_clock::now();
      timeToExpire = ndn::time::duration_cast<ndn::time::seconds>(duration);
    }
    scheduleAdjLsaExpiration(alsa.getKey(),
                             alsa.getLsSeqNo(), timeToExpire);
  }
  else {  //节点INCTIVE？
    if (chkAdjLsa->getLsSeqNo() < alsa.getLsSeqNo()) {
      //_LOG_DEBUG_YMZ("Updated Adj LSA. Updating LSDB");
      _LOG_DEBUG("Deleting Adj Lsa");
      chkAdjLsa->writeLog();
      chkAdjLsa->setLsSeqNo(alsa.getLsSeqNo());
      chkAdjLsa->setExpirationTimePoint(alsa.getExpirationTimePoint());
      if (!chkAdjLsa->isEqualContent(alsa)) {
        chkAdjLsa->getAdl().reset();
        chkAdjLsa->getAdl().addAdjacents(alsa.getAdl());
        m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
      }
      if (alsa.getOrigRouter() != m_nlsr.getConfParameter().getRouterPrefix()) {
        ndn::time::system_clock::Duration duration = alsa.getExpirationTimePoint() -
                                                     ndn::time::system_clock::now();
        timeToExpire = ndn::time::duration_cast<ndn::time::seconds>(duration);
      }
      cancelScheduleLsaExpiringEvent(chkAdjLsa->getExpiringEventId());
      chkAdjLsa->setExpiringEventId(scheduleAdjLsaExpiration(alsa.getKey(),
                                                             alsa.getLsSeqNo(),
                                                             timeToExpire));
      _LOG_DEBUG("Adding Adj Lsa");
      chkAdjLsa->writeLog();
    }
  }
  return true;
}

bool
Lsdb::buildAndInstallOwnAdjLsa()
{
  AdjLsa adjLsa(m_nlsr.getConfParameter().getRouterPrefix(),
                m_nlsr.getSequencingManager().getAdjLsaSeq() + 1,
                getLsaExpirationTimePoint(),
                m_nlsr.getAdjacencyList().getNumOfActiveNeighbor(),
                m_nlsr.getAdjacencyList());

  //Sync adjacency LSAs if link-state or dry-run HR is enabled.
  if (m_nlsr.getConfParameter().getHyperbolicState() != HYPERBOLIC_STATE_ON) {
    m_nlsr.getSequencingManager().increaseAdjLsaSeq();
    m_sync.publishRoutingUpdate();
  }

  return installAdjLsa(adjLsa);
}

bool
Lsdb::removeAdjLsa(const ndn::Name& key)
{
  std::list<AdjLsa>::iterator it = std::find_if(m_adjLsdb.begin(),
                                                m_adjLsdb.end(),
                                                ndn::bind(adjLsaCompareByKey, _1, key));
  if (it != m_adjLsdb.end()) {
    _LOG_DEBUG("Deleting Adj Lsa");
    (*it).writeLog();
    (*it).removeNptEntries(m_nlsr);
    m_adjLsdb.erase(it);
    return true;
  }
  return false;
}

bool
Lsdb::doesAdjLsaExist(const ndn::Name& key)
{
  std::list<AdjLsa>::iterator it = std::find_if(m_adjLsdb.begin(),
                                                m_adjLsdb.end(),
                                                bind(adjLsaCompareByKey, _1, key));
  if (it == m_adjLsdb.end()) {
    return false;
  }
  return true;
}

const std::list<AdjLsa>&
Lsdb::getAdjLsdb()
{
  return m_adjLsdb;
}

void
Lsdb::setLsaRefreshTime(const seconds& lsaRefreshTime)
{
  m_lsaRefreshTime = lsaRefreshTime;
}

void
Lsdb::setThisRouterPrefix(std::string trp)
{
  m_thisRouterPrefix = trp;
}

void
Lsdb::exprireOrRefreshNameLsa(const ndn::Name& lsaKey, uint64_t seqNo)
{
  _LOG_DEBUG("Lsdb::exprireOrRefreshNameLsa Called");
  _LOG_DEBUG("LSA Key : " << lsaKey << " Seq No: " << seqNo);
  NameLsa* chkNameLsa = findNameLsa(lsaKey);
  if (chkNameLsa != 0) {
    _LOG_DEBUG("LSA Exists with seq no: " << chkNameLsa->getLsSeqNo());
    if (chkNameLsa->getLsSeqNo() == seqNo) {
      if (chkNameLsa->getOrigRouter() == m_thisRouterPrefix) {
        _LOG_DEBUG("Own Name LSA, so refreshing it");
        _LOG_DEBUG("Deleting Name Lsa");
        chkNameLsa->writeLog();
        chkNameLsa->setLsSeqNo(chkNameLsa->getLsSeqNo() + 1);
        m_nlsr.getSequencingManager().setNameLsaSeq(chkNameLsa->getLsSeqNo());
        chkNameLsa->setExpirationTimePoint(getLsaExpirationTimePoint());
        _LOG_DEBUG("Adding Name Lsa");
        chkNameLsa->writeLog();
        // schedule refreshing event again
        chkNameLsa->setExpiringEventId(scheduleNameLsaExpiration(chkNameLsa->getKey(),
                                                                 chkNameLsa->getLsSeqNo(),
                                                                 m_lsaRefreshTime));
        m_sync.publishRoutingUpdate();
      }
      else {
        _LOG_DEBUG("Other's Name LSA, so removing form LSDB");
        removeNameLsa(lsaKey);
      }
    }
  }
}

void
Lsdb::exprireOrRefreshAdjLsa(const ndn::Name& lsaKey, uint64_t seqNo)
{
  _LOG_DEBUG("Lsdb::exprireOrRefreshAdjLsa Called");
  _LOG_DEBUG("LSA Key : " << lsaKey << " Seq No: " << seqNo);
  AdjLsa* chkAdjLsa = findAdjLsa(lsaKey);
  if (chkAdjLsa != 0) {
    _LOG_DEBUG("LSA Exists with seq no: " << chkAdjLsa->getLsSeqNo());
    if (chkAdjLsa->getLsSeqNo() == seqNo) {
      if (chkAdjLsa->getOrigRouter() == m_thisRouterPrefix) {
        _LOG_DEBUG("Own Adj LSA, so refreshing it");
        _LOG_DEBUG("Deleting Adj Lsa");
        chkAdjLsa->writeLog();
        chkAdjLsa->setLsSeqNo(chkAdjLsa->getLsSeqNo() + 1);
        m_nlsr.getSequencingManager().setAdjLsaSeq(chkAdjLsa->getLsSeqNo());
        chkAdjLsa->setExpirationTimePoint(getLsaExpirationTimePoint());
        _LOG_DEBUG("Adding Adj Lsa");
        chkAdjLsa->writeLog();
        // schedule refreshing event again
        chkAdjLsa->setExpiringEventId(scheduleAdjLsaExpiration(chkAdjLsa->getKey(),
                                                               chkAdjLsa->getLsSeqNo(),
                                                               m_lsaRefreshTime));
        m_sync.publishRoutingUpdate();
      }
      else {
        _LOG_DEBUG("Other's Adj LSA, so removing form LSDB");
        removeAdjLsa(lsaKey);
      }
      // schedule Routing table calculaiton
      m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
    }
  }
}

void
Lsdb::exprireOrRefreshCoordinateLsa(const ndn::Name& lsaKey,
                                    uint64_t seqNo)
{
  _LOG_DEBUG("Lsdb::exprireOrRefreshCorLsa Called ");
  _LOG_DEBUG("LSA Key : " << lsaKey << " Seq No: " << seqNo);
  CoordinateLsa* chkCorLsa = findCoordinateLsa(lsaKey);
  if (chkCorLsa != 0) {
    _LOG_DEBUG("LSA Exists with seq no: " << chkCorLsa->getLsSeqNo());
    if (chkCorLsa->getLsSeqNo() == seqNo) {
      if (chkCorLsa->getOrigRouter() == m_thisRouterPrefix) {
        _LOG_DEBUG("Own Cor LSA, so refreshing it");
        _LOG_DEBUG("Deleting Coordinate Lsa");
        chkCorLsa->writeLog();
        chkCorLsa->setLsSeqNo(chkCorLsa->getLsSeqNo() + 1);
        if (m_nlsr.getConfParameter().getHyperbolicState() != HYPERBOLIC_STATE_OFF) {
          m_nlsr.getSequencingManager().setCorLsaSeq(chkCorLsa->getLsSeqNo());
        }

        chkCorLsa->setExpirationTimePoint(getLsaExpirationTimePoint());
        _LOG_DEBUG("Adding Coordinate Lsa");
        chkCorLsa->writeLog();
        // schedule refreshing event again
        chkCorLsa->setExpiringEventId(scheduleCoordinateLsaExpiration(
                                        chkCorLsa->getKey(),
                                        chkCorLsa->getLsSeqNo(),
                                        m_lsaRefreshTime));
        // Only sync coordinate LSAs if link-state routing is disabled
        if (m_nlsr.getConfParameter().getHyperbolicState() != HYPERBOLIC_STATE_OFF) {
          m_sync.publishRoutingUpdate();
        }
      }
      else {
        _LOG_DEBUG("Other's Cor LSA, so removing form LSDB");
        removeCoordinateLsa(lsaKey);
      }
      if (m_nlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_ON) {
        m_nlsr.getRoutingTable().scheduleRoutingTableCalculation(m_nlsr);
      }
    }
  }
}

void
Lsdb::expressInterest(const ndn::Name& interestName, uint32_t timeoutCount,
                      steady_clock::TimePoint deadline)
{
  if (deadline == DEFAULT_LSA_RETRIEVAL_DEADLINE) {
    deadline = steady_clock::now() + ndn::time::seconds(static_cast<int>(LSA_REFRESH_TIME_MAX));
  }

  ndn::Name lsaName = interestName.getSubName(0, interestName.size()-1);
  uint64_t seqNo = interestName[-1].toNumber();

  if (m_highestSeqNo.find(lsaName) == m_highestSeqNo.end()) {
    m_highestSeqNo[lsaName] = seqNo;
  }
  else if (seqNo > m_highestSeqNo[lsaName]) {
    m_highestSeqNo[lsaName] = seqNo;
  }
  else if (seqNo < m_highestSeqNo[lsaName]) {
    return;
  }

  ndn::Interest interest(interestName);
  interest.setInterestLifetime(m_nlsr.getConfParameter().getLsaInterestLifetime());

  _LOG_DEBUG("Fetching Data for LSA: " << interestName << " Seq number: " << seqNo);
  //在fetch函数中实现发送interest
  ndn::util::SegmentFetcher::fetch(m_nlsr.getNlsrFace(), interest,
                                   m_nlsr.getValidator(),
                                   ndn::bind(&Lsdb::afterFetchLsa, this, _1, interestName),
                                   ndn::bind(&Lsdb::onFetchLsaError, this, _1, _2, interestName,
                                             timeoutCount, deadline, lsaName, seqNo));

#ifdef NS3_NLSR_SIM
  if (m_tracer.IsEnabled() && interestName.size() > intTypeLoc && interestName.get(intTypeLoc).toUri().compare("name") == 0)
    m_tracer.NameLsaTrace(interestName.toUri(), "outNameLsaInterest", std::to_string(++m_outNlsaInterest), std::to_string(interest.wireEncode().size()));
  else if (m_tracer.IsEnabled() && interestName.size() > intTypeLoc && interestName.get(intTypeLoc).toUri().compare("adjacency") == 0)
    m_tracer.LinkLsaTrace(interestName.toUri(), "outAdjLsaInterest", std::to_string(++m_outLlsaInterest), std::to_string(interest.wireEncode().size()));
  else if (m_tracer.IsEnabled() && interestName.size() > intTypeLoc && interestName.get(intTypeLoc).toUri().compare("coordinate") == 0)
    m_tracer.NameLsaTrace(interestName.toUri(), "outCordLsaInterest", std::to_string(++m_outClsaInterest), std::to_string(interest.wireEncode().size()));
#endif
}

void
Lsdb::processInterest(const ndn::Name& name, const ndn::Interest& interest)
{
  const ndn::Name& interestName(interest.getName());
  _LOG_DEBUG("Interest received for LSA: " << interestName);

#ifdef NS3_NLSR_SIM
  if (m_tracer.IsEnabled() && interestName.size() > intTypeLoc && interestName.get(intTypeLoc).toUri().compare("name") == 0)
    m_tracer.NameLsaTrace(interestName.toUri(), "inNameLsaInterest", std::to_string(++m_inNlsaInterest), std::to_string(interest.wireEncode().size()));
  else if (m_tracer.IsEnabled() && interestName.size() > intTypeLoc && interestName.get(intTypeLoc).toUri().compare("adjacency") == 0)
    m_tracer.LinkLsaTrace(interestName.toUri(), "inAdjLsaInterest", std::to_string(++m_inLlsaInterest), std::to_string(interest.wireEncode().size()));
  else if (m_tracer.IsEnabled() && interestName.size() > intTypeLoc && interestName.get(intTypeLoc).toUri().compare("coordinate") == 0)
    m_tracer.NameLsaTrace(interestName.toUri(), "inCordLsaInterest", std::to_string(++m_inClsaInterest), std::to_string(interest.wireEncode().size()));
#endif

  std::string chkString("LSA");
  int32_t lsaPosition = util::getNameComponentPosition(interest.getName(), chkString);

  if (lsaPosition >= 0) {

    ndn::Name originRouter = m_nlsr.getConfParameter().getNetwork();
    originRouter.append(interestName.getSubName(lsaPosition + 1,
                                                interest.getName().size() - lsaPosition - 3));

    uint64_t seqNo = interestName[-1].toNumber();
    _LOG_DEBUG("LSA sequence number from interest: " << seqNo);

    std::string interestedLsType = interestName[-2].toUri();

    if (interestedLsType == NameLsa::TYPE_STRING) {
      processInterestForNameLsa(interest, originRouter.append(interestedLsType), seqNo);
    }
    else if (interestedLsType == AdjLsa::TYPE_STRING) {
      processInterestForAdjacencyLsa(interest, originRouter.append(interestedLsType), seqNo);
    }
    else if (interestedLsType == CoordinateLsa::TYPE_STRING) {
      processInterestForCoordinateLsa(interest, originRouter.append(interestedLsType), seqNo);
    }
    else {
      _LOG_WARN("Received unrecognized LSA type: " << interestedLsType);
    }
  }
}

void
Lsdb::putLsaData(const ndn::Interest& interest, const std::string& content)
{
  LsaContentPublisher publisher(m_nlsr.getNlsrFace(),
                                m_nlsr.getKeyChain(),
                                m_lsaRefreshTime,
                                content);
  publisher.publish(interest.getName(),
                    ndn::security::signingByCertificate(m_nlsr.getDefaultCertName()));

  //| Move into LsaContentPublisher
// #ifdef NS3_NLSR_SIM
//   if (m_tracer.IsEnabled() && data->getName().size() > intTypeLoc && data->getName().get(intTypeLoc).toUri().compare("name") == 0)
//     m_tracer.NameLsaTrace(data->getName().toUri(), "outNameLsaData", std::to_string(++m_outNlsaData), std::to_string(data->wireEncode().size()));
//   else if (m_tracer.IsEnabled() && data->getName().size() > intTypeLoc && data->getName().get(intTypeLoc).toUri().compare("adjacency") == 0)
//     m_tracer.NameLsaTrace(data->getName().toUri(), "outAdjLsaData", std::to_string(++m_outLlsaData), std::to_string(data->getName().wireEncode().size()));
//   else if (m_tracer.IsEnabled() && data->getName().size() > intTypeLoc && data->getName().get(intTypeLoc).toUri().compare("coordinate") == 0)
//     m_tracer.NameLsaTrace(data->getName().toUri(), "outCordLsaData", std::to_string(++m_outClsaData), std::to_string(data->wireEncode().size()));
// #endif
}

void
Lsdb::processInterestForNameLsa(const ndn::Interest& interest,
                                const ndn::Name& lsaKey,
                                uint64_t seqNo)
{
  NameLsa*  nameLsa = m_nlsr.getLsdb().findNameLsa(lsaKey);
  if (nameLsa != 0) {
    if (nameLsa->getLsSeqNo() == seqNo) {
      std::string content = nameLsa->getData();
      putLsaData(interest,content);
    }
  }
}

void
Lsdb::processInterestForAdjacencyLsa(const ndn::Interest& interest,
                                     const ndn::Name& lsaKey,
                                     uint64_t seqNo)
{
  if (m_nlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_ON) {
    _LOG_ERROR("Received interest for an adjacency LSA when hyperbolic routing is enabled.");
  }

  AdjLsa* adjLsa = m_nlsr.getLsdb().findAdjLsa(lsaKey);
  if (adjLsa != 0) {
    if (adjLsa->getLsSeqNo() == seqNo) {
      std::string content = adjLsa->getData();
      putLsaData(interest,content);
    }
  }
}

void
Lsdb::processInterestForCoordinateLsa(const ndn::Interest& interest,
                                      const ndn::Name& lsaKey,
                                      uint64_t seqNo)
{
  if (m_nlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_OFF) {
    _LOG_ERROR("Received Interest for a coordinate LSA when link-state routing is enabled.");
  }

  CoordinateLsa* corLsa = m_nlsr.getLsdb().findCoordinateLsa(lsaKey);
  if (corLsa != 0) {
    if (corLsa->getLsSeqNo() == seqNo) {
      std::string content = corLsa->getData();
      putLsaData(interest,content);
    }
  }
}

void
Lsdb::onContentValidated(const ndn::shared_ptr<const ndn::Data>& data)
{
  const ndn::Name& dataName = data->getName();
  _LOG_DEBUG("Data validation successful for LSA: " << dataName);

  std::string chkString("LSA");
  int32_t lsaPosition = util::getNameComponentPosition(dataName, chkString);

  if (lsaPosition >= 0) {

    ndn::Name originRouter = m_nlsr.getConfParameter().getNetwork();
    originRouter.append(dataName.getSubName(lsaPosition + 1, dataName.size() - lsaPosition - 3));

    uint64_t seqNo = dataName[-1].toNumber();
    std::string dataContent(reinterpret_cast<const char*>(data->getContent().value()));

    std::string interestedLsType  = dataName[-2].toUri();

    if (interestedLsType == NameLsa::TYPE_STRING) {
      //_LOG_DEBUG_YMZ("Received Name LSA" + dataName.toUri());
      #ifdef NS3_NLSR_SIM
      if (m_tracer.IsEnabled())
        m_tracer.NameLsaTrace(data->getName().toUri(), "inNameLsaData", std::to_string(++m_inNlsaData), std::to_string(data->getContent().value_size() + data->getName().toUri().size()));
      #endif
      processContentNameLsa(originRouter.append(interestedLsType), seqNo, dataContent);
    }
    else if (interestedLsType == AdjLsa::TYPE_STRING) {
      //_LOG_DEBUG_YMZ("Received Adj LSA");
      #ifdef NS3_NLSR_SIM
      if (m_tracer.IsEnabled())
        m_tracer.LinkLsaTrace(data->getName().toUri(), "inAdjLsaData", std::to_string(++m_inLlsaData), std::to_string(data->getContent().value_size() + data->getName().toUri().size()));
      #endif
      processContentAdjacencyLsa(originRouter.append(interestedLsType), seqNo, dataContent);
    }
    else if (interestedLsType == CoordinateLsa::TYPE_STRING) {
      //_LOG_DEBUG_YMZ("Received Coordinate LSA");
      #ifdef NS3_NLSR_SIM
      if (m_tracer.IsEnabled())
        m_tracer.NameLsaTrace(data->getName().toUri(), "inNameLsaData", std::to_string(++m_inClsaData), std::to_string(data->getContent().value_size() + data->getName().toUri().size()));
      #endif
      processContentCoordinateLsa(originRouter.append(interestedLsType), seqNo, dataContent);
    }
    else {
      _LOG_WARN("Received unrecognized LSA Type: " << interestedLsType);
    }
  }
}

void
Lsdb::processContentNameLsa(const ndn::Name& lsaKey,
                            uint64_t lsSeqNo, std::string& dataContent)
{
  if (isNameLsaNew(lsaKey, lsSeqNo)) {
    NameLsa nameLsa;
    if (nameLsa.initializeFromContent(dataContent)) {
      installNameLsa(nameLsa);
    }
    else {
      _LOG_DEBUG("LSA data decoding error :(");
    }
  }
}

void
Lsdb::processContentAdjacencyLsa(const ndn::Name& lsaKey,
                                 uint64_t lsSeqNo, std::string& dataContent)
{
  if (isAdjLsaNew(lsaKey, lsSeqNo)) {
    AdjLsa adjLsa;
    if (adjLsa.initializeFromContent(dataContent)) {
      installAdjLsa(adjLsa);
    }
    else {
      _LOG_DEBUG("LSA data decoding error :(");
    }
  }
}

void
Lsdb::processContentCoordinateLsa(const ndn::Name& lsaKey,
                                  uint64_t lsSeqNo, std::string& dataContent)
{
  if (isCoordinateLsaNew(lsaKey, lsSeqNo)) {
    CoordinateLsa corLsa;
    if (corLsa.initializeFromContent(dataContent)) {
      installCoordinateLsa(corLsa);
    }
    else {
      _LOG_DEBUG("LSA data decoding error :(");
    }
  }
}

ndn::time::system_clock::TimePoint
Lsdb::getLsaExpirationTimePoint()
{
  ndn::time::system_clock::TimePoint expirationTimePoint = ndn::time::system_clock::now();
  expirationTimePoint = expirationTimePoint +
                        ndn::time::seconds(m_nlsr.getConfParameter().getRouterDeadInterval());
  return expirationTimePoint;
}

void
Lsdb::writeAdjLsdbLog()
{
  _LOG_DEBUG("---------------Adj LSDB-------------------");
  for (std::list<AdjLsa>::iterator it = m_adjLsdb.begin();
       it != m_adjLsdb.end() ; it++) {
    (*it).writeLog();
  }
}

//-----utility function -----
bool
Lsdb::doesLsaExist(const ndn::Name& key, const std::string& lsType)
{
  if (lsType == NameLsa::TYPE_STRING) {
    return doesNameLsaExist(key);
  }
  else if (lsType == AdjLsa::TYPE_STRING) {
    return doesAdjLsaExist(key);
  }
  else if (lsType == CoordinateLsa::TYPE_STRING) {
    return doesCoordinateLsaExist(key);
  }
  return false;
}

} // namespace nlsr
