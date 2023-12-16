test_file=$1
g++ sat_solver.cpp -Wall -O3 -std=c++17 -o sat_solver.out && ./sat_solver.out "$test_file"