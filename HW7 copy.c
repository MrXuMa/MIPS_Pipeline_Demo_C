/*Tyler Fink tdf22b*/
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#include<stdbool.h>

int text[105];
int regFile[32];
int dataMem[32];
int pc=0,cycle=1,stall=0,branch=0,mispred=0,tick=0;

typedef struct
{
	int op;
	int rs, rt,rd;
	int imm, brt,funct,shamt;
}Inst;

Inst inst[105];

typedef struct
{
	Inst in;
	int ins;
	int pc4;
	bool halt;
	int forward;
}IFID;

typedef struct 
{
	Inst in;
	int ins;
	int pc4;
	int rd1,rd2;
	bool halt;
	int forward;
}IDEX;

typedef struct
{
	Inst in;
	int ins;
	int alu, write;
	int reg;
	bool halt;
	int forward;
}EXMEM;

typedef struct
{
	Inst in;
	int ins;
	int mem,alu;
	int reg;
	bool halt;
	int forward;
}MEMWB;

typedef struct 
{
	int pc;
	IFID ifid;
	IDEX idex;
	EXMEM exmem;
	MEMWB memeb;
}state;

const int SNT=0, WNT=1, WT=2, ST=3;

typedef struct 
{
	int pc, target, state;
}BP;

BP bp[102];

int getOp(int ins)
{
	unsigned int newIns = ins >> 26;
	if(newIns ==0) // for R types
	{
		newIns = ins & 0x3F;
	}
	if(newIns == -29)
		newIns = 35;
	if(newIns == -21)
		 newIns = 43;
	return newIns;
}

int getRs(int ins)
{
	int newIns = ins & 0x03FFFFFF;
	newIns = newIns >>21;
	return newIns;
}

int getRt(int ins)
{
	int newIns = ins & 0x001FFFFF;
	newIns = newIns >>16;
	return newIns;
}

int getRd(int ins)
{
	int newIns = ins & 0x0000FFFF;
	newIns = newIns >>11;
	return newIns;
}

int getImm(int ins)
{
	int newIns = ins & 0x0000FFFF;
	short imm = (short)newIns;
	//printf("%d\t",imm);
	return imm;
}
int getBrTar(int ins, int pc)
{
	int target = ins &0x0000FFFF;
	short offset = (short) target;
	offset = offset * 4;
	return offset + pc;
}
int getFunct(int ins)
{
	int newIns = ins &0x0000003F;
	return newIns;
}
int getShamt(int ins)
{
	int newIns = ins&0x000007F0;
	newIns = newIns >> 6;
	return newIns;
}

