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
    MGraph(int n):n(n){
        for(int i=0; i<n; i++){
            edges.push_back(vector<double>(n, INF));
        }
    }
    void Dispath(int dist[],int path[],int s[],int v);
    void Dijkstra(int v);
    void printResult();
    double calculateCentrality(int node);
    vector<vector<double>> edges;
private:
    int n;   //n：节点总数
    vector<vector<int>> result;
};

}  //namespace nlsr


#endif  //DIJKSTRA_HPP