simul.x: top.cpp top.desc hard_config.h
	soclib-cc -P -p top.desc -I. -o simul.x

clean:
	soclib-cc -x -p top.desc -I.
	rm -rf *.o *.x term* tty* ext* temp nic_tx_file.txt

.PHONY: simul.x