void getReg(int newIns,char rs[] )
{
	if(newIns == 0)
		strcpy(rs,"0");
	else if(newIns ==1)
		strcpy(rs,"1");
	else if(newIns >1 && newIns <=3)
	{
		rs[0]='v';
		rs[1] = newIns-2 + 48;
		rs[2]='\0';
	}
	else if(newIns <=7)
	{
		rs[0]='a';
		rs[1] = newIns-4 + 48;
		rs[2]='\0';
	}
	else if(newIns <=15)
	{
		rs[0]='t';
		rs[1] = newIns-8 + 48;
		rs[2]='\0';
	}
	else if(newIns <=23)
	{
		rs[0]='s';
		rs[1] = newIns-16 + 48;
		rs[2]='\0';
	}
	else if(newIns <=25)
	{
		rs[0]='t';
		rs[1] = newIns-16 + 48;
		rs[2]='\0';
	}
	else if(newIns <=27)
	{
		rs[0]='k';
		rs[1] = newIns-26 + 48;
		rs[2]='\0';
	}	
	else if(newIns == 28)
		strcpy(rs,"gp");
	else if(newIns == 29)
		strcpy(rs,"sp");
	else if(newIns == 30)
		strcpy(rs,"fp");
	else if(newIns == 31)
		strcpy(rs,"ra");
}
void printInst(Inst in,int ins)
{
	char out[100] ="";
	char rs[5],rt[5],rd[5];
	int func;
	getReg(in.rs,rs);
	getReg(in.rt,rt);
	getReg(in.rd,rd);
	switch(in.op)
	{
		case 35: sprintf(out,"lw $%s,%d($%s)",rt,in.imm,rs);
				 break;
		case 43: sprintf(out,"sw $%s,%d($%s)",rt,in.imm,rs);
				break;
		case 12:sprintf(out,"addi $%s,$%s,%d",rt,rs,in.imm);
				break;
		case 13: sprintf(out,"ori $%s,$%s,%d",rt,rs,in.imm);
				break;
		case 5: sprintf(out,"beq $%s,$%s,%d",rs,rt,in.imm);
				break;
		case 1: sprintf(out,"halt");
				break;
		case 32: sprintf(out,"add $%s,$%s,$%s",rd,rs,rt);
				break;
		case 34: sprintf(out,"sub $%s,$%s,$%s",rd,rs,rt);
				break;
		case 0: if(ins==0)
					sprintf(out,"NOOP");
				else sprintf(out,"sll, $%s,$%s,%d",rd,rt,in.shamt);
				break;
	}
	printf("%s\n",out);
}
void getInst(int num, Inst *in, int pc)
{
	in->op = getOp(num);
	in->rs = getRs(num);
	in->rt = getRt(num);
	in->rd = getRd(num);
	in->imm = getImm(num);
	in->brt = getBrTar(num,pc);
	in->funct = getFunct(num);
	in->shamt = getShamt(num);
}
void printState(state st)
{
	int i;
	char rs[5],rt[5],rd[5];
	printf("********************\n");
	printf("State at the beginning of cycle %d\n",cycle);
	printf("\tPC = %d\n",pc);
	printf("\tData Memory:\n");
	for(i=0; i<16; i++)
	{
		printf("\t\tdataMem[%d] = %d\t\tdataMem[%d] = %d\n",i,dataMem[i],i+16,dataMem[i+16]);
	}
	printf("\tRegisters:\n");
	for(i=0; i<16; i++)
	{
		printf("\t\tregFile[%d] = %d\t\tregFile[%d] = %d\n",i,regFile[i],i+16,regFile[i+16]);
	}
	printf("\tIF/ID:\n");
	printf("\t\tInstruction: ");
	printInst(st.ifid.in,st.ifid.ins);
	printf("\t\tPCPlus4: %d\n",st.ifid.pc4);
	printf("\tID/EX:\n");
	printf("\t\tInstruction: ");
	printInst(st.idex.in,st.idex.ins);
	printf("\t\tPCPlus4: %d\n",st.idex.pc4);
	printf("\t\tbranchTarget: %d\n",st.idex.in.brt);
	printf("\t\treadData1: %d\n\t\treadData2: %d\n",st.idex.rd1,st.idex.rd2);
	printf("\t\timmed: %d\n",st.idex.in.imm);
	getReg(st.idex.in.rs,rs);
	getReg(st.idex.in.rt,rt);
	getReg(st.idex.in.rd,rd);
	printf("\t\trs: %s\n\t\trt: %s\n\t\trd: %s\n",rs,rt,rd);
	printf("\tEX/MEM\n");
	printf("\t\tInstruction: ");
	printInst(st.exmem.in,st.exmem.ins);
	getReg(st.exmem.reg,rs);
	printf("\t\taluResult: %d\n\t\twriteDataReg: %d\n\t\twriteReg: %s\n",st.exmem.alu,st.exmem.write,rs);
	printf("\tMEM/WB\n");
	printf("\t\tInstruction: ");
	printInst(st.memeb.in,st.memeb.ins);
	getReg(st.memeb.reg,rs);
	printf("\t\twriteDataMem: %d\n\t\twriteDataALU: %d\n\t\twriteReg: %s\n",st.memeb.mem,st.memeb.alu,rs);
}	
int read1(Inst in)
{
	return regFile[in.rs];
}
int read2(Inst in)
{
	if(in.op == 32 || in.op == 34 || in.op == 0 || in.op ==1 || in.op==5 )
		return regFile[in.rt];
	else return 0;
}
void init(state *st)
{
	st->pc = 0;
	st->ifid.in.op = 0; 
	st->ifid.ins = 0;	//This is enough for the computer to know its NOOP opcode is 0 & ins is 0
	st->ifid.pc4 = 0;
	st->idex.in.op = 0; 
	st->idex.ins = 0;
	st->idex.pc4 = 0;
	st->idex.in.brt = 0;
	st->idex.rd1 = 0;
	st->idex.rd2 = 0;
	st->idex.in.imm = 0;
	st->idex.in.rs = 0;
	st->idex.in.rt = 0;
	st->idex.in.rd = 0;
	st->exmem.in.op = 0;
	st->exmem.ins = 0;
	st->exmem.alu = 0;
	st->exmem.write = 0;
	st->exmem.reg = 0;
	st->memeb.in.op = 0;
	st->memeb.ins = 0;
	st->memeb.mem = 0;
	st->memeb.alu = 0;
	st->memeb.reg = 0;
	st->ifid.halt = false;
	st->idex.halt = false;
	st->exmem.halt = false;
	st->memeb.halt = false;
	st->idex.forward = 0;
}

