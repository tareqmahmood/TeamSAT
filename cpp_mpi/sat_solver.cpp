#include <iostream>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <set>
#include <unordered_map>
#include <utility>
#include <random>
#include <chrono>
#include <ratio>
#include <cmath>
#include <mpi.h>
#include <thread>

using namespace std;

// // Weight to be used in branching strategies
// int weight = 2;
// // Precomputing weights for quicker access
// int pow_weight[500];
// // Timeout a testcase
// int test_case_timeout = 20;

// mpi configs
int my_rank, mpi_nodes;
bool probed = false;
int probe_flag = 0;
MPI_Status mpi_status;
// mpi tags
int STOP_TAG = 50;
int LEARNING_TAG = 60;

// buffers
const int MPI_BUFFER_SIZE = 100000;
int MPI_BUFFER_ZS = MPI_BUFFER_SIZE;
int mpi_buffer[MPI_BUFFER_SIZE];
int new_clause_size = 0;
int* new_clause_buffer;

// Additional configs
int implications = 0;
int branches = 0;
int num_of_variables = 0;

// book keeping
vector<vector<int>> raw_clauses;
set<set<int>> clauses;
set<set<int>> clauses_with_learned;

int current_level = 0;
set<int> branch_variable;
unordered_map<int, int> branch_record;
set<set<int>> learnings;
int *assignment;

// graph

class Node
{
public:
    int variable;
    int value;
    int level;
    vector<Node*> parents;
    vector<Node*> children;
    set<int> clause;

    Node(){};
    Node(int _variable)
    {
        variable = _variable;
        value = -1;
        level = -1;
    }
};
unordered_map<int, Node*> graph;

template <class Type>
class OrderedSet
{
private:
    set<Type> seen;
    vector<Type> order;

public:
    OrderedSet() {}

    // Get the size of the set
    size_t size() const
    {
        return order.size();
    }

    Type &operator[](int index)
    {
        return order[index];
    }

    void insert(const Type &item)
    {
        if (seen.find(item) == seen.end())
        {
            seen.insert(item);
            order.push_back(item);
        }
    }

    void clear()
    {
        seen.clear();
        order.clear();
    }

    bool empty() const
    {
        return order.empty();
    }

    typename vector<Type>::const_iterator begin() const
    {
        return order.begin();
    }

    typename vector<Type>::const_iterator end() const
    {
        return order.end();
    }
};

unordered_map<int, OrderedSet<int>> propagation_history;

vector<string> split_string(string input_string, char delimiter)
{
    stringstream ss(input_string);
    vector<string> tokens;
    string token;
    while (getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }

    return tokens;
}

void read_file(string &input_file)
{
    num_of_variables = 0;
    raw_clauses.clear();

    ifstream input_file_steam(input_file);

    if (!input_file_steam.is_open())
    {
        cerr << "Error opening file: " << input_file << endl;
        return; // Return an error code
    }

    // Read the file line by line
    string line;
    while (getline(input_file_steam, line))
    {
        if (line[0] == 'c' || line[0] == 'p' || line[0] == '%' || line[0] == '0')
        {
            if (line[0] == 'p')
            {
                num_of_variables = stoi(split_string(line, ' ')[2]);
            }
        }
        else
        {
            vector<int> clause;
            vector<string> tokens = split_string(line, ' ');
            for (const string &t : tokens)
            {
                int literal = stoi(t);
                if (abs(literal) > 0 && abs(literal) <= num_of_variables)
                {
                    clause.push_back(literal);
                }
            }
            if (clause.size() > 0)
            {
                raw_clauses.push_back(clause);
            }
        }
        continue;
    }

    // Close the file
    input_file_steam.close();
}

set<int> regularize_clause(vector<int> &clause)
{
    set<int> r_clause;
    set<int> unique_clause;

    for (int literal : clause)
    {
        r_clause.insert(literal);
        unique_clause.insert(literal);
    }

    for (int literal : clause)
    {
        if (unique_clause.find(-literal) != unique_clause.end())
        {
            return set<int>();
        }
    }

    return r_clause;
}

void compress_CNF()
{
    clauses.clear();
    for (vector<int> &clause : raw_clauses)
    {
        set<int> r_clause = regularize_clause(clause);
        if (r_clause.size() == 0)
            continue;
        clauses.insert(r_clause);
    }
    // raw_clauses.clear();
}

