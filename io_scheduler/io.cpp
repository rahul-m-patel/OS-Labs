#include <iostream>
#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

int MAX_OPERATIONS = 10000;
int NUM_OPERATIONS = 0;
int CURRENT_OP = 0;
int ACTIVE_IO = -1;
long int CURRENT_TIME = 0;
long int CURRENT_TRACK = 0;
int DIRECTION = 0; //1 up, 0 down
int TOTAL_TIME = 0;
int TOTAL_MOVEMENT = 0;

typedef struct{
    int io_id;
    int arrival;
    int start_time;
    int end_time;
    long int track_no;
}io_t;

vector<io_t> io_operations;

class Scheduler{
    public:
        virtual int get_next_io() = 0;
        virtual void add_io(io_t io_op) = 0;
        virtual bool is_empty() = 0;
};

class FIFO : public Scheduler{
    private:
        queue<io_t> io_q;
    public:
        virtual int get_next_io(){
            int io_num = io_q.front().io_id;
            io_q.pop();
            return io_num;
        }
        virtual void add_io(io_t io_op){
            io_q.push(io_op);
        }
        virtual bool is_empty(){
            return io_q.empty();
        }  
};

class SSTF : public Scheduler{
    protected:
        vector<io_t> io_q;
    public:
        virtual int get_next_io(){
            int min_time = INT_MAX;
            int victim_index = -1;
            int victim = -1;
            for(int i=0;i<io_q.size();++i){
                if(abs(io_q[i].track_no - CURRENT_TRACK)<min_time){
                    min_time = abs(io_q[i].track_no - CURRENT_TRACK);
                    victim_index = i;
                    victim = io_q[i].io_id;
                }
            }
            io_q.erase(io_q.begin()+victim_index);
            return victim;
        }
        virtual void add_io(io_t io_op){
            io_q.push_back(io_op);
        }
        virtual bool is_empty(){
            return io_q.empty();
        } 
};

class LOOK : public SSTF{
    public:
        virtual int get_next_io(){
            int nearest_lower = INT_MIN;
            int nearest_upper = INT_MAX;
            int lower_index = -1;
            int upper_index = -1;
            for(int i=0;i<io_q.size();++i){
                if(io_q[i].track_no >= CURRENT_TRACK){
                    if(nearest_upper>io_q[i].track_no){
                        nearest_upper=io_q[i].track_no;
                        upper_index = i;
                    }
                }
                if(io_q[i].track_no <= CURRENT_TRACK){
                    if(nearest_lower<io_q[i].track_no){
                        nearest_lower=io_q[i].track_no;
                        lower_index = i;
                    }
                }
            }
            // if(CURRENT_TIME==716){
            //     cout<<"CURRENT_TRACK:"<<CURRENT_TRACK<<" "<<nearest_lower<<" "<<nearest_upper<<"\n";
            //     for(int i=0;i<io_q.size();++i){
            //         cout<<io_q[i].io_id<<" "<<io_q[i].track_no<<"\n";
            //     }
            // }
            
            //cout<<nearest_lower<<" "<<nearest_upper<<" "<<io_q.size()<<" "<<DIRECTION<<" "<<lower_index<<" "<<upper_index<<" "<<CURRENT_TRACK<<"\n";
            //cout<<CURRENT_OP<<" "<<NUM_OPERATIONS<<"\n";
            int victim_index = -1;
            if(DIRECTION==1){
                if(nearest_upper!=INT_MAX)victim_index = upper_index;
                else victim_index = lower_index;
            }
            else if(DIRECTION==0){
                if(nearest_lower!=INT_MIN)victim_index = lower_index;
                else victim_index = upper_index;
            }
            int victim = io_q[victim_index].io_id;
            io_q.erase(io_q.begin()+victim_index);
            return victim;
        }
};

class CLOOK : public LOOK{
    public:
        virtual int get_next_io(){
            int lowest = INT_MAX;
            int nearest_upper = INT_MAX;
            int lowest_index = -1;
            int upper_index = -1;
            for(int i=0;i<io_q.size();++i){
                if(io_q[i].track_no >= CURRENT_TRACK){
                    if(nearest_upper>io_q[i].track_no){
                        nearest_upper=io_q[i].track_no;
                        upper_index = i;
                    }
                }
                if(io_q[i].track_no < lowest){
                    lowest = io_q[i].track_no;
                    lowest_index = i;
                }
            }
            int victim_index = -1;
            if(nearest_upper!=INT_MAX)victim_index=upper_index;
            else victim_index = lowest_index;
            int victim = io_q[victim_index].io_id;
            io_q.erase(io_q.begin()+victim_index);
            return victim;
        }
};

