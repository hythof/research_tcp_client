BASE_CC=gcc -std=c11 -Wall -W -D_GNU_SOURCE -D_POSIX_C_SOURCE=201112L -I src/
PRODUCT_CC=${BASE_CC} -O2
PROFILE_CC=${BASE_CC} -Og

compile:
	@mkdir -p example/bin
	@${PRODUCT_CC} example/main.c src/*.c -o example/bin/product
	@${PROFILE_CC} example/main.c src/*.c -o example/bin/profile

perf: compile
	@perf record -o /tmp/perf.data -F 99 example/bin/profile
	@perf report -i /tmp/perf.data | head -n 30
	@perf stat example/bin/profile

profile: compile
	@CPUPROFILE=/tmp/cpu.prof LD_PRELOAD=/usr/lib/libprofiler.so.0 example/bin/profile
	@google-pprof --text example/bin/profile /tmp/cpu.prof | head -n 30
