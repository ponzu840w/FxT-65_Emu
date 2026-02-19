#!/bin/bash

# sampleコマンドを使った関数コール解析

./fxt65 2>/dev/null &
A=$!
sleep 2
sample $A 5 -f /tmp/fxt65_sample.txt
echo "done."
kill $A

cat /tmp/fxt65_sample.txt

