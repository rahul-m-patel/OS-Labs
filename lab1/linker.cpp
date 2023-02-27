#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <typeinfo>
using namespace std;



vector<pair<string,int>> symbol_table;

void __parseerror(int errcode,int linenum,int lineoffset) {
    string errstr[] = {
    "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either
    "SYM_EXPECTED", // Symbol Expected
    "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R 
    "SYM_TOO_LONG", // Symbol Name is too long
    "TOO_MANY_DEF_IN_MODULE", // > 16 
    "TOO_MANY_USE_IN_MODULE", // > 16
    "TOO_MANY_INSTR" // total num_instr exceeds memory size (512) 
    };
    cout<<"Parse Error line "<<linenum<<" offset "<<lineoffset<<": "<<errstr[errcode]<<'\n';
    exit(0);
}

int readInt(string token,int lineno,int offset){
    int num;
    try{
        num = stoi(token);
    }
    catch(out_of_range exp){
        __parseerror(0,lineno,offset);
    }
    catch(invalid_argument exp){
        __parseerror(0,lineno,offset);
    }
    return num;
}

string readSymbol(string token, int lineno, int offset){
    
    if(token.length()<1)__parseerror(1,lineno,offset);
    if(token.length()>16)__parseerror(3,lineno,offset);
    if(!isalpha(token[0]))__parseerror(1,lineno,offset);
    for(int i=1;i<token.length();++i){
        if(!isalnum(token[i]))__parseerror(1,lineno,offset);
    }
    return token;
}

string readIAER(string token, int lineno, int offset){
    if(token.length()!=1 || (token[0]!='I' && token[0]!='A' && token[0]!='E' && token[0]!='R'))__parseerror(2,lineno,offset);
    return token;
}

bool symbolAlreadyDefined(string sym){ //check if symbol is present in symbol table
    for(auto it:symbol_table)
        if(it.first==sym)return true;
    return false;
}

bool variableAlreadyDefined(string var, vector<string> &multiple_variable){ //check if multiple variables defined
    for(auto it:multiple_variable)
        if(it==var)return true;
    return false;
}

void passOne(string filename){
    string line;
    ifstream myfile;
    myfile.open(filename);
    if (myfile)
    {
        int lineno = 1;
        int offset = 1;
        int prev_offset;
        int def_flag = 0;
        int use_flag = -1;
        int instr_flag = -1;
        int defcount;
        int usecount;
        int instrcount;
        string symbol;
        int base_address = 0;
        string addressingMode;
        vector<pair<string,int>> temp_sym_table;
        int module_num = 1;
        vector<string> multiple_variable;
        while(getline(myfile,line))
        {
            char *tochar = new char[line.length()+1];
            tochar[line.length()]='\0';
            for(int i=0;i<line.length();++i)tochar[i]=line[i];
            char *tokens = strtok(tochar," \t");
            while(tokens!=NULL){
                int token_length = strlen(tokens);
                char *curr_token = tokens;
                char *temp = strtok(NULL,"");
                tokens = strtok(temp," \t");
                if(def_flag==0){
                    defcount = readInt(string(curr_token),lineno,offset);
                    if(defcount>16)__parseerror(4,lineno,offset);
                    if(defcount>0){
                        def_flag = 1;
                    }
                    else{
                        def_flag = -1;
                        use_flag = 0;
                    }
                }
                else if(def_flag==1){
                    symbol = readSymbol(string(curr_token),lineno,offset);
                    def_flag = 2;
                }
                else if(def_flag==2){
                    int relative = readInt(string(curr_token),lineno,offset);
                    temp_sym_table.push_back(make_pair(symbol,relative));
                    --defcount;
                    if(defcount>0)def_flag = 1;
                    else if(defcount==0){
                        def_flag = -1;
                        use_flag = 0;
                    }
                } 
                else if(use_flag==0){
                    usecount = readInt(string(curr_token),lineno,offset);
                    if(usecount>16)__parseerror(5,lineno,offset);
                    if(usecount>0){
                        use_flag = 1;
                        
                    }
                    else{
                        use_flag = -1;
                        instr_flag = 0;
                    }
                }
                else if(use_flag==1){
                    symbol = readSymbol(string(curr_token),lineno,offset);
                    --usecount;
                    if(usecount==0){
                        use_flag = -1;
                        instr_flag = 0;
                    }
                }
                else if(instr_flag==0){
                    instrcount = readInt(string(curr_token),lineno,offset);
                    for(auto it:temp_sym_table){
                        if(symbolAlreadyDefined(it.first)){
                            //for(auto it2:symbol_table)
                            //    if(it2.first==it.first)cout<<"-----DEBUG-----\nAlready present at"<<it2.second<<"\n-----DEBUG-----\n";
                            cout<<"Warning: Module "<<module_num<<": "<<it.first<<" redefined and ignored\n";
                            multiple_variable.push_back(it.first);
                            continue;
                        }
                        if(it.second>=instrcount){
                            cout<<"Warning: Module "<<module_num<<": "<<it.first<<" too big "<<it.second<<" (max="<<(instrcount-1)<<") assume zero relative\n";
                            symbol_table.push_back(make_pair(it.first,base_address));
                        }
                        else symbol_table.push_back(make_pair(it.first,base_address+it.second));
                    }
                    base_address+=instrcount;
                    if(base_address>512)__parseerror(6,lineno,offset);
                    if(instrcount>0)instr_flag=1;
                    else{
                        instr_flag = -1;
                        def_flag = 0;
                        ++module_num;
                        temp_sym_table.clear();
                    }
                }
                else if(instr_flag==1){
                    addressingMode = readIAER(string(curr_token),lineno,offset);
                    instr_flag = 2;
                }
                else if(instr_flag==2){
                    int addr = readInt(string(curr_token),lineno,offset);
                    --instrcount;
                    if(instrcount>0)instr_flag=1;
                    else{
                        instr_flag = -1;
                        def_flag = 0;
                        ++module_num;
                        temp_sym_table.clear();
                    }
                }
                if(tokens)offset+=token_length + 1 + strlen(temp) - strlen(tokens);
                else offset+=token_length;
            } 
            prev_offset = offset;
            ++lineno;
            offset = 1;
        }
        if(def_flag!=0){ 
                if(def_flag==1 || use_flag==1)__parseerror(1,lineno-1,prev_offset);
                else if(def_flag==2 || instr_flag==2 || use_flag==0 || instr_flag==0)__parseerror(0,lineno-1,prev_offset);
                else if(instr_flag==1)__parseerror(2,lineno-1,prev_offset);
        }
        //printf("Final spot: %d %d\n",lineno-1,prev_offset); 
        cout<<"Symbol Table\n";
        for(auto it:symbol_table){
            if(variableAlreadyDefined(it.first,multiple_variable)){
                cout<<it.first<<"="<<it.second<<" Error: This variable is multiple times defined; first value used\n";
            }
            else cout<<it.first<<'='<<it.second<<'\n';
        }
        myfile.close();
    }
    else cout << "Unable to open file\n";
}

