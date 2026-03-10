
//[[ INCLUDES ]]
#include <stdint.h>
#include <string.h>

//[[ MACROS ]]
#define VPXNULL 0
#define VPX_RPC 62
#define VPX_RSP 63
//== error codes ==

#define VPX_ERR_RREG 1
#define VPX_ERR_WREG 2

#define VPX_ERR_MEM_R8 3
#define VPX_ERR_MEM_R16 4
#define VPX_ERR_MEM_R32 5

#define VPX_ERR_MEM_W8 6
#define VPX_ERR_MEM_W16 7
#define VPX_ERR_MEM_W32 8

#define VPX_ERR_DIV_BY_ZERO 9 //Float version has _F at the end.
#define VPX_ERR_DIV_BY_ZERO_S 10
#define VPX_ERR_DIV_INT32_MAX_N1 11
#define VPX_ERR_CJMP_INVALID 12

#define VPX_ERR_RREG_64 13
#define VPX_ERR_WREG_64 14

#define VPX_ERR_INVALID_OPCODE 255

//this is essentially for formatting, if the system is little endian it does nothing
//otherwise it swaps
#ifdef VPX_BIG_ENDIAN

static inline uint32_t vpx2_32b_endian_fmt(uint32_t val){
    return __builtin_bswap32(val);
}
static inline uint32_t vpx2_16b_endian_fmt(uint16_t val){
    return __builtin_bswap16(val);
}
#else
//Likely optimized away to nothingness
static inline uint32_t vpx2_32b_endian_fmt(uint32_t val){
    return val;
}
static inline uint32_t vpx2_16b_endian_fmt(uint16_t val){
    return val;
}
#endif

//[[ MAIN STRUCT ]]
#ifndef VPX_DEFINED
struct Vpx2 {
    //[[ SYSTEM ]]
    uint32_t cpu_id;
    //[[ ERROR ]]
    uint8_t err_code;
    uint32_t err_val;
    uint32_t err_pc_state;
    //[[ MEMORY ]]
    uint8_t* mem_ptr;
    uint32_t mem_size;
    uint32_t registers[64];
};
typedef struct Vpx2 Vpx2;


#else

struct Vpx2;


#endif



//[[ ERROR LOG ]]
static inline void vpx2_log_err(Vpx2* vpx2, uint8_t code, uint32_t value){
    vpx2->err_code = code;
    vpx2->err_val = value;
    vpx2->err_pc_state = vpx2->registers[VPX_RPC];
}

//[[ REGISTER ]]

static inline uint32_t vpx2_rreg(Vpx2* vpx2, uint8_t reg){
    //You may remove this check if you provide a different
    //register stack without using unsafe mode
    if(reg >= 64){
        //Log attempted register read
        vpx2_log_err(vpx2, 1, reg);
        return 0;
    }
    return vpx2->registers[reg];
}
static inline void vpx2_wreg(Vpx2* vpx2, uint8_t reg, uint32_t val){
    if(reg >= 64){

        //Log attempted register write
        vpx2_log_err(vpx2, 2, reg);
        return;
    }
    vpx2->registers[reg] = val;
}



//[[ MEMORY ]]
static inline uint8_t vpx2_mem_r8(Vpx2* vpx2, uint32_t adr){
    if(adr >= vpx2->mem_size){
        vpx2_log_err(vpx2, 3, adr); //log code and value
        return 0;
    }
    return vpx2->mem_ptr[adr];
}
static inline uint16_t vpx2_mem_r16(Vpx2* vpx2, uint32_t adr){
    if(adr >= vpx2->mem_size - 2){
        vpx2_log_err(vpx2, 4, adr); //log code and value
        return 0;
    }
    uint16_t ds;
    memcpy(&ds, &vpx2->mem_ptr[adr], 2);
    return vpx2_16b_endian_fmt(ds);
}

static inline uint32_t vpx2_mem_r32(Vpx2* vpx2, uint32_t adr){
    if(adr >= vpx2->mem_size - 4){
        vpx2_log_err(vpx2, 5, adr); //log code and value
        return 0;
    }
    uint32_t ds;
    memcpy(&ds, &vpx2->mem_ptr[adr], 4);
    return vpx2_32b_endian_fmt(ds);
}

static inline void vpx2_mem_w8(Vpx2* vpx2, uint32_t adr, uint8_t val){
    if(adr >= vpx2->mem_size){
        vpx2_log_err(vpx2, 6, adr); //log code and value
        return;
    }
    vpx2->mem_ptr[adr] = val;
}
static inline void vpx2_mem_w16(Vpx2* vpx2, uint32_t adr, uint16_t val){
    if(adr >= vpx2->mem_size - 2){
        vpx2_log_err(vpx2, 7, adr); //log code and value
        return;
    }
    uint16_t tmp = vpx2_16b_endian_fmt(val);
    memcpy(&vpx2->mem_ptr[adr], &tmp, 2);
}
static inline void vpx2_mem_w32(Vpx2* vpx2, uint32_t adr, uint32_t val){
    if(adr >= vpx2->mem_size - 4){
        vpx2_log_err(vpx2, 8, adr); //log code and value
        return;
    }
    uint32_t tmp = vpx2_32b_endian_fmt(val);
    memcpy(&vpx2->mem_ptr[adr], &tmp, 4);
}



