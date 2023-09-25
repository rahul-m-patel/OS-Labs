#include <iostream>
#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

vector<int> randvals; //vector to store random numbers
int random_offset = 0; //index of random numbers vector
int event_count = 0;
int CURRENT_TIME = 0;
bool CALL_SCHEDULER = false;
int LAST_IO_TIME = 0;
int TOTAL_IO = 0;
bool is_MLFQ = false; //indicates whether scheduler is PRIO or PREPRIO
bool VERBOSE = false;
int completed_events = 0;

int my_random(int burst){ return 1 + (randvals[(random_offset++) % randvals.size()] % burst); }


class Process{
    public:
        int pid;
        int static_priority;
        int arrival_time;
        int total_CPU;
        int max_CPU_burst;
        int max_IO_burst;
        int state_ts; //current state timestamp
        int remaining_total;
        int remaining_cpu_burst;
        int dynamic_priority;
        int io_time;
        int end_time;
        int wait_time;
        int finished_events;
       
        Process(int pid, int static_priority, int arrival_time, int total_CPU, int max_CPU_burst, int max_IO_burst):
        pid(pid), static_priority(static_priority), arrival_time(arrival_time), total_CPU(total_CPU),
        max_CPU_burst(max_CPU_burst), max_IO_burst(max_IO_burst), state_ts(arrival_time), remaining_total(total_CPU), remaining_cpu_burst(0), dynamic_priority(static_priority-1),
        io_time(0),end_time(-1),wait_time(0),finished_events(-1)
        {

        }
};

Process *CURRENT_RUNNING_PROCESS = NULL;
enum STATES { CREATED, READY, RUNNING, BLOCK};
enum TRANSITIONS { TRANS_TO_READY, TRANS_TO_PREEMPT, TRANS_TO_RUN, TRANS_TO_BLOCK};
static const char *states_str[] = {"CREATED","READY","RUNNG","BLOCK"};

struct sortProcess{
    bool operator() (Process* p1, Process* p2){
        return p1->pid < p2->pid;
    }
};
set<Process*,sortProcess> completed_procs;

class Event{
    public:
        int eid;
        Process *process;
        STATES old_state;
        STATES new_state;
        TRANSITIONS transition;
        int timestamp;
    
        Event(int eid, Process *process, STATES old_state, STATES new_state, TRANSITIONS transition, int timestamp):
        eid(eid), process(process), old_state(old_state), new_state(new_state), transition(transition), timestamp(timestamp)
        {

        }
};

struct sortEvents{
    bool operator() (const Event* event1, const Event* event2){
        if(event1->timestamp == event2->timestamp)return event1->eid < event2->eid;
        return event1->timestamp < event2->timestamp;
    }
};

set<Event*, sortEvents> event_Q;

void create_eventQ(string inputfile, int max_priority){
    ifstream ifs(inputfile);
    int arrival_time, total_CPU, CPU_burst, IO_burst;
    int pid = 0;
    while(ifs >> arrival_time >> total_CPU >> CPU_burst >> IO_burst){
        Process *process = new Process(pid++, my_random(max_priority), arrival_time, total_CPU, CPU_burst, IO_burst);
        Event *event = new Event(event_count++, process, STATES::CREATED, STATES::READY, TRANSITIONS::TRANS_TO_READY,arrival_time);
        event_Q.insert(event);
    }
    ifs.close();
}

Event* get_event(){
    if(event_Q.empty())return NULL;
    Event* event = *(event_Q.begin());
    event_Q.erase(event_Q.begin());
    return event;
}

void insert_event(Event* event){
    event_Q.insert(event);
}

int get_next_event_time(){
    if(event_Q.empty())return -1;
    return (*event_Q.begin())->timestamp;
}

class Scheduler{
    public:
        int quantum;
        bool preempt;

        Scheduler(int quantum, bool preempt = false): quantum(quantum), preempt(preempt) {}
        virtual void add_process(Process *process) = 0;
        virtual Process* get_next_process() = 0;
        bool does_preempt(){
            return preempt;
        }
};

class FCFS_RR : public Scheduler{
    public:
        queue<Process*> process_Q;

        FCFS_RR() : Scheduler(10000) {}
        FCFS_RR(int quantum) : Scheduler(quantum) {}
        void add_process(Process *process){
            process_Q.push(process);
        }
        Process* get_next_process(){
            if(process_Q.empty())return NULL;
            Process *process = process_Q.front();
            process_Q.pop();
            return process;
        }

};

class LCFS : public Scheduler{
    public:
        stack<Process*> process_Q;

        LCFS() : Scheduler(10000) {}
        void add_process(Process *process){
            process_Q.push(process);
        }
        Process* get_next_process(){
            if(process_Q.empty())return NULL;
            Process *process = process_Q.top();
            process_Q.pop();
            return process;
        }

};

