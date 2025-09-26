/*
        Original author of the starter code
        Tanzir Ahmed
        Department of Computer Science & Engineering
        Texas A&M University
        Date: 2/8/20

        Please include your Name, UIN, and the date below
        Name:
        UIN:
        Date:
*/
#include <sstream>

#include "FIFORequestChannel.h"
#include "common.h"

using namespace std;

int main(int argc, char *argv[]) {
    int opt;
    int p = 1;
    double t = 0.0;
    int e = 1;
    bool person_flag = false;
    bool time_flag = false;
    bool ecg_flag = false;
    bool file_flag = false;
    int buffercapacity = MAX_MESSAGE;
    bool create_flag = false;

    string filename = "";
    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p':
                p = atoi(optarg);
                person_flag = true;
                break;
            case 't':
                t = atof(optarg);
                time_flag = true;
                break;
            case 'e':
                e = atoi(optarg);
                ecg_flag = true;
                break;
            case 'f':
                filename = optarg;
                file_flag = true;
                break;
            case 'm':
                buffercapacity = atoi(optarg);
                break;
            case 'c':
                create_flag = true;
                break;
        }
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Forking error");
        exit(1);
    } else if (pid == 0) {  // child process becomes the server
        char *buf_string = new char[32];
        snprintf(buf_string, 32, "%d", buffercapacity);
        char *cmd[] = {(char *)"./server", (char *)"-m", buf_string, nullptr};
        execvp("./server", cmd);
        cerr << "exec failed\n";
        return 0;
    }

    char *buf = new char[buffercapacity];
    // parent continues as the client
    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel *runningChannel = &chan;
    FIFORequestChannel *new_chan = nullptr;

    if (create_flag) {
        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        runningChannel->cwrite(&m, sizeof(MESSAGE_TYPE));
        runningChannel->cread(buf, 30);
        string new_channel_name(buf);
        cout << "New channel created: " << new_channel_name << endl;

        new_chan = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
        runningChannel = new_chan;
    }

    if (person_flag && time_flag && ecg_flag) {
        datamsg x(p, t, e);  // message to be sent

        memcpy(buf, &x, sizeof(datamsg));
        runningChannel->cwrite(buf, sizeof(datamsg));  // question
        double reply;
        runningChannel->cread(&reply, sizeof(double));  // answer
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    } else if (person_flag && !time_flag && !ecg_flag) {
        datamsg x(p, t, e);
        double ecgno_ans_1;
        double ecgno_ans_2;
        stringstream ss;

        FILE *fp = fopen("received/x1.csv", "a");
        for (int i = 0; i < 1000; i++) {
            memcpy(buf, &x, sizeof(datamsg));
            runningChannel->cwrite(buf, sizeof(datamsg));
            runningChannel->cread(&ecgno_ans_1, sizeof(double));

            x.ecgno = 2;
            memcpy(buf, &x, sizeof(datamsg));
            runningChannel->cwrite(buf, sizeof(datamsg));
            runningChannel->cread(&ecgno_ans_2, sizeof(double));

            ss.str("");
            ss.clear();
            ss << x.seconds << ',' << ecgno_ans_1 << ',' << ecgno_ans_2 << '\n';
            string line = ss.str();
            fwrite(line.c_str(), 1, line.size(), fp);

            x.seconds += 0.004;
            x.ecgno = 1;
        }

    } else if (file_flag) {
        filemsg fm(0, 0);
        string fname = filename;
        int len = sizeof(filemsg) + (fname.size() + 1);
        memcpy(buf, &fm, sizeof(filemsg));
        strcpy(buf + sizeof(filemsg), fname.c_str());
        runningChannel->cwrite(buf, len);  // I want the file length;

        __int64_t size;
        runningChannel->cread(&size, sizeof(__int64_t));

        // __int64_t size = get_file_size(filename); // This might be the issue IT WAS THE FUCKING ISSUE OMG
        cout << "The size of the file " << filename << " is " << size << " bytes" << endl;

        __int64_t start = 0;
        __int64_t end = buffercapacity;

        filename = "received/" + filename;
        FILE *fp = fopen(filename.c_str(), "wb");

        if (!fp) {
            cerr << "Client received request for file: " << filename << " which cannot be created" << endl;
            return 0;
        }

        while (start < size) {
            // cout << start << " : " << end << endl;
            // Setting Message
            end = min(end, size);  // To make sure the last chunk only read as much as needed
            fm.offset = start;
            fm.length = end - start;

            // Setting Buffer
            int len = sizeof(filemsg) + (fname.size() + 1);
            memcpy(buf, &fm, sizeof(filemsg));
            strcpy(buf + sizeof(filemsg), fname.c_str());
            // cout << fname << endl;
            // for (int i = 0; i < MAX_MESSAGE; i++)
            // {
            // 	cout << buf[i];
            // }
            runningChannel->cwrite(buf, len);            // send request
            runningChannel->cread(buf, end - start);     // read reply
            fwrite(buf, 1, end - start, fp);  // write to file

            // Update Variables
            start = end;
            end += buffercapacity;
        }
    }

    delete[] buf;
    
    // closing the channel
    MESSAGE_TYPE m = QUIT_MSG;
    runningChannel->cwrite(&m, sizeof(MESSAGE_TYPE));
    if(create_flag){
        delete runningChannel;
    }
    return 0;
}
