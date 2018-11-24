#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#include <iostream>
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

typedef unordered_map<int, pair<int, int> > d_vec;//first: min_dist; second: min_node
typedef unordered_map<int, d_vec> d_vecs;
unordered_map<int, d_vecs> all_vecs;

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
			if (node1_val == 0 || node2_val == 0) {
				continue;
			}
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
    		if (all_nodes.count(i) <= 0 || all_nodes.count(j) <= 0) {
    			continue;
    		}
    		if (f_tables[i][j].second == INT_MAX) {
    			continue;
    		}
    		if (i == j) {
    			fprintf(fpout, "%d %d %d\n", j, j, 0);
    			continue;
    		}
    		fprintf(fpout, "%d %d %d\n", j, f_tables[i][j].first, f_tables[i][j].second);
    	}
    	fprintf(fpout, "\n");
    }
    fclose(fpout);
}

queue<pair<int, d_vec> > message_queue;//first: message src, second: content

void print_table(int node) {
	printf("table for node %d:\n", node);
	for (int i = 1; i <= all_nodes.size(); i++) {
		for (int j = 1; j <= all_nodes.size(); j++) {
			printf("%d ", all_vecs[node][i][j].first);
		}
		printf("\n");
	}
	printf("table printing done\n");
}

void initialize_vectors() {
	int num_nodes = all_nodes.size();
	for (int node : all_nodes) {
		d_vecs vecs;

		d_vec my_vec;
		for (int v : all_nodes) {
			if (v == node) {
				my_vec[v] = make_pair(0, v);
				continue;
			}

			if (costs[node].count(v) > 0) {
	    		my_vec[v] = make_pair(costs[node][v], v);
	    	} else {
	    		my_vec[v] = make_pair(INT_MAX, INT_MAX);
	    	}
		}
		message_queue.push(make_pair(node, my_vec));
		vecs[node] = my_vec;

		for (pair<int, int> neighbor: costs[node]) {
    		int n_node = neighbor.first;
    		d_vec neighbor_vec;
    		//int n_cost = neighbor.second;
    		for (int v : all_nodes) {
    			neighbor_vec[v] = make_pair(INT_MAX, INT_MAX);
    		}
    		vecs[n_node] = neighbor_vec;
    	}
    	all_vecs[node] = vecs;
    	print_table(node);
	}
}

void dist_vector() {
	initialize_vectors();
	int num_nodes = all_nodes.size();
	f_tables.clear();

	while (!message_queue.empty()) {
		pair<int, d_vec> message = message_queue.front();
		message_queue.pop();
		int message_src = message.first;
		d_vec content = message.second;
		printf("current message src %d\n", message_src);
		for (pair<int, int> neighbor: costs[message_src]) {
			int n_node = neighbor.first;
			vector<int> updates;
			for (int node : all_nodes) {
				if (content[node].first < 
					all_vecs[n_node][message_src][node].first) {
					updates.push_back(node);
				}
			}	
			if (updates.size() == 0) {
				continue;
			}
			//update distance vector
			all_vecs[n_node][message_src] = content;
			print_table(n_node);
			
			for (int i = 0; i < updates.size(); i++) {
				int update = updates[i];
				//update has to be node's neighbor, so you don't need to 
				if (costs[n_node][message_src] + content[update].first <=
					all_vecs[n_node][n_node][update].first) {
					all_vecs[n_node][n_node][update].first = costs[n_node][message_src] + content[update].first;
					all_vecs[n_node][n_node][update].second = min(message_src, all_vecs[n_node][n_node][update].second);
				}
			}
			message_queue.push(make_pair(n_node, all_vecs[n_node][n_node]));
		}
	}

	for (int src : all_nodes) {
		unordered_map<int, pair<int, int> > f_table;
		printf("printing f_table\n");
		for (int dst : all_nodes) {
			f_table[dst].first = all_vecs[src][src][dst].second;
			f_table[dst].second = all_vecs[src][src][dst].first;
			printf("from %d to %d next_hop: %d, cost: %d\n", src, dst, f_table[dst].first, f_table[dst].second);
 		}
 		printf("printing f_table done\n");
 		f_tables[src] = f_table;
	}
	output_table();
}

void output_message(int src, int dst, char* message) {
	FILE * fpout;

	fpout = fopen("output.txt", "a");

	if (f_tables[src][dst].second == INT_MAX || 
		all_nodes.count(src) <= 0 ||
		all_nodes.count(dst) <=0) {
		fprintf(fpout, "from %d to %d cost infinite hops unreachable message %s\n", src, dst, message);
		fclose(fpout);
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
	if (hops.empty()) {
		fprintf(fpout, " ");
	}
	fprintf(fpout, "message %s\n", message);
	fprintf(fpout, "\n");
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
			int src = -1, dst = -1;
			sscanf(line.c_str(), "%d %d %[^\n]", &src, &dst, message);
			if (src == -1 || dst == -1) {
				continue;
			}
			printf("src: %d dst: %d ", src, dst);
			printf("message: %s\n", message);
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
			if (node1_val == 0 || node2_val == 0 || new_cost == 0) {
				continue;
			}
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
			dist_vector();
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



	dist_vector();
	send_message(argv[2]);
	read_change(argv[3], argv[2]);

    return 0;
}