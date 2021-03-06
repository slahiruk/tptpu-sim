#include "common.hpp"

int main(int argc, char *argv[]) {
    CPU *cpu = new CPU();
    // 256 * 48KiB (12 * 2^20B) per buffer, 256 * 96KiB total (24MiB)
    float buffer_size = (float)(3 * (1 << 22));
    UnifiedBuffer *ub = new UnifiedBuffer(buffer_size);
    float clock = 1;        // 1GHz
    float bw = 1000;        // 1000GB/s
    int channels = 4;
    int ranks = 8;
    // DDR3-1600K
    //DRAM *dram = new DRAM("DDR3_1600K", clock, channels, ranks);
    // DDR4-2400R
    DRAM *dram = new DRAM("DDR4_2400R", clock, channels, ranks);
    // 64KiB per tile, 4 tiles deep
    WeightFetcher *wf = new WeightFetcher(65536, 4);
    Interconnect *cpu_ub_icnt = new Interconnect((Unit *)cpu, (Unit *)ub, clock, bw, ub->GetCapacity(),
                                                 cpu->IsMainMemory(), cpu->GetSenderQueue(),
                                                 ub->GetServedQueue(), ub->GetWaitingQueue(), ub->GetRequestQueue());
    Interconnect *dram_wf_icnt = new Interconnect((Unit *)dram, (Unit *)wf, clock, bw, wf->GetCapacity(),
                                                  dram->IsMainMemory(), dram->GetSenderQueue(),
                                                  wf->GetServedQueue(), wf->GetWaitingQueue(), wf->GetRequestQueue());
    // 256 by 256 systolic array, accumulator size 2048
    int sa_width = 256;
    int sa_height = 256;
    int accumulator_size = 2048;
    float bw_ub_mmu = 256;  // probably meaningless
    float bw_wf_mmu = 100;  // probably meaningless
    MatrixMultiplyUnit *mmu = new MatrixMultiplyUnit(sa_width, sa_height, accumulator_size, ub, wf);
    Interconnect *ub_mmu_icnt = new Interconnect((Unit *)ub, (Unit *)mmu, clock, bw_ub_mmu, mmu->GetCapacity(),
                                                 ub->IsMainMemory(), ub->GetSenderQueue(),
                                                 mmu->GetUBServedQueue(), mmu->GetUBWaitingQueue(), mmu->GetUBRequestQueue());
    Interconnect *wf_mmu_icnt = new Interconnect((Unit *)wf, (Unit *)mmu, clock, bw_wf_mmu, mmu->GetCapacity(),
                                                 wf->IsMainMemory(), wf->GetSenderQueue(),
                                                 mmu->GetWFServedQueue(), mmu->GetWFWaitingQueue(), mmu->GetWFRequestQueue());
    std::vector<Interconnect *> *icnt_list = new std::vector<Interconnect *>();
    icnt_list->push_back(cpu_ub_icnt);
    icnt_list->push_back(dram_wf_icnt);
    icnt_list->push_back(ub_mmu_icnt);
    icnt_list->push_back(wf_mmu_icnt);
    Controller *ctrl = new Controller(mmu, icnt_list, dram->GetWeightTileQueue(), cpu->GetActivationTileQueue());
    // setting complete
    // generate request for matrix multiplication
    ctrl->MatrixMultiply(640, 640, 1080, true, 3, (unsigned int)0, (unsigned int)100000000);
    
    while (!(cpu_ub_icnt->IsIdle() && dram_wf_icnt->IsIdle() && ub_mmu_icnt->IsIdle() && wf_mmu_icnt->IsIdle() && mmu->IsIdle())) {
        mmu->Cycle();
        ub_mmu_icnt->Cycle();
        wf_mmu_icnt->Cycle();
        ub->Cycle();
        wf->Cycle();
        cpu_ub_icnt->Cycle();
        dram_wf_icnt->Cycle();
        cpu->Cycle();
        dram->Cycle();
    }

    // test complete
    cpu_ub_icnt->PrintStats("CPU - Unified Buffer Interconnect");
    dram_wf_icnt->PrintStats("DRAM - Weight Fetcher Interconnect");
    dram->PrintStats();
    mmu->PrintStats();
    return 0;
}
