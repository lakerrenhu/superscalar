#include <vector>
#include <fstream>
#include <stdio.h>
#include <algorithm>


#define INVALID_REG -1
enum INS_STATE { IF, ID, IS, EX, WB };

typedef struct tag_regtemp
{
    int tag;
    bool ready;
}REGTEMP;

typedef struct tag_cycle
{
    unsigned int begin;
    unsigned int duration;
}Cycle;

typedef struct tag_latency
{
    unsigned int ex;
    unsigned int mem_op;
}LATENCY;

typedef struct tag_regIdx
{
    int src1;
    int src2;
}REGNOS, SRCREGNO;

typedef struct tag_instruction {

    unsigned int	    tag;
    unsigned int 	op_type;
    int             dst_reg_no;

    SRCREGNO regnos;
    SRCREGNO regnos_temp;

    bool			reg_src1_ready;
    bool			reg_src2_ready;

    Cycle if_cly;
    Cycle id_cly;
    Cycle is_cly;
    Cycle ex_cly;
    Cycle wb_cly;

    LATENCY latency;
    INS_STATE	state;

    bool operator==(const tag_instruction& b) const
    {
        return this->tag == b.tag;
    }

    bool operator==(const unsigned int& tag) const
    {
        return this->tag == tag;
    }
}INSINFO;

const unsigned int TEMP_REG_LEN = 128;

unsigned int g_tag_index = 0;
unsigned int g_current_cycle = 0;
unsigned int g_woring_rate = 0;

REGTEMP g_regTemp[TEMP_REG_LEN];

std::vector<INSINFO>	fake_ROB;
std::vector<INSINFO> dispatch_list;
std::vector<INSINFO>	issue_list;
std::vector<INSINFO>	execute_list;

void FakeRetire()
{
    while ((!fake_ROB.empty())
        && (fake_ROB.begin()->state == WB))
    {
        printf("%d", fake_ROB.begin()->tag);
        printf(" fu{%d}", fake_ROB.begin()->op_type);
        printf(" src{%d,%d}", fake_ROB.begin()->regnos.src1, fake_ROB.begin()->regnos.src2);
        printf(" dst{%d}", fake_ROB.begin()->dst_reg_no);
        printf(" IF{%d,%d}", fake_ROB.begin()->if_cly.begin, fake_ROB.begin()->if_cly.duration);
        printf(" ID{%d,%d}", fake_ROB.begin()->id_cly.begin, fake_ROB.begin()->id_cly.duration);
        printf(" IS{%d,%d}", fake_ROB.begin()->is_cly.begin, fake_ROB.begin()->is_cly.duration);
        printf(" EX{%d,%d}", fake_ROB.begin()->ex_cly.begin, fake_ROB.begin()->ex_cly.duration);
        printf(" WB{%d,%d}\n", fake_ROB.begin()->wb_cly.begin, fake_ROB.begin()->wb_cly.duration);

        fake_ROB.erase(fake_ROB.begin());
    }
    return;
}

unsigned int summ(unsigned int a, unsigned int b)
{
    return(a+b);
}
unsigned int sub(unsigned int a, unsigned b)
{
    return(a-b);
}
void Execute()
{
    std::vector <unsigned int> remove_list;
    for (auto& it :execute_list)
    {
        if (it.latency.ex != it.latency.mem_op)
        {
            it.latency.ex=summ(it.latency.ex,1);
        }
        else
        {
            it.state = WB;
            //it.ex_cly.duration = g_current_cycle - it.ex_cly.begin;
            it.ex_cly.duration = sub(g_current_cycle , it.ex_cly.begin);
            it.wb_cly.begin = g_current_cycle;
            it.wb_cly.duration = 1;

            for (auto& fake : fake_ROB)
            {
                if (fake.tag == it.tag)
                {
                    fake = it;
                }
            }

            if (it.dst_reg_no != INVALID_REG)
            {
                for (auto& is :issue_list)
                {
                    if (is.regnos_temp.src1 == it.tag)
                    {
                        is.reg_src1_ready = true;
                    }
                    if (is.regnos_temp.src2 == it.tag)
                    {
                        is.reg_src2_ready = true;
                    }
                }
                if (it.tag == g_regTemp[it.dst_reg_no].tag)
                {
                    g_regTemp[it.dst_reg_no].ready = true;
                    g_regTemp[it.dst_reg_no].tag = it.dst_reg_no;
                }
            }
            remove_list.push_back(it.tag);
        }
    }

    for (auto tag : remove_list)
    {
        execute_list.erase(std::remove(execute_list.begin(), execute_list.end(), tag));
    }
}

void Issue(unsigned int working_rate)
{
    std::vector<INSINFO>     temp_issue_list;

    std::for_each(issue_list.begin(), issue_list.end(),
        [&](const INSINFO& it)
        {
            if (it.reg_src1_ready
                && it.reg_src2_ready)
            {
                temp_issue_list.push_back(it);
            }
        }
    );

    unsigned int current_num = 0;
    for (auto& it : temp_issue_list)
    {
        if (current_num < working_rate)
        {
            it.state = EX;
            //it.is_cly.duration = g_current_cycle - it.is_cly.begin;
            it.is_cly.duration = sub(g_current_cycle , it.is_cly.begin);
            it.ex_cly.begin = g_current_cycle;
            execute_list.push_back(it);

            issue_list.erase(std::remove(issue_list.begin(), issue_list.end(), it.tag));
            current_num=summ(current_num,1);
        }
    }
}

