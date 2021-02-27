#include "env.hpp"

namespace nlsr {
    Env::Env(ConfParameter& ConfParameter, ndn::Scheduler& scheduler)
    : m_confParameter(ConfParameter)
    , m_scheduler(scheduler)
    {

    }
    
    void Env::startRL(uint32_t seconds)
    {
        std::cout<< "进入Env::startRL" <<std::endl;
        /* m_scheduler.scheduleEvent(ndn::time::seconds(seconds),
                            ndn::bind(&Env::step, this, 1)); */
        ns3::Simulator::Schedule(ns3::Seconds(5.0), &Env::step, this, 1);
    }

    void Env::reset()
    {
        m_confParameter.setInfoInterestInterval(10);
        //m_observation = 0;
    }

    void Env::step(const int &action)
    {
        std::cout<< "进入Env::step" <<std::endl;
        uint32_t tmp = m_confParameter.getInfoInterestInterval();
        switch (action) {
            case 0: // decrease
                if(tmp > 5){
                    tmp--;
                    m_confParameter.setInfoInterestInterval(tmp);
                }
            break;

            case 1: // increase
                tmp += 5;
                m_confParameter.setInfoInterestInterval(tmp);
            break;

            default:
            break;
        }
        //参考 ns3::Simulator::Schedule(ns3::Seconds (1.0/centrality), &SyncLogicHandler::expressInterestForLsa, this, update, AdjLsa::TYPE_STRING, update.getAdjLsaSeqNo());
        //ns3::Simulator::Schedule(ns3::Seconds(10.0), &Env::getReward, this, reward);
        //ns3::Simulator::Schedule(ns3::Seconds(10.0), &Env::updateObservation, this);
        
        // 10秒之后，获取reward和observation
        m_scheduler.scheduleEvent(ndn::time::seconds(10),
                            ndn::bind(&Env::getRewardAndObservation, this));
        /* m_scheduler.scheduleEvent(ndn::time::seconds(10),
                            ndn::bind(&Env::updateObservation, this)); */

        // 可以这样继续下一步
        /* m_scheduler.scheduleEvent(ndn::time::seconds(12),
                            ndn::bind(&Env::startRL, this, 1)); */
    }

    void Env::getRewardAndObservation()
    {
        std::cout<< "进入Env::getRewardAndObservation" <<std::endl;

        double reward;
        if ((double)ns3::ndn::Consumer::numTimeOutInterests/ns3::ndn::Consumer::numOutInterests < m_observation.my_x){
            reward = 2;
        }
        else{
            reward = -2;
        }
        // std::cout<< "reward:" << reward <<std::endl;
        m_observation.ob_set((double)ns3::ndn::Consumer::numTimeOutInterests/ns3::ndn::Consumer::numOutInterests, 0, 0);
        
        vector<double> test_vec = {0, 0, 0};
        std::pair<Observation, vector<double>> mypair;
        mypair = make_pair(m_observation, test_vec);
        //m_agent.q_table.insert(mypair);   //这句还需要改，把q_table改成private

        m_scheduler.scheduleEvent(ndn::time::seconds(10),
                            ndn::bind(&Env::getRewardAndObservation, this));
    }

}