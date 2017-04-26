#include <stdio.h>
#include <string.h>

// some global data
int mem[0x8000];
unsigned int instruction;
int clock=1;
int pc=0;
int reg[32];
// how about putting ~flags into control signal structure?
int R_flag=0;
int J_flag=0;
int I_flag=0;

struct control_signal {
	int RegDest;
	int Jump;
	int Branch;
	int MemRead;
	int MemtoReg;
	int AluOp;
	int MemWrite;
	int AluSrc;
	int RegWrite;
};

struct r_inst {
	int readreg1;
	int readreg2;
	int writereg;
	int read1;
	int read2;
	// I like the original presentation
	int rs;
	int rt;
	unsigned int opcode;
	unsigned int func;
	unsigned int shamt;
	signed short imm; // note the type, here.
	int simm;
};

struct m_inst {
	int address;
	int memwrite;
	int readmem;
};

struct j_inst {
	int imm_j;
	int target_j;
	int target_brq;
};

struct inst {
	struct control_signal c;
	struct r_inst r;
	struct m_inst m;
	struct j_inst j;
};

// for every cycle, initialize instruction-rel data/control structure
void initialize(struct inst *in)
{
	memset(&in->c, 0, sizeof(struct control_signal));
	R_flag = 0;
	I_flag = 0;
	J_flag = 0;
}

// fetch instruction from mem
int fetch()
{
	if (pc == -1) return -1;
	// TERMINAL condition. if pc == -1
	else {
		// get instruction from memory
		instruction = mem[pc/4];
		printf("IF: pc: 0x%08x, 0x%08x\n", pc, mem[pc/4]);
		return 0;
	}
}

// decode instruction and gen. control signals
void decode(struct inst *in)
{
	in->r.opcode = (instruction & 0xFC000000) >> 26;
	if (in->r.opcode == 0x0) {
		// R-type instruction
		R_flag = 1;
		in->r.func = instruction & 0x3F;
		in->r.shamt = (instruction & 0x7C0) >> 6;
		in->c.RegDest = 1;
	}

	// I-type instructions
	if (in->r.opcode != 0 && in->r.opcode != 0x4 && in->r.opcode != 0x5) {
		I_flag = 1;
		in->c.AluSrc = 1;
	}
	// LW
	if (in->r.opcode == 0x23) {
		in->c.MemtoReg = 1;
		in->c.MemRead = 1;
	}
	// some more control signals
	if ( (in->r.opcode != 0x2b) && (in->r.opcode != 0x4) && (in->r.opcode != 0x5) &&
		(in->r.opcode != 0x2) && (in->r.func != 0x8) )
		in->c.RegWrite = 1;
	if (in->r.opcode == 0x2b)
		in->c.MemWrite = 1;
	if ( (in->r.opcode == 0x2) || (in->r.opcode == 0x3) ) {
		in->c.Jump = 1;
		I_flag = 0;
		J_flag = 1;
	}
	if ( (in->r.opcode == 0x4) || (in->r.opcode == 0x5) ) {
		in->c.Branch = 1;
		I_flag = 1;
	}

	// commonly used data
	in->r.rs = (instruction & 0x3E00000) >> 21;
	in->r.rt = (instruction & 0x1F0000) >> 16;

	if (in->c.RegDest == 1)
		// this is rd
		in->r.writereg = (instruction & 0xF800) >> 11;
	else
		in->r.writereg = in->r.rt;

	in->r.imm = (instruction & 0xFFFF);
	in->r.simm = in->r.imm;
	in->r.read1 = reg[in->r.rs];
	in->r.read2 = reg[in->r.rt];

	in->j.imm_j = (instruction & 0x3FFFFFF);
	in->j.target_j = in->j.imm_j << 2;
	in->j.target_brq = in->r.simm << 2;

	return;
}

