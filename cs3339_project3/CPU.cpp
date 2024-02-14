/*
 * This program simulates a MIPS CPU pipeline, designed for educational purposes at Texas State University.
 * It sequentially processes instructions through fetching, decoding, executing, memory access, and writeback stages.
 * Initialized with predefined register values, it executes a variety of MIPS instructions until reaching a stop condition.
 * The simulator provides detailed execution of arithmetic, branching, load/store operations, and includes utilities
 * for displaying execution statistics and register states, offering insights into the pipeline's behavior.
 */

#include "CPU.h"
#include "Stats.h"


/*
 * Initializes a MIPS CPU simulator with predefined register names and values.
 * Sets up the program counter, instruction and data memory, zeroes all registers,
 * and configures `gp` and `sp` with specific start addresses. Ready to simulate
 * MIPS instruction execution, tracking the instruction count and a stop flag.
 */

const string CPU::regNames[] = {"$zero","$at","$v0","$v1","$a0","$a1","$a2","$a3",
                                "$t0","$t1","$t2","$t3","$t4","$t5","$t6","$t7",
                                "$s0","$s1","$s2","$s3","$s4","$s5","$s6","$s7",
                                "$t8","$t9","$k0","$k1","$gp","$sp","$fp","$ra"};

CPU::CPU(uint32_t pc, Memory &iMem, Memory &dMem) : pc(pc), iMem(iMem), dMem(dMem) {
  for(int i = 0; i < NREGS; i++) {
    regFile[i] = 0;
  }
  hi = 0;
  lo = 0;
  regFile[28] = 0x10008000; // gp Global REGISTER
  regFile[29] = 0x10000000 + dMem.getSize(); // sp stack pointer (Store the memory address of the last element added)

  instructions = 0;
  stop = false;
}
/*
 * The `run` method continuously executes the CPU simulation cycle until a stop condition is met,
 * incrementing the instruction count and progressing through the fetch, decode, execute, memory access,
 * and writeback stages, while optionally printing the register file state.
 * 
 * `fetch` loads the next instruction from memory using the current program counter (PC) and then increments the PC.
 * 
 * `decode` extracts the opcode, register specifiers, immediate values, and addresses from the fetched instruction,
 * setting up control signals for execution but initially setting all to false or a default state.
 */

void CPU::run() {
  while(!stop) {
    instructions++;

    fetch();
    decode();
    execute();
    mem();
    writeback();

    D(printRegFile());
  }
}
//prepare to fetch the next instruction
void CPU::fetch() {
  instr = iMem.loadWord(pc);
  pc = pc + 4;
}