struct sortSRTF{
    bool operator() (Process* p1, Process* p2){
        if(p1->remaining_total == p2->remaining_total)return p1->finished_events > p2->finished_events;
        return p1->remaining_total > p2->remaining_total;
    }
};

class SRTF : public Scheduler{
    public:
        priority_queue<Process*, vector<Process*>, sortSRTF> process_Q;

        SRTF() : Scheduler(10000) {}
        void add_process(Process *process){
            process_Q.push(process);
        }
        Process* get_next_process(){
            if(process_Q.empty())return NULL;
            Process *process = process_Q.top();
            process_Q.pop();
            return process;
        }
};

class MLFQ : public Scheduler{
    public:
        vector<queue<Process*>> Q1;
        vector<queue<Process*>> Q2;
        int max_priority = 4;
        MLFQ(int quantum, bool preempt) : Scheduler(quantum, preempt) { Q1.resize(max_priority); Q2.resize(max_priority); }
        MLFQ(int quantum, bool preempt , int maxprio) : Scheduler(quantum, preempt) { max_priority = maxprio; Q1.resize(maxprio); Q2.resize(maxprio); }
        void add_process(Process *process){
            if(process->dynamic_priority >= 0){
                Q1[Q1.size() - 1 - process->dynamic_priority].push(process);
            }
            else{     
                process->dynamic_priority = process->static_priority - 1;
                Q2[Q2.size() - 1 - process->dynamic_priority].push(process);
            }
        }
        void print_mlfq(){
            for(int i =0;i<Q1.size();++i){
                queue<Process*> temp = Q1[i];
                while(!temp.empty()){
                    cout<<"Q1:"<<i<<" "<<temp.front()->pid<<endl;
                    temp.pop();
                }
            }
            for(int i =0;i<Q2.size();++i){
                queue<Process*> temp = Q2[i];
                while(!temp.empty()){
                    cout<<"Q2:"<<i<<" "<<temp.front()->pid<<endl;
                    temp.pop();
                }
            }
        }
        Process* get_next_process(){
            for(int i=0;i<2;++i){
                for(auto it = Q1.begin(); it!=Q1.end(); ++it){
                    if(!(it->empty())){
                        Process* proc = it->front();
                        it->pop();
                        return proc;
                    }
                }
                Q1.swap(Q2);
            }
            return NULL;
        }
};

