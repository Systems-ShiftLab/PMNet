all: clean pmSwitch_request pmSwitch_ack pmSwitch_ack_gen pmSwitch_cache_request pmSwitch_cache_response_gen export_ip

pmSwitch_request: pmSwitch_request.p4
	p4c-sdnet $@.p4 -o $@.sdnet --toplevel_name $@
	sdnet $@.sdnet -busType axi -workDir $@ -lineClock 300 -busWidth 64

pmSwitch_ack: pmSwitch_ack.p4
	p4c-sdnet $@.p4 -o $@.sdnet --toplevel_name $@
	sdnet $@.sdnet -busType axi -workDir $@ -lineClock 300 -busWidth 64

pmSwitch_ack_gen: pmSwitch_ack_gen.p4
	p4c-sdnet $@.p4 -o $@.sdnet --toplevel_name $@
	sdnet $@.sdnet -busType axi -workDir $@ -lineClock 300 -busWidth 64

pmSwitch_cache_request: pmSwitch_cache_request.p4
	p4c-sdnet $@.p4 -o $@.sdnet --toplevel_name $@
	sdnet $@.sdnet -busType axi -workDir $@ -lineClock 300 -busWidth 64

pmSwitch_cache_response_gen: pmSwitch_cache_response_gen.p4
	p4c-sdnet $@.p4 -o $@.sdnet --toplevel_name $@
	sdnet $@.sdnet -busType axi -workDir $@ -lineClock 300 -busWidth 64


export_ip:
	./export_ip.sh

clean:
	-rm -r *.sdnet pmSwitch_request pmSwitch_ack pmSwitch_ack_gen pmSwitch_cache_gen pmSwitch_cache_request pmSwitch_cache_response_gen


