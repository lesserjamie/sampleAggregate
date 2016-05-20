# sampleAggregate
# (c) 2016 Jamie Lesser, Emily Hoyt


How to run a job on SampleAggregate:

1. make sa

2. To run master:
      sa -m -p [port number] -n [number of sample packets] 

3. To run a worker:
      sa -p [master port number] -a [master address]