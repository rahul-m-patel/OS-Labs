#include <iostream>
#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

const int MAX_FRAMES = 128;
const int MAX_VMAS = 8;
const int MAX_VPAGES = 64;
const int MAX_PROCS = 10;
unsigned int NUM_FRAMES;
unsigned int NUM_PROCS;
bool VERBOSE_O = false;
bool VERBOSE_P = false;
bool VERBOSE_F = false;
bool VERBOSE_S = false;
ifstream input_file;
stringstream file_line;
unsigned long INSTRUCTION_COUNT = 0;
unsigned int CURRENT_PROCESS_PID = 0;
unsigned long long TOTAL_COST = 0;
unsigned long CONTEXT_SWITCHES = 0;
unsigned long PROCESS_EXITS = 0;
vector<int> RANDOM_VALUES;
vector<unsigned long> time_last_used;

typedef struct{
    unsigned int pid:4; //max 10 procs
    unsigned int vpage:6; //max 64 pages
    unsigned int mapping:1;
    unsigned int age;
}frame_t;

typedef struct{
    unsigned int starting_virtual_page:6; //max 64 pages
    unsigned int ending_virtual_page:6;
    unsigned int write_protected:1;
    unsigned int file_mapped:1;
}vma_t;

typedef struct{
    unsigned int valid:1; //presence in pagetable
    unsigned int referenced:1;
    unsigned int modified:1;
    unsigned int write_protected:1;
    unsigned int paged_out:1;
    unsigned int frame:7; //128 frames max
    unsigned int file_mapped:1;
    unsigned int zeros:17;
    unsigned int address_checked_earlier:1;
    unsigned int valid_address:1;
}pte_t;

typedef struct{
    vma_t VMAS[MAX_VMAS];
    pte_t PAGE_TABLE[MAX_VPAGES];
    unsigned int maps = 0;
    unsigned int unmaps = 0;
    unsigned long ins = 0;
    unsigned long outs = 0;
    unsigned int fins = 0;
    unsigned int fouts = 0;
    unsigned int zeros = 0;
    unsigned int segv = 0;
    unsigned int segprot = 0;
    unsigned int total_vmas = 0;
}process_t;

enum COSTS{
    read_write = 1,
    context_switch = 130,
    exit_cost = 1230,
    maps = 350,
    unmaps = 410,
    ins = 3200,
    outs = 2750,
    fins = 2350,
    fouts = 2800,
    zeros = 150,
    segv = 440,
    segprot = 410
};

frame_t FRAME_TABLE[MAX_FRAMES];
queue<unsigned int> FREE_POOL;
process_t PROCESS_POOL[MAX_PROCS];

class Pager{
    public:
        virtual unsigned int select_victim_frame() = 0;
};

class FIFO : public Pager{
    protected:
        unsigned int pointer = -1;
    public:
        virtual unsigned int select_victim_frame(){
            pointer++;
            pointer = pointer % NUM_FRAMES;
            return pointer;
        }
};

class RANDOM : public FIFO{
    public:
        virtual unsigned int select_victim_frame(){
            pointer++;
            pointer = pointer % RANDOM_VALUES.size();
            return RANDOM_VALUES[pointer] % NUM_FRAMES;
        }
};

class CLOCK : public FIFO{
    public:
        virtual unsigned int select_victim_frame(){
            pointer++;
            pointer = pointer % NUM_FRAMES;
            while(true){
                pte_t& PTE = PROCESS_POOL[FRAME_TABLE[pointer].pid].PAGE_TABLE[FRAME_TABLE[pointer].vpage];
                if(PTE.referenced){
                    PTE.referenced = 0;
                    pointer = (pointer+1) % NUM_FRAMES;
                }
                else break;
            }
            return pointer;
        }
};