//[[ CONCIDERING THEY USE WRITE AND READ FUNCTIONS, IT IS NOT REQUIRED TO MAKE SEPERATE SAFE AND UNSAFE VARIANTS ]]
static inline void vpx2_mem_pu8(Vpx2* vpx2, uint8_t val){
    //Push 8 bit
    uint32_t adr = vpx2_rreg(vpx2, VPX_RSP);
    //Write then increment.
    vpx2_mem_w8(vpx2, adr, val);
    vpx2_wreg(vpx2, VPX_RSP, adr+1);
}
static inline void vpx2_mem_pu16(Vpx2* vpx2, uint16_t val){
    //Push 16 bit
    uint32_t adr = vpx2_rreg(vpx2, VPX_RSP);
    uint16_t tmp = vpx2_16b_endian_fmt(val);
    //Write then increment.
    vpx2_mem_w16(vpx2, adr, tmp);
    vpx2_wreg(vpx2, VPX_RSP, adr+2);
}
static inline void vpx2_mem_pu32(Vpx2* vpx2, uint32_t val){
    //Push 32 bit
    uint32_t adr = vpx2_rreg(vpx2, VPX_RSP);
    uint32_t tmp = vpx2_32b_endian_fmt(val);
    //Write then increment.
    vpx2_mem_w32(vpx2, adr, tmp);
    vpx2_wreg(vpx2, VPX_RSP, adr+4);
}


static inline uint8_t vpx2_mem_po8(Vpx2* vpx2){
    //pop 8 bit
    uint32_t adr = vpx2_rreg(vpx2, VPX_RSP);
    //Decrement then read
    vpx2_wreg(vpx2, VPX_RSP, adr-1);

    return vpx2_mem_r8(vpx2, adr-1);
}
static inline uint16_t vpx2_mem_po16(Vpx2* vpx2){
    //pop 16 bit
    uint32_t adr = vpx2_rreg(vpx2, VPX_RSP);
    //Decrement then read
    vpx2_wreg(vpx2, VPX_RSP, adr-2);

    return vpx2_16b_endian_fmt(vpx2_mem_r16(vpx2, adr-2));
}
static inline uint32_t vpx2_mem_po32(Vpx2* vpx2){
    //pop 32 bit
    uint32_t adr = vpx2_rreg(vpx2, VPX_RSP);
    //Decrement then read
    vpx2_wreg(vpx2, VPX_RSP, adr-4);

    return vpx2_32b_endian_fmt(vpx2_mem_r32(vpx2, adr-4));
}

//[[ FETCH (basically pop but using RPC sorta) ]]

static inline uint8_t vpx2_mem_f8(Vpx2* vpx2){
    uint32_t adr = vpx2_rreg(vpx2, VPX_RPC);
    uint8_t val = vpx2_mem_r8(vpx2, adr);
    vpx2_wreg(vpx2, VPX_RPC, adr+1);
    return val;
}
static inline uint16_t vpx2_mem_f16(Vpx2* vpx2){
    uint32_t adr = vpx2_rreg(vpx2, VPX_RPC);
    uint16_t val = vpx2_mem_r16(vpx2, adr);
    vpx2_wreg(vpx2, VPX_RPC, adr+2);
    return val;
}
static inline uint32_t vpx2_mem_f32(Vpx2* vpx2){
    uint32_t adr = vpx2_rreg(vpx2, VPX_RPC);
    uint32_t val = vpx2_mem_r32(vpx2, adr);
    vpx2_wreg(vpx2, VPX_RPC, adr+4);
    return val;
}


//[[ ISA SECTION ]]

//[[ ISA INSTRUCTIONS ]]


static inline void vpx2_isa_cpuid(Vpx2* vpx2){
    //===========================================
    //Gets the value of the CPU-ID and information about it.
    //===========================================
    //C syntax: registers[r1] = cpu_id;
    //Pseudocode: r1 <- cpu_id
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    vpx2_wreg(vpx2, r1, vpx2->cpu_id);
    

}




static inline void vpx2_isa_mov(Vpx2* vpx2){
    //===========================================
    //Move value in r1 to r2.
    //===========================================
    //C syntax: registers[r1] = registers[r2];
    //Pseudocode: r1 <- r2
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t val = vpx2_rreg(vpx2, r2);
    vpx2_wreg(vpx2, r1, val);
    

}
static inline void vpx2_isa_movi(Vpx2* vpx2){
    //===========================================
    //Move immediate value to r1
    //===========================================
    //C syntax: registers[r1] = imm;
    //Pseudocode: r1 <- imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f32(vpx2);
    vpx2_wreg(vpx2, r1, imm);
}
static inline void vpx2_isa_inc(Vpx2* vpx2){
    //===========================================
    //Increment value of r1
    //===========================================
    //C syntax: registers[r1] = registers[r1]++;
    //Pseudocode: r1 <- r1 + 1
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint32_t val = vpx2_rreg(vpx2, r1);
    vpx2_wreg(vpx2, r1, val + 1);
}
static inline void vpx2_isa_dec(Vpx2* vpx2){
    //===========================================
    //Decrement value of r1
    //===========================================
    //C syntax: registers[r1] = registers[r1]--;
    //Pseudocode: r1 <- r1 - 1
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint32_t val = vpx2_rreg(vpx2, r1);
    vpx2_wreg(vpx2, r1, val - 1);
}