void initialize_solver()
{
    clauses_with_learned.clear();
    clauses_with_learned.insert(clauses.begin(), clauses.end());

    current_level = 0;
    branch_variable.clear();
    learnings.clear();

    branch_record.clear();
    propagation_history.clear();

    graph.clear();
    for (int v = 1; v <= num_of_variables; v++)
    {
        graph[v] = new Node(v);
    }

    implications = 0;
    branches = 0;
}

void wrapup()
{
    MPI_Buffer_detach(&mpi_buffer, &MPI_BUFFER_ZS);
    delete[] assignment;
}

bool assignment_over()
{
    for (int i = 1; i <= num_of_variables; i++)
    {
        if (assignment[i] == -1)
            return false;
    }
    return true;
}

int literal_value(int literal)
{
    return abs(literal);
}

int literal_sign(int literal)
{
    return literal > 0;
}

int evaluate_literal(int literal)
{
    if (assignment[literal_value(literal)] == -1)
    {
        return -1;
    }
    else if (assignment[literal_value(literal)] == literal_sign(literal))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int evaluate_clause(const set<int> &clause)
{
    int undecided = 0;
    for (int literal : clause)
    {
        if (evaluate_literal(literal) == 1)
        {
            return 1;
        }
        else if (evaluate_literal(literal) == -1)
        {
            undecided += 1;
        }
    }

    if (undecided > 0)
    {
        return -undecided;
    }
    else
    {
        return 0;
    }
}

int get_first_literal(const set<int> &clause)
{
    for (int literal : clause)
    {
        if (evaluate_literal(literal) == -1)
        {
            return literal;
        }
    }
    return 0;
}

void update_graph(int variable)
{
    graph[variable]->level = current_level;
    graph[variable]->value = assignment[variable];
}

void update_graph_with_clause(int variable, const set<int> &clause)
{
    graph[variable]->level = current_level;
    graph[variable]->value = assignment[variable];

    set<int>::iterator it = clause.begin();
    while(it != clause.end())
    {
        int l_literal = *it;
        int literal = literal_value(l_literal);

        if (variable != literal)
        {
            graph[literal]->children.push_back(graph[variable]);
            graph[variable]->parents.push_back(graph[literal]);
        }
        ++it;
    }
    graph[variable]->clause = clause;
}

void print_assignment()
{
    for (int v = 1; v <= num_of_variables; v++)
    {
        printf("%d ", assignment[v]);
    }
    printf("\n");
}

void print_all_clauses()
{
    for (const vector<int> &clause : raw_clauses)
    {
        for (const int literal : clause)
        {
            printf("%d ", literal);
        }
        printf("\n");
    }
}

bool verify_assignment()
{
    for (const vector<int> &clause : raw_clauses)
    {
        bool clause_true = false;
        if (clause.size() == 0)
            continue;
        for (const int literal : clause)
        {
            if (assignment[literal_value(literal)] == literal_sign(literal))
            {
                clause_true = true;
            }
        }
        if (!clause_true)
            return false;
    }
    return true;
}

set<int> unit_propagation()
{
    while (1)
    {
        OrderedSet<pair<int, set<int>>> propagation;
        for (const set<int> &clause : clauses_with_learned)
        {
            int result = evaluate_clause(clause);
            if (result == 1)
            {
                continue;
            }
            else if (result == 0)
            {
                return clause;
            }
            else if (result == -1)
            {
                propagation.insert(make_pair(get_first_literal(clause), clause));
            }
        }

        if (propagation.empty())
        {
            // FIXMEMAYBE:
            return set<int>();
        }

        for (const pair<int, set<int>> &propagation_entry : propagation)
        {
            int literal = propagation_entry.first;
            set<int> clause = propagation_entry.second;

            implications++;
            assignment[literal_value(literal)] = literal_sign(literal);

            update_graph_with_clause(literal_value(literal), clause);

            if (current_level != 0)
            {
                propagation_history[current_level].insert(literal);
            }
        }
    }
}

// int random_sign()
// {
//     int randomValue = rand() % 2;
//     return (randomValue == 0) ? -1 : 1;
// }

class SimpleRandomGenerator
{
private:
    unsigned int seed;

public:
    SimpleRandomGenerator(unsigned int initialSeed = 2) : seed(initialSeed) {}

    // Generate a pseudo-random number between 0 and RAND_MAX
    int getRandomNumber()
    {
        seed = (1103515245 * seed + 12345) % (1 << 31);
        return static_cast<int>(seed);
    }

    void setSeed(int newSeed)
    {
        seed = newSeed;
    }
};

SimpleRandomGenerator randomGenerator;

int select_variable()
{
    // RANDOM
    vector<int> unassigned;
    for (int v = 1; v <= num_of_variables; v++)
    {
        if (assignment[v] == -1)
        {
            unassigned.push_back(v);
        }
    }
    // int random_number = rand();

    int random_number = randomGenerator.getRandomNumber();
    int random_index = random_number % unassigned.size();
    int random_sign = ((random_number % 2) == 0) ? -1 : 1;
    return random_sign * unassigned[random_index];

    // uniform_int_distribution<> distr(0, unassigned.size() - 1);
    // return random_sign() * unassigned[distr(eng)];
}

bool stop_criterion_met(set<int> &clause)
{
    set<int> current_level_literal;
    for (int literal : clause)
    {
        if (graph[literal_value(literal)]->level == current_level)
        {
            current_level_literal.insert(literal);
        }
    }

    return current_level_literal.size() == 1;
}

int choose_literal(set<int> &clause, vector<int> &assignment_order)
{
    for (vector<int>::reverse_iterator it = assignment_order.rbegin(); it != assignment_order.rend(); ++it)
    {
        int literal = *it;

        if (clause.find(literal) != clause.end())
        {
            return literal;
        }

        if (clause.find(-literal) != clause.end())
        {
            return literal;
        }
    }
    return 0;
}

set<int> antecedent(int variable)
{
    return graph[variable]->clause;
}

set<int> resolve(set<int> &clause, set<int> &ante, int literal)
{
    // We are doing resolution in this step.
    // (x + y')( y + z) => (x + z)
    set<int> resolution(clause);
    resolution.insert(ante.begin(), ante.end());

    // getting rid of [literal, -literal]
    set<int>::iterator it;
    it = resolution.find(literal);
    if (it != resolution.end())
        resolution.erase(it);

    it = resolution.find(-literal);
    if (it != resolution.end())
        resolution.erase(it);

    return resolution;
}

pair<int, set<int>> get_learnings_and_level(set<int> &clause)
{
    set<int> literals;
    int backtrack_level = -1;

    for (int literal : clause)
    {
        literals.insert(literal);
        if (graph[literal_value(literal)]->level != current_level)
        {
            backtrack_level = max(graph[literal_value(literal)]->level, backtrack_level);
        }
    }

    if (backtrack_level == -1)
    {
        backtrack_level = current_level - 1;
    }

    return make_pair(backtrack_level, literals);
}

pair<int, set<int>> analyze_conflict(set<int> &conflict_clause)
{
    if (current_level == 0)
    {
        return make_pair(-1, set<int>());
    }

    // In current level the order in which variables were assigned
    vector<int> assignment_order;
    assignment_order.push_back(branch_record[current_level]);
    assignment_order.insert(
        assignment_order.end(),
        propagation_history[current_level].begin(),
        propagation_history[current_level].end());

    set<int> currrent_conflicting_clause(conflict_clause);

    while (!stop_criterion_met(currrent_conflicting_clause))
    {
        // Choose the most recent variable assignment done
        int literal = choose_literal(currrent_conflicting_clause, assignment_order);
        // Find ante, the clause to which this literal belongs to
        set<int> ante = antecedent(literal_value(literal));
        // Resolution is done on ante and currently conflicting clause and update it

        currrent_conflicting_clause = resolve(
            currrent_conflicting_clause, ante, literal_value(literal));
    }

    return get_learnings_and_level(currrent_conflicting_clause);
}

void backtrack(int backtrack_level)
{
    for (auto it = graph.begin(); it != graph.end(); ++it)
    {
        int variable = it->first;
        Node* node = it->second;

        if (node->level <= backtrack_level)
        {
            vector<Node *> pruned_children;
            for (Node* child : node->children)
            {
                if (child->level <= backtrack_level)
                {
                    pruned_children.push_back(child);
                }
            }
            node->children = pruned_children;
        }
        else
        {
            // graph.erase(variable);
            graph[variable] = new Node(variable);
            assignment[variable] = -1;
        }
    }

    branch_variable = set<int>();
    for (int variable = 1; variable <= num_of_variables; variable++)
    {
        if ((assignment[variable] != -1) && (graph[variable]->parents.size() == 0))
        {
            branch_variable.insert(variable);
        }
    }

    vector<int> levels;
    for (const auto &pair : branch_record)
    {
        levels.push_back(pair.first);
    }

    for (int level : levels)
    {
        if (level > backtrack_level)
        {
            auto bit = branch_record.find(level);
            branch_record.erase(bit);

            auto pit = propagation_history.find(level);
            propagation_history.erase(pit);
        }
    }
}

bool CDCL()
{
    
    while (!assignment_over())
    {
        // MPI jobs
        // checking for messages
        MPI_Iprobe(
            0, 
            MPI_ANY_TAG, 
            MPI_COMM_WORLD, 
            &probe_flag, 
            &mpi_status
        );

        if(probe_flag)
        {
            // someone found a solution
            if(mpi_status.MPI_TAG == STOP_TAG) 
            {
                // root is waiting for you to receive
                MPI_Recv(NULL, 0, MPI_CHAR, 0, STOP_TAG, MPI_COMM_WORLD, NULL);
                return true;
            }
            
            // someone found a new clause
            if(mpi_status.MPI_TAG == LEARNING_TAG) 
            {
                // get clause size
                MPI_Get_count(&mpi_status, MPI_INT, &new_clause_size);
                MPI_Recv(new_clause_buffer, new_clause_size, MPI_INT, 0, LEARNING_TAG, MPI_COMM_WORLD, NULL);
                
                set<int> learning(new_clause_buffer, new_clause_buffer + new_clause_size);

                learnings.insert(learning);
                clauses_with_learned.insert(learning);
            }

            probe_flag = 0;
        }


        set<int> conflict = unit_propagation();

        if (conflict.size() == 0)
        {
            if (assignment_over())
            {
                break;
            }

            branches++;
            int literal = select_variable();
            current_level++;
            assignment[literal_value(literal)] = literal_sign(literal);

            branch_variable.insert(literal_value(literal));
            branch_record[current_level] = literal_value(literal);
            propagation_history[current_level] = OrderedSet<int>();

            update_graph(literal_value(literal));
        }
        else
        {
            auto conflict_result = analyze_conflict(conflict);
            int backtrack_level = conflict_result.first;
            set<int> learning = conflict_result.second;
            if (backtrack_level < 0)
            {
                return false;
            }

            // TODO: This is a critical block. Wrap a lock around it
            // lock.lock()
            learnings.insert(learning);
            clauses_with_learned.insert(learning);
            // lock.unlock()

            backtrack(backtrack_level);
            current_level = backtrack_level;

            // TODO: send new learning to root
            copy(learning.begin(), learning.end(), new_clause_buffer);
            new_clause_size = learning.size();
            // printf("%d: sending a learning of size %d to root\n", my_rank, new_clause_size);
            MPI_Bsend(
                new_clause_buffer, 
                new_clause_size, 
                MPI_INT, 
                0, 
                LEARNING_TAG,
                MPI_COMM_WORLD
            );

        }
    }

    // tell root that you've solved it
    MPI_Send(
        assignment,
        num_of_variables + 1,
        MPI_INT,
        0,
        STOP_TAG,
        MPI_COMM_WORLD
    );

    // root is waiting for you to receive
    MPI_Recv(NULL, 0, MPI_CHAR, 0, STOP_TAG, MPI_COMM_WORLD, NULL);
    return true;
}

bool CDCL_solve()
{   
    compress_CNF();
    initialize_solver();
    return CDCL();
}

bool CDCL_root()
{
    MPI_Request* mpi_requests = new MPI_Request[mpi_nodes - 1];

    while(1)
    {
        MPI_Iprobe(
            MPI_ANY_SOURCE, 
            MPI_ANY_TAG, 
            MPI_COMM_WORLD, 
            &probe_flag, 
            &mpi_status
        );

        if(probe_flag)
        {
            // got a message
            // printf("%d: got a message from %d of tag %d\n", my_rank, mpi_status.MPI_SOURCE, mpi_status.MPI_TAG);

            // someone found a solution
            if(mpi_status.MPI_TAG == STOP_TAG) 
            {
                // get the solution
                // printf("%d: got a solution from %d\n", my_rank, mpi_status.MPI_SOURCE);

                MPI_Recv(
                    assignment, 
                    num_of_variables + 1,
                    MPI_INT,
                    mpi_status.MPI_SOURCE,
                    STOP_TAG,
                    MPI_COMM_WORLD,
                    NULL
                );

                // send message to everyone
                for(int c_node = 1; c_node < mpi_nodes; c_node++)
                {
                    MPI_Isend(
                        NULL, 
                        0, 
                        MPI_CHAR, 
                        c_node, 
                        STOP_TAG,
                        MPI_COMM_WORLD,
                        &mpi_requests[c_node - 1]
                    );
                }

                // wait for all to receive
                MPI_Waitall(mpi_nodes - 1, mpi_requests, NULL);
                return true;
            }

            // someone found a new clause
            else if(mpi_status.MPI_TAG == LEARNING_TAG) 
            {
                // get clause size
                MPI_Get_count(&mpi_status, MPI_INT, &new_clause_size);
                // printf("%d: got a learning from %d of size %d\n", my_rank, mpi_status.MPI_SOURCE, new_clause_size);


                MPI_Recv(new_clause_buffer, new_clause_size, MPI_INT, mpi_status.MPI_SOURCE, LEARNING_TAG, MPI_COMM_WORLD, NULL);

                set<int> learning(new_clause_buffer, new_clause_buffer + new_clause_size);

                if(learnings.find(learning) == learnings.end())
                {
                    // it's a new learning
                    // send everybody the learning
                    for(int c_node = 1; c_node < mpi_nodes; c_node++)
                    {
                        // except who sent it
                        if(c_node != mpi_status.MPI_SOURCE)
                        {
                            MPI_Bsend(
                                new_clause_buffer, 
                                new_clause_size, 
                                MPI_INT, 
                                c_node, 
                                LEARNING_TAG,
                                MPI_COMM_WORLD
                            );
                        }
                    }
                }
                
                learnings.insert(learning);
            }

            // reset the flag to receive new msg
            probe_flag = 0;
        }

        // this_thread::sleep_for(chrono::milliseconds(1));
        // printf("%d: waiting...\n", my_rank);
    }
}

void common_preprocess(string& filename)
{
    read_file(filename);
    int num_of_clauses = raw_clauses.size();
    if(my_rank == 0)
    {
        printf("%d variables. %d clauses\n", num_of_variables, num_of_clauses);
    }

    randomGenerator.setSeed(my_rank);

    assignment = new int[num_of_variables + 1];
    memset(assignment, -1, (num_of_variables + 1) * sizeof(int));

    new_clause_buffer = new int[num_of_variables];

    MPI_Buffer_attach(mpi_buffer, MPI_BUFFER_SIZE);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Usage: " << argv[0] << " <filename>" << endl;
        return 1; // Return an error code
    }

    // MPI configs
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_nodes);

    string filename = argv[1];
    common_preprocess(filename);

    double starttime, endtime;
    bool satisfied;

    MPI_Barrier(MPI_COMM_WORLD);
    starttime = MPI_Wtime();

        if(my_rank == 0)
        {
            satisfied = CDCL_root();
        }
        else 
        {
            satisfied = CDCL_solve();
        }
    
    MPI_Barrier(MPI_COMM_WORLD);
    endtime = MPI_Wtime();


    if(my_rank == 0)
    {
        printf("---------------- Result -----------------\n");
        if (satisfied)
        {
            if (!verify_assignment())
            {
                printf("Faulty assignment\n");
                return 1;
            }

            printf("SAT [Verified]\n");
            printf("New clauses %ld\n", learnings.size());

            cout << "Time " << (endtime - starttime) * 1000 << endl;
            cout << "SAT" << endl;
        }
        else
        {
            // cout << duration_sec.count() << endl;
            cout << "UNSAT" << endl;
        }
    }

    wrapup();

    MPI_Finalize();

    return 0;
}
