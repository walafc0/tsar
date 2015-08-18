simul.x: top.cpp top.desc hard_config.h
	soclib-cc -P -p top.desc -I. -o simul.x

.PHONY: tags cscope.out
cscope.out tags: top.desc
	soclib-cc -p $< --tags --tags-type=cscope --tags-output=cscope.out

clean:
	soclib-cc -x -p top.desc -I.
	rm -rf *.o *.x term* tty* ext* temp

.PHONY: simul.x
