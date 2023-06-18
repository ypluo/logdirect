# logdirect
Source code for paper "LogDirect: A Novel PMR-Based Data Persisting Path for Key-Value Stores with Low Latency and Strong Durability"

The code repository contains three main folders:
- `logdirect/`, it contains code for the evaluation of different remote data persistent paths.
- `logdirectaccess/`, it contains code for the evaluation of different accessing stacks for KVStores.
- `redis/`, it contains code for Redis codebase and evaluation scripts of Redis-AOF.

To run the code in logdirect and logdirectaccess, you need to add two patches to linux kernel 5.14, and boot with the new kernel on the server node. The patches are listed in `logdirect/patches`. We have removed all sensitive infos in scripts such as password.
