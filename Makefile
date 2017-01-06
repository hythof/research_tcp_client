BASE_CC=gcc -std=c11 -Wall -W -D_GNU_SOURCE -D_POSIX_C_SOURCE=201112L -I src/
PRODUCT_CC=${BASE_CC} -O2
PROFILE_CC=${BASE_CC} -O2
DEVELOP_CC=${BASE_CC} -Og -g -fno-inline -pthread

compile:
	@mkdir -p bin
	@mkdir -p bin
	@${PRODUCT_CC} benchmark/main.c src/*.c -o bin/product
	@${DEVELOP_CC} benchmark/main.c src/*.c -o bin/develop
	@${PROFILE_CC} benchmark/main.c src/*.c -o bin/profile

perf: compile
	@perf record bin/profile
	@perf report
	@rm -f perf.data

profile: compile
	@CPUPROFILE=/tmp/cpu.prof LD_PRELOAD=/usr/lib/libprofiler.so.0 ./bin/profile
	@google-pprof --text bin/profile /tmp/cpu.prof
