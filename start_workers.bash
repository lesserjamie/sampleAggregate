#!/bin/bash
# start_workers.sh
cat workers.txt | while read addr
do
    screen -dm bash -c "ssh 17jrl4@tswana.cs.williams.edu 'cd cs339/sampleAggregate; sa -p 8080 -a lulu.cs.williams.edu; exit'"
done