// I omit the comments because the code/printout are obvious.
int execute(struct inst* in)
{
	printf("EX: ");
	if (in->c.AluSrc == 1) {
		switch (in->r.opcode) {
		case 0x8:
		case 0x9:
			printf("R[ %d:rt ] = R[ %d:rs ] + 0x%x:imm (0x%08x) \n", 
					in->r.rt, in->r.rs, in->r.simm , in->r.read1+in->r.simm);
			return (in->r.read1+in->r.simm);
		case 0xc:
			printf("R[ %d:rt ]  = R[ %d:rs ] & 0x%x:ZeroExtImm (0x%08x) \n",
					in->r.rt, in->r.rs, in->r.simm, (in->r.read1 & (0x0000FFFF & in->r.imm)));
			return (in->r.read1 & (0x0000FFFF & in->r.imm));
		case 0x23:
		case 0x30:
			printf("R[ %d:rt ] = M[R[ %d:rs  ] + 0x%x:imm (0x%08x) ] \n",
				in->r.rt, in->r.rs, in->r.simm, (in->r.read1+in->r.simm));
			return (in->r.read1+in->r.simm);
		case 0xf:
			printf("R[ %d:rt ] = {0x%x:imm, 16'b0 } (0x%08x) \n", 
				in->r.rt, in->r.simm, (in->r.simm<<16));
			return (in->r.simm<<16);
		case 0xd:
			printf("R[ %d:rt ] = R[ %d:rs] | 0x%x:ZeroExtImm (0x%08x) \n", 
					in->r.rt, in->r.rs, (0xFFFF & in->r.imm), (in->r.read1 | (0xFFFF & in->r.imm)));
			return (in->r.read1 | (0xFFFF & in->r.imm));
		case 0x2b:
			printf("M[R[ %d:rs  ] + 0x%x:imm (0x%08x) ]  = R[ %d:rt ]\n",
				in->r.rs, in->r.simm, (in->r.read1+in->r.simm),in->r.rt);
			return (in->r.read1+in->r.simm);
		case 0xa:
		case 0xb:
			printf("R[ %d:rt ] = R[ %d:rs ] < 0x%x:SignExtImm) ? 1 : 0 (%d) \n",
				in->r.rt, in->r.rs, in->r.simm, (in->r.read1 < in->r.simm) ?1:0);
			return (in->r.read1 < in->r.simm) ? 1 : 0;
		}
	}

	else if ( in->c.AluSrc == 0) {
		if ( in->r.opcode == 4) {
			printf("if (R[rs] == R[rt] (%d) ) pc = pc+4+BranchAddr \n",
				(in->r.read1 == in->r.read2) ? 1 : 0);
			return (in->r.read1 == in->r.read2) ? 1 : 0;
		}
		else if (in->r.opcode == 5) {
			printf("if (R[rs] != R[rt] (%d) ) pc = pc+4+BranchAddr \n",
				(in->r.read1 != in->r.read2) ? 1 : 0);
			return (in->r.read1 != in->r.read2) ? 1 : 0;
		} else {
		switch(in->r.func) {
			case 0x0:
				printf("R[ %d:rd ] = R[ %d:rt ] << 0x%x: shamt (0x%08x) \n", 
						in->r.writereg, in->r.rt, in->r.shamt, in->r.read2 << in->r.shamt);
				return in->r.read2 << in->r.shamt;
			case 0x2:
				printf("R[ %d:rd ] = R[ %d:rt ] >> 0x%x: shamt (0x%08x)\n", 
						in->r.writereg, in->r.rt, in->r.shamt, in->r.read2 >> in->r.shamt);
				return in->r.read2 >> in->r.shamt;
			case 0x08:
				printf("PC = R[ %d:rs ] (0x%08x) \n", in->r.rs, in->r.read1);
				return in->r.read1;

			case 0x20:
			case 0x21:
				printf("R[ %d:rd ] = R[ %d:rs ] + R[ %d:rt ] (0x%08x) \n", 
						in->r.writereg, in->r.rs, in->r.rt, in->r.read1 + in->r.read2);
				return in->r.read1 + in->r.read2;
			case 0x22:
			case 0x23:
				printf("R[ %d:rd ] = R[ %d:rs ] - R[ %d:rt ] (0x%08x) \n", 
						in->r.writereg, in->r.rs, in->r.rt, in->r.read1 - in->r.read2);
				return in->r.read1 - in->r.read2;
			case 0x24:
				printf("R[ %d:rd ] = R[ %d:rs ] & R[ %d:rt ] (0x%08x)\n", 
						in->r.writereg, in->r.rs, in->r.rt, in->r.read1 & in->r.read2);
				return (unsigned)in->r.read1 & (unsigned)in->r.read2;
			case 0x25:
				printf("R[ %d:rd ] = R[ %d:rs ] | R[ %d:rt ] (0x%08x)\n", 
						in->r.writereg, in->r.rs, in->r.rt, ((unsigned)in->r.read1 | (unsigned)in->r.read2));
				return ((unsigned)in->r.read1 | (unsigned)in->r.read2);
			case 0x27:
				printf("R[ %d:rd ] = ~(R[ %d:rs ] | R[ %d:rt ]) (0x%08x)\n", 
						in->r.writereg, in->r.rs, in->r.rt, ~((unsigned)in->r.read1 | (unsigned)in->r.read2));
				return ~((unsigned)in->r.read1 | (unsigned)in->r.read2);
			case 0x2a:
			case 0x2b:
				printf("R[ %d:rd ] = (R[ %d:rs ] < R[ %d:rt ]) ? 1 : 0 (%d)\n", 
						in->r.writereg, in->r.rs, in->r.rt, 
						(in->r.read1 < in->r.read2) ? 1 : 0);
				return (in->r.read1 < in->r.read2) ? 1 : 0;
		}
	}}
}

