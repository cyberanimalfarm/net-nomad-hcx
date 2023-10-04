UNAME_M := $(shell uname -m)
	ifeq ($(UNAME_M),x86_64)
		ARCH = x86_64
	endif
	ifeq ($(UNAME_M),aarch64)
		LONG_BIT := $(shell getconf LONG_BIT)
		ifeq ($(LONG_BIT),64)
			ARCH = aarch64
		endif
		ifeq ($(LONG_BIT),32)
			ARCH = armhf
		endif
	endif

all: net-nomad-hcx clean

net-nomad-hcx: net-nomad-hcx.o libhcxdumptool.a libhcxpcapngtool.a
	g++ -o net-nomad-hcx_$(ARCH) net-nomad-hcx.cpp -Llib -L/usr/local/ssl/lib -I/usr/local/ssl/include -Iinclude -Ihcxpcapngtool/include/pcapngtool/ -lhcxdumptool -lhcxpcapngtool -lssl -lz -lbz2 -llzma -larchive -lcrypto -lz -lpcap -lcjson

net-nomad-hcx.o: net-nomad-hcx.cpp
	g++ -O -c net-nomad-hcx.cpp

hcxdumptool.o: hcxdumptool.c include/hcxdumptool.h include/cJSON.h 
	gcc -c -fPIC hcxdumptool.c -lcjson -Llib -Iinclude -I/usr/local/include -I/usr/local/include/cjson

libhcxdumptool.a: hcxdumptool.o
	ar cr lib/libhcxdumptool.a hcxdumptool.o

hcxpcapngtool.o: hcxpcapngtool/hcxpcapngtool.c
	gcc -c -fPIC hcxpcapngtool/hcxpcapngtool.c -Llib -L/usr/local/lib -L /usr/local/ssl/lib -I/usr/include -Ihcxpcapngtool/include/pcapngtool/ -Ihcxpcapngtool/include -I/usr/local/ssl/include -I/usr/local/include -I/usr/local/include/cjson -lssl -lcrypto -lcjson -lz -lbz2 -llzma -larchive

libhcxpcapngtool.a: hcxpcapngtool.o
	ar cr lib/libhcxpcapngtool.a hcxpcapngtool.o

libs: libhcxdumptool.a libhcxpcapngtool.a

cleanall:
	rm -f net-nomad-hcx *.o lib/*.a *.gch

clean:
	rm -f *.o *.gch