/*
 * Z16 Instruction Set Simulator (ISS)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Mohamed Shalan
 *
 * This simulator accepts a Z16 binary machine code file (with a .bin extension)
 * and assumes that the first instruction is located at memory address 0x0000.
 * It decodes each 16-bit instruction into a human-readable string and prints it,
 * then executes the instruction by updating registers, memory,
 * or performing I/O via ecall.
 *
 * Supported ecall services:
 * - ecall 1: Print an integer (value in register a0).
 * - ecall 5: Print a NULL-terminated string (address in register a0).
 * - ecall 3: Terminate the simulation.
 *
 * Usage:
 * z16sim <machine_code_file_name>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MEM_SIZE 65536 // 64KB memory

// Global simulated memory and register file.
unsigned char memory[MEM_SIZE];
uint16_t regs[8]; // 8 registers (16-bit each): x0, x1, x2, x3, x4, x5, x6, x7
uint16_t pc = 0;  // Program counter (16-bit)

// Register ABI names for display (x0 = t0, x1 = ra, x2 = sp, x3 = s0, x4 = s1, x5 = t1, x6 = a0, x7 = a1)
const char *regNames[8] = {"t0", "ra", "sp", "s0", "s1", "t1", "a0", "a1"};

// -----------------------
// Disassembly Function
// -----------------------
//
// Decodes a 16-bit instruction 'inst' (fetched at address 'pc') and writes a human-readable
// string to 'buf' (of size bufSize). This decoder uses the opcode (bits [2:0]) to distinguish
// among R-, I-, B-, L-, J-, U-, and System instructions.
void disassemble(uint16_t inst, uint16_t pc, char *buf, size_t bufSize) {
    uint8_t opcode = inst & 0x7;
    switch (opcode) {
        case 0x0: { // R-type: [15:12] funct4 | [11:9] rs2 | [8:6] rd/rs1 | [5:3] funct3 | [2:0] opcode
            uint8_t funct4 = (inst >> 12) & 0xF;
            uint8_t rs2 = (inst >> 9) & 0x7;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (funct4 == 0x0 && funct3 == 0x0)
                printf("add %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x1 && funct3 == 0x0)
                printf("sub %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x2 && funct3 == 0x1)
                printf("slt %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x3 && funct3 == 0x2)
                printf("sltu %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x4 && funct3 == 0x3)
                printf("sll %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x5 && funct3 == 0x3)
                printf("srl %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x6 && funct3 == 0x3)
                printf("sra %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x7 && funct3 == 0x4)
                printf("or %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x8 && funct3 == 0x5)
                printf("and %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0x9 && funct3 == 0x6)
                printf("xor %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0xA && funct3 == 0x7)
                printf("mv %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if (funct4 == 0xB && funct3 == 0x0)
                printf("jr %s", regNames[rd_rs1]);
            else if (funct4 == 0xC && funct3 == 0x0)
                printf("jalr %s, %s", regNames[rd_rs1], regNames[rs2]);
            break;
        }
        case 0x1: { // I-type: [15:9] imm[6:0] | [8:6] rd/rs1 | [5:3] funct3 | [2:0] opcode
            uint8_t imm7 = (inst >> 9) & 0xF;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (funct3 == 0x0)
                printf("addi %s, %i", regNames[rd_rs1], imm7);
            else if (funct3 == 0x1)
                printf("slti %s, %i", regNames[rd_rs1], imm7);
            else if (funct3 == 0x2)
                printf("sltui %s, %i", regNames[rd_rs1], imm7);
            else if (funct3 == 0x3){
                uint8_t shamt_mode = (imm7 >> 4) & 0x7;  
                uint8_t shamt = imm7 & 0xF;            

                if (shamt_mode == 0x1)
                    printf("slli %s, %u", regNames[rd_rs1], shamt);
                else if (shamt_mode == 0x2)
                    printf("srli %s, %u", regNames[rd_rs1], shamt);
                else if (shamt_mode == 0x4)
                    printf("srai %s, %u", regNames[rd_rs1], shamt);
                else
                    printf("unknown shift %s, imm=0x%02X", regNames[rd_rs1], imm7);

            }else if (funct3 == 0x4)
                printf("ori %s, %i", regNames[rd_rs1], imm7);
            else if (funct3 == 0x5)
                printf("andi %s, %i", regNames[rd_rs1], imm7);
            else if (funct3 == 0x6)
                printf("xori %s, %i", regNames[rd_rs1], imm7);
            else if (funct3 == 0x7)
                printf("li %s, %i", regNames[rd_rs1], imm7);

            break;
        }
        case 0x2: { // B-type (branch): [15:12] offset[4:1] | [11:9] rs2 | [8:6] rs1 | [5:3] funct3 | [2:0] opcode
            uint8_t offset = (inst >> 12) & 0xF;
            uint8_t rs2 = (inst >> 9) & 0x7;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (funct3 == 0x0)
                printf("beq %s, %s, %i", regNames[rd_rs1], regNames[rs2],offset);
            else if (funct3 == 0x1)
                printf("bne %s, %s, %i", regNames[rd_rs1], regNames[rs2], offset);
            else if (funct3 == 0x2)
                printf("bz %s, %i", regNames[rd_rs1], offset); // rs2 ignored
            else if (funct3 == 0x3)
                printf("bnz %s, %i", regNames[rd_rs1], offset); // rs2 ignored
            else if (funct3 == 0x4)
                printf("blt %s, %s, %i", regNames[rd_rs1], regNames[rs2], offset);
            else if (funct3 == 0x5)
                printf("bge %s, %s, %i", regNames[rd_rs1], regNames[rs2], offset);
            else if (funct3 == 0x6)
                printf("bltu %s, %s, %i", regNames[rd_rs1], regNames[rs2], offset);
            else if (funct3 == 0x7)
                printf("bgeu %s, %s, %i", regNames[rd_rs1], regNames[rs2], offset);

            break;
        }
        case 0x3: { // S-type: [15:12] imm[3:0] | [11:9] rs2 | [8:6] rs1 | [5:3] func3 | [2:0] opcode
            uint8_t offset = (inst >> 12) & 0xF;
            uint8_t rs2 = (inst >> 9) & 0x7;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (funct3 == 0x0)
                printf("sb %s, %i(%s)", regNames[rd_rs1], offset, regNames[rs2]);
            else if (funct3 == 0x1)
                printf("sw %s, %i(%s)", regNames[rd_rs1], offset, regNames[rs2]);
          
            break;
        }
        case 0x4: { // L-type: [15:12] imm[3:0] | [11:9] rs2 | [8:6] rd | [5:3] func3 | [2:0] opcode
            uint8_t offset = (inst >> 12) & 0xF;
            uint8_t rs2 = (inst >> 9) & 0x7;
            uint8_t rd = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (funct3 == 0x0)
                printf("lb %s, %i(%s)", regNames[rd], offset, regNames[rs2]);
            else if (funct3 == 0x1)
                printf("lw %s, %i(%s)", regNames[rd], offset, regNames[rs2]);
            else if (funct3 == 0x4)
                printf("lbu %s, %i(%s)", regNames[rd], offset, regNames[rs2]);
          
            break;
        }
        case 0x5: { // J-type: [15] link flag | [14:9] imm2[9:4] | [8:6] rd | [5:3] imm1[3:1] | [2:0] opcode
            uint8_t flag = (inst >> 15) & 0x1;
            uint8_t rd = (inst >> 6) & 0x7;
            uint8_t imm_high = (inst >> 9) & 0x3F;   // imm[9:4]
            uint8_t imm_low  = (inst >> 3) & 0x7;    // imm[3:1]
            uint16_t imm = (imm_high << 4) | (imm_low << 1);  // add the LSB 0

            if (flag == 0x0)
                printf("j %i", imm);
            else if (flag == 0x1)
                printf("jal %s, %i", regNames[rd], imm);
          
            break;
        }
        case 0x6: { // U-type: [15] link flag | [14:9] imm2[15:10] | [8:6] rd | [5:3] imm1[9:7] | [2:0] opcode
            uint8_t flag = (inst >> 15) & 0x1;
            uint8_t rd = (inst >> 6) & 0x7;
            uint8_t imm_high = (inst << 9) & 0x3F;   // imm[15:10]
            uint8_t imm_low  = (inst >> 3) & 0x7;    // imm[3:1]
            uint16_t imm = (imm_high << 10) | (imm_low << 7);  // add the LSB 0

            if (flag == 0x0)
                printf("lui %s, %i", regNames[rd], imm);
            else if (flag == 0x1)
                printf("auipc %s, %i", regNames[rd], imm);
          
            break;
        }
        case 0x7: { // SYS-type: [15:6] svc (10-bit system-call number) | [5:3] 000 | [2:0] opcode
            uint8_t svc = (inst >> 6) & 0x1FF;
            printf("ecall %i", svc);
            break;
        }

        default:
            snprintf(buf, bufSize, "Unknown opcode");
            break;
    }
}

// -----------------------
// Instruction Execution
// -----------------------
//
// Executes the instruction 'inst' (a 16-bit word) by updating registers, memory, and PC.
// Returns 1 to continue simulation or 0 to terminate (if ecall 3 is executed).
int executeInstruction(uint16_t inst) {
    uint8_t opcode = inst & 0x7;
    int pcUpdated = 0; // flag: if instruction updated PC directly

    switch (opcode) {
        case 0x0: { // R-type
            uint8_t funct4 = (inst >> 12) & 0xF;
            uint8_t rs2 = (inst >> 9) & 0x7;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (funct4 == 0x0 && funct3 == 0x0) // add
                regs[rd_rs1] = regs[rd_rs1] + regs[rs2];
            else if (funct4 == 0x1 && funct3 == 0x0) // sub
                regs[rd_rs1] = regs[rd_rs1] - regs[rs2];
            // complete the rest
            break;
        }
        case 0x1: { // I-type
            uint8_t imm7 = (inst >> 9) & 0x7F;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;
            int16_t simm = (imm7 & 0x40) ? (imm7 | 0xFF80) : imm7;
            // your code goes here
            break;
        }
        case 0x2: { // B-type (branch)
            // your code goes here
            break;
        }
        case 0x3: { // L-type (load/store)
            // your code goes here
            break;
        }
        case 0x5: { // J-type (jump)
            // your code goes here
            break;
        }
        case 0x6: { // U-type
            // your code goes here
            break;
        }
        case 0x7: { // System instruction (ecall)
            uint8_t svc = (inst >> 6) & 0x1FF;

            
            if (svc == 0x3FF)  return 0; 
            break;
        }
        default:
            printf("Unknown instruction opcode 0x%X\n", opcode);
            break;
    }

    if (!pcUpdated)
        pc += 2; // default: move to next instruction
    return 1;
}

// -----------------------
// Memory Loading
// -----------------------
//
// Loads the binary machine code image from the specified file into simulated memory.
void loadMemoryFromFile(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening binary file");
        exit(1);
    }
    size_t n = fread(memory, 1, MEM_SIZE, fp);
    fclose(fp);
    printf("Loaded %zu bytes into memory\n", n);
}

// -----------------------
// Main Simulation Loop
// -----------------------
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <machine_code_file>\n", argv[0]);
        exit(1);
    }

    loadMemoryFromFile(argv[1]);
    memset(regs, 0, sizeof(regs)); // initialize registers to 0
    pc = 0; // starting at address 0

    char disasmBuf[128];

    while (pc < MEM_SIZE) {
        // Fetch a 16-bit instruction from memory (little-endian)
        uint16_t inst = memory[pc] | (memory[pc + 1] << 8);
        printf("0x%04X: ", pc);
        disassemble(inst, pc, disasmBuf, sizeof(disasmBuf));
        printf("\n");

        if (!executeInstruction(inst))
            break;

        // Terminate if PC goes out of bounds
        if (pc >= MEM_SIZE)
            break;
    }

    return 0;
}
