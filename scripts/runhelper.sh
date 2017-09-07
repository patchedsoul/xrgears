
run_log_error () {
  ./bin/$1 $2 $3 2> $1.log
  cat $1.log
}

run_backtrace () {
  gdb -batch -ex "run" -ex "bt" ./bin/$1
}
