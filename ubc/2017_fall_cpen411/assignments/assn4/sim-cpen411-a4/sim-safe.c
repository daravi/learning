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


/* counter for cache misses */
static counter_t g_dcache_miss_ld;
static counter_t g_dcache_miss_st;
static counter_t g_dcache_wb;

static counter_t sim_num_load;
static counter_t sim_num_store;

enum CacheAccessMode
{
    READ,
    WRITE,
    UPDATE
};

enum CacheUpdateMode
{
    CHECKOUT,
    INSERT,
    REPLACE,
    DELETE
};

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
sim_reg_options(struct opt_odb_t *odb) {
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
sim_check_options(struct opt_odb_t *odb, int argc, char **argv) {
  /* nada */
}

/* register simulator-specific statistics */
void
sim_reg_stats(struct stat_sdb_t *sdb) {
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

  /* cache miss rates for part b */
  stat_reg_counter(sdb, "sim_num_load",
                   "total number of load instructions executed",
                   &sim_num_load, sim_num_load, NULL);
  stat_reg_counter(sdb, "sim_num_dcache_miss_ld",
                   "total number of data cache misses for load",
                   &g_dcache_miss_ld, g_dcache_miss_ld, NULL);
  stat_reg_formula(sdb, "sim_dcache_miss_ld_rate",
                   "data cache miss rate (percentage) for load",
                   "100*(sim_num_dcache_miss_ld / sim_num_load)", NULL);
  stat_reg_counter(sdb, "sim_num_store",
                   "total number of store instructions executed",
                   &sim_num_store, sim_num_store, NULL);
  stat_reg_counter(sdb, "sim_num_dcache_miss_st",
                   "total number of data cache misses for store",
                   &g_dcache_miss_st, g_dcache_miss_st, NULL);
  stat_reg_formula(sdb, "sim_dcache_miss_st_rate",
                   "data cache miss rate (percentage) for store",
                   "100*(sim_num_dcache_miss_st / sim_num_store)", NULL);
  stat_reg_counter(sdb, "sim_num_dcache_wb",
                   "total number of writeback events",
                   &g_dcache_wb, g_dcache_wb, NULL);
  stat_reg_formula(sdb, "sim_dcache_wb_rate",
                   "data cache writeback rate (percentage)",
                   "100*(sim_num_dcache_wb / sim_num_store)", NULL);

  ld_reg_stats(sdb);
  mem_reg_stats(mem, sdb);
}

/* initialize the simulator */
void
sim_init(void) {
  sim_num_refs = 0;

  /* allocate and initialize register file */
  regs_init(&regs);

  /* allocate and initialize memory space */
  mem = mem_create("mem");
  mem_init(mem);
}

/* load program into simulated state */
void
sim_load_prog(char *fname,    /* program to load */
              int argc, char **argv,  /* program arguments */
              char **envp) {  /* program environment */
  /* load program text and data, set up environment, memory, and regs */
  ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);
}

