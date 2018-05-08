
#include <iostream>
#include <mpi.h>
#include <vector>
#include <string>
#include <fstream>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <stdlib.h>

#define TAG_FIRST 1
#define TAG_ODD_SORT_STAGE 2
#define TAG_ODD_SORT_STAGE_BACK 3
#define TAG_EVEN_SORT_STAGE 4
#define TAG_EVEN_SORT_STAGE_BACK 5
#define TAG_LAST 6
#define FAKE_NUM 300 //300 because top limit is 255...

#define PROFILING false

using namespace std;

//merges, sorts and splits 2 sorted arrays
//alg similiar to merge sort (O(n))
void sortAndSplit(short left[], short right[], int size) {

	short full[size*2]; //full array
	int i = 0; //left subarray index
	int j = 0; //right subarray index
	int k = 0; //full array index

	while (i < size && j < size) {

		if (left[i] <= right[j]) {
			full[k] = left[i];
			i++;
		} else {
			full[k] = right[j];
			j++;
		}
		k++;
	}

	while (i < size) {
        full[k] = left[i];
        i++;
        k++;
    }

    while (j < size) {
        full[k] = right[j];
        j++;
        k++;
    }

    for (int l=0; l < 2*size; l++) {
    	if (l < size) {
    		left[l] = full[l];
    	} else {
    		right[l - size] = full[l];
    	}
    }
}

int main(int argc, char *argv[]) {

	int numCount;
	int processCount;
	int ID;
	int numPerProc;
	MPI_Status stat;
	std::chrono::time_point<std::chrono::high_resolution_clock> start, finish;
	vector<short> inputNumbers;
    short* inArray;
    short* myArray;
    short* rightArray;

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &processCount);
    MPI_Comm_rank(MPI_COMM_WORLD, &ID);

    if (ID == 0) { //root process: get input and distribute numbers to all procesors

	    //load file and save nums to vector
	    char fileName[] = "numbers";
	    int num;
	    ifstream inputFile;
	    inputFile.open(fileName);

	    while(inputFile.good()) {

	    	num = inputFile.get();
	    	if(!inputFile.good()) 
	    		break;

	    	if (!PROFILING)
	    		cout << num << " ";
	    	inputNumbers.push_back(num);
	    }
	    if (!PROFILING)
	    	cout << "\n";
	    inputFile.close();

        numCount = inputNumbers.size();
        int realLen = ((numCount / processCount) + 1) * processCount;
        inArray = (short*) malloc(realLen*sizeof(short));

        copy(inputNumbers.begin(), inputNumbers.end(), inArray);

    	//start measure time
    	if (PROFILING)
    		start = std::chrono::high_resolution_clock::now();

    	if (numCount % processCount == 0) //if nicely divisible
    		numPerProc = numCount / processCount;
    	else
    		numPerProc = numCount / processCount + 1;
    	if (numCount <= processCount) //if more processors than numbers
    		numPerProc = 1;

        if (numCount % processCount != 0) {
            for (int i = numCount; i < realLen; i++)
                inArray[i]=FAKE_NUM;
        }
    }

    //processors receiving their nums and sorting them sequentially
    //propagate n/p to everyone
    MPI_Bcast(&numPerProc, 1, MPI_INT, 0, MPI_COMM_WORLD);

    myArray = (short*) malloc(numPerProc*sizeof(short));
    rightArray = (short*) malloc(numPerProc*sizeof(short));

    //distribute array evenly to every proccess
    MPI_Scatter(inArray, numPerProc, MPI_SHORT, myArray, numPerProc, MPI_SHORT, 0, MPI_COMM_WORLD);

    //using optimal algorithm to sort sequentially (O(n*log n))
    sort(myArray, myArray + numPerProc);

    /*cout << "me: " << ID << ", mynums: ";

    for (int i = 0; i < numPerProc; i++) {
        cout << myArray[i] << ", ";
    }

    cout << "\n";*/

    for (int i = 0; i < ((processCount/2) + 1); i++) {

    	//odd proceses sort
    	//note: since indexing starts at 0, odd processors actually have even numbers (0, 2...) and vice versa (very confus)
    	if (ID % 2 == 1) { //even proccesses send to the left

    		//send nums
            MPI_Send(myArray, numPerProc, MPI_SHORT, ID - 1, TAG_ODD_SORT_STAGE, MPI_COMM_WORLD);
    		//wait for bigger nums
            MPI_Recv(myArray, numPerProc, MPI_SHORT, ID - 1, TAG_ODD_SORT_STAGE_BACK, MPI_COMM_WORLD, &stat);

    	} else if ((ID % 2 == 0) && (processCount - ID != 1)) { //odd procceses recieve data (except for last one, if exists)

            MPI_Recv(rightArray, numPerProc, MPI_SHORT, ID + 1, TAG_ODD_SORT_STAGE, MPI_COMM_WORLD, &stat);
    		//sort
    		sortAndSplit(myArray, rightArray, numPerProc);
            MPI_Send(rightArray, numPerProc, MPI_SHORT, ID + 1, TAG_ODD_SORT_STAGE_BACK, MPI_COMM_WORLD);
    	}

    	//even proccesses sort
    	if (ID % 2 == 0 && ID != 0) { //odd proccesses send to the left (except first)

    		//send nums
            MPI_Send(myArray, numPerProc, MPI_SHORT, ID - 1, TAG_EVEN_SORT_STAGE, MPI_COMM_WORLD);
            //wait for bigger nums
            MPI_Recv(myArray, numPerProc, MPI_SHORT, ID - 1, TAG_EVEN_SORT_STAGE_BACK, MPI_COMM_WORLD, &stat);

    	} else if ((ID % 2 == 1) && (processCount - ID != 1)) { //even procceses recieve data (except for last one, if exists)

    		MPI_Recv(rightArray, numPerProc, MPI_SHORT, ID + 1, TAG_EVEN_SORT_STAGE, MPI_COMM_WORLD, &stat);
            //sort
            sortAndSplit(myArray, rightArray, numPerProc);
            MPI_Send(rightArray, numPerProc, MPI_SHORT, ID + 1, TAG_EVEN_SORT_STAGE_BACK, MPI_COMM_WORLD);
    	}
    }

    //all proccses send their arrays to root
    MPI_Gather(myArray, numPerProc, MPI_SHORT, inArray, numPerProc, MPI_SHORT, 0, MPI_COMM_WORLD);

    if (ID == 0) {

    	if (PROFILING) {
    		auto finish = std::chrono::high_resolution_clock::now();
    		cout << std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count() << "\n";
    	}

    	for (int i = 0; i < numCount; i++) {
    		if (!PROFILING && inArray[i] != FAKE_NUM)
    		  cout << inArray[i] << "\n";
    	}
        free(inArray);
    }

    free(myArray);
    free(rightArray);

    MPI_Finalize(); 
    return 0;
}