static inline void vpx2_isa_or(Vpx2* vpx2){
    //===========================================
    //Do an OR operation on r2 and r3, write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] | registers[r3];
    //Pseudocode: r1 <- r2 or r3
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 | val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_xor(Vpx2* vpx2){
    //===========================================
    //Do an XOR operation on r2 and r3, write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] ^ registers[r3];
    //Pseudocode: r1 <- r2 xor r3
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 ^ val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_and(Vpx2* vpx2){
    //===========================================
    //Do an AND operation on r2 and r3, write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] & registers[r3];
    //Pseudocode: r1 <- r2 and r3
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 & val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_not(Vpx2* vpx2){
    //===========================================
    //Do a NOT operation on r2 and write to r1
    //===========================================
    //C syntax: registers[r1] = ~registers[r2];
    //Pseudocode: r1 <- not r2
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = ~val2;
    vpx2_wreg(vpx2, r1, val1);
}

static inline void vpx2_isa_ori(Vpx2* vpx2){
    //===========================================
    //Do an OR operation on r2 and immediate, write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] | imm;
    //Pseudocode: r1 <- r2 or imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 | imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_xori(Vpx2* vpx2){
    //===========================================
    //Do an XOR operation on r2 and immediate, write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] ^ imm;
    //Pseudocode: r1 <- r2 xor imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 ^ imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_andi(Vpx2* vpx2){
    //===========================================
    //Do an AND operation on r2 and immediate, write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] & imm;
    //Pseudocode: r1 <- r2 and imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 & imm;
    vpx2_wreg(vpx2, r1, val1);
}

static inline void vpx2_isa_sll(Vpx2* vpx2){
    //===========================================
    //Shift logical left of r2 by r3 and write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] << registers[r3];
    //Pseudocode: r1 <- r2 << r3
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 << val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_srl(Vpx2* vpx2){
    //===========================================
    //Shift logical right of r2 by r3 and write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] >> registers[r3];
    //Pseudocode: r1 <- r2 >> r3
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 >> val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_sra(Vpx2* vpx2){
    //===========================================
    //Shift arithmetic right of r2 by r3 and write to r1
    //WARNING: might not work always!
    //might add inline assembly version if this doesn't work. with preproccessor checks
    //===========================================
    //C syntax: registers[r1] = registers[r2] >> registers[r3];
    //Pseudocode: r1 <- r2 >> r3
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    int32_t val1 = (int32_t)val2 >> (int32_t)val3;
    vpx2_wreg(vpx2, r1, (uint32_t)val1);
}

static inline void vpx2_isa_slli(Vpx2* vpx2){
    //===========================================
    //Immediate Shift logical left of r2 by imm and write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] << imm;
    //Pseudocode: r1 <- r2 << imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f8(vpx2); //imm is 8 bits because you can't shift by more anyway

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 << imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_srli(Vpx2* vpx2){
    //===========================================
    //Immediate Shift logical right of r2 by imm and write to r1
    //===========================================
    //C syntax: registers[r1] = registers[r2] >> imm;
    //Pseudocode: r1 <- r2 >> imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
 

    uint32_t val1 = val2 >> imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_srai(Vpx2* vpx2){
    //===========================================
    //Immediate Shift arithmetic right of r2 by imm and write to r1
    //WARNING: might not work always!
    //might add inline assembly version if this doesn't work. with preproccessor checks
    //===========================================
    //C syntax: registers[r1] = registers[r2] >> imm;
    //Pseudocode: r1 <- r2 >> imm
    //===========================================

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    int32_t val1 = (int32_t)val2 >> (int32_t)imm;
    vpx2_wreg(vpx2, r1, (uint32_t)val1);
}

static inline void vpx2_isa_add(Vpx2* vpx2){
    //===========================================
    //Add r2 and r3, write result to r1. (No carry)
    //===========================================
    //C syntax: registers[r1] = registers[r2] + registers[r3];
    //Pseudocode: r1 <- r2 + r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 + val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_sub(Vpx2* vpx2){
    //===========================================
    //Subtract r2 by r3, write result to r1.
    //===========================================
    //C syntax: registers[r1] = registers[r2] - registers[r3];
    //Pseudocode: r1 <- r2 - r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 - val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_mul(Vpx2* vpx2){
    //===========================================
    //Multiply r2 by r3, write result to r1.
    //===========================================
    //C syntax: registers[r1] = registers[r2] * registers[r3];
    //Pseudocode: r1 <- r2 * r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    uint32_t val1 = val2 * val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_udiv(Vpx2* vpx2){
    //===========================================
    //Divide r2 by r3, write result to r1. (Unsigned)
    //===========================================
    //C syntax: registers[r1] = registers[r2] / registers[r3];
    //Pseudocode: r1 <- r2 / r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);
    if(val3 == 0){
        //Division by 0 error
        //Log aswell the register that contained it.
        vpx2_log_err(vpx2, VPX_ERR_DIV_BY_ZERO, r3);
        return;
    }

    uint32_t val1 = val2 / val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_sdiv(Vpx2* vpx2){
    //===========================================
    //Divide r2 by r3, write result to r1. (Signed)
    //===========================================
    //C syntax: registers[r1] = registers[r2] / registers[r3];
    //Pseudocode: r1 <- r2 / r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);
    //Signed version is stricter.
    if((val3 == 0) || ((int32_t)val2 == UINT32_MAX && (int32_t)val2 == -1)){
        //Division by 0 error (But signed)
        //Log aswell the register that contained it.
        vpx2_log_err(vpx2, VPX_ERR_DIV_BY_ZERO_S, r3);
        return;
    }
    if((int32_t)val2 == INT32_MIN && (int32_t)val3 == -1){
        vpx2_log_err(vpx2, VPX_ERR_DIV_INT32_MAX_N1, r2); //Signed conversion error.
        return;
    }


    int32_t val1 = (int32_t)val2 / (int32_t)val3;
    vpx2_wreg(vpx2, r1, (uint32_t)val1);
}
static inline void vpx2_isa_urem(Vpx2* vpx2){
    //===========================================
    //Modulo/Remainder of r2 by r3, write result to r1. (Unsigned)
    //===========================================
    //C syntax: registers[r1] = registers[r2] % registers[r3];
    //Pseudocode: r1 <- r2 % r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);
    if(val3 == 0){
        //Division by 0 error
        //Log aswell the register that contained it.
        vpx2_log_err(vpx2, 9, r3);
        return;
    }

    uint32_t val1 = val2 % val3;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_srem(Vpx2* vpx2){
    //===========================================
    //Modulo/Remainder of r2 by r3, write result to r1. (Signed)
    //===========================================
    //C syntax: registers[r1] = registers[r2] % registers[r3];
    //Pseudocode: r1 <- r2 % r3
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t r3 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    uint32_t val3 = vpx2_rreg(vpx2, r3);

    //Signed version is stricter.
    if(val3 == 0){
        //Division by 0 error (But signed)
        //Log aswell the register that contained it.
        vpx2_log_err(vpx2, 10, r3);
        return;
    }
    if((int32_t)val2 == INT32_MIN && (int32_t)val3 == -1){
        vpx2_log_err(vpx2, 11, r2);
        return;
    }


    int32_t val1 = (int32_t)val2 % (int32_t)val3;
    vpx2_wreg(vpx2, r1, (uint32_t)val1);
}

static inline void vpx2_isa_addi(Vpx2* vpx2){
    //===========================================
    //Add r2 and imm, write result to r1. (No carry)
    //===========================================
    //C syntax: registers[r1] = registers[r2] + imm;
    //Pseudocode: r1 <- r2 + imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 + imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_subi(Vpx2* vpx2){
    //===========================================
    //Subtract r2 and imm, write result to r1.
    //===========================================
    //C syntax: registers[r1] = registers[r2] - imm;
    //Pseudocode: r1 <- r2 - imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 - imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_muli(Vpx2* vpx2){
    //===========================================
    //Multiply r2 by imm, write result to r1.
    //===========================================
    //C syntax: registers[r1] = registers[r2] * imm;
    //Pseudocode: r1 <- r2 * imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = val2 * imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_udivi(Vpx2* vpx2){
    //===========================================
    //Divide r2 by imm, write result to r1. (Unsigned)
    //===========================================
    //C syntax: registers[r1] = registers[r2] / imm;
    //Pseudocode: r1 <- r2 / imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    if(imm == 0){
        //Division by 0 error
        vpx2_log_err(vpx2, VPX_ERR_DIV_BY_ZERO, 0);
        return;
    }

    uint32_t val1 = val2 / imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_sdivi(Vpx2* vpx2){
    //===========================================
    //Divide r2 by imm, write result to r1. (Signed)
    //===========================================
    //C syntax: registers[r1] = registers[r2] / imm;
    //Pseudocode: r1 <- r2 / imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    //Signed version is stricter.
    if(imm == 0){
        //Division by 0 error (But signed)
        vpx2_log_err(vpx2, VPX_ERR_DIV_BY_ZERO_S, 0);
        return;
    }
    if((int32_t)val2 == INT32_MIN && (int32_t)imm == -1){
        vpx2_log_err(vpx2, VPX_ERR_DIV_INT32_MAX_N1, r2); //Signed conversion error.
        return;
    }


    int32_t val1 = (int32_t)val2 / (int32_t)imm;
    vpx2_wreg(vpx2, r1, (uint32_t)val1);
}
static inline void vpx2_isa_uremi(Vpx2* vpx2){
    //===========================================
    //Modulo/Remainder of r2 by imm, write result to r1. (Unsigned)
    //===========================================
    //C syntax: registers[r1] = registers[r2] % imm;
    //Pseudocode: r1 <- r2 % imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);
    if(imm == 0){
        //Division by 0 error
        vpx2_log_err(vpx2, 9, 0);
        return;
    }

    uint32_t val1 = val2 % imm;
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_sremi(Vpx2* vpx2){
    //===========================================
    //Modulo/Remainder of r2 by imm, write result to r1. (Signed)
    //===========================================
    //C syntax: registers[r1] = registers[r2] % imm;
    //Pseudocode: r1 <- r2 % imm
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    //Signed version is stricter.
    if(imm == 0){
        //Division by 0 error (But signed)
        vpx2_log_err(vpx2, 10, 0);
        return;
    }
    if((int32_t)val2 == INT32_MIN && (int32_t)imm == -1){
        vpx2_log_err(vpx2, 11, r2);
        return;
    }


    int32_t val1 = (int32_t)val2 % (int32_t)imm;
    vpx2_wreg(vpx2, r1, (uint32_t)val1);
}

static inline void vpx2_isa_ld8(Vpx2* vpx2){
    //===========================================
    //Load 1B to r1 based on address in r2 + PC (Relative offset)
    //===========================================
    //C syntax: registers[r1] = mem_r8(registers[r2] + registers[PC]);
    //Pseudocode: r1 <- mem[r2 + PC]
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint8_t val1 = vpx2_mem_r8(vpx2, val2 + pc);
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_ld16(Vpx2* vpx2){
    //===========================================
    //Load 2B to r1 based on address in r2 + PC (Relative offset)
    //===========================================
    //C syntax: registers[r1] = mem_r16(registers[r2] + registers[PC]);
    //Pseudocode: r1 <- mem[r2 + PC]
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint16_t val1 = vpx2_mem_r16(vpx2, val2 + pc);
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_ld32(Vpx2* vpx2){
    //===========================================
    //Load 4B to r1 based on address in r2 + PC (Relative offset)
    //===========================================
    //C syntax: registers[r1] = mem_r32(registers[r2] + registers[PC]);
    //Pseudocode: r1 <- mem[r2 + PC]
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = vpx2_mem_r32(vpx2, val2 + pc);
    vpx2_wreg(vpx2, r1, val1);
}

static inline void vpx2_isa_st8(Vpx2* vpx2){
    //===========================================
    //Stores 1B from r1 (LSB) to address r2 + PC (Relative offset)
    //===========================================
    //C syntax: mem_w8(registers[r2] + registers[PC], registers[r1] & 0xff);
    //Pseudocode: mem[r2 + PC] <- r1
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    vpx2_mem_w8(vpx2, val2 + pc, val1);
}
static inline void vpx2_isa_st16(Vpx2* vpx2){
    //===========================================
    //Stores 2B from r1 (LSW) to address r2 + PC (Relative offset)
    //===========================================
    //C syntax: mem_w16(registers[r2] + registers[PC], registers[r1] & 0xffff);
    //Pseudocode: mem[r2 + PC] <- r1
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    vpx2_mem_w16(vpx2, val2 + pc, val1);
}
static inline void vpx2_isa_st32(Vpx2* vpx2){
    //===========================================
    //Stores 4B from r1 (LSW) to address r2 + PC (Relative offset)
    //===========================================
    //C syntax: mem_w32(registers[r2] + registers[PC], registers[r1] & 0xffffffff);
    //Pseudocode: mem[r2 + PC] <- r1
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    vpx2_mem_w32(vpx2, val2 + pc, val1);
}

static inline void vpx2_isa_ld8r(Vpx2* vpx2){
    //===========================================
    //Load 1B to r1 based on address in r2 + imm (Relative/Absolute offset)
    //===========================================
    //C syntax: registers[r1] = mem_r8(registers[r2] + imm);
    //Pseudocode: r1 <- mem[r2 + imm]
    //===========================================

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = vpx2_mem_r8(vpx2, val2 + imm);
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_ld16r(Vpx2* vpx2){
    //===========================================
    //Load 2B to r1 based on address in r2 + imm (Relative/Absolute offset)
    //===========================================
    //C syntax: registers[r1] = mem_r16(registers[r2] + imm);
    //Pseudocode: r1 <- mem[r2 + imm]
    //===========================================

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = vpx2_mem_r16(vpx2, val2 + imm);
    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_ld32r(Vpx2* vpx2){
    //===========================================
    //Load 4B to r1 based on address in r2 + imm (Relative/Absolute offset)
    //===========================================
    //C syntax: registers[r1] = mem_r32(registers[r2] + imm);
    //Pseudocode: r1 <- mem[r2 + imm]
    //===========================================

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint8_t imm = vpx2_mem_f32(vpx2);

    uint32_t val2 = vpx2_rreg(vpx2, r2);

    uint32_t val1 = vpx2_mem_r32(vpx2, val2 + imm);
    vpx2_wreg(vpx2, r1, val1);
}

static inline void vpx2_isa_st8r(Vpx2* vpx2){
    //===========================================
    //Stores 1B from r1 (LSB) to address r2 + imm (Relative/Absolute offset)
    //===========================================
    //C syntax: mem_w8(registers[r2] + imm, registers[r1] & 0xff);
    //Pseudocode: mem[r2 + imm] <- r1
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    vpx2_mem_w8(vpx2, val2 + imm, val1);
}
static inline void vpx2_isa_st16r(Vpx2* vpx2){
    //===========================================
    //Stores 2B from r1 (LSW) to address r2 + imm (Relative/Absolute offset)
    //===========================================
    //C syntax: mem_w16(registers[r2] + imm, registers[r1] & 0xffff);
    //Pseudocode: mem[r2 + imm] <- r1
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    vpx2_mem_w16(vpx2, val2 + imm, val1);
}
static inline void vpx2_isa_st32r(Vpx2* vpx2){
    //===========================================
    //Stores 4B from r1 to address r2 + imm (Relative/Absolute offset)
    //===========================================
    //C syntax: mem_w32(registers[r2] + imm, registers[r1]);
    //Pseudocode: mem[r2 + imm] <- r1
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    vpx2_mem_w32(vpx2, val2 + imm, val1);
}

static inline void vpx2_isa_jmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC += imm
    //===========================================
    //C syntax: registers[PC] = registers[PC] + imm;
    //Pseudocode: PC <- PC + imm
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint32_t imm = vpx2_mem_f32(vpx2);


    vpx2_wreg(vpx2, VPX_RPC, pc + imm);
}
static inline void vpx2_isa_jmpr(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = imm + r1
    //===========================================
    //C syntax: registers[PC] = r1 + imm;
    //Pseudocode: PC <- r1 + imm
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);

    uint32_t val1 = vpx2_rreg(vpx2, r1);


    vpx2_wreg(vpx2, VPX_RPC, val1 + imm);
}

static inline void vpx2_isa_jmps(Vpx2* vpx2){
    //===========================================
    //Jumps (Short) to specific address that is PC += imm
    //===========================================
    //C syntax: registers[PC] = registers[PC] + imm;
    //Pseudocode: PC <- PC + imm
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint32_t imm = vpx2_mem_f16(vpx2); //16 bits instead of 32


    vpx2_wreg(vpx2, VPX_RPC, pc + imm);
}
static inline void vpx2_isa_jmprs(Vpx2* vpx2){
    //===========================================
    //Jumps (Short) to specific address that is PC = imm + r1
    //===========================================
    //C syntax: registers[PC] = r1 + imm;
    //Pseudocode: PC <- r1 + imm
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f16(vpx2); //16 bits instead, less memory usage.

    uint32_t val1 = vpx2_rreg(vpx2, r1);


    vpx2_wreg(vpx2, VPX_RPC, val1 + imm);
}

static inline void vpx2_isa_zjmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 = 0.
    //===========================================
    //C syntax: if(registers[r1] == 0){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 == 0
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);

    if(val1 == 0){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}
static inline void vpx2_isa_ejmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 = r2.
    //===========================================
    //C syntax: if(registers[r1] == registers[r2]){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 == r2
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    if(val1 == val2){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}
static inline void vpx2_isa_nejmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 != r2.
    //===========================================
    //C syntax: if(registers[r1] != registers[r2]){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 != r2
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    if(val1 != val2){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}
static inline void vpx2_isa_gjmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 > r2.
    //===========================================
    //C syntax: if(registers[r1] > registers[r2]){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 > r2
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    if(val1 > val2){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}
static inline void vpx2_isa_gejmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 >= r2.
    //===========================================
    //C syntax: if(registers[r1] >= registers[r2]){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 >= r2
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    if(val1 >= val2){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}
static inline void vpx2_isa_sjmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 < r2.
    //===========================================
    //C syntax: if(registers[r1] < registers[r2]){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 < r2
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    if(val1 < val2){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}
static inline void vpx2_isa_sejmp(Vpx2* vpx2){
    //===========================================
    //Jumps (Long) to specific address that is PC = PC + imm
    //IFF the condition is met: r1 <= r2.
    //===========================================
    //C syntax: if(registers[r1] <= registers[r2]){
    //registers[PC] = registers[PC] + imm;
    //}
    //Pseudocode: PC <- PC + imm : r1 <= r2
    //===========================================

    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Gets PC - 1 to remove opcode index

    
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint8_t r2 = vpx2_mem_f8(vpx2);

    uint32_t imm = vpx2_mem_f32(vpx2);

    //[[ CONDITION CHECK ]]
    uint32_t val1 = vpx2_rreg(vpx2, r1);
    uint32_t val2 = vpx2_rreg(vpx2, r2);

    if(val1 <= val2){
        vpx2_wreg(vpx2, VPX_RPC, pc + imm);
    }
}

static inline void vpx2_isa_cjmp(Vpx2* vpx2){
    //===========================================
    //General conditional jump.
    //Calls corresponding jmp condition instruction depending in provided argument.
    //WARNING: this is almost fully useless. So it may be depracated.
    //===========================================
    //C syntax: switch(condition){case 0: ...}
    //Pseudocode: f(x, y) ?: cond
    //===========================================

    uint8_t con = vpx2_mem_f8(vpx2); //Get condition code.

    switch(con){
        default: vpx2_log_err(vpx2, VPX_ERR_CJMP_INVALID, con); //invalid conditon
        case 0: vpx2_isa_zjmp(vpx2); break;
        case 1: vpx2_isa_ejmp(vpx2); break;
        case 2: vpx2_isa_nejmp(vpx2); break;
        case 3: vpx2_isa_gjmp(vpx2); break;
        case 4: vpx2_isa_gejmp(vpx2); break;
        case 5: vpx2_isa_sjmp(vpx2); break;
        case 6: vpx2_isa_sejmp(vpx2); break;
    }






}

static inline void vpx2_isa_push8(Vpx2* vpx2){
    //===========================================
    //Push 1B value from r1 (LSB) into the stack.
    //===========================================
    //C syntax: append_stack(registers[r1])
    //Pseudocode: mem[sp] <- r1 : sp+=1
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);


    uint32_t val1 = vpx2_rreg(vpx2, r1);

    vpx2_mem_pu8(vpx2, val1); //Push

}
static inline void vpx2_isa_push16(Vpx2* vpx2){
    //===========================================
    //Push 2B value from r1 (LSW) into the stack.
    //===========================================
    //C syntax: append_stack(registers[r1])
    //Pseudocode: mem[sp] <- r1 : sp+=2
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);


    uint32_t val1 = vpx2_rreg(vpx2, r1);

    vpx2_mem_pu16(vpx2, val1); //Push

}
static inline void vpx2_isa_push32(Vpx2* vpx2){
    //===========================================
    //Push 4B value from r1 into the stack.
    //===========================================
    //C syntax: append_stack(registers[r1])
    //Pseudocode: mem[sp] <- r1 : sp+=4
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);


    uint32_t val1 = vpx2_rreg(vpx2, r1);

    vpx2_mem_pu32(vpx2, val1); //Push
}

static inline void vpx2_isa_pop8(Vpx2* vpx2){
    //===========================================
    //Pop value (1B) from stack and write to r1
    //===========================================
    //C syntax: sp--; registers[r1] = mem[sp];
    //Pseudocode: sp-=1 : r1 <- mem[sp]
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);

    uint32_t val1 = vpx2_mem_po8(vpx2);

    vpx2_wreg(vpx2, r1, val1);

}
static inline void vpx2_isa_pop16(Vpx2* vpx2){
    //===========================================
    //Pop value (2B) from stack and write to r1
    //===========================================
    //C syntax: sp-=2; registers[r1] = mem[sp];
    //Pseudocode: sp-=2 : r1 <- mem[sp]
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);

    uint32_t val1 = vpx2_mem_po16(vpx2);

    vpx2_wreg(vpx2, r1, val1);
}
static inline void vpx2_isa_pop32(Vpx2* vpx2){
    //===========================================
    //Pop value (4B) from stack and write to r1
    //===========================================
    //C syntax: sp-=4; registers[r1] = mem[sp];
    //Pseudocode: sp-=4 : r1 <- mem[sp]
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);

    uint32_t val1 = vpx2_mem_po32(vpx2);

    vpx2_wreg(vpx2, r1, val1);
}

static inline void vpx2_isa_call(Vpx2* vpx2){
    //===========================================
    //Call address that is defined by imm + PC
    //after storing return address to the stack.
    //===========================================
    //C syntax: append_stack(registers[PC]); registers[PC] = registers[PC] + imm;
    //Pseudocode: mem[sp] <- PC : PC <- PC + imm 
    //===========================================
    uint32_t rpc = vpx2_rreg(vpx2, VPX_RPC) - 1; //Relative increment PC.
    
    uint32_t imm = vpx2_mem_f32(vpx2);
    uint32_t spc = vpx2_rreg(vpx2, VPX_RPC); //Gets PC after the fetch for next instruction's address.

    vpx2_wreg(vpx2, VPX_RPC, rpc + imm);

    //Push to stack
    vpx2_mem_pu32(vpx2, spc);
}
static inline void vpx2_isa_callr(Vpx2* vpx2){
    //===========================================
    //Call address that is defined by PC = r1 + imm
    //after storing return address to the stack.
    //===========================================
    //C syntax: append_stack(registers[PC]); registers[PC] = registers[r1] + imm;
    //Pseudocode: mem[sp] <- PC : PC <- r1 + imm 
    //===========================================
    uint8_t r1 = vpx2_mem_f8(vpx2);
    uint32_t imm = vpx2_mem_f32(vpx2);
    uint32_t val1 = vpx2_rreg(vpx2, r1);


    uint32_t pc = vpx2_rreg(vpx2, VPX_RPC); //Gets PC after the fetch for next instruction's address.

    vpx2_wreg(vpx2, VPX_RPC, val1 + imm);

    //Push to stack
    vpx2_mem_pu32(vpx2, pc);
}
static inline void vpx2_isa_ret(Vpx2* vpx2){
    //===========================================
    //Return to address that is in the stack. PC = mem[sp]
    //===========================================
    //C syntax: registers[PC] = mem[sp];
    //Pseudocode: PC <- mem[sp];
    //===========================================



    uint32_t pc = vpx2_mem_po32(vpx2);

    vpx2_wreg(vpx2, VPX_RPC, pc); //Return to address.

}





//[[ ISA PRIMARY EXEC ]]


static inline uint8_t vpx2_exec(Vpx2* vpx2){
    //Triggers error on invalid opcode.
    uint8_t opcode = vpx2_mem_f8(vpx2); //Fetch opcode.

    switch(opcode){
        default: {
            //Log error and exit.
            vpx2_log_err(vpx2, VPX_ERR_INVALID_OPCODE, opcode);
            
            return 1; //Error!

        }
        case 0: return 0; //Does nothing, NOP
        case 1: return 255; //Hostcall.
        case 2: vpx2_isa_cpuid(vpx2); break;
        case 3: vpx2_isa_mov(vpx2); break;
        case 4: vpx2_isa_movi(vpx2); break;
        case 5: vpx2_isa_inc(vpx2); break;
        case 6: vpx2_isa_dec(vpx2); break;
        case 7: vpx2_isa_or(vpx2); break;
        case 8: vpx2_isa_xor(vpx2); break;
        case 9: vpx2_isa_and(vpx2); break;
        case 10: vpx2_isa_not(vpx2); break;
        case 11: vpx2_isa_ori(vpx2); break;
        case 12: vpx2_isa_xori(vpx2); break;
        case 13: vpx2_isa_andi(vpx2); break;
        case 14: vpx2_isa_sll(vpx2); break;
        case 15: vpx2_isa_srl(vpx2); break;
        case 16: vpx2_isa_sra(vpx2); break;
        case 17: vpx2_isa_slli(vpx2); break;
        case 18: vpx2_isa_srli(vpx2); break;
        case 19: vpx2_isa_srai(vpx2); break;
        case 20: vpx2_isa_add(vpx2); break;
        case 21: vpx2_isa_sub(vpx2); break;
        case 22: vpx2_isa_mul(vpx2); break;
        case 23: vpx2_isa_udiv(vpx2); break;
        case 24: vpx2_isa_sdiv(vpx2); break;
        case 25: vpx2_isa_urem(vpx2); break;
        case 26: vpx2_isa_srem(vpx2); break;
        case 27: vpx2_isa_addi(vpx2); break;
        case 28: vpx2_isa_subi(vpx2); break;
        case 29: vpx2_isa_muli(vpx2); break;
        case 30: vpx2_isa_udivi(vpx2); break;
        case 31: vpx2_isa_sdivi(vpx2); break;
        case 32: vpx2_isa_uremi(vpx2); break;
        case 33: vpx2_isa_sremi(vpx2); break;
        case 34: vpx2_isa_ld8(vpx2); break;
        case 35: vpx2_isa_ld16(vpx2); break;
        case 36: vpx2_isa_ld32(vpx2); break;
        case 37: vpx2_isa_st8(vpx2); break;
        case 38: vpx2_isa_st16(vpx2); break;
        case 39: vpx2_isa_st32(vpx2); break;
        case 40: vpx2_isa_ld8r(vpx2); break;
        case 41: vpx2_isa_ld16r(vpx2); break;
        case 42: vpx2_isa_ld32r(vpx2); break;
        case 43: vpx2_isa_st8r(vpx2); break;
        case 44: vpx2_isa_st16r(vpx2); break;
        case 45: vpx2_isa_st32r(vpx2); break;
        case 46: vpx2_isa_jmp(vpx2); break;
        case 47: vpx2_isa_jmpr(vpx2); break;
        case 48: vpx2_isa_jmps(vpx2); break;
        case 49: vpx2_isa_jmprs(vpx2); break;
        case 50: vpx2_isa_zjmp(vpx2); break;
        case 51: vpx2_isa_ejmp(vpx2); break;
        case 52: vpx2_isa_nejmp(vpx2); break;
        case 53: vpx2_isa_gjmp(vpx2); break;
        case 54: vpx2_isa_gejmp(vpx2); break;
        case 55: vpx2_isa_sjmp(vpx2); break;
        case 56: vpx2_isa_sejmp(vpx2); break;
        case 57: vpx2_isa_cjmp(vpx2); break;
        case 58: vpx2_isa_push8(vpx2); break;
        case 59: vpx2_isa_push16(vpx2); break;
        case 60: vpx2_isa_push32(vpx2); break;
        case 61: vpx2_isa_pop8(vpx2); break;
        case 62: vpx2_isa_pop16(vpx2); break;
        case 63: vpx2_isa_pop32(vpx2); break;
        case 64: vpx2_isa_call(vpx2); break;
        case 65: vpx2_isa_callr(vpx2); break;
        case 66: vpx2_isa_ret(vpx2); break;
    }
    if(vpx2->err_code){return 1;} //error!
    return 0; //Successful execution
}

//[[ PRIMARY FUNCTIONS ]]

static inline uint8_t vpx2_init(Vpx2* vpx2, uint8_t* mem_ptr, uint32_t mem_size){
    if(mem_ptr == VPXNULL){
        return 1; //Fail
    }
    if(mem_size == 0){
        return 1; //Fail
    }
    vpx2->mem_ptr = mem_ptr;
    vpx2->mem_size = mem_size;

    return 0; //Success


}
static inline uint8_t vpx2_start(Vpx2* vpx2){
    while(1){
        uint8_t rt = vpx2_exec(vpx2);
        if(rt == 1){return 1;} //error exit
        if(rt == 255){return 0;} //hostcall successful exit.

    }
    return 0;


}

//[[ SETUP STRUCT ]]
static inline Vpx2 vpx2_create_instance(){
    Vpx2 vpx2;
    vpx2.cpu_id = 0x2;

    vpx2.err_code = 0;
    vpx2.err_val = 0;
    vpx2.err_pc_state = 0;

    vpx2.mem_ptr = VPXNULL;
    vpx2.mem_size = 0;

    vpx2.registers[VPX_RPC] = 0;
    vpx2.registers[VPX_RSP] = 0;

    return vpx2;

}



//[[ DEFINE MACRO ]]
#define VPX_DEFINED
