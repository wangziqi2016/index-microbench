# index-microbench

## Generate Workloads ##

1. Download [YCSB](https://github.com/brianfrankcooper/YCSB/releases/latest)

   ```sh
   curl -O --location https://github.com/brianfrankcooper/YCSB/releases/download/0.11.0/ycsb-0.11.0.tar.gz
   tar xfvz ycsb-0.11.0.tar.gz
   mv ycsb-0.11.0 YCSB
   ```

2. Create Workload Spec

   The default workload a-f are in ./workload_spec

   You can of course generate your own spec and put it in this folder.

3. Modify workload_config.inp

   1st arg: workload spec file name
   2nd arg: key type (randint = random integer; monoint = monotonically increasing integer; email = email keys with host name reversed)

4. Generate

   ```sh
   make generate_workload
   ```

   The generated workload files will be in ./workloads

5. NOTE: To generate email-key workloads, you need an email list (list.txt)# index-microbench
