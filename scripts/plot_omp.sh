#!/bin/bash

. vars.sh


procs=(1 2 4 8 16 32 48)

for N in ${sizes[@]}; do

	folder="res_${N}_${NGP}"
	rm -rf times_n${N}_ngp${NGP}.dat
	t1=$(awk '/time =/{print $3}' ${folder}/res_1.dat)

	for i in ${procs[@]}; do

		awk -v t1=$t1 -v p=$i '/time =/{print p "\t" $3 "\t" (t1/$3)}' ${folder}/res_${i}.dat >> times_n${N}_ngp${NGP}.dat

	done
done