void printMemoryMap(int memory_map_index,int memoryLoc,string error_message){
    cout<<setfill('0')<<setw(3)<<memory_map_index<<":"<<" "<<setfill('0')<<setw(4)<<memoryLoc<<" "<<error_message<<"\n";
}

void passTwo(string filename){
    string line;
    ifstream myfile;
    myfile.open(filename);
    if (myfile)
    {
        int lineno = 1;
        int offset = 1;
        int prev_offset;
        int def_flag = 0;
        int use_flag = -1;
        int instr_flag = -1;
        int defcount;
        int usecount;
        int instrcount;
        string symbol;
        int base_address = 0;
        string addressingMode;
        vector<string> uselist_sym_table;
        int module_num = 1;
        vector<string> multiple_variable;
        int copy_instr_count;
        int memory_map_index = 0;
        vector<int> in_external; //symbols mentioned in external of each module
        vector<string> allSymbolsUsed; //keeps all symbols ever used(in entire program) 
        vector<pair<string,int>> symbolModuleNum; //keep track of all symbols and it's module number
        while(getline(myfile,line))
        {
            char *tochar = new char[line.length()+1];
            tochar[line.length()]='\0';
            for(int i=0;i<line.length();++i)tochar[i]=line[i];
            char *tokens = strtok(tochar," \t");
            while(tokens!=NULL){
                int token_length = strlen(tokens);
                char *curr_token = tokens;
                char *temp = strtok(NULL,"");
                tokens = strtok(temp," \t");
                if(def_flag==0){
                    defcount = readInt(string(curr_token),lineno,offset);
                    if(defcount>16)__parseerror(4,lineno,offset);
                    if(defcount>0){
                        def_flag = 1;
                    }
                    else{
                        def_flag = -1;
                        use_flag = 0;
                    }
                }
                else if(def_flag==1){
                    symbol = readSymbol(string(curr_token),lineno,offset);
                    
                    int seen = 0;
                    for(auto it:symbolModuleNum){
                        if(it.first==symbol){
                            seen = 1;
                            break;
                        }
                    }
                    if(seen==0)symbolModuleNum.push_back(make_pair(symbol,module_num));
                    
                    def_flag = 2;
                }
                else if(def_flag==2){
                    int relative = readInt(string(curr_token),lineno,offset);
                    --defcount;
                    if(defcount>0)def_flag = 1;
                    else if(defcount==0){
                        def_flag = -1;
                        use_flag = 0;
                    }
                } 
                else if(use_flag==0){
                    usecount = readInt(string(curr_token),lineno,offset);
                    if(usecount>16)__parseerror(5,lineno,offset);
                    if(usecount>0){
                        use_flag = 1;
                        
                    }
                    else{
                        use_flag = -1;
                        instr_flag = 0;
                    }
                }
                else if(use_flag==1){
                    symbol = readSymbol(string(curr_token),lineno,offset);
                    uselist_sym_table.push_back(symbol);
                    in_external.push_back(0);
                    --usecount;
                    if(usecount==0){
                        use_flag = -1;
                        instr_flag = 0;
                    }
                }
                else if(instr_flag==0){
                    instrcount = readInt(string(curr_token),lineno,offset);
                    copy_instr_count = instrcount;
                    
                    if(instrcount>0)instr_flag=1;
                    else{
                        instr_flag = -1;
                        def_flag = 0;
                        for(int i = 0; i< uselist_sym_table.size(); ++i){
                            if(in_external[i]==0)
                                cout<<"Warning: Module "<<module_num<<": "<<uselist_sym_table[i]<<" appeared in the uselist but was not actually used\n";
                        }
                        ++module_num;
                        uselist_sym_table.clear();
                        in_external.clear();
                    }
                }
                else if(instr_flag==1){
                    addressingMode = readIAER(string(curr_token),lineno,offset);
                    instr_flag = 2;
                }
                else if(instr_flag==2){
                    int addr = readInt(string(curr_token),lineno,offset);
                    int opcode=-1;
                    int operand=-1;
                    int memoryLoc=-1;
                    string error_message ="";
                    if(addr>9999){
                        opcode = 9;
                        operand = 999;
                        memoryLoc = 9999;
                        error_message="Error: Illegal opcode; treated as 9999 ";
                    }
                    else{
                        opcode = addr/1000;
                        operand = addr%1000;
                    }
                    if(addressingMode=="R" && memoryLoc!=9999){
                        if(operand<copy_instr_count)memoryLoc = base_address + opcode*1000 + operand;
                        else{
                            memoryLoc = base_address + opcode*1000;
                            error_message="Error: Relative address exceeds module size; zero used";
                        }
                    }
                    else if(addressingMode=="I"){
                        if(memoryLoc==9999)error_message="Error: Illegal immediate value; treated as 9999 ";
                        else memoryLoc = addr;
                    }
                    else if(addressingMode=="E"){
                        string sym = "";
                        if(operand>=uselist_sym_table.size()){
                            error_message="Error: External address exceeds length of uselist; treated as immediate ";
                            if(addr>9999){
                                memoryLoc = 9999;
                                error_message = "Error: Illegal immediate value; treated as 9999 ";
                            }
                            else memoryLoc = addr;
                        }
                        else {
                            sym = uselist_sym_table[operand];
                            in_external[operand] = 1;
                        }
                        if(sym!=""){
                            //in_external.push_back(sym);
                            if(symbolAlreadyDefined(sym)){
                                for(auto it:symbol_table){
                                    if(it.first==sym){
                                        operand = it.second;
                                        break;
                                    }
                                }
                                memoryLoc = opcode*1000 + operand;
                            }
                            else{
                                error_message="Error: "+sym+ " is not defined; zero used ";
                                memoryLoc = opcode*1000;
                            }
                        }
                    }
                    else if(addressingMode=="A" && memoryLoc!=9999){
                        if(operand<512)memoryLoc = addr;
                        else{
                            memoryLoc = opcode*1000;
                            error_message="Error: Absolute address exceeds machine size; zero used ";
                        }
                    }
                    printMemoryMap(memory_map_index,memoryLoc,error_message);
                    ++memory_map_index;
                    
                    --instrcount;
                    if(instrcount>0)instr_flag=1;
                    else{
                        instr_flag = -1;
                        def_flag = 0;
                        
                        for(int i = 0; i< uselist_sym_table.size(); ++i){
                            if(in_external[i]==0)
                                cout<<"Warning: Module "<<module_num<<": "<<uselist_sym_table[i]<<" appeared in the uselist but was not actually used\n";
                        }
                        for(int i=0;i<in_external.size();++i){
                            if(in_external[i]==1)
                                allSymbolsUsed.push_back(uselist_sym_table[i]);
                        }
                        base_address+=copy_instr_count;
                        if(base_address>512)__parseerror(6,lineno,offset);
                        ++module_num;
                        uselist_sym_table.clear();
                        in_external.clear();
                    }

                }
                if(tokens)offset+=token_length + 1 + strlen(temp) - strlen(tokens);
                else offset+=token_length;
            } 
            prev_offset = offset;
            ++lineno;
            offset = 1;
        }
        
        cout<<"\n";
        for(auto it1:symbolModuleNum){
            int seen = 0;
            for(auto it2:allSymbolsUsed){
                if(it1.first==it2){
                    seen = 1;
                    break;
                }
            }
            if(seen==0)cout<<"Warning: Module "<<it1.second<<": "<<it1.first<<" was defined but never used\n";
        }
        
        myfile.close();
    }
    else cout << "Unable to open file\n";
}

int main(int argc, char *argv[]){
    if(argc!=2){
        cout<<"Input file not given\n";
        exit(0);
    }
    passOne(string(argv[1]));
    cout<<"\nMemory Map\n";
    passTwo(string(argv[1]));
    return 0;
}