int whatType(int ins){
	int opcode = getOp(ins); 
	if(opcode == 32 || opcode == 34 || (opcode == 0 && ins != 0)){
		return 0;
	}
	else if(opcode == 12 || opcode == 13 || opcode == 35 || opcode == 43){
		return 1;
	}
	else
		return 3;
}

int whatForwardFunc(int curIns, int newIns){
	int curType = whatType(curIns);
	int newType = whatType(newIns);
	if(curIns == 0 || newIns == 0)
		return 0;
	// R to R type
	if(curType == 0 && newType == 0){ 			// R type / R type
		if(getRs(curIns) == getRd(newIns) || getRt(curIns) == getRd(newIns)){
			return 1;
		}
	}
	// R to I type
	else if(curType == 0 && newType == 1){  	//R type / I type
		if(getRt(curIns) == getRt(newIns) || getRs(curIns) == getRt(newIns)){
			if(getOp(newIns) == 35){
				return 5; 				// if the cur ins is a lw in the IFID phase we need to stall as well
			}
			else{
				
				return 1; 				// if the cur ins is not a lw then forward like normal
			}
		}
	}

	else if(curType == 1 && newType == 0){  // I type / R type
		if(getRs(curIns) == getRd(newIns)){
			return 1; 				// if the cur ins is not a lw then forward like normal
		}
	}

	else if(curType == 1 && newType == 1){  		// I type / I type
		if(getRs(curIns) == getRt(newIns)){
			if(getOp(newIns) == 35){
				return 5; 				// if the cur ins is a lw in the IFID phase we need to stall as well
			}
			else{
				
				return 1; 				// if the cur ins is not a lw then forward like normal
			}
		}
	}
	return 0;
}

void setIFID(int *txt,MEMWB *curStg,IFID *newStg,int pc0,const int srtDataM){
	int opContrl = curStg->in.op;					
	if(opContrl == 32 || opContrl == 34 || opContrl == 12 || opContrl == 13 || (opContrl == 0 && newStg->ins != 0)){
		regFile[curStg->reg] = curStg->alu;
	}
	else if(opContrl == 35){
		regFile[curStg->reg] = curStg->mem;
	}
	if(!newStg->halt){											// if there is no halt
		getInst(txt[pc0/4],&newStg->in,pc0);					// fetch the instruction
		if(txt[pc0/4] == 1){								// if the ins # is 1, declare the halt
			newStg->halt = true; 							// ifid identified a halt, now every following stage is a noop
		}
		newStg->ins = txt[pc/4];
	}
	else{												// if the halt has been declared...
		getInst(0,&newStg->in,pc0);							// set the instruction in that stage to a noop
		newStg->ins = 0; 									// the ins is a noop so the numerical representation must be 0
	}
	newStg->pc4 = pc0; 									// still count pc no matter what
}

void setIDEX(int *txt,IFID *curStg,IDEX *newStg,int *pc0){
	if(!newStg->halt){
		if(newStg->forward == 5){
			getInst(0,&newStg->in,*pc0); 	// noop
			newStg->ins = 0;
			stall++;
			newStg->forward = 0;
		}
		else{
			newStg->in = curStg->in;  					// now the new stage = the cur stage instruction
			newStg->ins = curStg->ins; 					// now the new stage = the cur stage ins
			newStg->halt = curStg->halt;

			newStg->forward = whatForwardFunc(txt[*pc0/4],newStg->ins);
			curStg->forward = newStg->forward;
			if(newStg->forward == 5)
				*pc0 = *pc0 - 4;
			}
		}
	
	else{
		getInst(0,&newStg->in,*pc0-4);
		newStg->ins = 0;
	}
	newStg->rd1 = read1(newStg->in);   					// rs
	newStg->rd2 = read2(newStg->in); 					// if it is an immd it will now be zero
	newStg->pc4 = curStg->pc4;
}