void Dispatch(unsigned int s, unsigned int n)
{
    std::vector<INSINFO> temp_dispatch_list;
    unsigned int		dispatch_count = 0;

    std::for_each(dispatch_list.begin(), dispatch_list.end(),
        [&](const INSINFO& v)
    {
        if (v.state == ID)
        {
            temp_dispatch_list.push_back(v);
        }
    });

    for (auto& it : temp_dispatch_list)
    {
        if ((issue_list.size() < s )
            && (dispatch_count < n))
        {
            dispatch_count=summ(dispatch_count,1);
            it.state = IS;
            if (it.regnos_temp.src1 != INVALID_REG)
            {
                if (g_regTemp[it.regnos_temp.src1].ready)
                {
                    it.reg_src1_ready = true;
                }
                else
                {
                    it.reg_src1_ready = false;
                    it.regnos_temp.src1 = g_regTemp[it.regnos_temp.src1].tag;
                }
            }
            else
            {
                it.reg_src1_ready = true;
            }

            if (it.regnos_temp.src2 != INVALID_REG)
            {
                if (g_regTemp[it.regnos_temp.src2].ready)
                {
                    it.reg_src2_ready = true;
                }
                else
                {
                    it.reg_src2_ready = false;
                    it.regnos_temp.src2 = g_regTemp[it.regnos_temp.src2].tag;
                }
            }
            else
            {
                it.reg_src2_ready = true;
            }

            if (it.dst_reg_no != INVALID_REG)
            {
                g_regTemp[it.dst_reg_no].ready = false;
                g_regTemp[it.dst_reg_no].tag = it.tag;
            }
            //it.id_cly.duration = g_current_cycle - it.id_cly.begin;
            it.id_cly.duration = sub(g_current_cycle , it.id_cly.begin);
            it.is_cly.begin = g_current_cycle;

            issue_list.push_back(it);
            dispatch_list.erase(dispatch_list.begin());
        }
    }
    std::for_each(dispatch_list.begin(), dispatch_list.end(),
        [](INSINFO& it)
        {
            if (it.state == IF)
            {
                it.state = ID;
                //it.if_cly.duration = g_current_cycle - it.if_cly.begin;
                it.if_cly.duration = sub(g_current_cycle , it.if_cly.begin);
                it.id_cly.begin = g_current_cycle;
            }
        });
}

void Fetch(FILE *file, unsigned int working_rate)
{
    unsigned int 	PC = 0;
    unsigned int    op_type;
    int             reg_dst = 0;
    int             reg_src1 = 0;
    int             reg_src2 = 0;
    unsigned int	fetch_count = 0;
    unsigned int file_read = 0;

    INSINFO ins;

    while ( (fetch_count < working_rate)
        && (dispatch_list.size() < 2 * working_rate)
        && (!feof(file))
        )
    {
        file_read = fscanf(file, "%x %d %d %d %d\n", &PC, &op_type, &reg_dst, &reg_src1, &reg_src2);
        if (5 != file_read)
        {
            printf("Invalid input trace file");
            break;
        }
        ins.op_type = op_type;
        ins.dst_reg_no = reg_dst;

        ins.state = IF;
        ins.tag = g_tag_index;
        ins.if_cly.begin = g_current_cycle;

        ins.regnos.src1 = reg_src1;
        ins.regnos.src2 = reg_src2;
        ins.regnos_temp = ins.regnos;

        ins.latency.ex = 1;
        ins.latency.mem_op = 1;

        if (op_type == 1)
        {
            ins.latency.mem_op = 2;
        }
        if (op_type == 2)
        {
            ins.latency.mem_op = 5;
        }

        dispatch_list.push_back(ins);
        fake_ROB.push_back(ins);
        g_tag_index=summ(g_tag_index,1);
        fetch_count=summ(fetch_count,1);
    }
}

bool Advance_Cycle(FILE *file)
{
    bool out1;
    out1= (fake_ROB.empty()&& feof(file)) ? false:true;
    return out1;
}


int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Invalid input params \r\n Please Input as <S> <N> <File>");
        return -1;
    }

    FILE *pFile = nullptr;
    int s = atoi(argv[1]);
    int working_rate = atoi(argv[2]);
    char* file_path = argv[3];
    pFile = fopen(file_path, "r");

    unsigned int ii=0;
    while (ii< TEMP_REG_LEN)
    {
        g_regTemp[ii].ready = true;
        g_regTemp[ii].tag = ii;
        ii=summ(ii,1);
    }

    while (Advance_Cycle(pFile))
    {
        FakeRetire();
        Execute();
        Issue(summ(working_rate,1));
        Dispatch(s, working_rate);
        Fetch(pFile, working_rate);
        g_current_cycle=summ(g_current_cycle,1);
    }
    fclose(pFile);

    printf("number of instructions   = %d\n", g_tag_index);
    printf("number of cycles         = %d\n", sub(g_current_cycle,1));
    printf("IPC                      = %0.5f\n", float(g_tag_index) / float(sub(g_current_cycle , 1)));

    return 0;
}