void CPU::decode() {
  uint32_t opcode;      // opcode field
  uint32_t rs, rt, rd;  // register specifiers
  uint32_t shamt;       // shift amount (R-type)
  uint32_t funct;       // funct field (R-type)
  uint32_t uimm;        // unsigned version of immediate (I-type)
  int32_t simm;         // signed version of immediate (I-type)
  uint32_t addr;        // jump address offset field (J-type)

  opcode = instr >> 26;
  rs = (instr >> 21) & 0x1f;
  rt = (instr >> 16) & 0x1f;
  rd = (instr >> 11) & 0x1f;
  shamt = (instr >> 6) & 0x1f;
  funct = instr & 0x3f;
  uimm = instr & 0xffff;
  simm = ((signed)uimm << 16) >> 16;
  addr = instr & 0x3ffffff;

  writeDest = false;
  opIsLoad = false;
  opIsStore = false;
  opIsMultDiv = false;
  aluOp = ADD;
  storeData = 0;

/*
 * The decode function interprets the current MIPS instruction, identifying its operation
 * and type in order to prepare it for execution. It handles a variety of MIPS instructions,
 * encompassing arithmetic operations like addition (addu) and subtraction (subu), logical
 * shifts such as left shift (sll) and arithmetic right shift (sra), control flow instructions
 * like jump (j), jump and link (jal), branch on equal (beq), and branch on not equal (bne),
 * as well as memory operations like load word (lw) and store word (sw). Additionally, it manages
 * special instructions like move from hi (mfhi), move from lo (mflo), multiply (mult), and divide (div),
 * which may affect the contents of hi and lo registers or modify the program flow directly with
 * instructions like jump register (jr). If an instruction is not supported, an error message is triggered.
 */

  D(cout << "  " << hex << setw(8) << pc - 4 << ": ");
  switch(opcode) {
    case 0x00:
      switch(funct) {
        //The operation being performed here is a logical left shift (sll).
        case 0x00: D(cout << "sll " << regNames[rd] << ", " << regNames[rs] << ", " << dec << shamt);
        // Indicates that the result of the operation should be written back to a destination register.
              writeDest = true;
              // Specifies the destination register where the result will be stored.
              destReg = rd;
              stats.registerDest(rd);
              //Sets the ALU operation to perform a logical left shift.
              aluOp = SHF_L;
              // Specifies the first operand for the ALU operation as the value stored in the source register rs
              aluSrc1 = regFile[rs];
              //Registers rs as one of the source registers for statistical tracking.
              stats.registerSrc(rs);
              //Specifies the second operand for the ALU operation as the shift amount shamt.
              aluSrc2 = shamt;
             break; 
        case 0x03: D(cout << "sra " << regNames[rd] << ", " << regNames[rs] << ", " << dec << shamt);

              writeDest = true; destReg = rd;
              stats.registerDest(rd);
              aluOp = SHF_R;
              aluSrc1 = regFile[rs];
              stats.registerSrc(rs);
              aluSrc2 = shamt;
             break; 
        case 0x08: D(cout << "jr " << regNames[rs]);
              pc = regFile[rs];
              stats.flush(2);
             break;
        case 0x10: D(cout << "mfhi " << regNames[rd]);
              writeDest = true;
              destReg = rd;
              stats.registerDest(rd);
              aluOp = ADD;
              aluSrc1 = hi;
              stats.registerSrc(REG_HILO);
              aluSrc2 = regFile[REG_ZERO];
             break;
        case 0x12: D(cout << "mflo " << regNames[rd]);
              writeDest = true;
              destReg = rd;
              stats.registerDest(rd);
              aluOp = ADD;
              aluSrc1 = lo;
              stats.registerSrc(REG_HILO);
              aluSrc2 = regFile[REG_ZERO];
             break;
        case 0x18: D(cout << "mult " << regNames[rs] << ", " << regNames[rt]);
              opIsMultDiv = true;
              stats.registerDest(REG_HILO);
              aluOp = MUL;
              aluSrc1 = regFile[rs];
              stats.registerSrc(rs);
              aluSrc2 = regFile[rt];
              stats.registerSrc(rt);
             break;
        case 0x1a: D(cout << "div " << regNames[rs] << ", " << regNames[rt]);
              opIsMultDiv = true;
              stats.registerDest(REG_HILO);
              aluOp = DIV;
              aluSrc1 = regFile[rs];
              stats.registerSrc(rs);
              aluSrc2 = regFile[rt];
              stats.registerSrc(rt);
              break;
        case 0x21: D(cout << "addu " << regNames[rd] << ", " << regNames[rs] << ", " << regNames[rt]);
              writeDest = true;
              destReg = rd;
              stats.registerDest(rd);
              aluOp = ADD;
              aluSrc1 = regFile[rs];
              stats.registerSrc(rs);
              aluSrc2 = regFile[rt];
              stats.registerSrc(rt);
             break;
        case 0x23: D(cout << "subu " << regNames[rd] << ", " << regNames[rs] << ", " << regNames[rt]);
              writeDest = true;
              destReg = rd;
              stats.registerDest(rd);
              aluOp = ADD;
              aluSrc1 = regFile[rs];
              stats.registerSrc(rs);
              aluSrc2 = -regFile[rt];
              stats.registerSrc(rt);
             break; //hint: subtract is the same as adding a negative
        case 0x2a: D(cout << "slt " << regNames[rd] << ", " << regNames[rs] << ", " << regNames[rt]);
              writeDest = true;
              destReg = rd;
              aluOp = CMP_LT;
              aluSrc1 = regFile[rs];
              stats.registerSrc(rs);
              aluSrc2 = regFile[rt];
              stats.registerSrc(rt);
             break;
        default: cerr << "unimplemented instruction: pc = 0x" << hex << pc - 4 << endl;
      }
      break;
    case 0x02: D(cout << "j " << hex << ((pc & 0xf0000000) | addr << 2)); // P1: pc + 4
             writeDest = false;
             pc = (pc & 0xf0000000) | addr << 2;
             stats.flush(2);
             break;
    case 0x03: D(cout << "jal " << hex << ((pc & 0xf0000000) | addr << 2)); // P1: pc + 4
          writeDest = true;
          destReg = REG_RA;
          stats.registerDest(REG_RA);
          aluOp = ADD;
          aluSrc1 = pc;
          aluSrc2 = regFile[REG_ZERO];
          pc = (pc & 0xf0000000) | addr << 2;
          stats.flush(2);
               break;
    case 0x04: D(cout << "beq " << regNames[rs] << ", " << regNames[rt] << ", " << pc + (simm << 2));
               stats.countBranch();
               stats.registerSrc(rs);
               stats.registerSrc(rt);
               if (regFile[rs] == regFile[rt])
               pc = pc + (simm << 2);
               stats.countTaken();
               stats.flush(2);
          break;  // read the handout carefully, update PC directly here as in jal example
    case 0x05: D(cout << "bne " << regNames[rs] << ", " << regNames[rt] << ", " << pc + (simm << 2));
               istats.countBranch();
               stats.registerSrc(rs);
               stats.registerSrc(rt);
               if (regFile[rs] != regFile[rt])
               pc = pc + (simm << 2);
               stats.countTaken();
               stats.flush(2);
               break;  // same comment as beq
    case 0x09: D(cout << "addiu " << regNames[rt] << ", " << regNames[rs] << ", " << dec << simm);
               writeDest = true;
               destReg = rt;
               stats.registerDest(rt);
               aluOp = ADD;
               aluSrc1 = regFile[rs];
               stats.registerSrc(rs);
               aluSrc2 = simm;
               break;
    case 0x0c: D(cout << "andi " << regNames[rt] << ", " << regNames[rs] << ", " << dec << uimm);
               writeDest = true;
               destReg = rt;
               stats.registerDest(rt);
               aluOp = AND;
               aluSrc1 = regFile[rs];
               stats.registerSrc(rs);
               aluSrc2 = uimm;
               break;
    case 0x0f: D(cout << "lui " << regNames[rt] << ", " << dec << simm);
               writeDest = true;
               stats.registerDest(rt);
               aluOp = SHF_L;
               aluSrc1 = simm;
               aluSrc2 = 16;
               break; //use the ALU to execute necessary op, you may set aluSrc2 = xx directly
    case 0x1a: D(cout << "trap " << hex << addr);
               switch(addr & 0xf) {
                 case 0x0: cout << endl; break;
                 case 0x1: cout << " " << (signed)regFile[rs];
                           break;
                 case 0x5: cout << endl << "? "; cin >> regFile[rt];
                           break;
                 case 0xa: stop = true; break;
                 default: cerr << "unimplemented trap: pc = 0x" << hex << pc - 4 << endl;
                          stop = true;
               }
               break;
    case 0x23: D(cout << "lw " << regNames[rt] << ", " << dec << simm << "(" << regNames[rs] << ")");
                 opIsLoad = true;
                 stats.countMemOp();
                 writeDest = true;
                 destReg = rt;
                 stats.registerDest(rt);
                 aluOp = ADD;
                 aluSrc1 = regFile[rs];
                 stats.registerSrc(rs);
                 aluSrc2 = simm;
               break;  // do not interact with memory here - setup control signals for mem()
    case 0x2b: D(cout << "sw " << regNames[rt] << ", " << dec << simm << "(" << regNames[rs] << ")");
                 opIsStore = true;
                 stats.countMemOp();
                 storeData = regFile[rt];
                 stats.registerSrc(rt);
                 aluOp = ADD;
                 aluSrc1 = regFile[rs];
                 stats.registerSrc(rs);
                 aluSrc2 = simm;
               break;  // same comment as lw
    default: cerr << "unimplemented instruction: pc = 0x" << hex << pc - 4 << endl;
  }
  D(cout << endl);
}