void setEXMEM(int *txt,IDEX *curStg,EXMEM *newStg, EXMEM *frdStg,MEMWB *frdOpStg, int pc0){
	int opContrl;
	if(!newStg->halt && curStg->ins != 0){
		newStg->in = curStg->in;
		newStg->ins = curStg->ins;
		newStg->halt = curStg->halt;
		newStg->forward = curStg->forward;
		newStg->write = 0;
		opContrl = newStg->in.op;

		if((whatType(newStg->ins) == 0) && (whatType(frdStg->ins) == 0)){
			if((getRs(newStg->ins) == getRs(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
			else if((getRt(newStg->ins) == getRt(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
			else if( ( getRt(newStg->ins) == getRs(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
			else if((getRs(newStg->ins) == getRt(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
		}

		if((whatType(newStg->ins) == 0) && (whatType(frdStg->ins) == 1)){
			if((getRs(newStg->ins) == getRs(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
			else if((getRt(newStg->ins) == getRs(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
		}

		if((whatType(newStg->ins) == 1) && (whatType(frdStg->ins) == 0)){
			if((getRs(newStg->ins) == getRs(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
			else if((getRs(newStg->ins) == getRt(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
		}

		if((whatType(newStg->ins) == 1) && (whatType(frdStg->ins) == 1)){
			if((getRs(newStg->ins) == getRs(frdStg->ins)) && frdStg->forward == 1){
				newStg->forward = 2;
			}
		}

		if(whatForwardFunc(curStg->ins,frdOpStg->ins) == 5 && frdStg->forward == 1){
			frdStg->forward = 3;
		}
		else if(whatForwardFunc(curStg->ins,frdOpStg->ins) == 5){
			frdStg->forward = 5;
		}

		//sll
		if(opContrl == 0 && newStg->ins != 0){
			if(frdStg->forward == 5){
				newStg->alu = frdOpStg->mem << (newStg->in.shamt);
			}
			else if(frdStg->forward == 1 || frdStg->forward == 2)
				newStg->alu = frdStg->alu << (newStg->in.shamt);
			else
				newStg->alu = curStg->rd1 << (newStg->in.shamt); 		// read value of the rs reg shifted by shamt
			newStg->reg = newStg->in.rd;
		}
		//halt
		else if(newStg->ins == 1){
			newStg->alu = 0;
			newStg->reg = 0;
			newStg->write = 0;
		}

		//Andi ins
		else if(opContrl == 12){
			if(frdStg->forward == 5){
				newStg->alu = frdOpStg->mem & (newStg->in.imm);
			}
			else if(frdStg->forward == 1 || frdStg->forward == 2)
				newStg->alu = frdStg->alu & (newStg->in.imm);
			else
				newStg->alu = curStg->rd1 & (newStg->in.imm);
			newStg->reg = newStg->in.rt;
		}
		//Ori ins
		else if(opContrl == 13){
			if(frdStg->forward == 5){
				newStg->alu = frdOpStg->mem | (newStg->in.imm);
			}
			else if(newStg->forward == 1)
				newStg->alu = frdStg->alu | (newStg->in.imm);
			else
				newStg->alu = curStg->rd1 | (newStg->in.imm);
			newStg->reg = newStg->in.rt;
		}
		//Add ins
		else if(opContrl == 32){
			if(frdStg->forward == 5){									//if noop before meaning lw
				newStg->write = frdOpStg->mem;
					if(getRt(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->mem + curStg->rd2;
						newStg->write = frdOpStg->mem;		
					}
					else{
						newStg->alu = curStg->rd1 + frdOpStg->mem;
						newStg->write = frdOpStg->mem;
					}
			}
			else if(frdStg->forward == 3){
				if(whatType(frdStg->ins) == 0){		//R
					if((getRt(frdOpStg->ins) == getRs(newStg->ins)) && (getRd(frdStg->ins) == getRt(newStg->ins) ) ){
						newStg->alu = frdOpStg->mem + frdStg->alu;
						newStg->write = frdOpStg->mem;
					}
					else{
						newStg->alu = frdStg->alu + frdOpStg->mem;
						newStg->write = frdOpStg->mem;
					}
				}
				else{
					if((getRt(frdOpStg->ins) == getRs(newStg->ins)) && (getRt(frdStg->ins) == getRt(newStg->ins) ) ){
						newStg->alu = frdOpStg->mem + frdStg->alu;
						newStg->write = frdOpStg->mem;
					}
					else{
						newStg->alu = frdStg->alu + frdOpStg->mem;
						newStg->write = frdOpStg->mem;
					}
				}
			}
			else if(frdStg->forward == 1 || frdStg->forward == 2){								//if normal forward
				if(whatType(frdStg->ins) == 0 && whatType(frdOpStg->ins) == 0){							// find type and if R type
					if(getRd(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu + frdOpStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRt(newStg->ins) && getRd(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu + frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu + curStg->rd2;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 + frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
				}
				else if(whatType(frdStg->ins) == 0 && whatType(frdOpStg->ins) == 1){							// find type and if R type
					if(getRd(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu + frdOpStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRt(newStg->ins) && getRt(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu + frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu + curStg->rd2;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 + frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
				}
				else if(whatType(frdStg->ins) == 1 && whatType(frdOpStg->ins) == 0){							// find type and if R type
					if(getRt(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu + frdOpStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRt(newStg->ins) && getRd(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu + frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu + curStg->rd2;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 + frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
				}
				else													//if I type
					if(getRt(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu + frdOpStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRt(newStg->ins) && getRt(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu + frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu + curStg->rd2;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 + frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
			}
			else{
				newStg->alu = curStg->rd1 + curStg->rd2;				//if there is no forwarding
			}
			newStg->reg = newStg->in.rd;
		}
		//Sub ins
		else if(opContrl == 34){
			if(frdStg->forward == 5){									//if noop before meaning lw
				newStg->write = frdOpStg->mem;
					if(getRt(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->mem - curStg->rd2;
						newStg->write = frdOpStg->mem;		
					}
					else{
						newStg->alu = curStg->rd1 - frdOpStg->mem;
						newStg->write = frdOpStg->mem;
					}
			}
			else if(frdStg->forward == 3){
				if(whatType(frdStg->ins) == 0){		//R
					if((getRt(frdOpStg->ins) == getRs(newStg->ins)) && (getRd(frdStg->ins) == getRt(newStg->ins) ) ){
						newStg->alu = frdOpStg->mem - frdStg->alu;
						newStg->write = frdOpStg->mem;
					}
					else{
						newStg->alu = frdStg->alu - frdOpStg->mem;
						newStg->write = frdOpStg->mem;
					}
				}
				else{
					if((getRt(frdOpStg->ins) == getRs(newStg->ins)) && (getRt(frdStg->ins) == getRt(newStg->ins) ) ){
						newStg->alu = frdOpStg->mem - frdStg->alu;
						newStg->write = frdOpStg->mem;
					}
					else{
						newStg->alu = frdStg->alu - frdOpStg->mem;
						newStg->write = frdOpStg->mem;
					}
				}
			}
			else if(frdStg->forward == 1 || frdStg->forward == 2){								//if normal forward
				if(whatType(frdStg->ins) == 0 && whatType(frdOpStg->ins) == 0){							// find type and if R type
					if(getRd(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu - frdOpStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRt(newStg->ins) && getRd(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu - frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu - curStg->rd2;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 - frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
				}
				else if(whatType(frdStg->ins) == 0 && whatType(frdOpStg->ins) == 1){							// find type and if R type
					if(getRd(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu - frdOpStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRt(newStg->ins) && getRt(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu - frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRd(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu - curStg->rd2;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 - frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
				}
				else if(whatType(frdStg->ins) == 1 && whatType(frdOpStg->ins) == 0){							// find type and if R type
					if(getRt(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu - frdOpStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRt(newStg->ins) && getRd(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu - frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRs(newStg->ins) && getRd(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu - curStg->rd2;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 - frdStg->alu;
						newStg->write = regFile[getRd(frdOpStg->ins)];
					}
				}
				else													//if I type
					if(getRt(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) == getRt(newStg->ins)){
						newStg->alu = frdStg->alu - frdOpStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRt(newStg->ins) && getRt(frdOpStg->ins) == getRs(newStg->ins)){
						newStg->alu = frdOpStg->alu - frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else if(getRt(frdStg->ins) == getRs(newStg->ins) && getRt(frdOpStg->ins) != getRt(newStg->ins)){
						newStg->alu = frdStg->alu - curStg->rd2;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
					else{
						newStg->alu = curStg->rd1 - frdStg->alu;
						newStg->write = regFile[getRt(frdOpStg->ins)];
					}
			}
			else{
				newStg->alu = curStg->rd1 - curStg->rd2;				//if there is no forwarding
			}
			newStg->reg = newStg->in.rd;
		}
		//lw ins
		else if(opContrl == 35){
			if(frdStg->forward == 5){
				newStg->alu = frdOpStg->mem + (newStg->in.imm);
			}
			else if(frdStg->forward == 1 || frdStg->forward == 2){
				newStg->alu = frdStg->alu + (newStg->in.imm);
			}
			else{
				newStg->alu = curStg->rd1 + (newStg->in.imm);
			}
			newStg->reg = curStg->in.rt;
		}
		//sw ins
		else if(opContrl == 43){
			if(frdStg->forward == 5){
				curStg->rd2 = frdOpStg->alu;
				newStg->alu = frdOpStg->mem + (newStg->in.imm);
			}
			else if(frdStg->forward == 1 || frdStg->forward == 2){
				newStg->alu = frdOpStg->mem + (newStg->in.imm);
			}
			else{
				newStg->alu = curStg->rd1 + (newStg->in.imm);
			}
			newStg->write = regFile[curStg->in.rt];
			newStg->reg = newStg->in.rt;
		}

	}
	else{
		getInst(0,&newStg->in,pc0);
		newStg->ins = 0;
		newStg->alu = 0;
		newStg->write = 0;
		newStg->reg = 0;
	}
}

void setMEMWB(int *txt,EXMEM *curStg, MEMWB *newStg,int pc0, const int srtDataM){
	if(!newStg->halt){
		newStg->in = curStg->in;
		newStg->ins = curStg->ins;
		newStg->forward = curStg ->forward;
		newStg->reg = 0;
		newStg->alu = 0;
		newStg->mem = 0;
		if(newStg->ins == 1 || newStg->ins == 0){
			newStg->reg = 0;
			newStg->alu = 0;
			newStg->mem = 0;
		} // if halt
		else{
			newStg->reg = curStg->reg;
			newStg->alu = curStg->alu;
			if(newStg->in.op == 35){
				newStg->mem = dataMem[(newStg->alu - srtDataM)/4];
			}
			else if(newStg->in.op == 43){
				dataMem[(newStg->alu-srtDataM)/4] = regFile[newStg->reg];
			}
			newStg->halt = curStg->halt;
		}	

	}
}


int main()
{

	// declare some variables here
	state curState,newState;
	int numData = 0;
	int instC = 1;					

	// read and parse the input into text and data segments
	while(scanf("%d",&text[instC]) == 1){				// while the file isnt at the end
		if (text[instC] == 1){
			text[instC] = 1;
			while(scanf("%d",&dataMem[numData]) == 1)	// test[] == 1 means text segement is over and data starts and goes until EOF
				numData++;
		}
		instC++;	
	}

	const int dataMemAdd = (instC-1) * 4;					// instruction count times 4 total gives us the start of data


	init(&curState);
	init(&newState);
	
	pc=0;
	cycle=1;
	printState(curState);

	/* It might be tempting to go "backwards" in your design i.e.
	 * Start from the WB stage. Do not do that as it makes it harder
	 * in this design to add in forwarding, etc. 
	 * It is recommended you follow the hardware here and start with IF */

	while(curState.memeb.ins != 1)
	{
		pc+=4;
		cycle++;
		if(cycle == 2){
			setIFID(text,&curState.memeb,&newState.ifid,pc,dataMemAdd);			
		}
		else if (cycle == 3){
			setIFID(text,&curState.memeb,&newState.ifid,pc,dataMemAdd);	
			setIDEX(text,&curState.ifid,&newState.idex,&pc);
		}
		else if (cycle == 4){
			setIFID(text,&curState.memeb,&newState.ifid,pc,dataMemAdd);	
			setIDEX(text,&curState.ifid,&newState.idex,&pc); 		
			setEXMEM(text,&curState.idex,&newState.exmem,&curState.exmem,&curState.memeb,pc);
		}
		else{
			setIFID(text,&curState.memeb,&newState.ifid,pc,dataMemAdd);	
			setIDEX(text,&curState.ifid,&newState.idex,&pc); 		
			setEXMEM(text,&curState.idex,&newState.exmem,&curState.exmem,&curState.memeb,pc);
			setMEMWB(text,&curState.exmem,&newState.memeb,pc,dataMemAdd);											
		}
		printState(newState);
		curState=newState;

	}
	printf("********************\n");
	printf("Total number of cycles executed: %d\n",cycle);
	printf("Total number of stalls: %d\n",stall);
	printf("Total number of branches: %d\n",branch);
	printf("Total number of mispredicted branches: %d\n",mispred);

	return 0;
}