int load_store (struct inst *in, int mem_address)
{
	printf("MM: ");
	// assume access mem_address
	in->m.address = mem_address;
	if (in->c.MemWrite == 1) {
		printf("Mem[0x%x] <= 0x%x\n", in->m.address, in->r.read2);
		mem[in->m.address/4] = in->r.read2;
		// should be don't care
		return 0;
	} else {
		// read mem unless it is not MemWrite inst.
		in->m.readmem = mem[in->m.address/4];
		printf("Mem[0x%x] => 0x%x\n", in->m.address, in->m.readmem);
		if (in->c.MemRead == 1) return in->m.readmem;
		// if it is not readmem, redirect WBresult back to writeback
		else return mem_address;
	}
}

void writeback(struct inst *in, int result) {
	if (in->r.opcode == 3) {
		// jal writes register 31
		reg[31] = pc+8;
		printf("reg[31] = 0x%x \n", reg[31]);
		pc = (in->j.target_j);
		printf("jal 0x%x \n", in->j.target_j);
		return;
	}
	if (in->c.RegWrite == 1) {
		// update reg only when it is necessary
		reg[in->r.writereg] = result;
		printf("WB: reg[%d] = 0x%x \n", in->r.writereg, result);
	}
	// all jumps are handled here.
	if (in->r.opcode == 0 && in->r.func == 8) {
		// jump register
		pc = in->r.read1;
		return;
	}
	if (in->c.Jump == 1)
		pc = in->j.target_j;
	else if (in->c.Branch && result == 1) {
		// jump only when the condition is met
		printf("branch to 0x%x\n", pc+4+in->j.target_brq);
		pc = pc + 4 + in->j.target_brq;
	} else
		// default fall-through
		pc = pc+4;
}

int initialize_sim(int argc, char * argv[])
{
	FILE *fp = NULL;
	int data1 = 0;
	int data2 = 0;
	int data3 = 0;
	int data4 = 0;
	int res = 0;
	int i = 0;

	// init some data
	memset(reg, 0, sizeof (reg));
	memset(mem, 0, sizeof (mem));
	reg[29] = 0x8000;
	reg[31] = -1;

	// open and read in code from file
	if (argc != 2) {
		printf("usage: mips_sim input.bin\n");
		return -1;
	}
	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		perror("input file not found");
		printf("usage: mips_sim input.bin\n");
		return -1;
	}
	do {
		res = fread( &data1, sizeof(unsigned char), 1, fp);
		res = fread( &data2, sizeof(unsigned char), 1, fp);
		res = fread( &data3, sizeof(unsigned char), 1, fp);
		res = fread( &data4, sizeof(unsigned char), 1, fp);
		mem[i] = data1<<24 | data2 <<16 | data3 <<8 | data4;
		printf("mem[0x%x]: 0x%08x\n", i, mem[i]);
		i++;
	} while (0 < res);
	fclose(fp);
	return 0;
}

int main(int argc, char *argv[])
{
	int res;
	int memnum;
	int Aluresult;
	int WBresult;
	struct inst in;

	res = initialize_sim(argc, argv);
	if (-1 == res) return 0;
	pc = 0;

	while(1) {
 		// main execution loop
		printf(" === Clock %d === \n", clock);
		initialize(&in);
		if (fetch() == -1) break;
		// TERMINAL CONDITION if PC == -1
		decode(&in);
		Aluresult = execute(&in);

		WBresult = load_store(&in, Aluresult);
		writeback(&in, WBresult);

		clock++;
	}
	printf("Final result: reg[2] 0x%x\n", reg[2]);
	return 0;
}
