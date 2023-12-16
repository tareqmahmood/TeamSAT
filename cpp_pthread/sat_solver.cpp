#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <random>
#include <ratio>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


using namespace std;

typedef set<int> clause_t;

class SatSolver;

class Coordinator
{
  private:
    SatSolver** sat_solvers;
    int n_solvers;
    set<clause_t> common_learnings;

  public:
    bool solved = false;        // shared variable
    int solver_thread = -1;     // shared variable
    pthread_mutex_t solve_lock; // shared variable, lock for solved and solver_thread


    pthread_mutex_t cond_lock;     // shared variable, lock for learnings_cond
    pthread_cond_t learnings_cond; // shared variable, condition variable for new_learnings
    pthread_mutex_t llock;         // shared variable, lock for new_learnings
    set<clause_t> learn_buf;       // shared variable


    Coordinator(SatSolver** sat_solvers, int n_solvers);
    ~Coordinator();
    int get_learning_size();
    void coordinate();
};

Coordinator* g_coordinator = NULL;


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

template<class Type>
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

    Type& operator[](int index)
    {
        return order[index];
    }

    void insert(const Type& item)
    {
        if (seen.find(item) == seen.end()) {
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

class SimpleRandomGenerator
{
  private:
    unsigned int seed;

  public:
    SimpleRandomGenerator(unsigned int initialSeed = 2)
      : seed(initialSeed)
    {
    }

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

vector<string>
split_string(string input_string, char delimiter)
{
    stringstream ss(input_string);
    vector<string> tokens;
    string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}


class SatSolver
{
  private:
    string filename;
    int seed;

    int implications = 0;
    int branches = 0;
    int num_of_variables = 0;

    vector<vector<int>> raw_clauses;
    set<clause_t> clauses;
    set<clause_t> clauses_with_learned;

    int current_level = 0;
    set<int> branch_variable;
    unordered_map<int, int> branch_record;
    set<clause_t> learnings;
    int* assignment;

    unordered_map<int, Node*> graph;
    unordered_map<int, OrderedSet<int>> propagation_history;

    SimpleRandomGenerator randomGenerator;


    void read_file(string& input_file)
    {
        num_of_variables = 0;
        raw_clauses.clear();

        ifstream input_file_steam(input_file);

        if (!input_file_steam.is_open()) {
            cerr << "Error opening file: " << input_file << endl;
            return; // Return an error code
        }

        // Read the file line by line
        string line;
        while (getline(input_file_steam, line)) {
            if (line[0] == 'c' || line[0] == 'p' || line[0] == '%' || line[0] == '0') {
                if (line[0] == 'p') {
                    num_of_variables = stoi(split_string(line, ' ')[2]);
                }
            } else {
                vector<int> clause;
                vector<string> tokens = split_string(line, ' ');
                for (const string& t : tokens) {
                    int literal = stoi(t);
                    if (abs(literal) > 0 && abs(literal) <= num_of_variables) {
                        clause.push_back(literal);
                    }
                }
                if (clause.size() > 0) {
                    raw_clauses.push_back(clause);
                }
            }
            continue;
        }

        // Close the file
        input_file_steam.close();
    }

    clause_t regularize_clause(vector<int>& clause)
    {
        clause_t r_clause;
        clause_t unique_clause;

        for (int literal : clause) {
            r_clause.insert(literal);
            unique_clause.insert(literal);
        }

        for (int literal : clause) {
            if (unique_clause.find(-literal) != unique_clause.end()) {
                return set<int>(); // empty clause
            }
        }

        return r_clause;
    }

    void compress_CNF()
    {
        clauses.clear();
        for (vector<int>& clause : raw_clauses) {
            clause_t r_clause = regularize_clause(clause);
            if (r_clause.size() == 0)
                continue;
            clauses.insert(r_clause);
        }
        // raw_clauses.clear();
    }

    void initialize_solver()
    {

        learnings.clear();
        clauses_with_learned.clear();
        clauses_with_learned.insert(clauses.begin(), clauses.end());

        current_level = 0;
        branch_variable.clear();


        branch_record.clear();
        propagation_history.clear();

        assignment = new int[num_of_variables + 1];
        memset(assignment, -1, (num_of_variables + 1) * sizeof(int));

        graph.clear();
        for (int v = 1; v <= num_of_variables; v++) {
            graph[v] = new Node(v);
        }

        implications = 0;
        branches = 0;
    }

    void wrapup()
    {
        delete[] assignment;
    }

    bool assignment_over()
    {
        for (int i = 1; i <= num_of_variables; i++) {
            if (assignment[i] == -1)
                return false;
        }
        return true;
    }

    bool solved()
    {
        // NOTE: this is a critical section
        {
            pthread_mutex_lock(&g_coordinator->solve_lock);
            if (g_coordinator->solved) {
                pthread_mutex_unlock(&g_coordinator->solve_lock);
                return true;
            }
            pthread_mutex_unlock(&g_coordinator->solve_lock);
        }
        return false;
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
        if (assignment[literal_value(literal)] == -1) {
            return -1;
        } else if (assignment[literal_value(literal)] == literal_sign(literal)) {
            return 1;
        } else {
            return 0;
        }
    }

    int evaluate_clause(const clause_t& clause)
    {
        int undecided = 0;
        for (int literal : clause) {
            if (evaluate_literal(literal) == 1) {
                return 1;
            } else if (evaluate_literal(literal) == -1) {
                undecided += 1;
            }
        }

        if (undecided > 0) {
            return -undecided;
        } else {
            return 0;
        }
    }

    int get_first_literal(const clause_t& clause)
    {
        for (int literal : clause) {
            if (evaluate_literal(literal) == -1) {
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

    void update_graph_with_clause(int variable, const clause_t& clause)
    {
        graph[variable]->level = current_level;
        graph[variable]->value = assignment[variable];

        clause_t::iterator it = clause.begin();
        while (it != clause.end()) {
            int l_literal = *it;
            int literal = literal_value(l_literal);

            if (variable != literal) {
                graph[literal]->children.push_back(graph[variable]);
                graph[variable]->parents.push_back(graph[literal]);
            }
            ++it;
        }
        graph[variable]->clause = clause;
    }

    clause_t unit_propagation()
    {
        while (1) {
            OrderedSet<pair<int, clause_t>> propagation;
            for (const clause_t& clause : clauses_with_learned) {
                int result = evaluate_clause(clause);
                if (result == 1) {
                    continue;
                } else if (result == 0) {
                    return clause;
                } else if (result == -1) {
                    propagation.insert(make_pair(get_first_literal(clause), clause));
                }
            }

            if (propagation.empty()) {
                // FIXMEMAYBE:
                return clause_t(); // empty clause
            }

            for (const pair<int, clause_t>& propagation_entry : propagation) {
                int literal = propagation_entry.first;
                clause_t clause = propagation_entry.second;

                implications++;
                assignment[literal_value(literal)] = literal_sign(literal);

                update_graph_with_clause(literal_value(literal), clause);

                if (current_level != 0) {
                    propagation_history[current_level].insert(literal);
                }
            }
        }
    }

    int select_variable()
    {
        // RANDOM
        vector<int> unassigned;
        for (int v = 1; v <= num_of_variables; v++) {
            if (assignment[v] == -1) {
                unassigned.push_back(v);
            }
        }

        int random_number = randomGenerator.getRandomNumber();
        int random_index = random_number % unassigned.size();
        int random_sign = ((random_number % 2) == 0) ? -1 : 1;
        return random_sign * unassigned[random_index];
    }

    bool stop_criterion_met(clause_t& clause)
    {
        set<int> current_level_literal;
        for (int literal : clause) {
            if (graph[literal_value(literal)]->level == current_level) {
                current_level_literal.insert(literal);
            }
        }

        return current_level_literal.size() == 1;
    }

    int choose_literal(clause_t& clause, vector<int>& assignment_order)
    {
        for (vector<int>::reverse_iterator it = assignment_order.rbegin(); it != assignment_order.rend(); ++it) {
            int literal = *it;

            if (clause.find(literal) != clause.end()) {
                return literal;
            }

            if (clause.find(-literal) != clause.end()) {
                return literal;
            }
        }
        return 0;
    }

    clause_t antecedent(int variable)
    {
        return graph[variable]->clause;
    }

    clause_t resolve(clause_t& clause, clause_t& ante, int literal)
    {
        // We are doing resolution in this step.
        // (x + y')( y + z) => (x + z)
        clause_t resolution(clause);
        resolution.insert(ante.begin(), ante.end());

        // getting rid of [literal, -literal]
        clause_t::iterator it;
        it = resolution.find(literal);
        if (it != resolution.end())
            resolution.erase(it);

        it = resolution.find(-literal);
        if (it != resolution.end())
            resolution.erase(it);

        return resolution;
    }

    pair<int, clause_t> get_learnings_and_level(clause_t& clause)
    {
        set<int> literals;
        int backtrack_level = -1;

        for (int literal : clause) {
            literals.insert(literal);
            if (graph[literal_value(literal)]->level != current_level) {
                backtrack_level = max(graph[literal_value(literal)]->level, backtrack_level);
            }
        }

        if (backtrack_level == -1) {
            backtrack_level = current_level - 1;
        }

        return make_pair(backtrack_level, literals);
    }

    pair<int, clause_t> analyze_conflict(clause_t& conflict_clause)
    {
        if (current_level == 0) {
            return make_pair(-1, clause_t());
        }

        // In current level the order in which variables were assigned
        vector<int> assignment_order;
        assignment_order.push_back(branch_record[current_level]);
        assignment_order.insert(
          assignment_order.end(),
          propagation_history[current_level].begin(),
          propagation_history[current_level].end());

        clause_t currrent_conflicting_clause(conflict_clause);

        while (!stop_criterion_met(currrent_conflicting_clause)) {
            // Choose the most recent variable assignment done
            int literal = choose_literal(currrent_conflicting_clause, assignment_order);
            // Find ante, the clause to which this literal belongs to
            clause_t ante = antecedent(literal_value(literal));
            // Resolution is done on ante and currently conflicting clause and update it

            currrent_conflicting_clause = resolve(
              currrent_conflicting_clause, ante, literal_value(literal));
        }

        return get_learnings_and_level(currrent_conflicting_clause);
    }

    void backtrack(int backtrack_level)
    {
        for (auto it = graph.begin(); it != graph.end(); ++it) {
            int variable = it->first;
            Node* node = it->second;

            if (node->level <= backtrack_level) {
                vector<Node*> pruned_children;
                for (Node* child : node->children) {
                    if (child->level <= backtrack_level) {
                        pruned_children.push_back(child);
                    }
                }
                node->children = pruned_children;
            } else {
                // graph.erase(variable);
                graph[variable] = new Node(variable);
                assignment[variable] = -1;
            }
        }

        branch_variable = set<int>();
        for (int variable = 1; variable <= num_of_variables; variable++) {
            if ((assignment[variable] != -1) && (graph[variable]->parents.size() == 0)) {
                branch_variable.insert(variable);
            }
        }

        vector<int> levels;
        for (const auto& pair : branch_record) {
            levels.push_back(pair.first);
        }

        for (int level : levels) {
            if (level > backtrack_level) {
                auto bit = branch_record.find(level);
                branch_record.erase(bit);

                auto pit = propagation_history.find(level);
                propagation_history.erase(pit);
            }
        }
    }

    bool CDCL()
    {
        while (!assignment_over()) {
            if (solved()) {
                break;
            }

            clause_t conflict = unit_propagation();

            if (conflict.size() == 0) {
                if (assignment_over()) {
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
            } else {
                auto conflict_result = analyze_conflict(conflict);
                int backtrack_level = conflict_result.first;
                clause_t learning = conflict_result.second;
                if (backtrack_level < 0) {
                    return false;
                }


                learnings.insert(learning);
                clauses_with_learned.insert(learning);

                // add the learnings from coordinator to this solver
                // NOTE: this is a critical section
                {
                    pthread_mutex_lock(&this->lbuf_lock);
                    if (learn_buffer.size() > 0) {
                        // printf("\ttid %d: processing learnings %ld\n", this->tid, learn_buffer.size());
                        for (const clause_t& clause : learn_buffer) {
                            learnings.insert(clause);
                            clauses_with_learned.insert(clause);
                        }
                        learn_buffer.clear();
                    }
                    pthread_mutex_unlock(&this->lbuf_lock);
                }

                // announce your learning to the coordinator, then backtrack
                // NOTE: this is a critical section
                {
                    // printf("\ttid %d: trying to get llock\n", this->tid);
                    pthread_mutex_lock(&g_coordinator->llock);
                    g_coordinator->learn_buf.insert(learning);
                    pthread_mutex_unlock(&g_coordinator->llock);

                    pthread_mutex_lock(&g_coordinator->cond_lock);
                    pthread_cond_signal(&g_coordinator->learnings_cond);
                    // printf("\ttid %d: signaled\n", this->tid);
                    pthread_mutex_unlock(&g_coordinator->cond_lock);
                }

                backtrack(backtrack_level);
                current_level = backtrack_level;
            }
            // printf("\ttid %d: working %d\n", this->tid, assignment_over());
        }

        // Announce that you have solved the problem
        // NOTE: this is a critical section
        {
            pthread_mutex_lock(&g_coordinator->solve_lock);
            // printf("\ttid %d: solved\n", this->tid);
            if (!g_coordinator->solved) {
                g_coordinator->solver_thread = this->tid;
                g_coordinator->solved = true;
            }
            pthread_mutex_unlock(&g_coordinator->solve_lock);
        }
        // NOTE: this is a critical section
        {
            pthread_mutex_lock(&g_coordinator->cond_lock);
            pthread_cond_signal(&g_coordinator->learnings_cond);
            pthread_mutex_unlock(&g_coordinator->cond_lock);
        }


        return true;
    }

    void print_assignment()
    {
        for (int v = 1; v <= num_of_variables; v++) {
            printf("%d ", assignment[v]);
        }
        printf("\n");
    }

    void print_all_clauses()
    {
        for (const vector<int>& clause : raw_clauses) {
            for (const int literal : clause) {
                printf("%d ", literal);
            }
            printf("\n");
        }
    }

  public:
    int tid;
    // pthread_mutex_t learn_lock;
    pthread_mutex_t lbuf_lock; // shared variable, lock for learn_buffer
    set<clause_t> learn_buffer;


    SatSolver(int tid)
      : tid(tid)
    {
        pthread_mutex_init(&this->lbuf_lock, NULL);
    }

    SatSolver(string filename, int seed)
    {
        this->filename = filename;
        this->seed = seed;
    }

    ~SatSolver()
    {
        delete[] assignment;
    }

    void init(string filename)
    {
        this->filename = filename;
        this->seed = this->tid + 1;
        this->tid = tid;
    }

    void preprocess()
    {
        read_file(filename);
        randomGenerator.setSeed(seed);
        // #pragma omp parallel master // TODO:
        {
            int num_of_clauses = raw_clauses.size();
            printf("%d variables. %d clauses\n", num_of_variables, num_of_clauses);
        }
    }

    bool solve()
    {
        compress_CNF();
        initialize_solver();
        return CDCL();
    }

    bool verify_assignment()
    {
        for (const vector<int>& clause : raw_clauses) {
            bool clause_true = false;
            if (clause.size() == 0)
                continue;
            for (const int literal : clause) {
                if (assignment[literal_value(literal)] == literal_sign(literal)) {
                    clause_true = true;
                }
            }
            if (!clause_true)
                return false;
        }
        return true;
    }

    int get_learning_size()
    {
        return learnings.size();
    }
};


Coordinator::Coordinator(SatSolver** sat_solvers, int n_solvers)
{
    this->sat_solvers = sat_solvers;
    this->n_solvers = n_solvers;
    this->solved = false;
    this->solver_thread = -1;

    // printf("\tCOORD: created\n");
    // for (int i = 0; i < n_solvers; i++)
    //     printf("\t\tCOORD: solver %d\n", sat_solvers[i]->tid);

    // create and init locks
    pthread_mutex_init(&this->solve_lock, NULL);
    pthread_mutex_init(&this->llock, NULL);
    pthread_mutex_init(&this->cond_lock, NULL);
    pthread_cond_init(&this->learnings_cond, NULL);
}


Coordinator::~Coordinator()
{
    // destruct new_learnings
    learn_buf.clear();
    // destroy mutex
    pthread_mutex_destroy(&this->llock);
    pthread_mutex_destroy(&this->cond_lock);
    pthread_mutex_destroy(&this->solve_lock);
    // destroy cond
    pthread_cond_destroy(&this->learnings_cond);
}


// @assumption: the problem is already solved and this function is called in a single threaded
int
Coordinator::get_learning_size()
{
    int size = -1;

    // NOTE: this is a critical section
    {
        if (solver_thread >= 0 && solver_thread < n_solvers)
            size = sat_solvers[solver_thread]->get_learning_size();
    }
    return size;
}

void
Coordinator::coordinate()
{
    // printf("\tCOORD: started. new learning size %ld\n", learn_buf.size());
    while (1) {
        // NOTE: this is a critical section
        {
            pthread_mutex_lock(&this->cond_lock);
            while (learn_buf.size() == 0 && !solved) {
                // printf("\tCOORD: waiting\n");
                pthread_cond_wait(&this->learnings_cond, &this->cond_lock);
            }
            // printf("\tCOORD: woke up %ld\n", learn_buf.size());
            pthread_mutex_unlock(&this->cond_lock);
        }

        // NOTE: this is a critical section
        {
            pthread_mutex_lock(&this->solve_lock);
            if (solved) {
                // printf("\tCOORD: solved\n");
                pthread_mutex_unlock(&this->solve_lock);
                break;
            }
            pthread_mutex_unlock(&this->solve_lock);
        }

        set<clause_t> new_learnings;
        // NOTE: this is a critical section
        {
            pthread_mutex_lock(&this->llock);
            if (learn_buf.size() > 0) {

                // // printf("\tCOORD: processing new learnings %ld\n", learn_buf.size());
                // // check if the broadcasted learnings are already in common_learnings
                // for (const clause_t& clause : learn_buf) {
                //     if (this->common_learnings.find(clause) == this->common_learnings.end()) {
                //         this->common_learnings.insert(clause);
                //         new_learnings.insert(clause);
                //     }
                // }
                // learn_buf.clear();
            }

            pthread_mutex_unlock(&this->llock);
        }
        // TODO: add new_learnings to all solvers
        // NOTE: this is a critical section
        if (new_learnings.size() > 0) {
            for (int i = 0; i < n_solvers; i++) {
                pthread_mutex_lock(&sat_solvers[i]->lbuf_lock);
                sat_solvers[i]->learn_buffer.insert(new_learnings.begin(), new_learnings.end());
                pthread_mutex_unlock(&sat_solvers[i]->lbuf_lock);
            }
        }
    }
    // printf("\tCOORD: finished\n");
    return;
}

// =============================================================================
// THREAD POOL
// =============================================================================


void*
work(void* arg)
{
    SatSolver* sat_solver = (SatSolver*)arg;
    sat_solver->solve();
    return NULL;
}

void
threaded_solve(SatSolver*** sat_solvers_ptr, int n_solvers)
{
    // printf("n_solvers: %d\n", n_solvers);
    SatSolver** sat_solvers = *sat_solvers_ptr;
    pthread_t* threads = new pthread_t[n_solvers];

    // Create threads
    for (int tid = 0; tid < n_solvers; tid++) {
        // printf("sat_solvers[%d]->tid = %d\n", tid, sat_solvers[tid]->tid);
        pthread_create(&threads[tid], NULL, work, (void*)sat_solvers[tid]);
    }


    // // run coordinator
    g_coordinator->coordinate();


    // Wait for all threads to finish
    for (int tid = 0; tid < n_solvers; tid++) {
        pthread_join(threads[tid], NULL);
        // printf("\tThread %d finished\n", tid);
    }

    delete[] threads;
}

// =============================================================================
// THREAD POOL END
// =============================================================================

int
main(int argc, char* argv[])
{
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <filename> <num_of_threads>" << endl;
        return 1; // Return an error code
    }
    string filename = argv[1];
    int n_threads = atoi(argv[2]);
    // NOTE: every thread except the coordinator thread solves the problem
    // so, there are `n_threads - 1` solvers
    int n_solvers = n_threads - 1;
    printf("n_solvers: %d\n", n_solvers);


    SatSolver** sat_solvers = new SatSolver*[n_solvers];
    for (int tid = 0; tid < n_solvers; tid++) {
        sat_solvers[tid] = new SatSolver(tid);
        sat_solvers[tid]->init(filename);
        sat_solvers[tid]->preprocess();
        // printf("\ttid %d: preprocessing done\n", tid);
    }

    g_coordinator = new Coordinator(sat_solvers, n_solvers);

    printf("Solving...\n");
    chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();
    threaded_solve(&sat_solvers, n_solvers);
    chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> duration_sec = chrono::duration_cast<chrono::duration<double, std::milli>>(end - start);
    printf("Solved\n");

    // return 0;
    printf("---------------- Result -----------------\n");
    if (g_coordinator->solver_thread >= 0) {
        if (!sat_solvers[g_coordinator->solver_thread]->verify_assignment()) {
            printf("Faulty assignment\n");
        } else {
            printf("SAT [Verified]\n");
            printf("New clauses %d\n", g_coordinator->get_learning_size());

            cout << "Time " << duration_sec.count() << endl;
            cout << "SAT" << endl;
        }
    } else {
        cout << duration_sec.count() << endl;
        cout << "UNSAT" << endl;
    }


    return 0;
}