# Explanation

```
make clean
make -j 
```
This program sets up an array with kNumValues entries, each of kVSize bytes.
The following line sets the limit of the memory pool
https://github.com/pohaoc/midas/blob/77e2afb8e6e541812ce0ba7362f69710dfc0bac5/apps/reclaim_example/test.cpp#L22
GC happens when the [threshold](https://github.com/pohaoc/midas/blob/77e2afb8e6e541812ce0ba7362f69710dfc0bac5/src/resource_manager.cpp#L101) is reached.
We iterate over the DS to count how many recomputations are triggered (to determine the % that was reclaimed).

## Steps to Reproduce
1. We can sanity check that about 50% of DS are reclaimed if we change the threshold and compile again.
   change [kAvailRatioThresh](https://github.com/pohaoc/midas/blob/77e2afb8e6e541812ce0ba7362f69710dfc0bac5/src/resource_manager.cpp#L101C26-L101C50) to 0.5
   ```
   cd ~/midas; make clean; make -j
   cd ./apps/reclaim_example; make clean; make -j
   ```
   
