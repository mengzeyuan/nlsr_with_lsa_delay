#ifndef ENV_HPP
#define ENV_HPP

#include "conf-parameter.hpp"
#include "../../apps/ndn-consumer.hpp"
#include "agent.hpp"
#include "observation.hpp"

#include <ndn-cxx/util/scheduler.hpp>

namespace nlsr{

    class Env
    {
    public:
        Env(ConfParameter& ConfParameter, ndn::Scheduler& scheduler);
        void reset();
        void step(const int &action);
        void getRewardAndObservation();
        void updateObservation();
        void startRL(uint32_t seconds);
    private:
        uint32_t hello_interval;
        ConfParameter m_confParameter;
        Observation m_observation;
        ndn::Scheduler& m_scheduler;
        Agent m_agent;
    };
    
}

#endif