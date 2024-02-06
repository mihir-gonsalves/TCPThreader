// your PA3 client code here
#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

// complete
void patient_thread_function (int p_no, int n, BoundedBuffer* request_buffer) {
    // functionality of the patient threads

    // For p patient, generate n data message objects requesting the 1st ECG value and place them in the request buffer
    // For these n data messages, the time starts at 0.0 and increments to 0.004 for each n value
    for (int i = 0; i < n; i++) {
        double msg_time = 0.004 * i;
        datamsg point(p_no, msg_time, EGCNO);
        char* buf = (char*) &point;
        request_buffer->push(buf, sizeof(datamsg));
    }
}

// complete
void file_thread_function (string filename, TCPRequestChannel *channel, BoundedBuffer* request_buffer) {
    // functionality of the file thread

    // 1. Get the file length (refer to PA1)
    filemsg fmessage(0, 0);
    string fname = filename;

    if (filename == "") {
        cout << "No file name given" << endl;
        return;
    }

    int len = sizeof(filemsg) + (fname.size() + 1);
	char* buf = new char[len]; // request buffer
	memcpy(buf, &fmessage, sizeof(filemsg));
	strcpy(buf + sizeof(filemsg), fname.c_str());
	channel->cwrite(buf, len);  // I want the file length;

	int64_t filelength = 0;
	channel->cread(&filelength, sizeof(int64_t)); // receive file length

    string fname_copy = fname;
    fname = "received/" + fname;

    // 2. Create the target file in the received director, and using fseek, set the offset to file length and close the file
        // a. For creation, use fopen() with mode "wb"
    FILE* file_pointer = fopen(fname.c_str(), "wb");
        // b. For setting the offset, use fseek()
    fseek(file_pointer, filelength, SEEK_SET);
        // c. For closing the file, use fclose()
    fclose(file_pointer);

    // Step 3: Create a file message request for the first chunk of data
    filemsg file_request(0, 0);

    while (file_request.offset < filelength) { // Step 6: Repeat until the end of the file is reached
        file_request.length = min(MAX_MESSAGE, static_cast<int>(filelength - file_request.offset)); // Step 5: Set the request length

        char* request = new char[sizeof(filemsg) + fname_copy.size() + 1]; // Allocate memory for the request
        memcpy(request, &file_request, sizeof(filemsg)); // Copy file message to the request buffer
        strcpy(request + sizeof(filemsg), fname_copy.c_str()); // Copy file name to the request buffer
        
        // Step 4: Push the file message request to the request buffer queue
        request_buffer->push(request, sizeof(filemsg) + fname_copy.size() + 1);

        // Step 5: Increment the offset for the next chunk to then place all these requests in the request buffer queue
        file_request.offset += file_request.length; 
    }
}

// complete
void worker_thread_function (TCPRequestChannel *channel, BoundedBuffer* request_buffer, BoundedBuffer* response_buffer) {    
    // functionality of the worker threads
    
    // Pops an element from the request buffer queue and identifies whether it is a data message, file message, or quit message request
    char buf[MAX_MESSAGE];
    request_buffer->pop(buf, MAX_MESSAGE);

    // Based on the message type, relevent action should be taken by the worker thread function
    MESSAGE_TYPE *message = (MESSAGE_TYPE*) buf;

    // forever loop
    while (true) {
        // If element is type data message
        if (*message == DATA_MSG) {
            // 1. Write the datamsg to the FIFO channel
            channel->cwrite(buf, sizeof(datamsg));
            // 2. Reads the response from the FIFO channel (double value)
            double response_data;
            channel->cread(&response_data, sizeof(double));
            // 3. Create a pair object with the key as the patient ID and the value as the response
                // a. You can create a pair using std::pair
            pair<int, double> data = {((datamsg*) buf)->person, response_data};
            // 4. Push the above pair to the response buffer
                // a. You can store the pair as pair<int, double> and put (char*) in front of it to cast it
            response_buffer->push((char*) &data, sizeof(data));
        }
        // If element is of type filemsg
        else if (*message == FILE_MSG) {
            // Get the filename from the message
            string filename = (char*) (((filemsg*) buf) + 1);
            // 1. Write the message to the FIFO channel
            channel->cwrite(buf, sizeof(filemsg) + filename.size() + 1);
            // 2. Reads the response from the FIFO channel (char array of size = m(MAX_MESSAGE))
            char *server_response = new char[MAX_MESSAGE]; // THIS NEEDS TO BE A POINTER OR ELSE TEST NINE WILL FAIL
            channel->cread(server_response, MAX_MESSAGE);
            // 3. Open the file using fopen() (use mode "rb+")
            string f_name = "received/" + filename;
            FILE* open_file = fopen(f_name.c_str(), "rb+");
            // 4. Set the seek to the offset
                // a. Get this offset from the file message object of step 1
            filemsg* f_msg = (filemsg*) buf;
            fseek(open_file, f_msg->offset, SEEK_SET);
            // 5. Write the response from step 2 to the opened file in step 3 using fwrite()
                // a. The size of the response is in the file message object of step 1
            fwrite(server_response, 1, f_msg->length, open_file);
            // 6. Close the file with fclose()
            fclose(open_file);
            delete[] server_response;
        }
        else if (*message == QUIT_MSG) {
        // If element is QUIT_MSG
            // 1. Writes the quit message to the FIFO channel
            channel->cwrite(message, sizeof(QUIT_MSG));
            delete channel;
            break;
        }

        // 2. Pops an element from the request buffer queue
        request_buffer->pop(buf, MAX_MESSAGE);
        message = (MESSAGE_TYPE*) buf;
    }
}

