#include <iostream>
#include "thread.h"
#include "disk.h" 

#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <string>
#include <cmath>
#include <stdlib.h> 
#include<numeric>
#include <map>
#include <cassert>

using std::cout;
using std::endl;
using std::map;
using std::vector;
using std::deque;
using std::string;
using std::pair;
using std::min;

// map<int, cv> requesterCVs;
int total_requests = 0;
cv requesterCVs = cv();
cv servicerCV = cv();
int disc_num;
int max_queue;
vector<string> file_names;
mutex requesterMutex;
// mutex servicerMutex;
deque<pair<int, int>> request_queue;
vector<bool> threads_occupied;
vector<int> threads_availability; // important variant which decides if the servicer should wait for more requrests or not.

bool can_any_thread_join() {
    for (int i = 0; i < (int)threads_availability.size(); i++) {
        if (threads_availability[i]) {
            bool found = false;
            for (pair<int, int> pr: request_queue) {
                if (pr.first == i) {found = true; break;}
            }
            if (!found) return true;
        }
    }
    return false;
}

void requester(void *aaa) {
    intptr_t id = (intptr_t) aaa;
    std::ifstream infile(file_names[id]);
    // begin critical section
    requesterMutex.lock();
    
    // cout << "Begin to initiate id " << id << endl;
    int cur_track;
    // bool thread_occupied = false;
    while(infile >> cur_track) {
        while((int)request_queue.size() >= max_queue || threads_occupied[id]) {
            requesterCVs.wait(requesterMutex);
        }
        threads_occupied[id] = true; // prevent two track from the same file onto queue.
        request_queue.push_back({id, cur_track});
        // cout << "requester " << id << " track " << cur_track;
        // cout << "\n";
        print_request(id, cur_track);
        // cout << ". Queue size is " << request_queue.size() << endl;

        // cout << max_queue << endl;
        // need to signal the servicer
        if ((int)request_queue.size() == max_queue || !can_any_thread_join()) {
            // cout << "signal the servicer" << endl;
            servicerCV.signal();
            // cout << "after signal" << endl;
        }
    }
    
    // cout << id << " is done" << endl;
    threads_availability[id] = 0;
    // end critical section
    requesterMutex.unlock();
}

void servicer(void *bbb) {
    intptr_t servicer_track = (intptr_t) bbb;
    // begin critical section
    
    while(true) {
        requesterMutex.lock();
        // while ((int)request_queue.size() < max_queue && accumulate(threads_availability.begin(), threads_availability.end(), 0) >= max_queue) {
        while ((int)request_queue.size() < max_queue && can_any_thread_join()) {
            // cout << request_queue.size() << " is too small. Need to get " << min(max_queue, accumulate(threads_availability.begin(), threads_availability.end(), 0)) << endl;
            servicerCV.wait(requesterMutex);
        }
        // cout << "available_request left is " << accumulate(threads_availability.begin(), threads_availability.end(), 0) << endl;
        if ((int)request_queue.size() == 0) break;

        // search for the nearest track
        int min_track_diff = 10086;
        int target_request_it = -1;
        // cout << "Tracks to choose from: ";
        for (int counter = 0; counter < (int)request_queue.size(); counter++) {
            int tmp_diff = abs(request_queue[counter].second - servicer_track);
            // cout << request_queue[counter].second << " ";
            if (tmp_diff < min_track_diff) {
                min_track_diff = tmp_diff; 
                target_request_it = counter;
            }
        }
        // cout << endl;

        int service_id = request_queue[target_request_it].first;
        int service_id_track = request_queue[target_request_it].second;
        // cout << "service requester " << service_id << " track " << service_id_track << endl;
        print_service(service_id, service_id_track);
        servicer_track = service_id_track;
        threads_occupied[service_id] = false; // Very critical! Make sure the thread can send another requrest from now on.
        request_queue.erase(request_queue.begin() + target_request_it); // Erase the handled request.
        total_requests--; // decrease the total requests left.

        requesterCVs.broadcast(); // Wake up! You may hand in your request!
        // end critical section
        requesterMutex.unlock();
    }
    // cout << "还可以干" << endl;

}

void manageThreads(){
    for(intptr_t id = 0; id < disc_num; ++id){
        thread((thread_startfunc_t)requester, (void*) id);
    }
    thread((thread_startfunc_t)servicer, (void*) 0);
}

int main(int argc, char **argv)
{
    max_queue = atoi(argv[1]);
    // cout << "max_queue: " << max_queue << endl;
    disc_num = argc - 2;
    // cout << "argc: " << argc << endl;



    for (intptr_t id = 0; id < argc - 2; ++id) {
        // requesterCVs[id] = cv();
        file_names.push_back(string(argv[id + 2]));
        threads_occupied.push_back(false);
        threads_availability.push_back(1);
        std::ifstream infile(file_names[id]);
        int cur_track;
        while(infile >> cur_track) {
            total_requests++;
        }
    }
    // cout << total_requests << endl;
    cpu::boot((thread_startfunc_t) manageThreads, (void *) 0, 0);
}