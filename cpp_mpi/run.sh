test_file=$1
mpicxx sat_solver.cpp -Wall -O3 -o sat_solver.out && mpirun -np 4 ./sat_solver.out "$test_file"