// complete
void histogram_thread_function (HistogramCollection* hc, BoundedBuffer* response_buffer) {
    // functionality of the histogram threads
    // Pops an element from the response buffer queue and updates the histogram collection object of that patient
    pair<int, double> data; // a. We know that the element is a pair
    
    // forever loop
    while (true){
        // 1. Pops an element from the response buffer
        response_buffer->pop((char*) &data, sizeof(data));
        
        // Break out of loop when all data points have been processed
        if (data.first < 0){
            break;
        }

        // 2. Since the key corresponds to the patient, invoke the update function from the histogram collection object to update the ecg value for that particular patient
        hc->update(data.first, data.second);
    }
}

int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f, host, port = "";	// name of file to be transferred
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
            case 'a':
                host = optarg;
                break;
            case 'r':
                port = optarg;
                break;
		}
	}
    
	// // fork and exec the server
    // int pid = fork();
    // if (pid == 0) {
    //     execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    // }
    
	// initialize overhead (including the control channel)
	TCPRequestChannel* chan = new TCPRequestChannel(host, port);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    // 1. Create w TCPRequestChannels
    // 2. Create all threads
        // a. Create 'w' worker threads and associate them to worker_thread_function
        // b. Create 'h' histogram threads and associate them to histogram_thread_function
        // c. For file request
            // i. Create 1 thread and associate it with file_thread_function
        // d. For data point requests
            // i. Create p threads and associate them to patient_thread_function

    vector<thread> p_threads;
    thread f_thread;
    vector<thread> w_threads;
    vector<thread> h_threads;
    vector<TCPRequestChannel*> channel;

        // e. Write a separate function for creating a new channel and calling that function w times in a for loop
    for(int i = 0; i < w; i++) {
        // // Send new_channel request to server
        // MESSAGE_TYPE new_channel = NEWCHANNEL_MSG;
        // chan->cwrite(&new_channel, sizeof(new_channel));
        // // Name new channel
        // chan->cread(channel_name, sizeof(channel_name));
        // TCPRequestChannel *newFIFOchannel = new TCPRequestChannel(channel_name, TCPRequestChannel::CLIENT_SIDE);
        // channel.push_back(newFIFOchannel);
        TCPRequestChannel* new_channel = new TCPRequestChannel(host, port);
        channel.push_back(new_channel);
        w_threads.push_back(thread(worker_thread_function, channel[i], &request_buffer, &response_buffer));
    }

    // Associate all threads (order (producer -> consumer): file, patient, worker, histogram)
    if (f != "") {
        p_threads.push_back(thread(file_thread_function, f, chan, &request_buffer));
    }
    else {
        // patient threads
        for (int i = 0; i < p; i++) {
            p_threads.push_back(thread(patient_thread_function, i + 1, n, &request_buffer));
        }
    }

    // histogram threads
    if (f == "") {
        for (int i = 0; i < h; i++) {
            h_threads.push_back(thread(histogram_thread_function, &hc, &response_buffer));
        }
    }

    // Join all threads
    if (f != "") {
        p_threads[0].join();
    } 
    else {
        // patient threads
        for (int i = 0; i < p; i++) {
            p_threads[i].join();
        }
    }

    // request buffer (for worker threads this will be the final entry and will help break out of the loop)
    for (int i = 0; i < w; i++) {
        MESSAGE_TYPE quit_msg = QUIT_MSG;
        request_buffer.push((char*) &quit_msg, sizeof(quit_msg));
    }
    // worker threads 
    for (int i = 0; i < w; i++) {
        w_threads[i].join();
    }
    
    // response buffer (for histogram threads this will be the final entry and will help break out of the loop)
    pair<int, double> data = {-1, -1};
    for (int i = 0; i < h; i++) {
        response_buffer.push((char*) &data, sizeof(data));
    }
    // histogram threads
    for (int i = 0; i < h; i++) {
        h_threads[i].join();
    }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);
}



























