# NAME: Kevin Tang
# EMAIL: kevintang2023@ucla.edu
# ID: 805419480

default: 
	gcc -g -Wall -Wextra lab4c_tcp.c -o lab4c_tcp -lmraa -lm

buildtest:
	gcc -g -Wall -Wextra lab4c_tcp.c -o lab4c_tcp -D PC -lm

dist: lab4c.tgz
submission_files = Makefile README lab4b.c test.sh

lab4c.tgz: $(submission_files)
	tar -czf lab4c-805419480.tar.gz $(submission_files)

.PHONY: default dist clean

clean:
	rm -f *.tar.gz lab4c_tcp lab4b_tls *.test
