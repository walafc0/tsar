simul.x: top.cpp top.desc
	soclib-cc  -P -p top.desc -I. -o simul.x

clean:
	soclib-cc -x -p top.desc -I.
	rm -rf *.o *.x tty* term*

.PHONY:simul.x

