#include <stdio.h>
#include <stdlib.h>
#include <time.h> 
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace std;

int num_nodes;
int pckt_size;
vector<int> ranges;
int max_attempt;
int simu_time;
bool channelOccupied = false;
vector<int> backoffs;
vector<int> attempts;

//Statistics for analysis
int channel_idle_time = 0;
int channel_occu_time = 0;
int total_collision = 0;
int transmitted_pckt = 0;
int unused_time = 0;
vector<int> collisions;

vector<int>success_transmits;

double calculateVariance(vector<int> data) {
	int size = data.size();
	int sum = 0; 

	for (int i = 0; i < size; i++) {
		sum += data[i];
	}

	double mean = (double)sum / (double)size;

	double sqDiff = 0;
	for (int i = 0; i < size; i++) {
		sqDiff += (data[i] - mean) * (data[i] - mean);
	}

	return sqDiff / (double)size;
}

void parseLine(string line) {
	string input_type = "";
    istringstream ss(line);
    for(std::string s; ss >> s; ) {
        if (input_type == "N") {
        	num_nodes = atoi(s.c_str());
        }
        if (input_type == "L") {
        	pckt_size = atoi(s.c_str());
        }
        if (input_type == "M") {
        	max_attempt = atoi(s.c_str());
        }
        if (input_type == "T") {
        	simu_time = atoi(s.c_str());
        }
        if (input_type == "R") {
        	ranges.push_back(atoi(s.c_str()));
        }

        if (s == "N" || s == "L" || s == "R" || s == "M" || s == "T") {
        	input_type = s;
        }
    }
}


void readInput(char* filename) {
	ifstream in;
	string line;
	in.open(filename);

	if (in.is_open()) {
		for (int i =0; i < 5; i++) {
			getline(in, line);
			parseLine(line); 
		}
		in.close();
		cout << "------Simulation parameters initializing------" << endl;
	    cout << "number of nodes: " << num_nodes << endl;
		cout << "size of packet: " << pckt_size << endl;
		cout << "backoff times: ";
		for (int i : ranges) {
			cout << i << " ";
		}
		cout << endl;
		cout << "max attempt:" << max_attempt << endl;
		cout << "time of simulation:" << simu_time << endl;
	} else {
		exit(1);
	}
}

int getRandom(int range) {
	return rand() % (range + 1);
}

void run_simulation() {
	int time_to_finish = 0;
	int max_range = ranges[ranges.size() - 1];
	int transmit_pckt;
	vector<int> Rs;
	attempts.clear();
	collisions.clear();
	for (int i = 0; i < num_nodes; i++) {
		attempts.push_back(0);
		collisions.push_back(0);
		Rs.push_back(ranges[0]);
		success_transmits.push_back(0);
	}

	//initialize backoffs
	srand (time(NULL));
	backoffs.clear();
	for (int i = 0; i < num_nodes; i++) {
		backoffs.push_back(getRandom(Rs[i]));
		//cout << backoffs[i] << endl;
	}

	//run simulation for simu_time
	for (int t = 0; t < simu_time; t++) {
		//getchar();
		//if channel occupied, transmit the packet and freeze all counts
		if (channelOccupied) {
			if (time_to_finish > 0) {
				time_to_finish--;
				channel_occu_time++;
				continue;
			}
			transmitted_pckt++;
			channelOccupied = false;
			success_transmits[transmit_pckt] += 1;
			Rs[transmit_pckt] = ranges[0];
			attempts[transmit_pckt] = 0;
			backoffs[transmit_pckt] = getRandom(Rs[transmit_pckt]);	
		}

		// cout << "backoffs: ";
		// for (int i = 0; i < num_nodes; i++) {
		// 	cout << backoffs[i] << " ";
		// }
		// cout << endl;
		// cout << "attempts: ";
		// for (int i = 0; i < num_nodes; i++) {
		// 	cout << attempts[i] << " ";
		// }
		// cout << endl;
		// cout << "Rs: ";
		// for (int i = 0; i < num_nodes; i++) {
		// 	cout << Rs[i] << " ";
		// }
		// cout << endl;

		vector<int> candidates;
		for (int idx = 0; idx < num_nodes; idx++) {
			if (backoffs[idx] == 0) {
				candidates.push_back(idx);
			}
		}
		//cout << "candidates size" << candidates.size() << endl;
		//if no one counts to 0, decrement everyone and continue
		if (candidates.size() == 0) {
			for (int i = 0; i < num_nodes; i++) {
				backoffs[i] -= 1;
			}
			channel_idle_time++;
			unused_time++;
			continue;
		}
		//if there is no collision, start transmission
		if (candidates.size() == 1) {
			channelOccupied = true;
			time_to_finish = pckt_size - 1;
			channel_occu_time++;
			transmit_pckt = candidates[0];
			continue;
		}
		//collision happens
		if (candidates.size() > 1) {
			channel_idle_time++;
			total_collision += candidates.size();
			for (int idx : candidates) {
				//double the backoff range
				if (Rs[idx] * 2 <= max_range) {
					Rs[idx] *= 2;
				}
				collisions[idx] += 1;
				attempts[idx]++;
				//reach max attempt, drop the pckt and reset
				if (attempts[idx] >= max_attempt) {
					Rs[idx] = ranges[0];
					attempts[idx] = 0;
				}
				backoffs[idx] = getRandom(Rs[idx]) + 1;
			}
			for (int i = 0; i < num_nodes; i++) {
				backoffs[i] -= 1;
			}
		}

	}
}

