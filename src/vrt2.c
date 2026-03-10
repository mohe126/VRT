#include "vpx2.c"
#include <stdint.h>

//[[ MACROS ]]
#define VRT_RHC 61

#define VRT_HC_ARG 60 //[[registers [50:60] are likely to be used for arguments]]

//VRT2: Vpx Run Time 2
//This is not a library but is intended to be an easily configurable
//file to add to your own.. extended runtime

//[[ VRT2 INSTANCE ]]

struct Vrt2 {
    Vpx2 cpu; //VPX instance

    uint8_t int_enabled;
    uint8_t int_state; //If there is an interrupt.


    uint32_t int_vec;
    uint32_t int_code;

    uint32_t exception_vec;
    uint8_t exception_code;
    uint32_t exception_val;

    uint32_t syscall_vec;


}; //Machine instance

typedef struct Vrt2 Vrt2;



//[[ KERNEL PANIC ]]

static void vrt2_kernel_panic(Vrt2* machine){
    //Runs if the kernel triggers an error-interrupt.

    char* err_msg = "KERNEL PANIC. HALTING.";
    memcpy(&machine->cpu.mem_ptr[0], err_msg, sizeof("KERNEL PANIC. HALTING."));

}

//[[ VRT vector jumps ]]
static void vrt2_exception_call(Vrt2* machine, Vpx2* proc){
    vpx2_mem_pu32(&machine->cpu, machine->cpu.registers[VPX_RPC]);

    //[[ SETUP VALUES ]]
    machine->cpu.registers[VPX_RPC] = machine->exception_vec;
    machine->exception_code = proc->err_code;
    machine->exception_val = proc->err_val;

}
static void vrt2_interrupt_call(Vrt2* machine, Vpx2* proc){
    //Save return address to stack
    vpx2_mem_pu32(&machine->cpu, machine->cpu.registers[VPX_RPC]);

    //[[ Expects the interrupt-function to have setup the rest of values. ]]

    machine->cpu.registers[VPX_RPC] = machine->int_vec;


}
static void vrt2_syscall_call(Vrt2* machine){
    vpx2_mem_pu32(&machine->cpu, machine->cpu.registers[VPX_RPC]);


    machine->cpu.registers[VPX_RPC] = machine->syscall_vec;
    //There is no syscall code. As that's handled by ABI.
    //Process instance is unneccecary so not included
}

static void vrt2_kernel_return(Vrt2* machine){
    //Returns from interrupt, syscall, or exception
    //This gets called with a special HCALL.
    uint32_t ret_adr = vpx2_mem_po32(&machine->cpu);

    machine->cpu.registers[VPX_RPC] = ret_adr;
}

//[[ PROCESS RUNNING ROUTINE ]]

static uint8_t vrt2_proc_run(Vrt2* machine, Vpx2* proc){

    while(1){
        uint8_t rt = vpx2_exec(proc);
        if(rt == 1){
            //Error: Exception call
            vrt2_exception_call(machine, proc);
            return 1;
        }
        else if(rt == 255){
            vrt2_syscall_call(machine);
            return 0; //Intentional syscall
        }

        //[[ INTERRUPT CHECK ]]

        if(!machine->int_state && machine->int_enabled){
            vrt2_interrupt_call(machine, proc);
            return 0; //Interrupt, intentional.
        }

    }
}



//[[ HOSTCALL FUNCTIONS ]]


