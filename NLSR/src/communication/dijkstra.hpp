#ifndef DIJKSTRA_HPP
#define DIJKSTRA_HPP

#include <iostream>
#include <vector>
#include "stdio.h"

using namespace std;

#define INF 999999

namespace nlsr{

    class MGraph{
public:
    MGraph(int n,int e):n(n),e(e){
        edges.push_back({0,5,INF,7,INF,INF});
        edges.push_back({INF,0,4,INF,INF,INF});
        edges.push_back({8,INF,0,INF,INF,9});
        edges.push_back({INF,INF,5,0,INF,6});
        edges.push_back({INF,INF,INF,5,0,INF});
        edges.push_back({3,INF,INF,INF,1,0});
    }
    void Dispath(int dist[],int path[],int s[],int v);
    void Dijkstra(int v);
    void printResult();
private:
    int n,e;//n：节点总数，e：边总数
    vector<vector<int>> edges;
    vector<vector<int>> result;
};

}  //namespace nlsr


#endif  //DIJKSTRA_HPP