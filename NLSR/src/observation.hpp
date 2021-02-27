#ifndef OBSERVATION_H
#define OBSERVATION_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>

using namespace std;

namespace nlsr{

class Observation {  //坐标（x，y）
public:
    Observation(double x, double y, double z):my_x(x),my_y(y),my_z(z){

    }
    Observation() {
        my_x = 0;
        my_y = 0;
        my_z = 0;
    }
    void ob_set(double x,double y,double z) {
        my_x=x;
        my_y=y;
        my_z=z;
    }
    double my_x, my_y, my_z;
};

struct HashFunc
{
	std::size_t operator()(const Observation &observation) const 
	{
		using std::size_t;
		using std::hash;

		return ((hash<int>()(observation.my_x)
			^ (hash<int>()(observation.my_y) << 1)) >> 1)
			^ (hash<int>()(observation.my_z) << 1);
	}
};

struct EqualKey
{
	bool operator () (const Observation &lhs, const Observation &rhs) const
	{
		return lhs.my_x  == rhs.my_x
			&& lhs.my_y == rhs.my_y
			&& lhs.my_z  == rhs.my_z;
	}
};

/* template<>
    struct hash<Observation>{//哈希的模板定制
    public:
        size_t operator()(const Observation &o) const
        {
            return hash<double>()(o.my_x) ^ hash<double>()(o.my_y);
        }

    };

template<>
    struct equal_to<Observation>{//等比的模板定制
    public:
        bool operator()(const Observation &o1, const Observation &o2) const
        {
            return o1.my_x == o2.my_x && o1.my_y == o2.my_y;
        }

    }; */


}

#endif