class NRU : public FIFO{
    private:
        unsigned long prev_reset = -1;
    public:
        virtual unsigned int select_victim_frame(){            
            pointer++;
            pointer = pointer % NUM_FRAMES;
            vector<int> classes(4,-1);
            for(int i=0;i<NUM_FRAMES;++i){
                pte_t& PTE = PROCESS_POOL[FRAME_TABLE[pointer].pid].PAGE_TABLE[FRAME_TABLE[pointer].vpage];
                int class_no = 2*PTE.referenced + PTE.modified;
                if(classes[class_no]==-1)classes[class_no] = pointer;
                if(class_no == 0){
                    if(INSTRUCTION_COUNT - prev_reset < 50)return pointer;
                }
                if((INSTRUCTION_COUNT - prev_reset >= 50) && PTE.referenced)PTE.referenced = 0;
                
                pointer++;
                pointer = pointer % NUM_FRAMES;                
            }
            if(INSTRUCTION_COUNT - prev_reset >= 50)prev_reset = INSTRUCTION_COUNT;
            for(int i=0;i<4;++i){
                if(classes[i]!=-1){
                    pointer = classes[i];
                    return pointer;
                }
            }
            return pointer;
        }
};

class AGING : public FIFO{
    public:
      virtual unsigned int select_victim_frame(){
        pointer++;
        pointer = pointer % NUM_FRAMES;
        unsigned int min = pointer;
        for(int i=0;i<NUM_FRAMES;++i){
            FRAME_TABLE[pointer].age = FRAME_TABLE[pointer].age >> 1;
            pte_t& PTE = PROCESS_POOL[FRAME_TABLE[pointer].pid].PAGE_TABLE[FRAME_TABLE[pointer].vpage];
            if(PTE.referenced){
                PTE.referenced = 0;
                FRAME_TABLE[pointer].age = (FRAME_TABLE[pointer].age | 0x80000000);
            }
            if(FRAME_TABLE[pointer].age < FRAME_TABLE[min].age) min = pointer;
            pointer++;
            pointer = pointer % NUM_FRAMES;
        }
        pointer = min;
        return pointer;
    }  
};

class WORKINGSET : public FIFO{
    public:
        virtual unsigned int select_victim_frame(){
            pointer++;
            pointer = pointer % NUM_FRAMES;
            unsigned int min = pointer;
            for(int i=0;i<NUM_FRAMES;++i){
                pte_t& PTE = PROCESS_POOL[FRAME_TABLE[pointer].pid].PAGE_TABLE[FRAME_TABLE[pointer].vpage];
                if(PTE.referenced){
                    PTE.referenced = 0;
                    time_last_used[pointer] = INSTRUCTION_COUNT;
                }
                else if(PTE.referenced==0 && (INSTRUCTION_COUNT - time_last_used[pointer]) > 49) return pointer;
                else if(PTE.referenced==0 && (INSTRUCTION_COUNT - time_last_used[pointer]) <= 49){
                    if(time_last_used[pointer] < time_last_used[min])min = pointer;
                }
                pointer++;
                pointer = pointer % NUM_FRAMES;
            }
            pointer = min;
            return pointer;
        }
};

bool get_next_instruction(){
    string s;
    while(getline(input_file,s)){
        if(s[0]=='#')continue;
        file_line.clear();
        file_line.str(s);
        return true;
    }
    return false;
}

unsigned int get_frame(int vpage, Pager* THE_PAGER){
    unsigned int frame_no;
    if(FREE_POOL.empty()){
        frame_no = THE_PAGER->select_victim_frame();
        unsigned int victim_pid = FRAME_TABLE[frame_no].pid;
        unsigned int victim_vpage = FRAME_TABLE[frame_no].vpage;
        process_t& victim_proc = PROCESS_POOL[victim_pid];
        victim_proc.unmaps++;
        TOTAL_COST += COSTS::unmaps;
        pte_t& victim_pte = PROCESS_POOL[victim_pid].PAGE_TABLE[victim_vpage];
        victim_pte.valid = 0;
        if(VERBOSE_O)cout<<" UNMAP "<<victim_pid<<":"<<victim_vpage<<"\n";
        if(victim_pte.modified){
            if(victim_pte.file_mapped){
                victim_pte.paged_out = 0;
                victim_proc.fouts++;
                TOTAL_COST += COSTS::fouts;
                if(VERBOSE_O)cout<<" FOUT\n";
            }
            else{
                victim_pte.paged_out = 1;
                victim_proc.outs++;
                TOTAL_COST += COSTS::outs;
                if(VERBOSE_O)cout<<" OUT\n";
            }
        }
    }
    else{
        frame_no = FREE_POOL.front();
        FREE_POOL.pop();
    }
    
    return frame_no;
}

