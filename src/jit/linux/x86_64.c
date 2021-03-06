#define _GNU_SOURCE //mremap
#include <sys/mman.h> //mmap
#include <string.h> //memcpy
#include <stdlib.h> //calloc - realloc

#include "../compiler.h"
#include "../../dynarray.h"

// Currently only supports x86_64
// Gotta do this more modular.

#define append_const(x)\
    {\
        const char opcode[] = x;\
        dynarray_append_array(array, (void*)opcode, sizeof(opcode) - 1);\
    } 

#define append_int(x, type)\
    {\
        type val = x;\
        dynarray_append_array(array, &val, sizeof(val));\
    }

void write_instructions(dynarray_t *array, bf_instruction_t **c_instruction) {
    while (1) {
        switch ((*c_instruction)->type)
        {
                                // End of the function: pops all the registers.
        case BF_INSTR_EMPTY:    append_const(
                                    "\x5e"
                                    "\x5f" 
                                    "\x5a"
                                    "\x59"
                                    "\x5b"
                                    "\x58"
                                    "\x5d"
                                    "\xc3" // ret
                                );
                                return;

                                // add ax, <data>
        case BF_INSTR_RIGHT:    append_const("\x66\x81\xc1"); 
                                append_int((*c_instruction)->repeat_for + 1, u_int16_t);
                                break;

                                // sub ax, <data>
        case BF_INSTR_LEFT:     append_const("\x66\x81\xe9");
                                append_int((*c_instruction)->repeat_for + 1, u_int16_t);
                                break;

                                // movzx rcx, byte [rbx+rax] 
        case BF_INSTR_ADD:      append_const("\x80\x04\x0b");
                                append_int((*c_instruction)->repeat_for + 1, u_int8_t);
                                break;

                                // movzx rcx, byte [rbx+rax] 
        case BF_INSTR_SUB:      append_const("\x80\x2c\x0b");
                                append_int((*c_instruction)->repeat_for + 1, u_int8_t);
                                break;

        case BF_INSTR_PUTC:     append_const(
                                    // lea rsi, [rbx+rcx]
                                    "\x48\x8d\x34\x0b"
                                    // mov rax, 0x1
                                    "\x48\xc7\xc0\x01\x00\x00\x00"
                                    // mov rdi, 0x1
                                    "\x48\xc7\xc7\x01\x00\x00\x00"
                                    // mov rdx,0x1
                                    "\x48\xc7\xc2\x01\x00\x00\x00"
                                    // push rcx (gotta change register since this is being used by the syscall as well)
                                    "\x51" 
                                    // syscall
                                    "\x0f\x05"
                                    // pop rcx
                                    "\x59"
                                );
                                break;

        case BF_INSTR_GETC:     append_const(
                                    // lea rsi, [rbx+rcx]
                                    "\x48\x8d\x34\x0b"
                                    // mov rax, 0x1
                                    "\x48\xc7\xc0\x00\x00\x00\x00"
                                    // mov rdi, 0x1
                                    "\x48\xc7\xc7\x00\x00\x00\x00"
                                    // mov rdx,0x1
                                    "\x48\xc7\xc2\x01\x00\x00\x00"
                                    // push rcx (gotta change register since this is being used by the syscall as well)
                                    "\x51" 
                                    // syscall
                                    "\x0f\x05"
                                    // pop rcx
                                    "\x59"
                                );
                                break;
                    
        
        case BF_INSTR_LOOP_S:   *c_instruction = &(*c_instruction)[1];

                                append_const(
                                    // movzx rax, [rbx+rcx]
                                    "\x48\x0f\xb6\x04\x0b" 
                                    // test al, al
                                    "\x84\xc0"
                                )

                                append_const("\x0f\x84");
                                append_int(0, int32_t);

                                // All the following hardcoded numbers are related to instruction sizes.

                                int j_address_pos = array->count - 4;
                                write_instructions(array, c_instruction);

                                int current_pos = array->count;
                                append_const("\xe9");
                                append_int((int)((j_address_pos - 9) - (current_pos + 5)), int32_t)

                                *((int*)(array->data + j_address_pos)) = (current_pos + 5) - ((j_address_pos - 2) + 6);

                                break;

        case BF_INSTR_LOOP_E:   return;
        
        case BF_OPT_LOOP_TO_ZERO: append_const("\xc6\x04\x0b\x00");
        }
        *c_instruction = &(*c_instruction)[1];
    }   
}

void bf_jit(bf_vm* vm, bf_instruction_t* c_instruction) {
    dynarray_t* array = dynarray_new(sizeof(char));

    // Stack initialisation

    append_const(
        "\x55"          // push rbp
        "\x48\x89\xE5"  // mov rbp, rsp
        
        "\x50"          // push rax
        "\x53"          // push rbx
        "\x51"          // push rcx
        "\x52"          // push rdx
        "\x57"          // push rdi
        "\x56"          // push rsi

        "\x48\x31\xc9"  // xor rcx, rcx
    );

    // movabs rbx, <cells_address>
    append_const("\x48\xBB");       
    append_int((u_int64_t)&vm->cells[0], u_int64_t);

    write_instructions(array, &c_instruction);

    void (*fn)(void);
    fn = mmap(NULL, array->count, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    memcpy(fn, array->data, array->count);

    fn();

    munmap(fn, array->count);
    dynarray_free(array);
}