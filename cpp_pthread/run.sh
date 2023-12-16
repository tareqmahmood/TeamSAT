test_file=$1
n_threads=$2
g++ sat_solver.cpp -Wall -O3 -std=c++17 -o sat_solver.out -pthread && ./sat_solver.out "$test_file" $n_threads