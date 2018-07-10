#!/bin/bash

EXEC=../build_release/test/test3d_1 

function generic {

nx_size=( 2 4 12 )
ny_size=( 2 4 12 16 )
nz_size=( 2 4 )
dir=1 # sigxx[0] sigyy[1] sigzz[2] sigxy[3] sigxz[4] sigyz[5] 

for nx in ${nx_size[@]}; do 
 for ny in ${ny_size[@]}; do 
  for nz in ${nz_size[@]}; do 
   echo $nx $ny $nz
   $EXEC $nx $ny $nz $dir 130 > out.dat
   awk '/eps/{printf("%lf ", $3)} /sig/{printf("%lf\n", $(3+dir))}' out.dat > s_vs_e_${nx}_${ny}_${nz}.dat
  done
 done
done

echo "set term png ; set output \"curves.png\";" > file
echo "plot \\" >> file

for nx in ${nx_size[@]}; do 
 for ny in ${ny_size[@]}; do 
  for nz in ${nz_size[@]}; do 
   echo "\"s_vs_e_${nx}_${ny}_${nz}.dat\" u 1:2 w lp title '$nx-$ny-$nz',\\" >> file
  done
 done
done

#gnuplot file

}

function uniform {

nn_size=( 2 4 6 8 )
dir=0 # sigxx[0] sigyy[1] sigzz[2] sigxy[3] sigxz[4] sigyz[5] 

for nn in ${nn_size[@]}; do 
   echo $nn
   $EXEC $nn $nn $nn $dir 100 > /dev/null
   mv micropp_eps_sig_ctan.dat micropp_epssig_${nn}.dat
done

echo "set term png ; set output \"curves.png\";" > gnuplot.in
echo "plot \\" >> gnuplot.in

for nn in ${nn_size[@]}; do 
   echo "\"micropp_epssig_${nn}.dat\" u $((dir+1)):$((dir+7)) w lp title '${nn}',\\" >> gnuplot.in
done

gnuplot gnuplot.in

}

#generic
uniform
