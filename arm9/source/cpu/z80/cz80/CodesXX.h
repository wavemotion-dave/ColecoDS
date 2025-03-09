/******************************************************************************
*  ColecoDS Z80 CPU 
*
* Note: Most of this file is from the ColEm emulator core by Marat Fayzullin
*       but heavily modified for specific NDS use. If you want to use this
*       code, you are advised to seek out the much more portable ColEm core
*       and contact Marat.       
*
******************************************************************************/

/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                         CodesXX.h                       **/
/**                                                         **/
/** This file contains implementation for FD/DD tables of   **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

case ADD_B:    M_ADD(CPU.BC.B.h);break;
case ADD_C:    M_ADD(CPU.BC.B.l);break;
case ADD_D:    M_ADD(CPU.DE.B.h);break;
case ADD_E:    M_ADD(CPU.DE.B.l);break;
case ADD_H:    M_ADD(CPU.XX.B.h);break;
case ADD_L:    M_ADD(CPU.XX.B.l);break;
case ADD_A:    M_ADD(CPU.AF.B.h);break;
case ADD_xHL:  I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_ADD(I);break;
case ADD_BYTE: I=OpZ80(CPU.PC.W++);M_ADD(I);break;

case SUB_B:    M_SUB(CPU.BC.B.h);break;
case SUB_C:    M_SUB(CPU.BC.B.l);break;
case SUB_D:    M_SUB(CPU.DE.B.h);break;
case SUB_E:    M_SUB(CPU.DE.B.l);break;
case SUB_H:    M_SUB(CPU.XX.B.h);break;
case SUB_L:    M_SUB(CPU.XX.B.l);break;
case SUB_A:    CPU.AF.B.h=0;CPU.AF.B.l=N_FLAG|Z_FLAG;break;
case SUB_xHL:  I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_SUB(I);break;
case SUB_BYTE: I=OpZ80(CPU.PC.W++);M_SUB(I);break;

case AND_B:    M_AND(CPU.BC.B.h);break;
case AND_C:    M_AND(CPU.BC.B.l);break;
case AND_D:    M_AND(CPU.DE.B.h);break;
case AND_E:    M_AND(CPU.DE.B.l);break;
case AND_H:    M_AND(CPU.XX.B.h);break;
case AND_L:    M_AND(CPU.XX.B.l);break;
case AND_A:    M_AND(CPU.AF.B.h);break;
case AND_xHL:  I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_AND(I);break;
case AND_BYTE: I=OpZ80(CPU.PC.W++);M_AND(I);break;

case OR_B:     M_OR(CPU.BC.B.h);break;
case OR_C:     M_OR(CPU.BC.B.l);break;
case OR_D:     M_OR(CPU.DE.B.h);break;
case OR_E:     M_OR(CPU.DE.B.l);break;
case OR_H:     M_OR(CPU.XX.B.h);break;
case OR_L:     M_OR(CPU.XX.B.l);break;
case OR_A:     M_OR(CPU.AF.B.h);break;
case OR_xHL:   I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_OR(I);break;
case OR_BYTE:  I=OpZ80(CPU.PC.W++);M_OR(I);break;

case ADC_B:    M_ADC(CPU.BC.B.h);break;
case ADC_C:    M_ADC(CPU.BC.B.l);break;
case ADC_D:    M_ADC(CPU.DE.B.h);break;
case ADC_E:    M_ADC(CPU.DE.B.l);break;
case ADC_H:    M_ADC(CPU.XX.B.h);break;
case ADC_L:    M_ADC(CPU.XX.B.l);break;
case ADC_A:    M_ADC(CPU.AF.B.h);break;
case ADC_xHL:  I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_ADC(I);break;
case ADC_BYTE: I=OpZ80(CPU.PC.W++);M_ADC(I);break;

case SBC_B:    M_SBC(CPU.BC.B.h);break;
case SBC_C:    M_SBC(CPU.BC.B.l);break;
case SBC_D:    M_SBC(CPU.DE.B.h);break;
case SBC_E:    M_SBC(CPU.DE.B.l);break;
case SBC_H:    M_SBC(CPU.XX.B.h);break;
case SBC_L:    M_SBC(CPU.XX.B.l);break;
case SBC_A:    M_SBC(CPU.AF.B.h);break;
case SBC_xHL:  I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_SBC(I);break;
case SBC_BYTE: I=OpZ80(CPU.PC.W++);M_SBC(I);break;

case XOR_B:    M_XOR(CPU.BC.B.h);break;
case XOR_C:    M_XOR(CPU.BC.B.l);break;
case XOR_D:    M_XOR(CPU.DE.B.h);break;
case XOR_E:    M_XOR(CPU.DE.B.l);break;
case XOR_H:    M_XOR(CPU.XX.B.h);break;
case XOR_L:    M_XOR(CPU.XX.B.l);break;
case XOR_A:    CPU.AF.B.h=0;CPU.AF.B.l=P_FLAG|Z_FLAG;break;
case XOR_xHL:  I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_XOR(I);break;
case XOR_BYTE: I=OpZ80(CPU.PC.W++);M_XOR(I);break;

case CP_B:     M_CP(CPU.BC.B.h);break;
case CP_C:     M_CP(CPU.BC.B.l);break;
case CP_D:     M_CP(CPU.DE.B.h);break;
case CP_E:     M_CP(CPU.DE.B.l);break;
case CP_H:     M_CP(CPU.XX.B.h);break;
case CP_L:     M_CP(CPU.XX.B.l);break;
case CP_A:     CPU.AF.B.l=N_FLAG|Z_FLAG;break;
case CP_xHL:   I=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));M_CP(I);break;
case CP_BYTE:  I=OpZ80(CPU.PC.W++);M_CP(I);break;
               
case LD_BC_WORD: M_LDWORD(BC);break;
case LD_DE_WORD: M_LDWORD(DE);break;
case LD_HL_WORD: M_LDWORD(XX);break;
case LD_SP_WORD: M_LDWORD(SP);break;

case LD_PC_HL: CPU.PC.W=CPU.XX.W;JumpZ80(CPU.PC.W);break;
case LD_SP_HL: CPU.SP.W=CPU.XX.W;break;
case LD_A_xBC: CPU.AF.B.h=RdZ80(CPU.BC.W);break;
case LD_A_xDE: CPU.AF.B.h=RdZ80(CPU.DE.W);break;

case ADD_HL_BC:  M_ADDW(XX,BC);break;
case ADD_HL_DE:  M_ADDW(XX,DE);break;
case ADD_HL_HL:  M_ADDW(XX,XX);break;
case ADD_HL_SP:  M_ADDW(XX,SP);break;

case DEC_BC:   CPU.BC.W--;break;
case DEC_DE:   CPU.DE.W--;break;
case DEC_HL:   CPU.XX.W--;break;
case DEC_SP:   CPU.SP.W--;break;

case INC_BC:   CPU.BC.W++;break;
case INC_DE:   CPU.DE.W++;break;
case INC_HL:   CPU.XX.W++;break;
case INC_SP:   CPU.SP.W++;break;

case DEC_B:    M_DEC(CPU.BC.B.h);break;
case DEC_C:    M_DEC(CPU.BC.B.l);break;
case DEC_D:    M_DEC(CPU.DE.B.h);break;
case DEC_E:    M_DEC(CPU.DE.B.l);break;
case DEC_H:    M_DEC(CPU.XX.B.h);break;
case DEC_L:    M_DEC(CPU.XX.B.l);break;
case DEC_A:    M_DEC(CPU.AF.B.h);break;
case DEC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W));M_DEC(I);
               WrZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++),I);
               break;

case INC_B:    M_INC(CPU.BC.B.h);break;
case INC_C:    M_INC(CPU.BC.B.l);break;
case INC_D:    M_INC(CPU.DE.B.h);break;
case INC_E:    M_INC(CPU.DE.B.l);break;
case INC_H:    M_INC(CPU.XX.B.h);break;
case INC_L:    M_INC(CPU.XX.B.l);break;
case INC_A:    M_INC(CPU.AF.B.h);break;
case INC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W));M_INC(I);
               WrZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++),I);
               break;

case RLCA:
  I=(CPU.AF.B.h&0x80? C_FLAG:0);
  CPU.AF.B.h=(CPU.AF.B.h<<1)|I;
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;
case RLA:
  I=(CPU.AF.B.h&0x80? C_FLAG:0);
  CPU.AF.B.h=(CPU.AF.B.h<<1)|(CPU.AF.B.l&C_FLAG);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;
case RRCA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(I? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;
case RRA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(CPU.AF.B.l&C_FLAG? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  break;

case RST00:    M_RST(0x0000);break;
case RST08:    M_RST(0x0008);break;
case RST10:    M_RST(0x0010);break;
case RST18:    M_RST(0x0018);break;
case RST20:    M_RST(0x0020);break;
case RST28:    M_RST(0x0028);break;
case RST30:    M_RST(0x0030);break;
case RST38:    M_RST(0x0038);break;

case PUSH_BC:  M_PUSH(BC);break;
case PUSH_DE:  M_PUSH(DE);break;
case PUSH_HL:  M_PUSH(XX);break;
case PUSH_AF:  M_PUSH(AF);break;

case POP_BC:   M_POP(BC);break;
case POP_DE:   M_POP(DE);break;
case POP_HL:   M_POP(XX);break;
case POP_AF:   M_POP(AF);break;

case SCF:  S(C_FLAG);R(N_FLAG|H_FLAG);break;
case CPL:  CPU.AF.B.h=~CPU.AF.B.h;S(N_FLAG|H_FLAG);break;
case NOP:  break;
case OUTA: I=OpZ80(CPU.PC.W++);OutZ80(I|(CPU.AF.W&0xFF00),CPU.AF.B.h);break;
case INA:  I=OpZ80(CPU.PC.W++);CPU.AF.B.h=InZ80(I|(CPU.AF.W&0xFF00));break;

case EX_DE_HL: J.W=CPU.DE.W;CPU.DE.W=CPU.HL.W;CPU.HL.W=J.W;break;
case EX_AF_AF: J.W=CPU.AF.W;CPU.AF.W=CPU.AF1.W;CPU.AF1.W=J.W;break;  
  
case LD_B_B:   CPU.BC.B.h=CPU.BC.B.h;break;
case LD_C_B:   CPU.BC.B.l=CPU.BC.B.h;break;
case LD_D_B:   CPU.DE.B.h=CPU.BC.B.h;break;
case LD_E_B:   CPU.DE.B.l=CPU.BC.B.h;break;
case LD_H_B:   CPU.XX.B.h=CPU.BC.B.h;break;
case LD_L_B:   CPU.XX.B.l=CPU.BC.B.h;break;
case LD_A_B:   CPU.AF.B.h=CPU.BC.B.h;break;
case LD_xHL_B: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.BC.B.h);break;

case LD_B_C:   CPU.BC.B.h=CPU.BC.B.l;break;
case LD_C_C:   CPU.BC.B.l=CPU.BC.B.l;break;
case LD_D_C:   CPU.DE.B.h=CPU.BC.B.l;break;
case LD_E_C:   CPU.DE.B.l=CPU.BC.B.l;break;
case LD_H_C:   CPU.XX.B.h=CPU.BC.B.l;break;
case LD_L_C:   CPU.XX.B.l=CPU.BC.B.l;break;
case LD_A_C:   CPU.AF.B.h=CPU.BC.B.l;break;
case LD_xHL_C: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.BC.B.l);break;

case LD_B_D:   CPU.BC.B.h=CPU.DE.B.h;break;
case LD_C_D:   CPU.BC.B.l=CPU.DE.B.h;break;
case LD_D_D:   CPU.DE.B.h=CPU.DE.B.h;break;
case LD_E_D:   CPU.DE.B.l=CPU.DE.B.h;break;
case LD_H_D:   CPU.XX.B.h=CPU.DE.B.h;break;
case LD_L_D:   CPU.XX.B.l=CPU.DE.B.h;break;
case LD_A_D:   CPU.AF.B.h=CPU.DE.B.h;break;
case LD_xHL_D: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.DE.B.h);break;

case LD_B_E:   CPU.BC.B.h=CPU.DE.B.l;break;
case LD_C_E:   CPU.BC.B.l=CPU.DE.B.l;break;
case LD_D_E:   CPU.DE.B.h=CPU.DE.B.l;break;
case LD_E_E:   CPU.DE.B.l=CPU.DE.B.l;break;
case LD_H_E:   CPU.XX.B.h=CPU.DE.B.l;break;
case LD_L_E:   CPU.XX.B.l=CPU.DE.B.l;break;
case LD_A_E:   CPU.AF.B.h=CPU.DE.B.l;break;
case LD_xHL_E: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.DE.B.l);break;

case LD_B_H:   CPU.BC.B.h=CPU.XX.B.h;break;
case LD_C_H:   CPU.BC.B.l=CPU.XX.B.h;break;
case LD_D_H:   CPU.DE.B.h=CPU.XX.B.h;break;
case LD_E_H:   CPU.DE.B.l=CPU.XX.B.h;break;
case LD_H_H:   CPU.XX.B.h=CPU.XX.B.h;break;
case LD_L_H:   CPU.XX.B.l=CPU.XX.B.h;break;
case LD_A_H:   CPU.AF.B.h=CPU.XX.B.h;break;
case LD_xHL_H: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.HL.B.h);break;

case LD_B_L:   CPU.BC.B.h=CPU.XX.B.l;break;
case LD_C_L:   CPU.BC.B.l=CPU.XX.B.l;break;
case LD_D_L:   CPU.DE.B.h=CPU.XX.B.l;break;
case LD_E_L:   CPU.DE.B.l=CPU.XX.B.l;break;
case LD_H_L:   CPU.XX.B.h=CPU.XX.B.l;break;
case LD_L_L:   CPU.XX.B.l=CPU.XX.B.l;break;
case LD_A_L:   CPU.AF.B.h=CPU.XX.B.l;break;
case LD_xHL_L: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.HL.B.l);break;

case LD_B_A:   CPU.BC.B.h=CPU.AF.B.h;break;
case LD_C_A:   CPU.BC.B.l=CPU.AF.B.h;break;
case LD_D_A:   CPU.DE.B.h=CPU.AF.B.h;break;
case LD_E_A:   CPU.DE.B.l=CPU.AF.B.h;break;
case LD_H_A:   CPU.XX.B.h=CPU.AF.B.h;break;
case LD_L_A:   CPU.XX.B.l=CPU.AF.B.h;break;
case LD_A_A:   CPU.AF.B.h=CPU.AF.B.h;break;
case LD_xHL_A: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.AF.B.h);break;

case LD_xBC_A: WrZ80(CPU.BC.W,CPU.AF.B.h);break;
case LD_xDE_A: WrZ80(CPU.DE.W,CPU.AF.B.h);break;

case LD_B_xHL:    CPU.BC.B.h=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;
case LD_C_xHL:    CPU.BC.B.l=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;
case LD_D_xHL:    CPU.DE.B.h=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;
case LD_E_xHL:    CPU.DE.B.l=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;
case LD_H_xHL:    CPU.HL.B.h=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;
case LD_L_xHL:    CPU.HL.B.l=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;
case LD_A_xHL:    CPU.AF.B.h=RdZ80(CPU.XX.W+(offset)OpZ80(CPU.PC.W++));break;

case LD_B_BYTE:   CPU.BC.B.h=OpZ80(CPU.PC.W++);break;
case LD_C_BYTE:   CPU.BC.B.l=OpZ80(CPU.PC.W++);break;
case LD_D_BYTE:   CPU.DE.B.h=OpZ80(CPU.PC.W++);break;
case LD_E_BYTE:   CPU.DE.B.l=OpZ80(CPU.PC.W++);break;
case LD_H_BYTE:   CPU.XX.B.h=OpZ80(CPU.PC.W++);break;
case LD_L_BYTE:   CPU.XX.B.l=OpZ80(CPU.PC.W++);break;
case LD_A_BYTE:   CPU.AF.B.h=OpZ80(CPU.PC.W++);break;
case LD_xHL_BYTE: J.W=CPU.XX.W+(offset)OpZ80(CPU.PC.W++);
                  WrZ80(J.W,OpZ80(CPU.PC.W++));break;

case LD_xWORD_HL:
  J.B.l=OpZ80(CPU.PC.W++);
  J.B.h=OpZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.XX.B.l);
  WrZ80(J.W,CPU.XX.B.h);
  break;

case LD_HL_xWORD:
  J.B.l=OpZ80(CPU.PC.W++);
  J.B.h=OpZ80(CPU.PC.W++);
  CPU.XX.B.l=RdZ80(J.W++);
  CPU.XX.B.h=RdZ80(J.W);
  break;

case LD_A_xWORD:
  J.B.l=OpZ80(CPU.PC.W++);
  J.B.h=OpZ80(CPU.PC.W++);
  CPU.AF.B.h=RdZ80(J.W);
  break;

case LD_xWORD_A:
  J.B.l=OpZ80(CPU.PC.W++);
  J.B.h=OpZ80(CPU.PC.W++);
  WrZ80(J.W,CPU.AF.B.h);
  break;

case EX_HL_xSP:
  J.B.l=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W++,CPU.XX.B.l);
  J.B.h=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W--,CPU.XX.B.h);
  CPU.XX.W=J.W;
  break;
