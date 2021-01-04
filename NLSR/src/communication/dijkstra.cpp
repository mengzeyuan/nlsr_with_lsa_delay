#include "dijkstra.hpp"
#include <cmath>
#include <route/map.hpp>

namespace nlsr
{
void
MGraph::allocateAdjMatrix()
{
  adjMatrix = new double*[n];

  for (size_t i = 0; i < n; ++i) {
    adjMatrix[i] = new double[n];
  }
}

void
MGraph::initMatrix()
{
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      adjMatrix[i][j] = INF;
    }
  }
}

void
MGraph::makeAdjMatrix(Lsdb& lsdb, Map pMap)
{
  std::list<AdjLsa> adjLsdb = lsdb.getAdjLsdb();
  for (std::list<AdjLsa>::iterator it = adjLsdb.begin(); it != adjLsdb.end() ; it++) {

    int32_t row = pMap.getMappingNoByRouterName((*it).getOrigRouter());

    std::list<Adjacent> adl = (*it).getAdl().getAdjList();
    for (std::list<Adjacent>::iterator itAdl = adl.begin(); itAdl != adl.end() ; itAdl++) {

      int32_t col = pMap.getMappingNoByRouterName((*itAdl).getName());
      double cost = (*itAdl).getLinkCost();

      if ((row >= 0 && row < static_cast<int32_t>(n)) &&
          (col >= 0 && col < static_cast<int32_t>(n)))
      {
        adjMatrix[row][col] = cost;
      }
    }
  }

  // Links that do not have the same cost for both directions should have their
  // costs corrected:
  //
  //   If the cost of one side of the link is 0, both sides of the link should have their cost
  //   corrected to 0.
  //
  //   Otherwise, both sides of the link should use the larger of the two costs.
  //
  // for (size_t row = 0; row < n; ++row) {
  //   for (size_t col = 0; col < n; ++col) {
  //     double toCost = adjMatrix[row][col];
  //     double fromCost = adjMatrix[col][row];

  //     if (fromCost != toCost) {
  //       double correctedCost = 0.0;

  //       if (toCost != 0 && fromCost != 0) {
  //         // If both sides of the link are up, use the larger cost
  //         correctedCost = std::max(toCost, fromCost);
  //       }

  //       // _LOG_WARN("Cost between [" << row << "][" << col << "] and [" << col << "][" << row <<
  //       //           "] are not the same (" << toCost << " != " << fromCost << "). " <<
  //       //           "Correcting to cost: " << correctedCost);

  //       adjMatrix[row][col] = correctedCost;
  //       adjMatrix[col][row] = correctedCost;
  //     }
  //   }
  // }
}

void
MGraph::freeAdjMatrix()
{
  for (size_t i = 0; i < n; ++i) {
    delete [] adjMatrix[i];
  }
  delete [] adjMatrix;
}

void 
MGraph::printResult()
{
    cout << "进入函数MGraph::printResult" << endl;
    for(auto it = result.begin(); it != result.end(); ++it){
        for(std::size_t j = 0; j < (*it).size(); ++j){
            cout<< (*it)[j] <<" ";
        }
        cout<<endl;
    }
    cout << "完成函数MGraph::printResult" << endl;
}

double MGraph::calculateCentrality(int node){
    double sum=0;
    for(std::size_t i = 0; i < result.size(); i++){
        vector<int>::iterator it = find(result[i].begin(), result[i].end(), node);
        if (it != result[i].end()){
            sum++;
        }
    }
    return sum/(n-1);
}

void 
MGraph::Dispath(double dist[],int path[],int s[],int v)
{
    int i,k;
    for(i=0;i<n;i++)
        if(s[i]==1 && i!=v)
    {
        //printf("从顶点%d到顶点%d的路径长度为：%d\t路径为：",v,i,dist[i]);
        vector<int> apath;//存放一条最短路径（逆向）及其顶点个数
        apath.push_back(i);//添加路径上的终点
        k=path[i];
        if(k==-1) {//没有路径的情况
            //printf("无路径\n");
        }
        else//存在路径时输出该路径
        {
            while(k!=v)
            {
                apath.push_back(k);
                k=path[k];
            }
            apath.push_back(v);//添加路径上的起点
            // printf(" %d",apath[d]);//先输出起点
            // for(j=d-1;j>=0;j--)//再输出其他顶点
            //     printf(",%d",apath[j]);
            // printf("\n");
            result.push_back(apath);
        }
    }
    cout << "进入函数MGraph::Dispath" << endl;
}

void 
MGraph::Dijkstra(int v)
{
    cout << "进入函数MGraph::Dijkstra" << endl;
    double dist[n];//dist[i]保存从源点到i的目前的最短路径长度
    int path[n];//path[i]保存当前最短路径中的前一个顶点的编号
    int s[n];//标记已找到最短路径的顶点，s[i]=0表示未找到,s[i]=1表示已找到。
    int i,j,u;
    double mindis;
    for(i=0;i<n;i++)
    {
        dist[i]=adjMatrix[v][i];//距离初始化
        s[i]=0;//s[ ]置空
        if(adjMatrix[v][i]<INF)//路径初始化
            path[i]=v;//顶点v到顶点i有边时，置顶点i的前一个顶点为v
        else
            path[i]=-1;//顶点v到顶点i没有边时，置顶点i的前一个顶点为-1
    }
    s[v]=1;//源点编号v放入s中
    path[v]=0;
    for(i=0;i<n;i++)//循环直到所有顶点的最短路径都求出
    {
        mindis=INF;//mindis置最小长度初值为无穷大
        u=-1;
        for(j=0;j<n;j++)
            if(s[j]==0 && dist[j]<mindis)//选取不在s[]中且具有最小距离的顶点u
        {
            u=j;
            mindis=dist[j];
        }
        s[u]=1;//顶点u加入s
        for(j=0;j<n;j++)//修改不在s中的顶点的距离
            if(s[j]==0)
            if(adjMatrix[u][j]<INF && dist[u]+adjMatrix[u][j]<dist[j])
        {
            dist[j]=dist[u]+adjMatrix[u][j];
            path[j]=u;
        }
    }
    Dispath(dist,path,s,v);
}

} // namespace nlsr