void simulate(Pager* THE_PAGER){
    while(get_next_instruction()){
        char operation;
        int vpage;
        file_line >> operation >> vpage;
        if(VERBOSE_O)cout << INSTRUCTION_COUNT<<": ==> "<<operation <<" "<<vpage<<"\n";
        
        
        if(operation == 'c'){
            CONTEXT_SWITCHES++;
            CURRENT_PROCESS_PID = vpage;
            TOTAL_COST += COSTS::context_switch;
        }
        else if(operation == 'e'){
            PROCESS_EXITS++;
            TOTAL_COST += COSTS::exit_cost;
            cout<<"EXIT current process "<<vpage<<"\n";
            for(int i=0;i<MAX_VPAGES;++i){
                pte_t& PTE = PROCESS_POOL[vpage].PAGE_TABLE[i];
                if(PTE.valid){
                    PTE.valid = 0;
                    FRAME_TABLE[PTE.frame].mapping = 0;
                    PROCESS_POOL[vpage].unmaps++;
                    TOTAL_COST += COSTS::unmaps;
                    FREE_POOL.push(PTE.frame);
                    if(VERBOSE_O)cout<<" UNMAP "<<vpage<<":"<<i<<"\n";
                    if(PTE.modified){
                        if(PTE.file_mapped){
                            PROCESS_POOL[vpage].fouts++;
                            TOTAL_COST += COSTS::fouts;
                            if(VERBOSE_O)cout<<" FOUT\n";
                        }
                    }
                }
                PTE.paged_out = 0;
            }
        }
        else if(operation == 'r' || operation == 'w'){
            process_t& CURRENT_RUNNING_PROCESS = PROCESS_POOL[CURRENT_PROCESS_PID];
            pte_t& PTE = CURRENT_RUNNING_PROCESS.PAGE_TABLE[vpage];
            PTE.referenced = 1;
            if(!PTE.valid){
                PTE.modified = 0;
                if(!PTE.address_checked_earlier){
                    for(int i=0;i<CURRENT_RUNNING_PROCESS.total_vmas;++i){
                        vma_t& vma = CURRENT_RUNNING_PROCESS.VMAS[i];
                        if(vpage >= vma.starting_virtual_page && vpage<= vma.ending_virtual_page){
                            PTE.valid_address = 1;
                            PTE.write_protected = vma.write_protected;
                            PTE.file_mapped = vma.file_mapped;
                            break;
                        }
                    }
                    PTE.address_checked_earlier = 1;
                }
                if(!PTE.valid_address){
                    CURRENT_RUNNING_PROCESS.segv++;
                    TOTAL_COST += COSTS::segv;
                    if(VERBOSE_O)cout<<" SEGV\n";
                }
                else{
                    
                    PTE.frame = get_frame(vpage, THE_PAGER);
                    PTE.valid = 1;
                    FRAME_TABLE[PTE.frame].pid = CURRENT_PROCESS_PID;
                    FRAME_TABLE[PTE.frame].mapping = 1;
                    FRAME_TABLE[PTE.frame].vpage = vpage;
                    FRAME_TABLE[PTE.frame].age = 0;
                    if(PTE.paged_out){
                        CURRENT_RUNNING_PROCESS.ins++;
                        TOTAL_COST += COSTS::ins;
                        if(VERBOSE_O)cout<<" IN\n";
                    }
                    else if(PTE.file_mapped){
                        CURRENT_RUNNING_PROCESS.fins++;
                        TOTAL_COST += COSTS::fins;
                        if(VERBOSE_O)cout<<" FIN\n";
                    }
                    else{
                        CURRENT_RUNNING_PROCESS.zeros++;
                        TOTAL_COST += COSTS::zeros;
                        if(VERBOSE_O)cout<<" ZERO\n";
                    }
                    CURRENT_RUNNING_PROCESS.maps++;
                    TOTAL_COST += COSTS::maps;
                    if(VERBOSE_O)cout<<" MAP "<<PTE.frame<<"\n";
                    
                }
            }
            
            TOTAL_COST += COSTS::read_write;
        
            if(operation == 'w'){
                if(PTE.write_protected){
                    PTE.modified = 0;
                    CURRENT_RUNNING_PROCESS.segprot++;
                    TOTAL_COST += COSTS::segprot;
                    if(VERBOSE_O)cout<<" SEGPROT\n";
                }
                else PTE.modified = 1;
            }
            
        }
        INSTRUCTION_COUNT++;
    }
}

