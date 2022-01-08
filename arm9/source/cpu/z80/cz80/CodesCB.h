/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                         CodesCB.h                       **/
/**                                                         **/
/** This file contains implementation for the CB table of   **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

case RLC_B: M_RLC(CPU.BC.B.h);break;  case RLC_C: M_RLC(CPU.BC.B.l);break;
case RLC_D: M_RLC(CPU.DE.B.h);break;  case RLC_E: M_RLC(CPU.DE.B.l);break;
case RLC_H: M_RLC(CPU.HL.B.h);break;  case RLC_L: M_RLC(CPU.HL.B.l);break;
case RLC_xHL: I=RdZ80(CPU.HL.W);M_RLC(I);WrZ80(CPU.HL.W,I);break;
case RLC_A: M_RLC(CPU.AF.B.h);break;

case RRC_B: M_RRC(CPU.BC.B.h);break;  case RRC_C: M_RRC(CPU.BC.B.l);break;
case RRC_D: M_RRC(CPU.DE.B.h);break;  case RRC_E: M_RRC(CPU.DE.B.l);break;
case RRC_H: M_RRC(CPU.HL.B.h);break;  case RRC_L: M_RRC(CPU.HL.B.l);break;
case RRC_xHL: I=RdZ80(CPU.HL.W);M_RRC(I);WrZ80(CPU.HL.W,I);break;
case RRC_A: M_RRC(CPU.AF.B.h);break;

case RL_B: M_RL(CPU.BC.B.h);break;  case RL_C: M_RL(CPU.BC.B.l);break;
case RL_D: M_RL(CPU.DE.B.h);break;  case RL_E: M_RL(CPU.DE.B.l);break;
case RL_H: M_RL(CPU.HL.B.h);break;  case RL_L: M_RL(CPU.HL.B.l);break;
case RL_xHL: I=RdZ80(CPU.HL.W);M_RL(I);WrZ80(CPU.HL.W,I);break;
case RL_A: M_RL(CPU.AF.B.h);break;

case RR_B: M_RR(CPU.BC.B.h);break;  case RR_C: M_RR(CPU.BC.B.l);break;
case RR_D: M_RR(CPU.DE.B.h);break;  case RR_E: M_RR(CPU.DE.B.l);break;
case RR_H: M_RR(CPU.HL.B.h);break;  case RR_L: M_RR(CPU.HL.B.l);break;
case RR_xHL: I=RdZ80(CPU.HL.W);M_RR(I);WrZ80(CPU.HL.W,I);break;
case RR_A: M_RR(CPU.AF.B.h);break;

case SLA_B: M_SLA(CPU.BC.B.h);break;  case SLA_C: M_SLA(CPU.BC.B.l);break;
case SLA_D: M_SLA(CPU.DE.B.h);break;  case SLA_E: M_SLA(CPU.DE.B.l);break;
case SLA_H: M_SLA(CPU.HL.B.h);break;  case SLA_L: M_SLA(CPU.HL.B.l);break;
case SLA_xHL: I=RdZ80(CPU.HL.W);M_SLA(I);WrZ80(CPU.HL.W,I);break;
case SLA_A: M_SLA(CPU.AF.B.h);break;

case SRA_B: M_SRA(CPU.BC.B.h);break;  case SRA_C: M_SRA(CPU.BC.B.l);break;
case SRA_D: M_SRA(CPU.DE.B.h);break;  case SRA_E: M_SRA(CPU.DE.B.l);break;
case SRA_H: M_SRA(CPU.HL.B.h);break;  case SRA_L: M_SRA(CPU.HL.B.l);break;
case SRA_xHL: I=RdZ80(CPU.HL.W);M_SRA(I);WrZ80(CPU.HL.W,I);break;
case SRA_A: M_SRA(CPU.AF.B.h);break;

case SLL_B: M_SLL(CPU.BC.B.h);break;  case SLL_C: M_SLL(CPU.BC.B.l);break;
case SLL_D: M_SLL(CPU.DE.B.h);break;  case SLL_E: M_SLL(CPU.DE.B.l);break;
case SLL_H: M_SLL(CPU.HL.B.h);break;  case SLL_L: M_SLL(CPU.HL.B.l);break;
case SLL_xHL: I=RdZ80(CPU.HL.W);M_SLL(I);WrZ80(CPU.HL.W,I);break;
case SLL_A: M_SLL(CPU.AF.B.h);break;

case SRL_B: M_SRL(CPU.BC.B.h);break;  case SRL_C: M_SRL(CPU.BC.B.l);break;
case SRL_D: M_SRL(CPU.DE.B.h);break;  case SRL_E: M_SRL(CPU.DE.B.l);break;
case SRL_H: M_SRL(CPU.HL.B.h);break;  case SRL_L: M_SRL(CPU.HL.B.l);break;
case SRL_xHL: I=RdZ80(CPU.HL.W);M_SRL(I);WrZ80(CPU.HL.W,I);break;
case SRL_A: M_SRL(CPU.AF.B.h);break;
    
case BIT0_B: M_BIT(0,CPU.BC.B.h);break;  case BIT0_C: M_BIT(0,CPU.BC.B.l);break;
case BIT0_D: M_BIT(0,CPU.DE.B.h);break;  case BIT0_E: M_BIT(0,CPU.DE.B.l);break;
case BIT0_H: M_BIT(0,CPU.HL.B.h);break;  case BIT0_L: M_BIT(0,CPU.HL.B.l);break;
case BIT0_xHL: I=RdZ80(CPU.HL.W);M_BIT(0,I);break;
case BIT0_A: M_BIT(0,CPU.AF.B.h);break;

case BIT1_B: M_BIT(1,CPU.BC.B.h);break;  case BIT1_C: M_BIT(1,CPU.BC.B.l);break;
case BIT1_D: M_BIT(1,CPU.DE.B.h);break;  case BIT1_E: M_BIT(1,CPU.DE.B.l);break;
case BIT1_H: M_BIT(1,CPU.HL.B.h);break;  case BIT1_L: M_BIT(1,CPU.HL.B.l);break;
case BIT1_xHL: I=RdZ80(CPU.HL.W);M_BIT(1,I);break;
case BIT1_A: M_BIT(1,CPU.AF.B.h);break;

case BIT2_B: M_BIT(2,CPU.BC.B.h);break;  case BIT2_C: M_BIT(2,CPU.BC.B.l);break;
case BIT2_D: M_BIT(2,CPU.DE.B.h);break;  case BIT2_E: M_BIT(2,CPU.DE.B.l);break;
case BIT2_H: M_BIT(2,CPU.HL.B.h);break;  case BIT2_L: M_BIT(2,CPU.HL.B.l);break;
case BIT2_xHL: I=RdZ80(CPU.HL.W);M_BIT(2,I);break;
case BIT2_A: M_BIT(2,CPU.AF.B.h);break;

case BIT3_B: M_BIT(3,CPU.BC.B.h);break;  case BIT3_C: M_BIT(3,CPU.BC.B.l);break;
case BIT3_D: M_BIT(3,CPU.DE.B.h);break;  case BIT3_E: M_BIT(3,CPU.DE.B.l);break;
case BIT3_H: M_BIT(3,CPU.HL.B.h);break;  case BIT3_L: M_BIT(3,CPU.HL.B.l);break;
case BIT3_xHL: I=RdZ80(CPU.HL.W);M_BIT(3,I);break;
case BIT3_A: M_BIT(3,CPU.AF.B.h);break;

case BIT4_B: M_BIT(4,CPU.BC.B.h);break;  case BIT4_C: M_BIT(4,CPU.BC.B.l);break;
case BIT4_D: M_BIT(4,CPU.DE.B.h);break;  case BIT4_E: M_BIT(4,CPU.DE.B.l);break;
case BIT4_H: M_BIT(4,CPU.HL.B.h);break;  case BIT4_L: M_BIT(4,CPU.HL.B.l);break;
case BIT4_xHL: I=RdZ80(CPU.HL.W);M_BIT(4,I);break;
case BIT4_A: M_BIT(4,CPU.AF.B.h);break;

case BIT5_B: M_BIT(5,CPU.BC.B.h);break;  case BIT5_C: M_BIT(5,CPU.BC.B.l);break;
case BIT5_D: M_BIT(5,CPU.DE.B.h);break;  case BIT5_E: M_BIT(5,CPU.DE.B.l);break;
case BIT5_H: M_BIT(5,CPU.HL.B.h);break;  case BIT5_L: M_BIT(5,CPU.HL.B.l);break;
case BIT5_xHL: I=RdZ80(CPU.HL.W);M_BIT(5,I);break;
case BIT5_A: M_BIT(5,CPU.AF.B.h);break;

case BIT6_B: M_BIT(6,CPU.BC.B.h);break;  case BIT6_C: M_BIT(6,CPU.BC.B.l);break;
case BIT6_D: M_BIT(6,CPU.DE.B.h);break;  case BIT6_E: M_BIT(6,CPU.DE.B.l);break;
case BIT6_H: M_BIT(6,CPU.HL.B.h);break;  case BIT6_L: M_BIT(6,CPU.HL.B.l);break;
case BIT6_xHL: I=RdZ80(CPU.HL.W);M_BIT(6,I);break;
case BIT6_A: M_BIT(6,CPU.AF.B.h);break;

case BIT7_B: M_BIT(7,CPU.BC.B.h);break;  case BIT7_C: M_BIT(7,CPU.BC.B.l);break;
case BIT7_D: M_BIT(7,CPU.DE.B.h);break;  case BIT7_E: M_BIT(7,CPU.DE.B.l);break;
case BIT7_H: M_BIT(7,CPU.HL.B.h);break;  case BIT7_L: M_BIT(7,CPU.HL.B.l);break;
case BIT7_xHL: I=RdZ80(CPU.HL.W);M_BIT(7,I);break;
case BIT7_A: M_BIT(7,CPU.AF.B.h);break;

case RES0_B: M_RES(0,CPU.BC.B.h);break;  case RES0_C: M_RES(0,CPU.BC.B.l);break;
case RES0_D: M_RES(0,CPU.DE.B.h);break;  case RES0_E: M_RES(0,CPU.DE.B.l);break;
case RES0_H: M_RES(0,CPU.HL.B.h);break;  case RES0_L: M_RES(0,CPU.HL.B.l);break;
case RES0_xHL: I=RdZ80(CPU.HL.W);M_RES(0,I);WrZ80(CPU.HL.W,I);break;
case RES0_A: M_RES(0,CPU.AF.B.h);break;

case RES1_B: M_RES(1,CPU.BC.B.h);break;  case RES1_C: M_RES(1,CPU.BC.B.l);break;
case RES1_D: M_RES(1,CPU.DE.B.h);break;  case RES1_E: M_RES(1,CPU.DE.B.l);break;
case RES1_H: M_RES(1,CPU.HL.B.h);break;  case RES1_L: M_RES(1,CPU.HL.B.l);break;
case RES1_xHL: I=RdZ80(CPU.HL.W);M_RES(1,I);WrZ80(CPU.HL.W,I);break;
case RES1_A: M_RES(1,CPU.AF.B.h);break;

case RES2_B: M_RES(2,CPU.BC.B.h);break;  case RES2_C: M_RES(2,CPU.BC.B.l);break;
case RES2_D: M_RES(2,CPU.DE.B.h);break;  case RES2_E: M_RES(2,CPU.DE.B.l);break;
case RES2_H: M_RES(2,CPU.HL.B.h);break;  case RES2_L: M_RES(2,CPU.HL.B.l);break;
case RES2_xHL: I=RdZ80(CPU.HL.W);M_RES(2,I);WrZ80(CPU.HL.W,I);break;
case RES2_A: M_RES(2,CPU.AF.B.h);break;

case RES3_B: M_RES(3,CPU.BC.B.h);break;  case RES3_C: M_RES(3,CPU.BC.B.l);break;
case RES3_D: M_RES(3,CPU.DE.B.h);break;  case RES3_E: M_RES(3,CPU.DE.B.l);break;
case RES3_H: M_RES(3,CPU.HL.B.h);break;  case RES3_L: M_RES(3,CPU.HL.B.l);break;
case RES3_xHL: I=RdZ80(CPU.HL.W);M_RES(3,I);WrZ80(CPU.HL.W,I);break;
case RES3_A: M_RES(3,CPU.AF.B.h);break;

case RES4_B: M_RES(4,CPU.BC.B.h);break;  case RES4_C: M_RES(4,CPU.BC.B.l);break;
case RES4_D: M_RES(4,CPU.DE.B.h);break;  case RES4_E: M_RES(4,CPU.DE.B.l);break;
case RES4_H: M_RES(4,CPU.HL.B.h);break;  case RES4_L: M_RES(4,CPU.HL.B.l);break;
case RES4_xHL: I=RdZ80(CPU.HL.W);M_RES(4,I);WrZ80(CPU.HL.W,I);break;
case RES4_A: M_RES(4,CPU.AF.B.h);break;

case RES5_B: M_RES(5,CPU.BC.B.h);break;  case RES5_C: M_RES(5,CPU.BC.B.l);break;
case RES5_D: M_RES(5,CPU.DE.B.h);break;  case RES5_E: M_RES(5,CPU.DE.B.l);break;
case RES5_H: M_RES(5,CPU.HL.B.h);break;  case RES5_L: M_RES(5,CPU.HL.B.l);break;
case RES5_xHL: I=RdZ80(CPU.HL.W);M_RES(5,I);WrZ80(CPU.HL.W,I);break;
case RES5_A: M_RES(5,CPU.AF.B.h);break;

case RES6_B: M_RES(6,CPU.BC.B.h);break;  case RES6_C: M_RES(6,CPU.BC.B.l);break;
case RES6_D: M_RES(6,CPU.DE.B.h);break;  case RES6_E: M_RES(6,CPU.DE.B.l);break;
case RES6_H: M_RES(6,CPU.HL.B.h);break;  case RES6_L: M_RES(6,CPU.HL.B.l);break;
case RES6_xHL: I=RdZ80(CPU.HL.W);M_RES(6,I);WrZ80(CPU.HL.W,I);break;
case RES6_A: M_RES(6,CPU.AF.B.h);break;

case RES7_B: M_RES(7,CPU.BC.B.h);break;  case RES7_C: M_RES(7,CPU.BC.B.l);break;
case RES7_D: M_RES(7,CPU.DE.B.h);break;  case RES7_E: M_RES(7,CPU.DE.B.l);break;
case RES7_H: M_RES(7,CPU.HL.B.h);break;  case RES7_L: M_RES(7,CPU.HL.B.l);break;
case RES7_xHL: I=RdZ80(CPU.HL.W);M_RES(7,I);WrZ80(CPU.HL.W,I);break;
case RES7_A: M_RES(7,CPU.AF.B.h);break;

case SET0_B: M_SET(0,CPU.BC.B.h);break;  case SET0_C: M_SET(0,CPU.BC.B.l);break;
case SET0_D: M_SET(0,CPU.DE.B.h);break;  case SET0_E: M_SET(0,CPU.DE.B.l);break;
case SET0_H: M_SET(0,CPU.HL.B.h);break;  case SET0_L: M_SET(0,CPU.HL.B.l);break;
case SET0_xHL: I=RdZ80(CPU.HL.W);M_SET(0,I);WrZ80(CPU.HL.W,I);break;
case SET0_A: M_SET(0,CPU.AF.B.h);break;

case SET1_B: M_SET(1,CPU.BC.B.h);break;  case SET1_C: M_SET(1,CPU.BC.B.l);break;
case SET1_D: M_SET(1,CPU.DE.B.h);break;  case SET1_E: M_SET(1,CPU.DE.B.l);break;
case SET1_H: M_SET(1,CPU.HL.B.h);break;  case SET1_L: M_SET(1,CPU.HL.B.l);break;
case SET1_xHL: I=RdZ80(CPU.HL.W);M_SET(1,I);WrZ80(CPU.HL.W,I);break;
case SET1_A: M_SET(1,CPU.AF.B.h);break;

case SET2_B: M_SET(2,CPU.BC.B.h);break;  case SET2_C: M_SET(2,CPU.BC.B.l);break;
case SET2_D: M_SET(2,CPU.DE.B.h);break;  case SET2_E: M_SET(2,CPU.DE.B.l);break;
case SET2_H: M_SET(2,CPU.HL.B.h);break;  case SET2_L: M_SET(2,CPU.HL.B.l);break;
case SET2_xHL: I=RdZ80(CPU.HL.W);M_SET(2,I);WrZ80(CPU.HL.W,I);break;
case SET2_A: M_SET(2,CPU.AF.B.h);break;

case SET3_B: M_SET(3,CPU.BC.B.h);break;  case SET3_C: M_SET(3,CPU.BC.B.l);break;
case SET3_D: M_SET(3,CPU.DE.B.h);break;  case SET3_E: M_SET(3,CPU.DE.B.l);break;
case SET3_H: M_SET(3,CPU.HL.B.h);break;  case SET3_L: M_SET(3,CPU.HL.B.l);break;
case SET3_xHL: I=RdZ80(CPU.HL.W);M_SET(3,I);WrZ80(CPU.HL.W,I);break;
case SET3_A: M_SET(3,CPU.AF.B.h);break;

case SET4_B: M_SET(4,CPU.BC.B.h);break;  case SET4_C: M_SET(4,CPU.BC.B.l);break;
case SET4_D: M_SET(4,CPU.DE.B.h);break;  case SET4_E: M_SET(4,CPU.DE.B.l);break;
case SET4_H: M_SET(4,CPU.HL.B.h);break;  case SET4_L: M_SET(4,CPU.HL.B.l);break;
case SET4_xHL: I=RdZ80(CPU.HL.W);M_SET(4,I);WrZ80(CPU.HL.W,I);break;
case SET4_A: M_SET(4,CPU.AF.B.h);break;

case SET5_B: M_SET(5,CPU.BC.B.h);break;  case SET5_C: M_SET(5,CPU.BC.B.l);break;
case SET5_D: M_SET(5,CPU.DE.B.h);break;  case SET5_E: M_SET(5,CPU.DE.B.l);break;
case SET5_H: M_SET(5,CPU.HL.B.h);break;  case SET5_L: M_SET(5,CPU.HL.B.l);break;
case SET5_xHL: I=RdZ80(CPU.HL.W);M_SET(5,I);WrZ80(CPU.HL.W,I);break;
case SET5_A: M_SET(5,CPU.AF.B.h);break;

case SET6_B: M_SET(6,CPU.BC.B.h);break;  case SET6_C: M_SET(6,CPU.BC.B.l);break;
case SET6_D: M_SET(6,CPU.DE.B.h);break;  case SET6_E: M_SET(6,CPU.DE.B.l);break;
case SET6_H: M_SET(6,CPU.HL.B.h);break;  case SET6_L: M_SET(6,CPU.HL.B.l);break;
case SET6_xHL: I=RdZ80(CPU.HL.W);M_SET(6,I);WrZ80(CPU.HL.W,I);break;
case SET6_A: M_SET(6,CPU.AF.B.h);break;

case SET7_B: M_SET(7,CPU.BC.B.h);break;  case SET7_C: M_SET(7,CPU.BC.B.l);break;
case SET7_D: M_SET(7,CPU.DE.B.h);break;  case SET7_E: M_SET(7,CPU.DE.B.l);break;
case SET7_H: M_SET(7,CPU.HL.B.h);break;  case SET7_L: M_SET(7,CPU.HL.B.l);break;
case SET7_xHL: I=RdZ80(CPU.HL.W);M_SET(7,I);WrZ80(CPU.HL.W,I);break;
case SET7_A: M_SET(7,CPU.AF.B.h);break;
