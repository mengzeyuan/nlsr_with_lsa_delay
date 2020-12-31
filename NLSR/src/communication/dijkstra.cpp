#include "dijkstra.hpp"

namespace nlsr
{
void 
MGraph::printResult()
{
    for(int i=0;i<result.size();++i){
        for(int j=0;j<result[i].size();++j){
            cout<<result[i][j]<<" ";
        }
        cout<<endl;
    }
}

double MGraph::calculateCentrality(int node){
    double sum=0;
    for(int i=0;i<result.size();i++){
        vector<int>::iterator it = find(result[i].begin(), result[i].end(), node);
        if (it != result[i].end()){
            sum++;
        }
    }
    return sum/(n-1);
}

void 
MGraph::Dispath(int dist[],int path[],int s[],int v)
{
    int i,j,k;
    for(i=0;i<n;i++)
        if(s[i]==1&&i!=v)
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
}

void 
MGraph::Dijkstra(int v)
{
    int dist[n];//dist[i]保存从源点到i的目前的最短路径长度
    int path[n];//path[i]保存当前最短路径中的前一个顶点的编号
    int s[n];//标记已找到最短路径的顶点，s[i]=0表示未找到,s[i]=1表示已找到。
    int mindis,i,j,u;
    for(i=0;i<n;i++)
    {
        dist[i]=edges[v][i];//距离初始化
        s[i]=0;//s[ ]置空
        if(edges[v][i]<INF)//路径初始化
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
            if(edges[u][j]<INF && dist[u]+edges[u][j]<dist[j])
        {
            dist[j]=dist[u]+edges[u][j];
            path[j]=u;
        }
    }
    Dispath(dist,path,s,v);
}

} // namespace nlsr


