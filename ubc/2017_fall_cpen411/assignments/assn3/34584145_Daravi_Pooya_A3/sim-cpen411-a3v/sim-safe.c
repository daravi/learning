/* sim-safe.c - sample functional simulator implementation */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "options.h"
#include "stats.h"
#include "sim.h"

/*
 * This file implements a functional simulator.  This functional simulator is
 * the simplest, most user-friendly simulator in the simplescalar tool set.
 * Unlike sim-fast, this functional simulator checks for all instruction
 * errors, and the implementation is crafted for clarity rather than speed.
 */

/* misprediction and conditional branch counters */
static counter_t g_total_cond_branches = 0;
static counter_t g_total_cust_mispredictions = 0;

/* simulated registers */
static struct regs_t regs;

/* simulated memory */
static struct mem_t *mem = NULL;

/* track number of refs */
static counter_t sim_num_refs = 0;

/* maximum number of inst's to execute */
static unsigned int max_insts;

/* register simulator-specific options */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
"sim-safe: This simulator implements a functional simulator.  This\n"
"functional simulator is the simplest, most user-friendly simulator in the\n"
"simplescalar tool set.  Unlike sim-fast, this functional simulator checks\n"
"for all instruction errors, and the implementation is crafted for clarity\n"
"rather than speed.\n"
		 );

  /* instruction limit */
  opt_reg_uint(odb, "-max:inst", "maximum number of inst's to execute",
	       &max_insts, /* default */0,
	       /* print */TRUE, /* format */NULL);

}

/* check simulator-specific option values */
void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  /* nada */
}

/* register simulator-specific statistics */
void
sim_reg_stats(struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, "sim_num_insn",
		   "total number of instructions executed",
		   &sim_num_insn, sim_num_insn, NULL);
  stat_reg_counter(sdb, "sim_num_refs",
		   "total number of loads and stores executed",
		   &sim_num_refs, 0, NULL);
  stat_reg_int(sdb, "sim_elapsed_time",
	       "total simulation time in seconds",
	       &sim_elapsed_time, 0, NULL);
  stat_reg_formula(sdb, "sim_inst_rate",
		   "simulation speed (in insts/sec)",
		   "sim_num_insn / sim_elapsed_time", NULL);

  stat_reg_counter(sdb, "sim_num_cond_branches",
       "total number of conditional branches executed",
       &g_total_cond_branches, 
       g_total_cond_branches, NULL);
  // hierarchical predictor
  stat_reg_counter(sdb, "sim_num_custom_mispredict",
       "total number of mispredictions using a hierarchical predictor",
       &g_total_cust_mispredictions, 
       g_total_cust_mispredictions, NULL);
  stat_reg_formula(sdb, "sim_custom_pred_accuracy",
       "branch prediction accuracy using a hierarchical predictor",
       "1 - sim_num_custom_mispredict / sim_num_cond_branches", NULL);

  ld_reg_stats(sdb);
  mem_reg_stats(mem, sdb);
}

/* initialize the simulator */
void
sim_init(void)
{
  sim_num_refs = 0;

  /* allocate and initialize register file */
  regs_init(&regs);

  /* allocate and initialize memory space */
  mem = mem_create("mem");
  mem_init(mem);
}

