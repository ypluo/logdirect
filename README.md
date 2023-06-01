# logdirect
Source code for "LogDirect: A Novel PMR-Based Data Persisting Path for Key-Value Stores with Low Latency and Strong Durability"

The code repository contains two main folders: `logdirect/` and `logdirectaccess/`. `logdirect/` contains code for the evaluation of different remote data persistent paths, and `logdirectaccess/` contains code for the evaluation of different accessing stacks for KVStores.

To run the code in logdirect and logdirectaccess, you need to add two patches to linux kernel 5.14, and boot with the new kernel on the server node. The patches are listed in `logdirect/patches`.