class FLOOK : public LOOK{
    private:
        vector<io_t> add_q;
    public:
        virtual int get_next_io(){
            if(io_q.empty())io_q.swap(add_q);
            return LOOK::get_next_io();
        }
        virtual void add_io(io_t io_op){
            if(ACTIVE_IO==-1)io_q.push_back(io_op);
            else add_q.push_back(io_op);
        }
        virtual bool is_empty(){
            return io_q.empty() && add_q.empty();
        }
};

void Simulate(Scheduler& sched){
    while(true){
        //cout<<CURRENT_OP<<" "<<NUM_OPERATIONS<<" "<<ACTIVE_IO<<" "<<CURRENT_TRACK<<" "<<CURRENT_TIME<<"\n";
        if(CURRENT_OP<NUM_OPERATIONS && io_operations[CURRENT_OP].arrival==CURRENT_TIME){
            sched.add_io(io_operations[CURRENT_OP]);
            CURRENT_OP++;
        }
        if(ACTIVE_IO!=-1 && io_operations[ACTIVE_IO].track_no==CURRENT_TRACK){
            io_operations[ACTIVE_IO].end_time=CURRENT_TIME;
            ACTIVE_IO=-1;
        }
        if(ACTIVE_IO==-1){
            if(!sched.is_empty()){
                ACTIVE_IO = sched.get_next_io();
                io_operations[ACTIVE_IO].start_time = CURRENT_TIME;
                //cout<<CURRENT_TIME<<":"<<ACTIVE_IO<<" "<<io_operations[ACTIVE_IO].track_no<<" "<<CURRENT_TRACK<<"\n";
                if(CURRENT_TRACK!=io_operations[ACTIVE_IO].track_no){
                    if(CURRENT_TRACK<io_operations[ACTIVE_IO].track_no)DIRECTION = 1;
                    else DIRECTION = 0;
                }
                else continue;
            }
            else{
                //cout<<ACTIVE_IO<<" "<<sched.is_empty()<<" "<<CURRENT_OP<<" "<<NUM_OPERATIONS<<"\n";
                if(CURRENT_OP==NUM_OPERATIONS){
                    TOTAL_TIME = CURRENT_TIME;
                    break;
                }
            }
        }
        if(ACTIVE_IO!=-1){
            if(DIRECTION==1)CURRENT_TRACK++;
            else CURRENT_TRACK--;
            TOTAL_MOVEMENT++;
        }
        CURRENT_TIME++;
    }
}

int main(int argc, char* argv[]){
    int c;
    char algo;
    while ((c = getopt (argc, argv, "s:")) != -1){
        switch(c)
        {
            case 's':
                algo = optarg[0];
                break;
            default:
                cout<<"Invalid argument";
                exit(1);
        }
    }
    Scheduler *sched = NULL;
    if(algo=='N'){
        sched = new FIFO();
    }
    else if(algo=='S'){
        sched = new SSTF();
    }
    else if(algo=='L'){
        sched = new LOOK();
    }
    else if(algo=='C'){
        sched = new CLOOK();
    }
    else if(algo=='F'){
        sched = new FLOOK();
    }
    else{
        cout<<"Invalid Algorithm\n";
        return 0;
    }
    ifstream input_file;
    stringstream file_line;
    input_file.open(argv[2]);
    string s;
    while(getline(input_file,s)){
        if(s[0]=='#')continue;
        file_line.clear();
        file_line.str(s);
        int arrival;
        long int track;
        file_line >> arrival >> track;
        io_t io{NUM_OPERATIONS,arrival,0,0,track};
        io_operations.push_back(io);
        NUM_OPERATIONS++;    
    }
    Simulate(*sched);
    unsigned long total_turnaround = 0;
    unsigned long total_wait = 0;
    unsigned long total_io = 0;
    int max_wait = 0;
    for(int i=0;i<NUM_OPERATIONS;++i){
        io_t& io_op = io_operations[i];
        printf("%5d: %5d %5d %5d\n", i, io_op.arrival, io_op.start_time, io_op.end_time);
        total_io += io_op.end_time - io_op.start_time;
        total_turnaround+=io_op.end_time - io_op.arrival;
        total_wait+=io_op.start_time - io_op.arrival;
        max_wait = max(max_wait, io_op.start_time - io_op.arrival);     
    }
    printf("SUM: %d %d %.4lf %.2lf %.2lf %d\n",TOTAL_TIME, TOTAL_MOVEMENT, (double)total_io/TOTAL_TIME, (double)total_turnaround/NUM_OPERATIONS, (double)total_wait/NUM_OPERATIONS, max_wait);
    return 0;
}