/*
 * In `execute`, the ALU performs the operation defined by `aluOp` on `aluSrc1` and `aluSrc2`, storing the result in `aluOut`.
 * `mem` handles memory operations: loading data from memory if `opIsLoad` is true, or storing data into memory if `opIsStore`.
 * During `writeback`, results are written to the destination register unless it's the zero register. For multiply/divide, updates `hi`/`lo`.
 * `printRegFile` displays the current state of all registers, while `printFinalStats` provides execution statistics including cycles, CPI, and branch efficiency.
 */


void CPU::execute() {
  aluOut = alu.op(aluOp, aluSrc1, aluSrc2);
}

void CPU::mem() {
  if(opIsLoad)
    writeData = dMem.loadWord(aluOut);
  else
    writeData = aluOut;

  if(opIsStore)
    dMem.storeWord(storeData, aluOut);
}

void CPU::writeback() {
  if(writeDest && destReg > 0) // skip when write is to zero_register
    regFile[destReg] = writeData;
  
  if(opIsMultDiv) {
    hi = alu.getUpper();
    lo = alu.getLower();
  }
}

void CPU::printRegFile() {
  cout << hex;
  for(int i = 0; i < NREGS; i++) {
    cout << "    " << regNames[i];
    if(i > 0) cout << "  ";
    cout << ": " << setfill('0') << setw(8) << regFile[i];
    if( i == (NREGS - 1) || (i + 1) % 4 == 0 )
      cout << endl;
  }
  cout << "    hi   : " << setfill('0') << setw(8) << hi;
  cout << "    lo   : " << setfill('0') << setw(8) << lo;
  cout << dec << endl;
}

void CPU::printFinalStats() {

  cout << "Program finished at pc = 0x" << hex << pc << "  (" << dec << instructions << " instructions executed)" << endl;
  cout << "Cycles: " << stats.getCycles() << endl;
  cout << "CPI: " << fixed << setprecision(2) << (float)stats.getCycles() / instructions << endl;
  cout << "Bubbles: " << stats.getBubbles() << endl;
  cout << "Flushes: " << stats.getFlushes() << endl;
  cout << "Mem ops: " << setprecision(1) << 100.0 * stats.getMemOps() / instructions << "% of instructions" << endl;
  cout << "Branches: " << 100.0 * stats.getBranches() / instructions << "% of instructions" << endl;
  cout << "  % Taken: " << 100.0 * stats.getTaken() / stats.getBranches() << endl;
}



