#ifndef DIJKSTRA_HPP
#define DIJKSTRA_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include "stdio.h"

using namespace std;

#define INF 999999

namespace nlsr{

    class MGraph{
public:
    MGraph(int n,int e):n(n),e(e){
        edges.push_back({0,2,INF,INF,INF,INF,INF});
        edges.push_back({2,0,2,2,INF,INF,INF});
        edges.push_back({INF,2,0,INF,INF,INF,2});
        edges.push_back({INF,2,INF,0,2,2,2});
        edges.push_back({INF,INF,INF,2,0,INF,INF});
        edges.push_back({INF,INF,INF,2,INF,0,2});
        edges.push_back({INF,INF,2,2,INF,2,0});
    }
    void Dispath(int dist[],int path[],int s[],int v);
    void Dijkstra(int v);
    void printResult();
    double calculateCentrality(int node);
private:
    int n,e;//n：节点总数，e：边总数
    vector<vector<int>> edges;
    vector<vector<int>> result;
};

}  //namespace nlsr


#endif  //DIJKSTRA_HPP