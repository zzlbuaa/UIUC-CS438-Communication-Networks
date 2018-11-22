#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#include <fstream>
#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <string>
#include <sstream>

using namespace std;

//typedef pair<int, int> E;
//typedef map<E, int> CostMap;

typedef unordered_map<int, int> neighbors;
typedef unordered_map<int, neighbors > CostMap;

//typedef unordered_map<int, int> f_table;
//typedef unordered_map<int, f_table> f_tables;

unordered_map<int, unordered_map<int, pair<int, int> > > f_tables;

CostMap costs;
unordered_set<int> all_nodes;

void readTopo(char* filename) {
	ifstream in;
	string line;
	in.open(filename);

	if (in.is_open()) {
		while (getline(in, line)) {
			stringstream ss(line);
			string node1, node2, cost;
			ss >> node1;
			ss >> node2;
			ss >> cost;
			int node1_val = atoi(node1.c_str());
			int node2_val = atoi(node2.c_str());
			all_nodes.insert(node1_val);
			all_nodes.insert(node2_val);
			//costs[E((node1_val), node2_val)] = atoi(cost.c_str());
			costs[node1_val][node2_val] = atoi(cost.c_str());
			costs[node2_val][node1_val] = atoi(cost.c_str());
		
		}
		in.close();
	} else {
		exit(1);
	}
}

void output_table() {
	FILE * fpout;
	fpout = fopen("output.txt", "a");

	int num_nodes = all_nodes.size();
	for (int i = 1; i <= num_nodes; i++) {
    	for (int j = 1; j <= num_nodes; j++) {
    		if (f_tables[i][j].second == INT_MAX) {
    			continue;
    		}
    		fprintf(fpout, "%d %d %d\n", j, f_tables[i][j].first, f_tables[i][j].second);
    	}
    }
    fclose(fpout);
}

//should return the forwarding table for each node
void dijkstra() {
	int num_nodes = all_nodes.size();
	f_tables.clear();
	//unordered_map<int, unordered_map<int, pair<int, int> > > f_tables;
	for (int node: all_nodes) {
    	printf("current node: %d\n",node);
    	//Initialization:
    	unordered_set<int> confirmed_nodes;
    	unordered_map<int, int> distances;
    	unordered_map<int, int> prev_nodes;

    	confirmed_nodes.insert(node);

    	for (int v: all_nodes) {
    		if (costs[node].count(v) > 0) {
    			distances[v] = costs[node][v];
    		} else {
    			distances[v] = INT_MAX;
    		}
    		prev_nodes[v] = node;
    		printf("node: %d, distance: %d\n", v, distances[v]);
    	}

    	queue<int> min_nodes;
    	//Loop all_nodes.size() - 1 times
    	for (int i = 0; i < num_nodes - 1; i++) {
    		int min_dist = INT_MAX;
    		int min_node = INT_MAX;
    		//Step1: select min node that is not confirmed
    		for (int v: all_nodes) {
    			if (confirmed_nodes.count(v) > 0) {
    				continue;
    			}
    			if (distances[v] < min_dist) {
    				min_dist = distances[v];
    				min_node = v;
    			
    			}
    			if (distances[v] == min_dist) {
    				//Choose the lower ID if there's a tie
    				min_node = min(v, min_node);
    				
    			}
    		}
    		printf("current min_node: %d\n", min_node);
    		confirmed_nodes.insert(min_node);
    		min_nodes.push(min_node);

    		for (pair<int, int> neighbor: costs[min_node]) {
    			int n_node = neighbor.first;
    			int n_cost = neighbor.second; 
    			if (confirmed_nodes.count(n_node) > 0) {
    				continue;
    			}
    			if (distances[min_node] != INT_MAX &&
    				distances[min_node] + n_cost < distances[n_node]) {
    				distances[n_node] = distances[min_node] + n_cost;
    				prev_nodes[n_node] = min_node;
    			}
    		}
    		printf("node: %d, distance: %d\n", min_node, distances[min_node]);
    	}

    	unordered_map<int, pair<int, int> > f_table;
    	f_table[node].first = node;
    	f_table[node].second = 0;
    	while (!min_nodes.empty()) {
    		int cur = min_nodes.front();
    		min_nodes.pop();
    		if (prev_nodes[cur] == node) {
    			f_table[cur].first = cur;
    			f_table[cur].second = distances[cur];
    		} else {
    			f_table[cur].first = f_table[prev_nodes[cur]].first;
    			f_table[cur].second = distances[cur];
    		}
    	}
    	f_tables[node] = f_table;
    }
    output_table();
    //return f_tables;
}