void output_stats(char* filename) {
	FILE * fpout;

	fpout = fopen(filename, "a");

	//original
	fprintf(fpout, "Channel utilization (in percentage) %%%f\n", 100 * (1.0 * channel_occu_time) / (1.0 * simu_time));
	fprintf(fpout, "Channel idle fraction (in percentage) %%%f\n", 100 * (1.0 * unused_time) / (1.0 * simu_time));
	fprintf(fpout, "Total number of collisions %d\n", total_collision);
	fprintf(fpout, "Variance in number of successful transmissions (across all nodes) %f\n", calculateVariance(success_transmits));
	fprintf(fpout, "Variance in number of collisions (across all nodes) %f\n", calculateVariance(collisions));


	//b
	// fprintf(fpout, "%d\n", num_nodes);
	// fprintf(fpout, "%f\n", (1.0 * unused_time) / (1.0 * simu_time));

	//c
	// fprintf(fpout, "%d\n", num_nodes);
	// fprintf(fpout, "%d\n", total_collision);

	//d
	// fprintf(fpout, "%d\n", ranges[0]);
	// fprintf(fpout, "%d\n", num_nodes);
	// fprintf(fpout, "%f\n", (1.0 * channel_occu_time) / (1.0 * simu_time));

	//e
	// fprintf(fpout, "%d\n", pckt_size);
	// fprintf(fpout, "%d\n", num_nodes);
	// fprintf(fpout, "%f\n", (1.0 * channel_occu_time) / (1.0 * simu_time));

	//fprintf(fpout, "%d\n", num_nodes);
	//fprintf(fpout, "%d\n", channel_occu_time);
	//fprintf(fpout, "%d\n", channel_idle_time);
	//fprintf(fpout, "%d\n", unused_time);
	//fprintf(fpout, "%d\n", total_collision);
	//fprintf(fpout, "%d\n", transmitted_pckt);
	fprintf(fpout, "\n");
	fclose(fpout);
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fclose(fpOut);

    readInput(argv[1]);

    cout << "------Start simulation------" << endl;

	//original
	//statistics for analysis
	channel_idle_time = 0;
	channel_occu_time = 0;
	total_collision = 0;
	transmitted_pckt = 0;
	unused_time = 0;

	run_simulation();
	output_stats("output.txt");
    
    //uncomment for test c
  //   for (int n = 10; n <= 500; n += 10) {
  //   	//statistics for analysis
  //   	channel_idle_time = 0;
		// channel_occu_time = 0;
		// total_collision = 0;
		// transmitted_pckt = 0;
		// unused_time = 0;
		
		// //variable
		// num_nodes = n;

		// run_simulation();
		// output_stats("output.txt");
  //   }

    //uncomment for b
  //   for (int n = 10; n <= 500; n += 10) {
  //   	//statistics for analysis
  //   	channel_idle_time = 0;
		// channel_occu_time = 0;
		// total_collision = 0;
		// transmitted_pckt = 0;
		// unused_time = 0;
		
		// //variable
		// num_nodes = n;

		// run_simulation();
		// output_stats("output.txt");
  //   }

    //uncomment for test of r_start
 //    for (int r_start = 1; r_start <= 16; r_start *= 2) {
 //    	cout << "r_start: " << r_start << endl; 
 //    	ranges.clear();
 //    	int r = r_start;
 //    	for (int i = 0; i < 7; i++) {
 //    		ranges.push_back(r);
 //    		r *= 2;
 //    	}
	//     for (int n = 10; n <= 500; n += 10) {
	//     	//statistics for analysis
	//     	channel_idle_time = 0;
	// 		channel_occu_time = 0;
	// 		total_collision = 0;
	// 		transmitted_pckt = 0;
	// 		unused_time = 0;
			
	// 		//variable
	// 		num_nodes = n;

	// 		run_simulation();
	// 		output_stats("output.txt");
	//     }
	// }

	//psize
	// for (int p_len = 20; p_len <= 100; p_len += 20) {
 //    	cout << "p_len: " << p_len << endl; 
 //    	pckt_size = p_len;
	//     for (int n = 10; n <= 500; n += 10) {
	//     	//statistics for analysis
	//     	channel_idle_time = 0;
	// 		channel_occu_time = 0;
	// 		total_collision = 0;
	// 		transmitted_pckt = 0;
	// 		unused_time = 0;
			
			
	// 		//variable
	// 		num_nodes = n;

	// 		run_simulation();
	// 		output_stats("output.txt");
	//     }
	// }


    printf("Simulation finished.\n");
    cout << "channel_idle_time: " << channel_idle_time << endl;
    cout << "channel_occu_time: " << channel_occu_time << endl; 
    cout << "total_collision: " << total_collision << endl; 
    cout << "transmitted_pckt" << transmitted_pckt << endl;
    cout << "unused_time" << unused_time << endl;
    cout << "collisions: ";
	for (int i = 0; i < num_nodes; i++) {
		cout << collisions[i] << " ";
	}
	cout << endl;
	cout << "success transmits: ";
	for (int i = 0; i < num_nodes; i++) {
		cout << success_transmits[i] << " ";
	}
	cout << endl;
    return 0;
}