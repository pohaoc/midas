# Explanation

```
make clean
make -j
[Terminal #1]
sudo ~/midas/scripts/run_daemon.sh
[Terminal #2]
./test
```
This program iterates over the DS to count how many recomputations are triggered (and determine the % that was reclaimed)..
This [sets](https://github.com/pohaoc/midas/blob/fa4bc70f359048dd8b804106755d635f8f4f1207/apps/reclaim_example/test.cpp#L22) the limit of the memory pool
```
pool->update_limit(nbytes);
```
GC happens when the DS has used limit x [kAvailRatioThresh](https://github.com/pohaoc/midas/blob/77e2afb8e6e541812ce0ba7362f69710dfc0bac5/src/resource_manager.cpp#L101) bytes.


## Steps to Reproduce
Make sure to recompile Midas if we change the threshold.
   ```
   cd ~/midas; make clean; make -j
   cd ./apps/reclaim_example; make clean; make -j
   ```

1. We can sanity check that about 50% of DS are reclaimed if we change [kAvailRatioThresh](https://github.com/pohaoc/midas/blob/77e2afb8e6e541812ce0ba7362f69710dfc0bac5/src/resource_manager.cpp#L101C26-L101C50) to 0.5 and compile again.

2. We want to be able to programatically control when GC occurs. Suppose we fix kAvailRatioThresh to 0.01 and manually halve the pool limit to meet the GC condition. If we uncomment this [line](https://github.com/pohaoc/midas/blob/fa4bc70f359048dd8b804106755d635f8f4f1207/apps/reclaim_example/test.cpp#L33) and recompile, the program downsizes the memory pool after populating the DS - we expect GC to evict half of the data.
   
However, the system (a) **does not reclaim 50%** (b) **reports an error**.