void output_message(int src, int dst, char* message) {
	FILE * fpout;

	fpout = fopen("output.txt", "a");

	if (f_tables[src][dst].second == INT_MAX) {
		fprintf(fpout, "from %d to %d cost infinite hops unreachable message %s\n", src, dst, message);
		return;//or continue
	}

	vector<int> hops;
	int curt_hop = src;
	while (curt_hop != dst) {
		hops.push_back(curt_hop);
		curt_hop = f_tables[curt_hop][dst].first;	
	}
	fprintf(fpout, "from %d to %d cost %d hops ", src, dst, f_tables[src][dst].second);
	for (int hop : hops) {
		fprintf(fpout, "%d ", hop);
	}
	fprintf(fpout, "message %s\n", message);
	fclose(fpout);

}

void send_message(char* messagefile) {
	//messagefile: 2 1 send this message from 2 to 1
	ifstream in;
	string line;
	in.open(messagefile);

	if (in.is_open()) {
		while (getline(in, line)) {
			int message_len = line.length();
			char message[message_len];
			int src, dst;

			sscanf(line.c_str(), "%d %d %[^\0]", &src, &dst, message);
			output_message(src, dst, message);
		}
		in.close();
	} else {
		exit(2);
	}
}

void read_change(char* changefile, char* messagefile) {
	ifstream in;
	string line;
	in.open(changefile);

	if (in.is_open()) {
		while (getline(in, line)) {
			stringstream ss(line);
			string node1, node2, cost;
			ss >> node1;
			ss >> node2;
			ss >> cost;
			int node1_val = atoi(node1.c_str());
			int node2_val = atoi(node2.c_str());
			int new_cost = atoi(cost.c_str());
			if (new_cost == -999) {
				costs[node1_val].erase(node2_val);
				costs[node2_val].erase(node1_val);
				if (costs.count(node1_val) <= 0) {
					all_nodes.erase(node1_val);
				}
				if (costs.count(node2_val) <= 0) {
					all_nodes.erase(node2_val);
				}
			} else {
				all_nodes.insert(node1_val);
				all_nodes.insert(node2_val);
				costs[node1_val][node2_val] = new_cost;
				costs[node2_val][node1_val] = new_cost;
			}
			dijkstra();
			send_message(messagefile);
		}
		in.close();
	} else {
		exit(1);
	}
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fclose(fpOut);

    readTopo(argv[1]);
    printf("costs contains:\n");



	dijkstra();
	send_message(argv[2]);
	read_change(argv[3], argv[2]);
    // for(
    // 	CostMap::const_iterator emit = costs.begin();
    // 	emit != costs.end();
    // 	emit++) {
    // 	printf("(%d %d) %d\n", emit->first.first, emit->first.second, emit->second);
    // }

    // for(
    // 	auto emit = all_nodes.begin();
    // 	emit != all_nodes.end();
    // 	emit++) {
    // 	printf("%d\n", *emit);
    // }

    // for (pair<int, neighbors > costmap : costs) {
    // 	for (pair<int, int> elem : costmap.second) {
    // 		printf("(%d %d) %d\n", costmap.first, elem.first, elem.second);
    // 	}
    // }

    // for (int elem: all_nodes) {
    // 	printf("%d\n", elem);
    // }

    return 0;
}