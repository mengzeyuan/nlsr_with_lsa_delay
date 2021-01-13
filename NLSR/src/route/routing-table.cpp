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
#include <string>
#include <list>

#include "routing-table.hpp"
#include "nlsr.hpp"
#include "map.hpp"
#include "conf-parameter.hpp"
#include "routing-table-calculator.hpp"
#include "routing-table-entry.hpp"
#include "name-prefix-table.hpp"
#include <boost/lexical_cast.hpp>  //ymz
#ifdef NS3_NLSR_SIM
#include "nlsr-logger.hpp"
#else
#include "logger.hpp"
#endif

#define _LOG_DEBUG_YMZ(v) NS_LOG_UNCOND(m_instanceId << " " << v)  //ymz

namespace nlsr {

int RoutingTable::m_instanceCounter = 0;  //ymz

INIT_LOGGER("RoutingTable");

using namespace std;

void
RoutingTable::calculate(Nlsr& pnlsr)
{
  pnlsr.getLsdb().writeCorLsdbLog();
  pnlsr.getLsdb().writeNameLsdbLog();
  pnlsr.getLsdb().writeAdjLsdbLog();
  pnlsr.getNamePrefixTable().writeLog();
  if (pnlsr.getIsRoutingTableCalculating() == false) {
    //setting routing table calculation
    pnlsr.setIsRoutingTableCalculating(true);

    bool isHrEnabled = pnlsr.getConfParameter().getHyperbolicState() != HYPERBOLIC_STATE_OFF;

    if ((!isHrEnabled
         &&
         pnlsr.getLsdb()
         .doesLsaExist(pnlsr.getConfParameter().getRouterPrefix().toUri()
                       + "/" + "adjacency", std::string("adjacency")))
        ||
        (isHrEnabled
         &&
         pnlsr.getLsdb()
         .doesLsaExist(pnlsr.getConfParameter().getRouterPrefix().toUri()
                       + "/" + "coordinate", std::string("coordinate")))) {
      if (pnlsr.getIsBuildAdjLsaSheduled() != 1) {
        _LOG_TRACE("Clearing old routing table");
        clearRoutingTable();
        // for dry run options
        clearDryRoutingTable();

        _LOG_DEBUG("Calculating routing table");

        // calculate Link State routing
        if ((pnlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_OFF)
            || (pnlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_DRY_RUN)) {
          calculateLsRoutingTable(pnlsr);
        }
        //calculate hyperbolic routing
        if (pnlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_ON) {
          calculateHypRoutingTable(pnlsr);
        }
        //calculate dry hyperbolic routing
        if (pnlsr.getConfParameter().getHyperbolicState() == HYPERBOLIC_STATE_DRY_RUN) {
          calculateHypDryRoutingTable(pnlsr);
        }
        //need to update NPT here
        _LOG_DEBUG("Calling Update NPT With new Route");
        pnlsr.getNamePrefixTable().updateWithNewRoute();
        writeLog(pnlsr.getConfParameter().getHyperbolicState());
        pnlsr.getNamePrefixTable().writeLog();
        pnlsr.getFib().writeLog();
      }
      else {
        _LOG_DEBUG("Adjacency building is scheduled, so"
                   " routing table can not be calculated :(");
      }
    }
    else {
      _LOG_DEBUG("No Adj LSA of router itself,"
                 " so Routing table can not be calculated :(");
      clearRoutingTable();
      clearDryRoutingTable(); // for dry run options
      // need to update NPT here
      _LOG_DEBUG("Calling Update NPT With new Route");
      pnlsr.getNamePrefixTable().updateWithNewRoute();
      writeLog(pnlsr.getConfParameter().getHyperbolicState());
      pnlsr.getNamePrefixTable().writeLog();
      pnlsr.getFib().writeLog();
      //debugging purpose end
    }
    pnlsr.setIsRouteCalculationScheduled(false); //clear scheduled flag
    pnlsr.setIsRoutingTableCalculating(false); //unsetting routing table calculation
  }
  else {
    scheduleRoutingTableCalculation(pnlsr);
  }
}


void
RoutingTable::calculateLsRoutingTable(Nlsr& nlsr)
{
  _LOG_DEBUG_YMZ("RoutingTable::calculateLsRoutingTable Called");

  Map map;
  map.createFromAdjLsdb(nlsr);
  map.writeLog();

  size_t nRouters = map.getMapSize();

  LinkStateRoutingTableCalculator calculator(nRouters);

  calculator.calculatePath(map, ndn::ref(*this), nlsr);
}

void
RoutingTable::calculateHypRoutingTable(Nlsr& nlsr)
{
  Map map;
  map.createFromCoordinateLsdb(nlsr);
  map.writeLog();

  size_t nRouters = map.getMapSize();

  HyperbolicRoutingCalculator calculator(nRouters, false,
                                         nlsr.getConfParameter().getRouterPrefix());

  calculator.calculatePaths(map, ndn::ref(*this),
                            nlsr.getLsdb(), nlsr.getAdjacencyList());
}

void
RoutingTable::calculateHypDryRoutingTable(Nlsr& nlsr)
{
  Map map;
  map.createFromAdjLsdb(nlsr);
  map.writeLog();

  size_t nRouters = map.getMapSize();

  HyperbolicRoutingCalculator calculator(nRouters, true,
                                         nlsr.getConfParameter().getRouterPrefix());

  calculator.calculatePaths(map, ndn::ref(*this),
                            nlsr.getLsdb(), nlsr.getAdjacencyList());
}

void
RoutingTable::scheduleRoutingTableCalculation(Nlsr& pnlsr)
{
  if (pnlsr.getIsRouteCalculationScheduled() != true) {
    //_LOG_DEBUG_YMZ("Scheduling routing table calculation in " << m_routingCalcInterval);

    m_scheduler.scheduleEvent(m_routingCalcInterval,
                              ndn::bind(&RoutingTable::calculate, this, ndn::ref(pnlsr)));

    pnlsr.setIsRouteCalculationScheduled(true);
  }
}

static bool
routingTableEntryCompare(RoutingTableEntry& rte, ndn::Name& destRouter)
{
  return rte.getDestination() == destRouter;
}

// function related to manipulation of routing table
void
RoutingTable:: addNextHop(const ndn::Name& destRouter, NextHop& nh)
{
  _LOG_DEBUG_YMZ("Adding " << nh << " for destination: " << destRouter);

  RoutingTableEntry* rteChk = findRoutingTableEntry(destRouter);
  if (rteChk == 0) {     //没找到对应的destRouter,即路由表中没有对应条目指向destRouter
    RoutingTableEntry rte(destRouter);  //那么新建一个指向该destRouter的RoutingTableEntry
    rte.getNexthopList().addNextHop(nh);//在该新建entry下插入下一跳faceuri+routecost
    m_rTable.push_back(rte);            //将RoutingTableEntry插入RoutingTable
  }
  else {
    rteChk->getNexthopList().addNextHop(nh);
  }
}

RoutingTableEntry*
RoutingTable::findRoutingTableEntry(const ndn::Name& destRouter)
{
  std::list<RoutingTableEntry>::iterator it = std::find_if(m_rTable.begin(),
                                                           m_rTable.end(),
                                                           ndn::bind(&routingTableEntryCompare,
                                                                     _1, destRouter));
  if (it != m_rTable.end()) {
    return &(*it);
  }
  return 0;
}

void
RoutingTable::writeLog(int hyperbolicState)
{
  _LOG_DEBUG("---------------Routing Table------------------");
  for (std::list<RoutingTableEntry>::iterator it = m_rTable.begin() ;
       it != m_rTable.end(); ++it) {
    _LOG_DEBUG("Destination: " << (*it).getDestination());
    _LOG_DEBUG("Nexthops: ");
    (*it).getNexthopList().writeLog();
  }

  if (hyperbolicState == HYPERBOLIC_STATE_DRY_RUN) {
    _LOG_DEBUG("--------Hyperbolic Routing Table(Dry)---------");
    for (std::list<RoutingTableEntry>::iterator it = m_dryTable.begin() ;
        it != m_dryTable.end(); ++it) {
      _LOG_DEBUG("Destination: " << (*it).getDestination());
      _LOG_DEBUG("Nexthops: ");
      (*it).getNexthopList().writeLog();
    }
  }
}

//function related to manipulation of dry routing table
void
RoutingTable::addNextHopToDryTable(const ndn::Name& destRouter, NextHop& nh)
{
  _LOG_DEBUG("Adding " << nh << " to dry table for destination: " << destRouter);

  std::list<RoutingTableEntry>::iterator it = std::find_if(m_dryTable.begin(),
                                                           m_dryTable.end(),
                                                           ndn::bind(&routingTableEntryCompare,
                                                                     _1, destRouter));
  if (it == m_dryTable.end()) {
    RoutingTableEntry rte(destRouter);
    rte.getNexthopList().addNextHop(nh);
    m_dryTable.push_back(rte);
  }
  else {
    (*it).getNexthopList().addNextHop(nh);
  }
}

void
RoutingTable::clearRoutingTable()
{
  if (m_rTable.size() > 0) {
    m_rTable.clear();
  }
}

void
RoutingTable::clearDryRoutingTable()
{
  if (m_dryTable.size() > 0) {
    m_dryTable.clear();
  }
}

}//namespace nlsr

