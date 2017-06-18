
all:
	make -C bin
	make -C usr.bin

install:
	make -C bin install
	make -C usr.bin install

clean:
	-make -C bin clean
	-make -C usr.bin clean
