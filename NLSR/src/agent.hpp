#ifndef AGENT_HPP
#define AGENT_HPP

#include <iostream>
#include "observation.hpp"
#include <unordered_map>
#include <vector>

using namespace std;

namespace nlsr{

class Agent
{
public:
    Agent();
    Agent(vector<int> actions, double learning_rate=0.01, double reward_decay=0.9, double e_greedy=0.9):
    lr(learning_rate), gamma(reward_decay), epsilon(e_greedy), nActions(actions.size())
    {
        vector<double> tmp(nActions);
        q_table_row = tmp;
    }
    void check_state_exist(const Observation& observation);

private:
    vector<int> myaction;
    double lr;
    double gamma;
    double epsilon;
    int nActions;
    vector<double> q_table_row;
    unordered_map<Observation, vector<double>, HashFunc, EqualKey> q_table;
};

}



#endif