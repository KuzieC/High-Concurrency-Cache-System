#include "include/consistentHash.h"

#include <mutex>
#include <iostream>
#include <algorithm>
consistentHash::consistentHash(int replicanum, int minreplica, int maxreplica, double rebalancerthreashold):
        replicaNum(replicanum), 
        minReplica(minreplica), 
        maxReplica(maxreplica), 
        rebalanceThreshold(rebalancerthreashold){}
    
bool consistentHash::Add(const std::string& node){
    std::unique_lock<std::shared_mutex> lock(mtx);
    for(int i = 0; i < replicaNum; i++){
        auto hashkey = node + "-" + std::to_string(i);
        int hash = hashFunction(hashkey);
        if(hashToNode.find(hash) != hashToNode.end()){
            return false; 
        }
        hashToNode[hash] = node;
        hashRing.push_back(hash);
    }
    NodeToReplicaNum[node] = replicaNum;
    std::sort(hashRing.begin(), hashRing.end());
    return true;
}

bool consistentHash::Remove(const std::string& node){
    if(NodeToReplicaNum.find(node) == NodeToReplicaNum.end()){
        return false; 
    }
    std::unique_lock<std::shared_mutex> lock(mtx);
    replicaNum = NodeToReplicaNum[node];
    for(int i = 0; i < replicaNum; i++){
        auto hashkey = node + "-" + std::to_string(i);
        int hash = hashFunction(hashkey);
        hashToNode.erase(hash);
        auto it = std::remove(hashRing.begin(), hashRing.end(), hash);
        hashRing.erase(it, hashRing.end());
    }
    NodeToReplicaNum.erase(node);
    return true;
}

std::string consistentHash::Get(const std::string& key){
    std::shared_lock<std::shared_mutex> lock(mtx);
    if(hashRing.empty()){
        std::cerr << "Hash ring is empty, no nodes available." << std::endl;
        return ""; 
    }
    int hash = hashFunction(key);
    auto it = std::lower_bound(hashRing.begin(), hashRing.end(), hash);
    if(it == hashRing.end()){
        it = hashRing.begin(); 
    }
    return hashToNode[*it];
}