int main(int argc, char* argv[]){
    int c;
    char algo;
    string options;
    while ((c = getopt (argc, argv, "f:a:o:")) != -1){
        switch(c)
        {
            case 'f':
                NUM_FRAMES = stoi(optarg);
                break;
            case 'a':
                algo = optarg[0];
                break;
            case 'o':
                options = optarg;
                break;
            default:
                cout<<"Invalid argument";
                exit(1);
        }
    }
    for(int i=0;i<options.length();++i){
        if(options[i]=='O')VERBOSE_O = true;
        else if(options[i]=='P')VERBOSE_P = true;
        else if(options[i]=='F')VERBOSE_F = true;
        else if(options[i]=='S')VERBOSE_S = true;
    }
    argv += optind;
    Pager* THE_PAGER=NULL;
    if(algo=='f'){
        THE_PAGER = new FIFO();
    }
    else if(algo=='r'){
        ifstream random_ifs;
        random_ifs.open(argv[1]);
        int r;
        random_ifs>>r;
        while(random_ifs>>r)RANDOM_VALUES.push_back(r);
        THE_PAGER = new RANDOM();
    }
    else if(algo=='c'){
        THE_PAGER = new CLOCK();
    }
    else if(algo=='e'){
        THE_PAGER = new NRU();
    }
    else if(algo=='a'){
        THE_PAGER = new AGING();
    }
    else if(algo=='w'){
        time_last_used.resize(NUM_FRAMES);
        THE_PAGER = new WORKINGSET();
    }
    input_file.open(argv[0]);
    if(get_next_instruction()){
        //int total_processes;
        file_line >> NUM_PROCS;
        for(int i=0;i<NUM_PROCS;++i){
            if(get_next_instruction()){
                int total_vmas;
                file_line >> total_vmas;
                PROCESS_POOL[i].total_vmas = total_vmas;
                for(int j=0;j<total_vmas;++j){
                    if(get_next_instruction()){
                        unsigned int starting_virtual_page, ending_virtual_page, write_protected, file_mapped;
                        file_line >> starting_virtual_page >> ending_virtual_page >> write_protected >> file_mapped;
                        vma_t vma{starting_virtual_page, ending_virtual_page, write_protected, file_mapped};
                        PROCESS_POOL[i].VMAS[j] = vma;
                    }
                    else{
                        cout<<"Expected VMAs";
                        return 0;
                    }
                }
            }
            else{
                cout<<"Expected number of VMAs";
                return 0;
            }
        }
    }
    else{
        cout << "Expected number of procs";
        return 0;
    }
    for(unsigned int i = 0; i<NUM_FRAMES; ++i){
        FREE_POOL.push(i);
    }

    simulate(THE_PAGER);
    if(VERBOSE_P){
        for(int i=0;i<NUM_PROCS;++i){     
            cout<<"PT["<<i<<"]:";
            for(int j=0;j<MAX_VPAGES;++j){
                pte_t& pte = PROCESS_POOL[i].PAGE_TABLE[j];
                if(!pte.valid)cout<<(pte.paged_out == 1 ? " #":" *");
                else cout<<" "<<j<<":"<<(pte.referenced == 1 ? "R":"-")<<(pte.modified == 1 ? "M":"-")<<(pte.paged_out == 1 ? "S":"-");
            }
            cout<<"\n";
        }
    }
    if(VERBOSE_F){
        cout<<"FT:";
        for(int i=0;i<NUM_FRAMES;++i){
            frame_t& frame = FRAME_TABLE[i];
            if(frame.mapping==0)cout<<" *";
            else cout<<" "<<frame.pid<<":"<<frame.vpage;
        }
        cout<<"\n";
    }
    if(VERBOSE_S){
        for(int i=0;i<NUM_PROCS;++i){ 
            process_t& proc = PROCESS_POOL[i];
            cout<<"PROC["<<i<<"]: U="<<proc.unmaps<<" M="<<proc.maps<<" I="<<proc.ins<<" O="<<proc.outs<<" FI="<<proc.fins<<" FO="<<proc.fouts<<" Z="<<proc.zeros<<" SV="<<proc.segv<<" SP="<<proc.segprot<<"\n";
        }
        printf("TOTALCOST %lu %lu %lu %llu %lu\n", INSTRUCTION_COUNT, CONTEXT_SWITCHES, PROCESS_EXITS, TOTAL_COST, sizeof(pte_t));
    }
    return 0;
}