/* print simulator-specific configuration information */
void
sim_aux_config(FILE *stream) {  /* output stream */
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void
sim_aux_stats(FILE *stream) { /* output stream */
  /* nada */
}

/* un-initialize simulator-specific state */
void
sim_uninit(void) {
  /* nada */
}


/*
 * configure the execution engine
 */

/*
 * precise architected register accessors
 */

/* next program counter */
#define SET_NPC(EXPR)   (regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC     (regs.regs_PC)

/* general purpose registers */
#define GPR(N)      (regs.regs_R[N])
#define SET_GPR(N,EXPR)   (regs.regs_R[N] = (EXPR))

#if defined(TARGET_PISA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)    (regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR) (regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)    (regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR) (regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)    (regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR) (regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)    (regs.regs_C.hi = (EXPR))
#define HI      (regs.regs_C.hi)
#define SET_LO(EXPR)    (regs.regs_C.lo = (EXPR))
#define LO      (regs.regs_C.lo)
#define FCC     (regs.regs_C.fcc)
#define SET_FCC(EXPR)   (regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_Q(N)    (regs.regs_F.q[N])
#define SET_FPR_Q(N,EXPR) (regs.regs_F.q[N] = (EXPR))
#define FPR(N)      (regs.regs_F.d[(N)])
#define SET_FPR(N,EXPR)   (regs.regs_F.d[(N)] = (EXPR))

/* miscellaneous register accessors */
#define FPCR      (regs.regs_C.fpcr)
#define SET_FPCR(EXPR)    (regs.regs_C.fpcr = (EXPR))
#define UNIQ      (regs.regs_C.uniq)
#define SET_UNIQ(EXPR)    (regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)           \
((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT)           \
((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT)           \
((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)            \
((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)         \
((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)         \
((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)         \
((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)          \
((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST) sys_syscall(&regs, mem_access, mem, INST, TRUE)

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



struct block {
  int m_valid; // is block valid?
  int m_lru; // between 0 and n-1 for n-way cache. Where 0 means LRU.
  md_addr_t m_tag; // tag used to determine whether we have a cache hit
};

struct cache {
  struct cache *parent;
  struct cache *child;
  struct block **m_line_array;
  unsigned m_ways;
  unsigned m_total_lines;
  unsigned m_set_shift;
  unsigned m_set_mask;
  unsigned m_tag_shift;
};

void create_cache( struct cache *c, unsigned csize, unsigned cways, unsigned cbsize) {
  c->m_ways = cways;
  c->m_total_lines = csize / (cbsize * cways);
  c->m_line_array = (struct block **) calloc( sizeof(struct block *), c->m_total_lines );
  unsigned int idx;
  for ( idx = 0; idx < c->m_total_lines; idx++ )
    c->m_line_array[idx] = (struct block *) calloc( sizeof(struct block), cways );
  unsigned offset_bits = log2(cbsize);
  c->m_set_shift = offset_bits;
  unsigned index_bits = log2(c->m_total_lines);
  if (index_bits == 0) c->m_set_mask = 0;
  else                 c->m_set_mask = (1 << index_bits) - 1;
  c->m_tag_shift = offset_bits + index_bits;
}

int cache_full( struct cache *c, unsigned idx, unsigned *pos) {
  unsigned i;
  for ( i = 0; i < c->m_ways; i++ ) {
    if (!c->m_line_array[idx][i].m_valid) {
      *pos = i;
      return FALSE;
    } else if (c->m_line_array[idx][i].m_lru == 0) {
      *pos = i;
    }
  }

  return TRUE;
}

int cache_hit( struct cache *c, unsigned idx, unsigned tag, unsigned *pos) {
  unsigned i;
  for ( i = 0; i < c->m_ways; i++ ) {
    if (c->m_line_array[idx][i].m_valid && (c->m_line_array[idx][i].m_tag == tag)) {
      *pos = i;
      return TRUE;
    }
  }
  return FALSE;
}

void cache_update( struct cache *c, unsigned idx, unsigned pos, enum CacheUpdateMode cum, unsigned tag, unsigned *evicted_tag ) {
  unsigned i;
  switch ( cum )
  {
    case CHECKOUT:
      break;
    case INSERT:
      c->m_line_array[idx][pos].m_valid = 1;
      c->m_line_array[idx][pos].m_tag = tag;
      break;
    case REPLACE:
      c->m_line_array[idx][pos].m_valid = 1;
      *evicted_tag = c->m_line_array[idx][pos].m_tag;
      c->m_line_array[idx][pos].m_tag = tag;
      break;
    case DELETE:
      c->m_line_array[idx][pos].m_valid = 0;
      for ( i = 0; i < c->m_ways; i++ ) {
        if ( c->m_line_array[idx][i].m_lru < c->m_line_array[idx][pos].m_lru ) {
          c->m_line_array[idx][i].m_lru++;
        }
      }
      c->m_line_array[idx][pos].m_lru = 0;
      return;
    default:
      break;
  }
  /* update lru */
  for ( i = 0; i < c->m_ways; i++ ) {
    if ( c->m_line_array[idx][i].m_lru > c->m_line_array[idx][pos].m_lru ) {
      c->m_line_array[idx][i].m_lru--;
    }
  }
  c->m_line_array[idx][pos].m_lru = c->m_ways - 1;

  return;
}

void cache_access( struct cache *c, enum CacheAccessMode cam, unsigned addr, counter_t *miss_counter, counter_t *wb_counter, struct cache *rc ) {
  unsigned idx, tag, evicted_tag, pos;          // pos: block position within a line
  idx = (addr >> c->m_set_shift)&c->m_set_mask; // line index within the cache
  tag = (addr >> c->m_tag_shift);
  assert( idx < c->m_total_lines );

  /* pos: block position within a set */
  for ( pos = 0; pos < c->m_ways; pos++ ) {
    if (c->m_line_array[idx][pos].m_valid && (c->m_line_array[idx][pos].m_tag == tag)) {
      /* cache hit: update lru bits */
      for ( i = 0; i < c->m_ways; i++ ) {
        if ( c->m_line_array[idx][i].m_lru > c->m_line_array[idx][pos].m_lru ) {
          c->m_line_array[idx][i].m_lru--;
        }
      }
      c->m_line_array[idx][pos].m_lru = c->m_ways - 1;
      return;
    }
  }

  /* cache miss */
  for ( pos = 0; pos < c->vc->m_ways; pos++ ) {
    if (c->vc->m_line_array[idx][pos].m_valid && (c->vc->m_line_array[idx][pos].m_tag == tag)) {
      /* victim cache hit: promote to main cache and update lru bits */
      /* promote to main cache */
      for (i = 0; i < c->m_ways; i++) {
        if ( c->m_line_array[idx][i].m_lru == 0 ) {
          /* evict and replace */
          if ( c->m_line_array[idx][i].m_valid ) {
            /* evict to victim cache */
            
          }
          c->m_line_array[idx][i].m_valid = 1;
          c->m_line_array[idx][i].m_tag = tag;
          /* update lru bits */
          for ( i = 0; i < c->m_ways; i++ ) {
            if ( c->m_line_array[idx][i].m_lru > 0 ) {
              c->m_line_array[idx][i].m_lru--;
            }
          }
          c->m_line_array[idx][i].m_lru = c->m_ways - 1;
          return;
        }
      }
      for ( i = 0; i < c->m_ways; i++ ) {
        if ( c->vc->m_line_array[idx][i].m_lru > c->vc->m_line_array[idx][pos].m_lru ) {
          c->vc->m_line_array[idx][i].m_lru--;
        }
      }
      c->vc->m_line_array[idx][pos].m_lru = c->m_ways - 1;
      return;
    }
  }


  *miss_counter = *miss_counter + 1;
  for (pos = 0; pos < c->m_ways; pos++) {
    if ( c->m_line_array[idx][pos].m_lru == 0 ) {
      /* evict and replace, update writeback counter */
      if ( c->m_line_array[idx][pos].m_valid )
        *wb_counter = *wb_counter + 1;
      c->m_line_array[idx][pos].m_valid = 1;
      c->m_line_array[idx][pos].m_tag = tag;
      /* update lru bits */
      for ( i = 0; i < c->m_ways; i++ ) {
        if ( c->m_line_array[idx][i].m_lru > 0 ) {
          c->m_line_array[idx][i].m_lru--;
        }
      }
      c->m_line_array[idx][pos].m_lru = c->m_ways - 1;
      return;
    }
  }
}

/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void) {
  md_inst_t inst;
  register md_addr_t addr;
  enum md_opcode op;
  register int is_write;
  enum md_fault_type fault;

  unsigned int data_addr;

  /* create l1$ (data cache) and l2$ (victim cache) */
  struct cache *dcache = (struct cache *) calloc( sizeof(struct cache), 1 );
  /* uncomment only one of these three architectures to measure its miss rates */
  // create_cache(dcache, 32*1024, 1, 64); // direct-mapped
  // create_cache(dcache, 32*1024, 2, 64); // 2-way
  create_cache(dcache, 32*1024, 4, 64); // 4-way
  
  /* comment out the victim cache to compare and measure its effectiveness */
  struct cache *vcache = (struct cache *) calloc( sizeof(struct cache), 1 );
  create_cache(vcache, 2*1024, 2*1024/64, 64); // fully associative victim cache

  /* connect data cache to its victim cache */
  dcache->child = vcache;
  vcache->parent = dcache;

  fprintf(stderr, "sim: ** starting functional simulation **\n");

  /* set up initial default next PC */
  regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);


  while (TRUE) {
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
    switch (op) {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)    \
     case OP:             \
     SYMCAT(OP,_IMPL);            \
     break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)         \
     case OP:             \
     panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)            \
     { fault = (FAULT); break; }
#include "machine.def"
    default:
      panic("attempted to execute a bogus opcode");
    }

    if (fault != md_fault_none)
      fatal("fault (%d) detected @ 0x%08p", fault, regs.regs_PC);

    if (verbose) {
      myfprintf(stderr, "%10n [xor: 0x%08x] @ 0x%08p: ",
                sim_num_insn, md_xor_regs(&regs), regs.regs_PC);
      md_print_insn(inst, regs.regs_PC, stderr);
      if (MD_OP_FLAGS(op) & F_MEM)
        myfprintf(stderr, "  mem: 0x%08p", addr);
      fprintf(stderr, "\n");
      /* fflush(stderr); */
    }

    if (MD_OP_FLAGS(op) & F_MEM) {
      sim_num_refs++;
      data_addr = GPR(BS) + OFS;
      if (MD_OP_FLAGS(op) & F_STORE) {
        is_write = TRUE;
        sim_num_store++;
        cache_access(dcache, WRITE, data_addr, &g_dcache_miss_st, &g_dcache_wb, dcache);
      } else if (MD_OP_FLAGS(op) & F_LOAD) {
        sim_num_load++;
        cache_access(dcache, READ, data_addr, &g_dcache_miss_ld, &g_dcache_wb, dcache);
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
