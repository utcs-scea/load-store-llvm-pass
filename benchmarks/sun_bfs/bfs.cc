#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <cstring>
#include <chrono>

class Graph {
private:
    std::vector<int> row_ptr;
    std::vector<int> col_idx;
    int num_vertices;

public:
    Graph(int n, int d) : num_vertices(n) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, n - 1);

        row_ptr.push_back(0);
        for (int i = 0; i < n; ++i) {
            int degree = std::poisson_distribution<>(d)(gen);
            for (int j = 0; j < degree; ++j) {
                int neighbor = dis(gen);
                if (neighbor != i) {
                    col_idx.push_back(neighbor);
                }
            }
            row_ptr.push_back(col_idx.size());
        }
    }

    std::vector<int> bfs_sssp(int start) const {
        std::vector<int> distances(num_vertices, -1);
        std::queue<int> q;

        distances[start] = 0;
        q.push(start);

        while (!q.empty()) {
            int current = q.front();
            q.pop();

            for (int i = row_ptr[current]; i < row_ptr[current + 1]; ++i) {
                int neighbor = col_idx[i];
                if (distances[neighbor] == -1) {
                    distances[neighbor] = distances[current] + 1;
                    q.push(neighbor);
                }
            }
        }

        return distances;
    }

    int get_num_vertices() const { return num_vertices; }
};

void print_usage() {
    std::cout << "Usage: ./program [-n rounds] [-g graph_size_power] [-d avg_degree] [-p]" << std::endl;
    std::cout << "  -n [int] : number of rounds of BFS-based SSSP to run (default: 10)" << std::endl;
    std::cout << "  -g [int] : defines the size of the graph. 2^(the int provided) (default: 20)" << std::endl;
    std::cout << "  -d [int] : average degree of each vertex in the graph (default: 16)" << std::endl;
    std::cout << "  -p       : if present, print debug output" << std::endl;
    std::cout << "  -h       : print this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    int num_rounds = 10;
    int graph_size_power = 20;
    int avg_degree = 16;
    bool print_debug = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            num_rounds = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            graph_size_power = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            avg_degree = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0) {
            print_debug = true;
        } else {
            print_usage();
            return 1;
        }
    }

    int num_vertices = 1 << graph_size_power;

    if (print_debug) {
        std::cout << "Initializing graph with " << num_vertices << " vertices and average degree " << avg_degree << std::endl;
    }

    Graph graph(num_vertices, avg_degree);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_vertices - 1);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int round = 0; round < num_rounds; ++round) {
        int start_vertex = dis(gen);
        int end_vertex = dis(gen);

        if (print_debug) {
            std::cout << "Round " << round + 1 << ": BFS-SSSP from vertex " << start_vertex << " to " << end_vertex << std::endl;
        }

        std::vector<int> distances = graph.bfs_sssp(start_vertex);

        if (print_debug) {
            std::cout << "  Distance: " << distances[end_vertex] << std::endl;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Completed " << num_rounds << " rounds of BFS-SSSP in " << duration.count() << " ms" << std::endl;

    return 0;
}