static void vrt2_hcall_memcpy_vpx(Vrt2* machine){
    //[[ ARGS: SRC (ARG0), COUNT (ARG1), DIST [VPX_PTR] (ARG2)]]
    //Reads count-amount of bytes from data in VPX_PTR, outputs to address in 
    uint32_t src_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    uint32_t count = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 1);
    uint32_t dist_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 2);

    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr], &machine->cpu.mem_ptr[src_vpx_ptr], count);
}
static void vrt2_hcall_memcpy_host(Vrt2* machine){
    //[[ ARGS: SRC (ARG0), COUNT (ARG1), DIST [VPX_PTR] (ARG2)]]
    //Reads count-amount of bytes from data in VPX_PTR, outputs to address in 
    uint32_t src_hc_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    //Source is not exactly a pointer! it's a pointer to the data in VPX that has the host CPU pointer you want to read from.
    
    uint32_t count = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 1);
    uint32_t dist_hc_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 2);

    uintptr_t src = *(uintptr_t*)(machine->cpu.mem_ptr + src_hc_ptr);
    uintptr_t dist = *(uintptr_t*)(machine->cpu.mem_ptr + dist_hc_ptr);

    memcpy((void*)dist, (void*)src, count);
}
static void vrt2_hcall_memcpy_inter_host_vpx(Vrt2* machine){
    //Reads data from Host memory and writes to VPX memory, useful for reading from flash or FS
    uint32_t src_hc_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);    
    uint32_t count = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 1);
    uint32_t dist_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 2);

    uintptr_t src = *(uintptr_t*)(machine->cpu.mem_ptr + src_hc_ptr);

    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr], (void*)src, count);
}
static void vrt2_hcall_func_call(Vrt2* machine){

    void (*func_ptr)(void);
    uint32_t src_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    memcpy(func_ptr, &machine->cpu.mem_ptr[src_vpx_ptr], sizeof(func_ptr));

    func_ptr(); //Call function

}
static void vrt2_hcall_get_host_ptr(Vrt2* machine){
    uint32_t conv_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG); //pointer to convert into host pointer.
    uint32_t dist_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 1);

    uint8_t* converted_host_ptr = &machine->cpu.mem_ptr[conv_vpx_ptr];

    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr], &converted_host_ptr, sizeof(converted_host_ptr));
}
static void vrt2_hcall_call_process(Vrt2* machine){
    //[[ no create process, done by kernel ]]
    //This might be slow, if so, optimize later.
    uint32_t src_proc_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    uint32_t mode = vpx2_rreg(&machine->cpu, VRT_HC_ARG - 1);
    Vpx2 proc;
    //[ First 4 bytes: CPUID ]
    memcpy(&proc.cpu_id, &machine->cpu.mem_ptr[src_proc_vpx_ptr], sizeof(uint32_t));
    proc.err_code = 0;
    proc.err_pc_state = 0;
    proc.err_val = 0;

    //[ Second 8 bytes: mem_ptr ]
    //Strictly 64 bits so processes are cross platform.
    memcpy(&proc.mem_ptr, &machine->cpu.mem_ptr[src_proc_vpx_ptr + sizeof(uint64_t)], sizeof(proc.mem_ptr));
    //[ Third 4 bytes: mem_size ]
    memcpy(&proc.mem_size, &machine->cpu.mem_ptr[src_proc_vpx_ptr + sizeof(uint64_t) + 4], 4);
    //[[ Lastly: Registers ]]
    memcpy(proc.registers, &machine->cpu.mem_ptr[src_proc_vpx_ptr + sizeof(uint64_t) + 8], 64 * 4);

    //[[ RUN PROCESS ]]
    vrt2_proc_run(machine, &proc);
}
static void vrt2_hcall_int_enable(Vrt2* machine){
    uint32_t state = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    if(state){
        machine->int_enabled = 1;
    }else{
        machine->int_enabled = 0;
    }
}
static void vrt2_hcall_set_except_vec(Vrt2* machine){
    //Sets exception vector.

    uint32_t vector = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    machine->exception_vec = vector;

}
static void vrt2_hcall_set_syscall_vec(Vrt2* machine){
    //Sets system call vector.

    uint32_t vector = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    machine->syscall_vec = vector;

}
static void vrt2_hcall_set_interrupt_vec(Vrt2* machine){
    //Sets interrupt vector.

    uint32_t vector = vpx2_rreg(&machine->cpu, VRT_HC_ARG);
    machine->int_vec = vector;

}
static void vrt2_hcall_get_except(Vrt2* machine){
    //Gets Exception values
    uint32_t dist_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);

    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr], &machine->exception_code, 1);
    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr + 1], &machine->exception_val, 4);
    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr + 5], &machine->exception_vec, 4);
}
static void vrt2_hcall_get_interrupt(Vrt2* machine){
    //Get interrupt values
    uint32_t dist_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);

    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr], &machine->int_code, 4);
    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr + 4], &machine->int_vec, 4);
}
static void vrt2_hcall_get_syscall(Vrt2* machine){
    //Gets syscall values, just the vector however.
    uint32_t dist_vpx_ptr = vpx2_rreg(&machine->cpu, VRT_HC_ARG);

    memcpy(&machine->cpu.mem_ptr[dist_vpx_ptr], &machine->syscall_vec, 4);
}

static void vrt2_hcall_vrt_ret(Vrt2* machine){
    vrt2_kernel_return(machine);
}

//[[ MAIN ]]


uint8_t vrt2_hostcall(Vrt2* machine){
    uint32_t code = vpx2_rreg(&machine->cpu, VRT_RHC);
    switch(code){
        default: return 1; break; //error!
        case 0: vrt2_hcall_memcpy_vpx(machine); break;
        case 1: vrt2_hcall_memcpy_host(machine); break;
        case 2: vrt2_hcall_memcpy_inter_host_vpx(machine); break;

        case 3: vrt2_hcall_func_call(machine); break;

        case 4: vrt2_hcall_get_host_ptr(machine); break;

        case 5: vrt2_hcall_call_process(machine); break;

        case 6: vrt2_hcall_int_enable(machine); break;

        case 7: vrt2_hcall_set_except_vec(machine); break;
        case 8: vrt2_hcall_set_syscall_vec(machine); break;
        case 9: vrt2_hcall_set_interrupt_vec(machine); break;

        case 10: vrt2_hcall_get_except(machine); break;
        case 11: vrt2_hcall_get_interrupt(machine); break;
        case 12: vrt2_hcall_get_syscall(machine); break;

        case 13: vrt2_hcall_vrt_ret(machine); break;

    }


    return 0;
}
void vrt2_start(Vrt2* machine){
    //Main execution loop
    while(1){
        uint8_t rt = vpx2_exec(&machine->cpu);
        if(rt == 1){vrt2_kernel_panic(machine); return;} //PANIC
        if(rt == 255){vrt2_hostcall(machine);} //HOSTCALL
    }

}
