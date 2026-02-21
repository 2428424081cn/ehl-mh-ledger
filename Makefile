CC = clang
CFLAGS = -O3 -march=native -pthread
B3_DIR = ./BLAKE3/c
LDFLAGS = -L$(B3_DIR) -lblake3

all: mhf_engine ledger_node verify_tool

mhf_engine:
	$(CC) $(CFLAGS) mh_parallel.c -o mh_parallel

ledger_node:
	$(CC) $(CFLAGS) ledger_core.c -o ehl_ledger_bin

verify_tool:
	$(CC) $(CFLAGS) verify_ledger.c -o verify_ledger

clean:
	rm -f ehl_ledger_bin mh_parallel verify_ledger ledger.db

