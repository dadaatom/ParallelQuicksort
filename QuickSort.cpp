#include <math.h>
#include <iostream>
#include <bitset>
#include <string>
#include <random>
#include <time.h>
#include <stdlib.h>
#include "mpi.h"
#include <stdio.h>


std::vector<int> B = std::vector<int>(0);

// This function is a recursive (sequential). //
void quickSort(std::vector<int>& list, int min, int max)
{
	if (min < max)
	{
		int pivot = list[max];
		int i = (min - 1);

		for (int j = min; j <= max - 1; j++)
		{
			if (list[j] < pivot)
			{
				i++;

				int temp = list[i];
				list[i] = list[j];
				list[j] = temp;
			}
		}

		int temp = list[i + 1];
		list[i + 1] = list[max];
		list[max] = temp;

		int index = i + 1;
		quickSort(list, min, index - 1);
		quickSort(list, index + 1, max);
	}
}

// This function is for display purposes only. //
std::string vectorToString(std::vector<int> list) {
	std::string str = "{";
	for (int i = 0; i < list.size(); i++) {
		str = str + std::to_string(list[i]);
		if (i < list.size() - 1) {
			str = str + ", ";
		}
	}
	return str + "}";
}

int main(int argc, char* argv[])
{
	int id = 0;
	int process_count = 0;
	int num_elements = 0;
	bool show_lists = false;
	double startTime = 0;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	MPI_Comm_size(MPI_COMM_WORLD, &process_count);

	std::cout << "Created process " << id << "!\n";

	std::vector<int> unsortedList;

	if (argc == 3 || argc == 2) {
		num_elements = atoi(argv[1]);
		if (argc == 3) {
			show_lists = (atoi(argv[2]) == 1);
		}
	}

	if (num_elements >= process_count) {
		if (id == 0) {
			// Generates a list of random values to be sorted. //
			std::cout << "Generating unsorted list with " << num_elements << " elements...\n";

			unsortedList = std::vector<int>(0);

			for (int i = 0; i < num_elements; i++) {
				unsortedList.push_back((num_elements * ((double)rand() / (double)RAND_MAX)));
			}

			if (show_lists) {
				std::cout << "List to sort: " << vectorToString(unsortedList) << "\n";
			}
		}

		// Tracks execution time. //
		if (id == 0) {
			startTime = MPI_Wtime();
		}

		if (log2(process_count) - ((int)log2(process_count)) == 0) {
			int dimensions = log2(process_count);
			if (id == 0) {
				// Distributes initial B lists accross all processes. //
				std::cout << "Scattering list to " << process_count << " processes...\n";

				int partitionSize = num_elements / process_count;
				for (int i = 0; i < process_count; i++) {

					int max = partitionSize;
					if (i == process_count - 1) {
						max = unsortedList.size() - (i * partitionSize);
					}

					std::vector<int> toSend = std::vector<int>(0);

					for (int j = 0; j < max; j++) {
						toSend.push_back(unsortedList[i * partitionSize + j]);
					}

					if (i == 0) {
						B = toSend;
					}
					else {
						int size = toSend.size();
						MPI_Send(&size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
						MPI_Send(&toSend[0], size, MPI_INT, i, 1, MPI_COMM_WORLD);
					}
				}
			}
			else {
				// Recieves initial list from master process (id = 0). //

				MPI_Status status;
				int recvSize = 0;

				MPI_Recv(&recvSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
				B.resize(recvSize);
				MPI_Recv(&B[0], recvSize, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
			}

			//std::cout << "Process " << id << " recieved: " << vectorToString(B) << "\n";
			if (id == 0) {
				std::cout << "Running sort on " << dimensions << " dimensional hypercube topology...\n";
			}

			// Executes hypercube quicksort algorithm from the textbook. //
			for (int i = 0; i < dimensions; i++) {
				MPI_Status status;

                //Two pivot selection strategies.
                //int pivot = B[std::floor(((B.size()-1) * ((double)rand() / (double)RAND_MAX)))]
                int pivot = B[std::floor(B.size()/2)];

				//MPI_Bcast(&pivot, 1, MPI_INT, 0, MPI_COMM_WORLD);

				std::vector<int> B1 = std::vector<int>(0);
				std::vector<int> B2 = std::vector<int>(0);

				for (int x : B) {
					if (x <= pivot) {
						B1.push_back(x);
					}
					else {
						B2.push_back(x);
					}
				}

				std::vector<int> toSend;
				int linkID = 0;

				if (((id >> (i)) & 1) == 0) {
					linkID = abs((id + pow(2, i)));

					toSend = B2;
					B = B1;
				}
				else {
					linkID = abs((id - pow(2, i)));

					toSend = B1;
					B = B2;
				}

				std::vector<int> C;

				int sendSize = toSend.size();
				int recvSize = 0;

				// Has to send / recieve in different orders to deadlocks. //
				if (((id >> (i)) & 1) == 0) {
					MPI_Send(&sendSize, 1, MPI_INT, linkID, 2, MPI_COMM_WORLD);
					if (sendSize > 0) {
						MPI_Send(&toSend[0], sendSize, MPI_INT, linkID, 3, MPI_COMM_WORLD);
					}

					MPI_Recv(&recvSize, 1, MPI_INT, linkID, 2, MPI_COMM_WORLD, &status);
					C = std::vector<int>(recvSize);
					if (recvSize > 0) {
						MPI_Recv(&C[0], recvSize, MPI_INT, linkID, 3, MPI_COMM_WORLD, &status);
					}
				}
				else {
					MPI_Recv(&recvSize, 1, MPI_INT, linkID, 2, MPI_COMM_WORLD, &status);
					C = std::vector<int>(recvSize);
					if (recvSize > 0) {
						MPI_Recv(&C[0], recvSize, MPI_INT, linkID, 3, MPI_COMM_WORLD, &status);
					}

					MPI_Send(&sendSize, 1, MPI_INT, linkID, 2, MPI_COMM_WORLD);
					if (sendSize > 0) {
						MPI_Send(&toSend[0], sendSize, MPI_INT, linkID, 3, MPI_COMM_WORLD);
					}
				}

				//std::cout << "          Process " << id << " linked with " << linkID << " on iteration " << i << "!\n";
				
				for (int x : C) {
					B.push_back(x);
				}
			}

			// Executes sequential quicksort on subset B. //
			quickSort(B, 0, B.size() - 1);

			if (id == 0) {
				// Gathers all sorted lists and merges before finally displaying. //
				std::cout << "Gathering lists...\n";

				std::vector<int> sorted = B;

				for (int i = 1; i < process_count; i++) {
					MPI_Status status;

					int size = 0;
					MPI_Recv(&size, 1, MPI_INT, i, 4, MPI_COMM_WORLD, &status);
					std::vector<int> subsorted = std::vector<int>(size);
					MPI_Recv(&subsorted[0], size, MPI_INT, i, 5, MPI_COMM_WORLD, &status);

					for (int x : subsorted) {
						sorted.push_back(x);
					}
				}

				double elapsedTime = MPI_Wtime() - startTime;

				if (show_lists) {
					std::cout << "Sorted: " << vectorToString(sorted) << "\n";
				}
				else {
					std::cout << "Sorted!\n";
				}

				std::cout << "Elapsed time: " << elapsedTime << " seconds!\n\n";

			}
			else {
				int sizeToSend = B.size();
				MPI_Send(&sizeToSend, 1, MPI_INT, 0, 4, MPI_COMM_WORLD);
				MPI_Send(&B[0], sizeToSend, MPI_INT, 0, 5, MPI_COMM_WORLD);
			}
		}
		else if (id == 0) {
			std::cout << "Number of processes cannot be structured into a n-dimensional hypercube!\n";
		}
	}
	else if (id == 0) {
		std::cout << "Too many processes have been assigned for a problem of this size!\n";
	}

	MPI_Finalize();
	return 0;
}
