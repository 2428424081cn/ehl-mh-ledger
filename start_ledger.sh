#!/bin/bash
echo "Starting EHL-MH Ledger Node..."
./ehl_ledger_bin &
watch -n 10 "./verify_ledger | tail -n 5"