/* load program into simulated state */
void
sim_load_prog(char *fname,		/* program to load */
	      int argc, char **argv,	/* program arguments */
	      char **envp)		/* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);
}

/* print simulator-specific configuration information */
void
sim_aux_config(FILE *stream)		/* output stream */
{
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void
sim_aux_stats(FILE *stream)		/* output stream */
{
  /* nada */
}

/* un-initialize simulator-specific state */
void
sim_uninit(void)
{
  /* nada */
}


/*
 * configure the execution engine
 */

/*
 * precise architected register accessors
 */

/* next program counter */
#define SET_NPC(EXPR)		(regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC			(regs.regs_PC)

/* general purpose registers */
#define GPR(N)			(regs.regs_R[N])
#define SET_GPR(N,EXPR)		(regs.regs_R[N] = (EXPR))

#if defined(TARGET_PISA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)		(regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)	(regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)		(regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)	(regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)		(regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR)	(regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)		(regs.regs_C.hi = (EXPR))
#define HI			(regs.regs_C.hi)
#define SET_LO(EXPR)		(regs.regs_C.lo = (EXPR))
#define LO			(regs.regs_C.lo)
#define FCC			(regs.regs_C.fcc)
#define SET_FCC(EXPR)		(regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_Q(N)		(regs.regs_F.q[N])
#define SET_FPR_Q(N,EXPR)	(regs.regs_F.q[N] = (EXPR))
#define FPR(N)			(regs.regs_F.d[(N)])
#define SET_FPR(N,EXPR)		(regs.regs_F.d[(N)] = (EXPR))

/* miscellaneous register accessors */
#define FPCR			(regs.regs_C.fpcr)
#define SET_FPCR(EXPR)		(regs.regs_C.fpcr = (EXPR))
#define UNIQ			(regs.regs_C.uniq)
#define SET_UNIQ(EXPR)		(regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST)	sys_syscall(&regs, mem_access, mem, INST, TRUE)

#define DNA         (0)

/* general register dependence decoders */
#define DGPR(N)         (N)
#define DGPR_D(N)       ((N) &~1)

/* floating point register dependence decoders */
#define DFPR_L(N)       (((N)+32)&~1)
#define DFPR_F(N)       (((N)+32)&~1)
#define DFPR_D(N)       (((N)+32)&~1)

/* miscellaneous register dependence decoders */
#define DHI         (0+32+32)
#define DLO         (1+32+32)
#define DFCC            (2+32+32)
#define DTMP            (3+32+32)

/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void)
{
  md_inst_t inst;
  register md_addr_t addr;
  enum md_opcode op;
  register int is_write;
  enum md_fault_type fault;

  const int bpred_mem_log = 18;
  const int bpred_mem_size = (1<<bpred_mem_log);

  // custom predictor indexes
  unsigned index_cust_sim = 0;
  unsigned index_cust_trn = 0;
  unsigned index_cust_gbht = 0;
  unsigned index_cust_pbht = 0;
  // other
  int prediction = 0;
  int gprediction = 0;
  int pprediction = 0;
  int cust_inst_tag = 0;

  // custom predictor parameters (acquired by tweeking!)
  const int cust_sim_mem_size = bpred_mem_size / 2;
  const int cust_trn_mem_size = bpred_mem_size / 2;
  const int cust_sim_row_size = 2;
  const int cust_trn_tag_size = 4;
  const int cust_psuedo_size = 3;  
  const int cust_ghist_size = 11;
  const int cust_gbht_size = 3;
  const int cust_phist_size = 3;
  const int cust_pbht_size = 3;

  const int cust_trn_row_size = 
    cust_trn_tag_size +
    cust_psuedo_size  +
    cust_gbht_size    +
    cust_phist_size   +
    cust_pbht_size;

  const int cust_sim_index_size = log2(cust_sim_mem_size) - log2(cust_sim_row_size);
  const int cust_trn_index_size = log2(cust_trn_mem_size) - log2(cust_trn_row_size);
  const int cust_sim_rows = (bpred_mem_size/2) / cust_sim_row_size;
  const int cust_trn_rows = (bpred_mem_size/2) / cust_trn_row_size;
  // custom predictor tables
  int bpred_cust_sim[ cust_sim_rows ];
  int bpred_cust_trn_tag[ cust_trn_rows ];
  int bpred_cust_phist[ cust_trn_rows ];
  int bpred_cust_pbht[ cust_trn_rows ];
  int bpred_cust_gbht[ cust_trn_rows ];
  int bpred_cust_pseudo_bht[ cust_trn_rows ];
  int cust_phistory = 0;
  int cust_ghistory = 0;
  int cust_trn_tag = 0;
  
  // initialize to pshare state
  // int i=0;
  // for (i=0; i<cust_trn_rows; i++) bpred_cust_pseudo_bht[i] = ((1<<cust_psuedo_size)-1);

  fprintf(stderr, "sim: ** starting functional simulation **\n");

  /* set up initial default next PC */
  regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);


  while (TRUE)
    {
      /* maintain $r0 semantics */
      regs.regs_R[MD_REG_ZERO] = 0;
#ifdef TARGET_ALPHA
      regs.regs_F.d[MD_REG_ZERO] = 0.0;
#endif /* TARGET_ALPHA */

      /* get the next instruction to execute */
      MD_FETCH_INST(inst, mem, regs.regs_PC);

      /* keep an instruction count */
      sim_num_insn++;

      /* set default reference address and access mode */
      addr = 0; is_write = FALSE;

      /* set default fault - none */
      fault = md_fault_none;

      /* decode the instruction */
      MD_SET_OPCODE(op, inst);

      /* execute the instruction */
      switch (op)
	{
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
	case OP:							\
          SYMCAT(OP,_IMPL);						\
          break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
        case OP:							\
          panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)						\
	  { fault = (FAULT); break; }
#include "machine.def"
	default:
	  panic("attempted to execute a bogus opcode");
      }

      if (fault != md_fault_none)
	fatal("fault (%d) detected @ 0x%08p", fault, regs.regs_PC);

      if (verbose)
	{
	  myfprintf(stderr, "%10n [xor: 0x%08x] @ 0x%08p: ",
		    sim_num_insn, md_xor_regs(&regs), regs.regs_PC);
	  md_print_insn(inst, regs.regs_PC, stderr);
	  if (MD_OP_FLAGS(op) & F_MEM)
	    myfprintf(stderr, "  mem: 0x%08p", addr);
	  fprintf(stderr, "\n");
	  /* fflush(stderr); */
	}

      if (MD_OP_FLAGS(op) & F_MEM)
	{
	  sim_num_refs++;
	  if (MD_OP_FLAGS(op) & F_STORE)
	    is_write = TRUE;
	}

  // 
  if ( MD_OP_FLAGS(op) & F_COND ) {
    g_total_cond_branches++;
    int actual_outcome = (regs.regs_NPC != (regs.regs_PC + sizeof(md_inst_t)));

    /*
      *******************************************
      part (v): custom predictor
                - hierarchical: 2-bit + tournament
      counter type: hierarchical
      cheap counte type: simple
      expensive counter type: tournament
      tournament counter parts: pshare + gshare
      predictor parameters: see above
      *******************************************
    */
    index_cust_sim = (regs.regs_PC>>3) & ((1<<cust_sim_index_size)-1);
    assert( index_cust_sim < cust_sim_rows );

    index_cust_trn = (regs.regs_PC>>3) & ((1<<cust_trn_index_size)-1);
    assert( index_cust_trn < cust_trn_rows );
    cust_inst_tag = (regs.regs_PC>>(3+cust_trn_index_size)) & ((1<<cust_trn_tag_size)-1);
    assert( cust_inst_tag < (1<<cust_trn_tag_size) );
    cust_trn_tag = bpred_cust_trn_tag[ index_cust_trn ];
    if ( cust_inst_tag == cust_trn_tag ) {
      // using the tournament predictor:
      // gshare history:
      index_cust_gbht = index_cust_trn ^ (cust_ghistory<<(cust_trn_index_size-cust_ghist_size));
      assert( index_cust_gbht < cust_trn_rows );
      gprediction = bpred_cust_gbht[ index_cust_gbht ] >> (cust_gbht_size-1);
      // pshare history:
      cust_phistory = bpred_cust_phist[ index_cust_trn ];
      index_cust_pbht = index_cust_trn ^ (cust_phistory<<(cust_trn_index_size-cust_phist_size));
      assert( index_cust_pbht < cust_trn_rows );
      pprediction = bpred_cust_pbht[ index_cust_pbht ] >> (cust_pbht_size-1);

      // read prediction based on psuedo predictor state
      if ( bpred_cust_pseudo_bht[ index_cust_trn ] >> (cust_psuedo_size-1) ) {
        // use private history
        prediction = pprediction;

      } else {
        // use global history
        prediction = gprediction;
      }

      // update the pseudo predictor counter
      if ((pprediction == actual_outcome) && (gprediction != actual_outcome)) {
        if ( bpred_cust_pseudo_bht[ index_cust_trn ] < ((1<<cust_psuedo_size)-1) )
          bpred_cust_pseudo_bht[ index_cust_trn ]++;
      } else if ((pprediction != actual_outcome) && (gprediction == actual_outcome)) {
        if ( bpred_cust_pseudo_bht[ index_cust_trn ] > 0 )
          bpred_cust_pseudo_bht[ index_cust_trn ]--;
      }

      // update stats
      if ( prediction != actual_outcome ) g_total_cust_mispredictions++;

      // update predictor based on actual outcome of branch
      // update pshare bht (2-bit saturating counter)
      if ( actual_outcome ) {
        if ( bpred_cust_pbht[ index_cust_pbht ] < ((1<<cust_pbht_size)-1) )
          bpred_cust_pbht[ index_cust_pbht ]++;
      } else {
        if ( bpred_cust_pbht[ index_cust_pbht ] > 0 )
          bpred_cust_pbht[ index_cust_pbht ]--;
      }
      // update pshare pht
      bpred_cust_phist[ index_cust_trn ] = ((cust_phistory << 1) + actual_outcome) & ((1<<cust_phist_size)-1);
      // update gshare bht (2-bit saturating counter)
      if ( actual_outcome ) {
        if ( bpred_cust_gbht[ index_cust_gbht ] < ((1<<cust_gbht_size)-1) )
          bpred_cust_gbht[ index_cust_gbht ]++;
      } else {
        if ( bpred_cust_gbht[ index_cust_gbht ] > 0 )
          bpred_cust_gbht[ index_cust_gbht ]--;
      }
      // update gshare history
      cust_ghistory = ((cust_ghistory << 1) + actual_outcome) & ((1<<cust_ghist_size)-1);
    } else {
      // using the simple 2-bit predictor
      prediction = bpred_cust_sim[ index_cust_sim ] >> (cust_sim_row_size-1);
      // update stats
      if ( prediction != actual_outcome ) {
        // update stats
        g_total_cust_mispredictions++;
        // update tag
        bpred_cust_trn_tag[ index_cust_trn ] = cust_inst_tag;     
      }
      // update simple predictor counter
      if ( actual_outcome ) {
        if ( bpred_cust_sim[ index_cust_sim ] < ((1<<cust_sim_row_size)-1) )
          bpred_cust_sim[ index_cust_sim ]++;
      } else {
        if ( bpred_cust_sim[ index_cust_sim ] > 0 )
          bpred_cust_sim[ index_cust_sim ]--;
      }
    }
  }

      /* go to the next instruction */
      regs.regs_PC = regs.regs_NPC;
      regs.regs_NPC += sizeof(md_inst_t);

      /* finish early? */
      if (max_insts && sim_num_insn >= max_insts)
	return;
    }
}
