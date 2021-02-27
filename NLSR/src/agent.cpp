#include "agent.hpp"

namespace nlsr{

    Agent::Agent(){

    }

    void Agent::check_state_exist(const Observation& observation){
        if(q_table.find(observation) == q_table.end()) {
            // append new state to q table
            std::pair<Observation, vector<double>> mypair;
            mypair = make_pair(observation, q_table_row);
            q_table.insert(mypair);
        }
    }
}