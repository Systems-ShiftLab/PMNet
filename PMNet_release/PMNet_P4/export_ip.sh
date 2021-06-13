#!/bin/bash
modules=(pmSwitch_request pmSwitch_ack pmSwitch_ack_gen pmSwitch_cache_request pmSwitch_cache_response_gen)

for i in ${modules[@]}; do
cd $i/$i
vivado -mode batch -source "$i"_vivado_packager.tcl
cd ../../
done

#cd pmSwitch_switch/XilinxSwitch/
#vivado -mode batch -source XilinxSwitch_vivado_packager.tcl
#cd ../../