void Simulation(Scheduler &scheduler){
    Event *event;
    while( event = get_event() ){
        
        Process *process = event->process;
        CURRENT_TIME = event->timestamp;
        int transition = event->transition;
        //cout<<"Process pid:"<<process->pid<<" Event transition:"<<event->transition<<"timestamp"<<CURRENT_TIME<<"\n";
        int timeInPrevState = CURRENT_TIME - process->state_ts;
        process->state_ts = CURRENT_TIME;
        process->finished_events = completed_events;
        string old_state = states_str[event->old_state];
        string new_state = states_str[event->new_state];
        delete event;
        event = NULL;

        switch(transition){
            case TRANS_TO_READY:
                if(VERBOSE)cout<<CURRENT_TIME<<" "<<process->pid<<" "<<timeInPrevState<<": "<<old_state<<" -> "<<new_state<<"\n";
                process->dynamic_priority = process->static_priority - 1;
                scheduler.add_process(process);
                if(scheduler.does_preempt() && CURRENT_RUNNING_PROCESS!=NULL){
                    auto pending_event = event_Q.end();
                    for(auto it = event_Q.begin(); it!=event_Q.end(); ++it){
                        if((*it)->process->pid == CURRENT_RUNNING_PROCESS->pid){
                            pending_event = it;
                            break;
                        }
                    }
                    if(pending_event!=event_Q.end()){
                        int pending_time  = (*pending_event)->timestamp;
                        bool decision = (process->dynamic_priority > CURRENT_RUNNING_PROCESS->dynamic_priority);
                        if(VERBOSE)cout<< "---> PRIO preemption "<<CURRENT_RUNNING_PROCESS->pid<< " by "<<process->pid<<" ? "<<decision<<" TS="<<pending_time<<" now="<< CURRENT_TIME<< ") --> ";
                        if(decision && (pending_time != CURRENT_TIME)){
                            if(VERBOSE)cout<<"YES\n";
                            CURRENT_RUNNING_PROCESS->remaining_total+=(pending_time-CURRENT_TIME);
                            CURRENT_RUNNING_PROCESS->remaining_cpu_burst+=(pending_time-CURRENT_TIME);
                            //cout<<"PID:"<<CURRENT_RUNNING_PROCESS->pid<<" Remaning CPU:"<<CURRENT_RUNNING_PROCESS->remaining_total<<" Remainifn CPU burst:"<<CURRENT_RUNNING_PROCESS->remaining_cpu_burst<<"\n";
                            event_Q.erase(pending_event); //some output statements here
                            Event* evnt = new Event(event_count++,CURRENT_RUNNING_PROCESS,STATES::RUNNING,STATES::READY,TRANSITIONS::TRANS_TO_PREEMPT,CURRENT_TIME);
                            insert_event(evnt);
                        }
                        else {
                            if(VERBOSE)cout<<"NO\n";
                        }
                    }
                }
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_PREEMPT:
                if(VERBOSE)cout<<CURRENT_TIME<<" "<<process->pid<<" "<<timeInPrevState<<": "<<old_state<<" -> "<<new_state<<" cb="<<to_string(process->remaining_cpu_burst)<<" rem="<<to_string(process->remaining_total)<<" prio="<<to_string(process->dynamic_priority)<<"\n";
                if(is_MLFQ)process->dynamic_priority--;
                scheduler.add_process(process);
                CURRENT_RUNNING_PROCESS = NULL;
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_RUN: {
                int cpu_burst = process->remaining_cpu_burst;
                if(cpu_burst <= 0)cpu_burst = my_random(process->max_CPU_burst);
                if(process->remaining_total < cpu_burst) cpu_burst = process->remaining_total;
                if(VERBOSE)cout<<CURRENT_TIME<<" "<<process->pid<<" "<<timeInPrevState<<": "<<old_state<<" -> "<<new_state<<" cb="<<to_string(cpu_burst)<<" rem="<<to_string(process->remaining_total)<<" prio="<<to_string(process->dynamic_priority)<<"\n";
                if(cpu_burst > scheduler.quantum){
                    int quant = scheduler.quantum;
                    process->remaining_total -= quant;
                    process->remaining_cpu_burst = cpu_burst - quant;
                    //cout<<"Process pid:"<<process->pid<<"\n";
                    Event* evnt = new Event(event_count++,process,STATES::RUNNING,STATES::READY,TRANSITIONS::TRANS_TO_PREEMPT,CURRENT_TIME+quant);
                    insert_event(evnt);
                    
                }
                else{
                    process->remaining_total -= cpu_burst;
                    process->remaining_cpu_burst = 0;
                    Event* evnt = new Event(event_count++,process,STATES::RUNNING,STATES::BLOCK,TRANSITIONS::TRANS_TO_BLOCK,CURRENT_TIME+cpu_burst);
                    insert_event(evnt);
                    
                }
                break;
            }
            case TRANS_TO_BLOCK: {
                int io_burst = 0;
                if(process->remaining_total > 0) io_burst = my_random(process->max_IO_burst);
                process->io_time += io_burst;
                if(LAST_IO_TIME < CURRENT_TIME + io_burst){
                    if(LAST_IO_TIME < CURRENT_TIME)TOTAL_IO += io_burst;
                    else TOTAL_IO += (CURRENT_TIME + io_burst - LAST_IO_TIME); 
                    LAST_IO_TIME = CURRENT_TIME + io_burst;
                }
                
                if(process->remaining_total > 0){
                    if(VERBOSE)cout<<CURRENT_TIME<<" "<<process->pid<<" "<<timeInPrevState<<": "<<old_state<<" -> "<<new_state<<" ib="<<to_string(io_burst)<<" rem="<<to_string(process->remaining_total)<<"\n";
                    Event *evnt = new Event(event_count++,process,STATES::BLOCK,STATES::READY,TRANSITIONS::TRANS_TO_READY,CURRENT_TIME+io_burst);
                    insert_event(evnt);
                    
                }
                else{
                    if(VERBOSE)cout<<CURRENT_TIME<<" "<<process->pid<<" "<<timeInPrevState<<": Done\n";
                    process->end_time = CURRENT_TIME;
                    completed_procs.insert(process);
                }
                CURRENT_RUNNING_PROCESS = NULL;
                CALL_SCHEDULER = true ;
                break;
            }
        }
        completed_events++;
        if(CALL_SCHEDULER){
            if(get_next_event_time() == CURRENT_TIME)continue;
            CALL_SCHEDULER = false;
            if(CURRENT_RUNNING_PROCESS == NULL){
                
                //if(is_MLFQ)scheduler.print_mlfq();

                CURRENT_RUNNING_PROCESS = scheduler.get_next_process();
                if(CURRENT_RUNNING_PROCESS == NULL)continue;
                //cout<<"Process pid:"<<CURRENT_RUNNING_PROCESS->pid<<"\n";
                CURRENT_RUNNING_PROCESS->wait_time += CURRENT_TIME - CURRENT_RUNNING_PROCESS->state_ts;
                Event* evnt = new Event(event_count++,CURRENT_RUNNING_PROCESS,STATES::READY,STATES::RUNNING,TRANSITIONS::TRANS_TO_RUN,CURRENT_TIME);
                insert_event(evnt);
                
            }
        }
    }
}


