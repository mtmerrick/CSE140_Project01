#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#undef mips /* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo(int changedReg, int changedMem);
unsigned int Fetch(int);
void Decode(unsigned int, DecodedInstr *, RegVals *);
int Execute(DecodedInstr *, RegVals *);
int Mem(DecodedInstr *, int, int *);
void RegWrite(DecodedInstr *, int, int *);
void UpdatePC(DecodedInstr *, int);
void PrintInstruction(DecodedInstr *);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

//Global i-format
int addiu = 0x9;
int andi = 0xc;
int ori = 0xd;
int lui = 0xf;
int lw = 0x23;
int sw = 0x2b;

//Global r-format
int addu = 0x21;
int subu = 0x23;
int sll = 0x0;
int srl = 0x2;
int and = 0x24;
int or = 0x25;
int slt = 0x2a;
int beq = 0x4;
int bne = 0x5;

//Global j-format
int j = 0x2;
int jal = 0x3;
int jr = 0x8;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer(FILE *filein, int printingRegisters, int printingMemory, int debugging, int interactive)
{
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k = 0; k < 32; k++)
    {
        mips.registers[k] = 0;
    }

    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS + MAXNUMDATA) * 4;

    for (k = 0; k < MAXNUMINSTRS + MAXNUMDATA; k++)
    {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein))
    {
        /*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k > MAXNUMINSTRS)
        {
            fprintf(stderr, "Program too big.\n");
            exit(1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i)
{
    return (i >> 24) | (i >> 8 & 0x0000ff00) | (i << 8 & 0x00ff0000) | (i << 24);
}

/*
 *  Run the simulation.
 */
void Simulate()
{
    char s[40]; /* used for handling interactive input */
    unsigned int instr;
    int changedReg = -1, changedMem = -1, val;
    DecodedInstr d;

    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1)
    {
        if (mips.interactive)
        {
            printf("> ");
            fgets(s, sizeof(s), stdin);
            if (s[0] == 'q')
            {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch(mips.pc);

        printf("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode(instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

        UpdatePC(&d, val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo(changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo(int changedReg, int changedMem)
{
    int k, addr;
    printf("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1)
    {
        printf("No register was updated.\n");
    }
    else if (!mips.printingRegisters)
    {
        printf("Updated r%2.2d to %8.8x\n",
               changedReg, mips.registers[changedReg]);
    }
    else
    {
        for (k = 0; k < 32; k++)
        {
            printf("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k + 1) % 4 == 0)
            {
                printf("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1)
    {
        printf("No memory location was updated.\n");
    }
    else if (!mips.printingMemory)
    {
        printf("Updated memory at address %8.8x to %8.8x\n",
               changedMem, Fetch(changedMem));
    }
    else
    {
        printf("Nonzero memory\n");
        printf("ADDR	  CONTENTS\n");
        for (addr = 0x00400000 + 4 * MAXNUMINSTRS;
             addr < 0x00400000 + 4 * (MAXNUMINSTRS + MAXNUMDATA);
             addr = addr + 4)
        {
            if (Fetch(addr) != 0)
            {
                printf("%8.8x  %8.8x\n", addr, Fetch(addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch(int addr)
{
    return mips.memory[(addr - 0x00400000) / 4];
}

void rDecode(unsigned int instr, DecodedInstr *d, RegVals *rVals)
{
    //r-type inst format: opcode: 31-26 (6); rs: 25-21 (5); rt: 20-16 (5); rd: 15-11 (5); shamt: 10-6 (5); funct: 5-0 (6)
    d->type = R;
    unsigned int clone = instr;
    //funct
    clone = clone << 26;
    d->r->funct = clone >> 26;
    //rs
    clone = instr;
    clone = clone << 6;
    d->r->rs = clone >> 27;
    rVals->R_rs = d->r->rs;
    //rt
    clone = instr;
    clone = clone << 11;
    d->r->rt = clone >> 27;
    rVals->R_rt = d->r->rt;
    //rd
    clone = instr;
    clone = clone << 16;
    d->r->rd = clone >> 27;
    rVals->R_rd = d->r->rd;
    //shamt
    clone = instr;
    clone = clone << 21;
    d->r->shamt = clone >> 27;
}

void iDecode(unsigned int instr, DecodedInstr *d, RegVals *rVals)
{
    //i-type inst format: opcode: 31-26 (6); rs: 25-21 (5); rt: 20-16 (5); immediate: 15-0 (16)
    d->type = I;
    unsigned int clone = instr;
    //rs
    clone = instr;
    clone = clone << 6;
    d->i->rs = clone >> 27;
    rVals->R_rs = d->i->rs;
    //rt
    clone = instr;
    clone = clone << 11;
    d->i->rt = clone >> 27;
    rVals->R_rt = d->i->rt;
    //imm
    clone = instr;
    clone = clone << 6;
    d->i->addr_or_immed = clone >> 27;

}




/* Decode instr, returning decoded instruction. */
void Decode(unsigned int instr /*32 bit address*/, DecodedInstr *d, RegVals *rVals /*register values*/)
{
    unsigned int opcode = instr;
    d->op = opcode >> 26;

    switch (d->op)
    {
        case 0x0:
        {
            rDecode (instr, d, rVals);
            break;
        }
        case 0x9:   
        {
            //addiu
            iDecode (instr, d, rVals);
            break;
        }
        case 0xc:   
        {
            //andi
            iDecode (instr, d, rVals);
            break;
        }
        case 0xd:   
        {
            //ori
            iDecode (instr, d, rVals);
            break;
        }
        case 0xf:   
        {
            //lui
            iDecode (instr, d, rVals);
            break;
        }
        case 0x23:   
        {
            //lw
            iDecode (instr, d, rVals);
            break;
        }
        case 0x2b:   
        {
            //sw
            iDecode (instr, d, rVals);
            break;
        }
        case 0x2:   
        {
            //j
            d->type = J;
            unsigned int targ = instr << 6;
            d->j->target = targ >> 6;
            break;
        }
        case 0x3:   
        {
            //jal
            d->type = J;
            unsigned int targ = instr << 6;
            d->j->target = targ >> 6;
            break;
        }
    }
}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction(DecodedInstr *d)
{
    /* Your code goes here */
    switch (d->op)
    {
        case 0x0:
        {
            //r-type
            switch(d->r->funct)
            {
                case 0x21:
                {
                    cout<<"addu"<<\t;
                    break;
                }
                    
                case 0x23:
                {
                    cout<<"subu"<<\t;
                    break;
                }
                case 0x0:
                {
                    cout<<"sll"<<\t;
                    break;
                }
                case 0x2:
                {
                    cout<<"srl"<<\t;
                    break;
                }
                case 0x24:
                {
                    cout<<"and"<<\t;
                    break;
                }
                case 0x25:
                {
                    cout<<"or"<<\t;
                    break;
                }
                case 0x2a:
                {
                    cout<<"slt"<<\t;
                    break;
                }
                case 0x4:
                {
                    cout<<"beq"<<\t;
                    break;
                }
                case 0x5:
                {
                    cout<<"bne"<<\t;
                }
                case 0x8:   
                {
                    cout<<"jr"<<\t;
                }
            }
            break;
        }
        case 0x9:   
        {
            cout<<"addiu"<<\t;
            break;
        }
        case 0xc:   
        {
            cout<<"andi"<<\t;
            break;
        }
        case 0xd:   
        {
            cout<<"ori"<<\t;
            break;
        }
        case 0xf:   
        {
            cout<<"lui"<<\t;
            break;
        }
        case 0x23:   
        {
            cout<<"lw"<<\t;
            break;
        }
        case 0x2b:   
        {
            cout<<"sw"<<\t;
            break;
        }
        case 0x2:   
        {
            cout<<"j"<<\t;
            break;
        }
        case 0x3:   
        {
            cout<<"jal"<<\t;
            break;
        }
    }
    switch(d->type)
    {
        case R:
        {
            cout<<"$"<<d->r->rd<<", $"<<d->r->rs<<", $"<<d->r->rt<<endl;
        }
        case I:
        {
            cout<<"$"<<d->i->rt<<", $"<<d->i->rs<<", "<<d->i->addr_or_immed<<endl;
        }
        case J:
        {
            cout<<d->j->target<<endl;
        }
    }
}

/* Perform computation needed to execute d, returning computed value */
int Execute(DecodedInstr *d, RegVals *rVals)
{
    /* Your code goes here */
    return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC(DecodedInstr *d, int val)
{
    mips.pc += 4;
    /* Your code goes here */
    switch(d->type)
    {
        case J:
        {
            switch(d->op){
                case 0x3:   
                {
                    //jal
                    Computer->registers[30] = Computer->pc;
                    break;
                }
            }
            Computer->pc = d->j->target;
            
        }
        case R:
        {
            //jr
            if(d->funct == 0x8){
                Computer->pc = d->r->rs;
            }
        }
        case default:
        {}
    }
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem(DecodedInstr *d, int val, int *changedMem)
{
    /* Your code goes here */
    return 0;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite(DecodedInstr *d, int val, int *changedReg)
{
    /* Your code goes here */
}
