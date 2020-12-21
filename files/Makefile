# NAME: Kevin Tang
# EMAIL: kevintang2023@ucla.edu
# ID: 805419480

default: 
	gcc -g -Wall -Wextra lab4c_tcp.c -o lab4c_tcp -lmraa -lm
	gcc -g -Wall -Wextra lab4c_tls.c -o lab4c_tls -lmraa -lm -lssl -lcrypto

buildtest:
	gcc -g -Wall -Wextra lab4c_tcp.c -o lab4c_tcp -D PC -lm
	gcc -g -Wall -Wextra lab4c_tls.c -o lab4c_tls -D PC -lm -lssl -lcrypto

dist: lab4c.tgz
submission_files = Makefile README lab4c_tcp.c lab4c_tls.c

lab4c.tgz: $(submission_files)
	tar -czf lab4c-805419480.tar.gz $(submission_files)

.PHONY: default dist clean

clean:
	rm -f *.tar.gz lab4c_tcp lab4b_tls *.test
