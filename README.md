# VIPR certificate checking using Satistiability Modulo Theories

# Parallel Configuration:

To adjust the files to your environment, modify `local_runner.sh` to update your working directory and the path to the SMT checker (we tested with [`cvc5`](https://github.com/cvc5/cvc5)).
```
-CVC=<cvc_path>
-DIRNAME=<working_directory>
+CVC=/home/myusername/bin/cvc5
+DIRNAME=/home/myusername/vipr_checker
```
Then, modify ``remote_execution_manager.cpp`` changing `<working_directory> with your path:
```
- "<working_directory>/local_runner.sh",
+ "/home/myusername/local_runner.sh",
```

In addition, still in ``remote_execution_manager.cpp``, change ``localhost`` with a list of machines available in your cluster, and the number of hardware cores available in each of them
```
- add_machine(string("localhost"), 8);
- add_machine(string("mymachine1"), 64);
+ add_machine(string("mymachine2"), 64);
...
+ add_machine(string("mymachineN"), 64);
```

If you are not running under Linux, please remove the ``-DLINUX`` flag in the ``Makefile``.

# Building

First, make sure that your compiler fully supports C++20. Using ``clang++`` version 20+ or ``g++`` version 13+ is required. If you want to use ``g++``, change the first line of the ``Makefile`` appropriately. All of our tests were run using ``clang++''.

```
make clean;
make
```

# Running

Do **not** run the ``vipr_checker`` file directly; instead use one of our scripts such as ``run_one.sh``, which accepts a VIPR file and the block size used for the parallel DER generation.

```
./run_one.sh dano3_3.vipr 50
```

Replace dano_3_3.vipr with whichever vipr file you want to check.
Replace 50 with the block size of your choice. If you prefer the tool to decide the block size based on the hardware parallelism, make block size ``0''.

**Note that the program will work only if you can access the machines specified in ``remote_execution_manager.cpp`` with ssh without a password, because that’s how we dispatch local and remote executions.**
