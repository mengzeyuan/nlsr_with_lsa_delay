#ifndef DIJKSTRA_HPP
#define DIJKSTRA_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include "stdio.h"
#include "nlsr.hpp"
#include "lsdb.hpp"
#include <route/map.hpp>

using namespace std;

#define INF 999999.0

namespace nlsr{

    class MGraph{
public:
    MGraph(int n):n(n){
    }
    MGraph(){};
    void Dispath(double dist[],int path[],int s[],int v);
    void Dijkstra(int v);
    void printResult();
    void printAdjMatrix();
    double calculateCentrality(int node);
    void allocateAdjMatrix();
    void initMatrix();
    void makeAdjMatrix(Lsdb& lsdb, Map pMap);
    void freeAdjMatrix();
private:
    vector<vector<int>> result;
    //vector<vector<double>> edges;
    double** adjMatrix;
    int n;   //n：节点总数
};

}  //namespace nlsr


#endif  //DIJKSTRA_HPP