#include <stdio.h>
#include "../include/Lru.h"
// add more test cases here
int main() {
    Lru<int, int> lru(3);
    lru.put(1, 100);
    lru.put(2, 200);
    lru.put(3, 300);
    printf("%d\n", lru.get(1)); // returns 1
    lru.put(4, 400); // evicts key 2
    printf("%d\n", lru.get(2)); // returns -1 (not found)
    lru.put(5, 500); // evicts key 3
    printf("%d\n", lru.get(3)); // returns -1 (not found)
    return 0;
}