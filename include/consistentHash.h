#ifndef consistenthashh
#define consistenthashh
#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class consistentHash{
public:
    consistentHash(int replicanum, int minreplica, int maxreplica, double rebalancerthreashold);

    bool Add(const std::string& node);
    bool Remove(const std::string& node);
    std::string Get(const std::string& key);
private:

    mutable std::shared_mutex mtx;
    int replicaNum;
    int minReplica;
    int maxReplica;
    double rebalanceThreshold;
    std::atomic<long long> totalTraffic;
    int hashFunction(const std::string& key){ return static_cast<int>(std::hash<std::string>{}(key));}
    std::vector<int> hashRing;
    std::unordered_map<int,std::string> hashToNode;
    std::unordered_map<std::string,int> NodeToReplicaNum;
    std::unordered_map<std::string,std::atomic<long long>> NodeTrafficCount;
};

#endif 