int main(int argc, char* argv[]){
    if(argc<3){
        cout<<"Less number of arguments";
        exit(1);
    }
    string inputfile = argv[2];
    string randfile = argv[3];
    
    ifstream ifs(randfile);
    int temp;
    ifs >> temp; //this is no. of random numbers in randfile
    randvals.resize(temp,-1);
    int i = 0;
    while(ifs>>temp)randvals[i++]=temp;
    ifs.close();
    
    int max_priority = 4;

    int c;
    string cval;
    while ((c = getopt (argc, argv, "s:")) != -1){
        switch(c)
        {
            case 's':
                cval = optarg;
                break;
            default:
                cout<<"Invalid argument";
                exit(1);
        }
    }
    if(cval[0]=='F'){
        create_eventQ(inputfile,max_priority);
        Scheduler *scheduler = new FCFS_RR();
        Simulation(*scheduler);
        cout<<"FCFS\n";
    }
    else if(cval[0]=='L'){
        create_eventQ(inputfile,max_priority);
        Scheduler *scheduler = new LCFS();
        Simulation(*scheduler);
        cout<<"LCFS\n";
    }
    else if(cval[0]=='S'){
        create_eventQ(inputfile,max_priority);
        Scheduler *scheduler = new SRTF();
        Simulation(*scheduler);
        cout<<"SRTF\n";
    }
    else if(cval[0]=='R'){
        create_eventQ(inputfile,max_priority);
        int quantum = stoi(cval.substr(1,cval.length()-1));
        Scheduler *scheduler = new FCFS_RR(quantum);
        Simulation(*scheduler);
        cout<<"RR "<<quantum<<"\n";
    }
    else if(cval[0]=='P'){
        is_MLFQ = true;
        if(cval.find(':')==string::npos){
            create_eventQ(inputfile,max_priority);
            int quantum = stoi(cval.substr(1,cval.length()-1));
            Scheduler *scheduler = new MLFQ(quantum,false);
            Simulation(*scheduler);
            cout<<"PRIO "<<quantum<<"\n";
        }
        else{
            auto colon = cval.find(':');
            max_priority = stoi(cval.substr(colon+1,cval.length()-colon-1));
            create_eventQ(inputfile,max_priority);
            int quantum = stoi(cval.substr(1,colon-1));
            Scheduler *scheduler = new MLFQ(quantum,false,max_priority);
            Simulation(*scheduler);
            cout<<"PRIO "<<quantum<<"\n";
        }
    }
    else if(cval[0]=='E'){
        is_MLFQ = true;
        if(cval.find(':')==string::npos){
            create_eventQ(inputfile,max_priority);
            int quantum = stoi(cval.substr(1,cval.length()-1));
            Scheduler *scheduler = new MLFQ(quantum,true);
            Simulation(*scheduler);
            cout<<"PREPRIO "<<quantum<<"\n";
        }
        else{
            auto colon = cval.find(':');
            max_priority = stoi(cval.substr(colon+1,cval.length()-colon-1));
            create_eventQ(inputfile,max_priority);
            int quantum = stoi(cval.substr(1,colon-1));
            Scheduler *scheduler = new MLFQ(quantum,true,max_priority);
            Simulation(*scheduler);
            cout<<"PREPRIO "<<quantum<<"\n";
        }
    }
    else{
        cout<<"Invalid scheduler type";
        exit(1);
    }

    int finish_time = 0, total_cpu_used = 0, total_wait = 0, total_turnaround = 0;
    int no_of_procs = completed_procs.size();
    while(!completed_procs.empty()){
        Process *process = *(completed_procs.begin());
        completed_procs.erase(completed_procs.begin());
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", process->pid, process->arrival_time, process->total_CPU, process->max_CPU_burst, process->max_IO_burst,  process->static_priority, process->end_time, (process->end_time - process->arrival_time), process->io_time, process->wait_time);   
        finish_time = max(finish_time,process->end_time);
        total_cpu_used += process->total_CPU;
        total_wait += process->wait_time;
        total_turnaround += (process->end_time - process->arrival_time);
        delete(process);
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", finish_time, (100.0 * (total_cpu_used/(double)finish_time)), (100.0 * (TOTAL_IO/(double)finish_time)), ((double)total_turnaround/no_of_procs), ((double)total_wait/no_of_procs), (100.0 * (no_of_procs/(double)finish_time)));
    return 0;
}