/*  The mening of the defined macros is as follows:
 *  VECLEN:         The length of a singleprecision vector register
 *  vec_add:        Add to single precision vectors.
 *  vec_mul:        Multiply to single precision vectors.
 *  vec_mov:        Moves data around
 *  vec_mov1:       Load one element in a vector and zero all other entries!
 *  vec_splat:      Load one element relpicated in all positions in the vector.
 *  vec_load_apart: Load elements from different memory positions into a register.
 *  vec_sum:        Sums a register.
 *  vec_store_one:  Stores lowest element in vector to memory, no zero-extend!
 * Meaning of suffixes is as follows:
 *    mr means memory to register
 *    rr means register to register
 *    rm means register to memory
 *    a means that instruction needs aligned data
 *    1 means that the instructions only operates on the lowest element of the
 *      vector.
 *
 * The _1 instructions work under one important assumption: That you never mix them
 * with regular instructions, e.g. loading into a register with a normal mov, and
 * then using add_rr_1 will not work under 3dnow! since it is in reality a normal add.
 * However, if using a mov_1 first, the upper part of the register will be zeroed,
 * and it will therefore work. The _1 system is more robust under SSE, but other
 * architectures might be implemented the same way as 3dnow!
 *
 */


/* Things to try:
 *    Non-temporal stores
 *    Sequences of instructions instead of movups
 *
 *
 *
 *
 */



#define gen_vec_rr(op,reg1,reg2) \
        __asm__ __volatile__ (#op " " #reg1 ", " #reg2 \
                              :  /* nothing */ \
                              : /* nothing */)


#define w(p) p

#define nop()             __asm__ __volatile__ ("nop")

#define rep()             __asm__ __volatile__ ("rep")

#define align()           __asm__ __volatile__ (".align 16")


#ifdef x87double

#define st0 %%st(0)
#define st1 %%st(1)
#define st2 %%st(2)
#define st3 %%st(3)
#define st4 %%st(4)
#define st5 %%st(5)
#define st6 %%st(6)
#define st7 %%st(7)


#define gen_stack_rt(op,reg) \
        __asm__ __volatile__ (#op " " #reg \
                              :  /* nothing */ \
                              : /* nothing */)

#define gen_stack_tr(op,reg) \
        __asm__ __volatile__ (#op " %%st(0)," #reg \
                              :  \
                              : )


#define gen_stack_rr(op,reg1,reg2) \
        __asm__ __volatile__ (#op " " #reg1 ", " #reg2 \
                              :  /* nothing */ \
                              : /* nothing */)

#define gen_stack_t(op)  \
        __asm__ __volatile__ (#op \
                              :  /* nothing */ \
                              : /* nothing */)


#define gen_stack_tm(op,mem) \
        __asm__ __volatile__ (#op " %0" \
                              : "=m" (((mem)[0])) \
                              :  )

#define gen_stack_mt(op,mem) \
        __asm__ __volatile__ (#op " %0" \
                              :  \
                              : "m" (((mem)[0])))


#define stack_mov_mt_push(mem)  gen_stack_mt(fldl,mem)

#define stack_add_tr_pop(reg)   gen_stack_tr(faddp,reg)
#define stack_add_mt(mem)       gen_stack_mt(faddl,mem)

#define stack_mul_tr(reg)       gen_stack_tr(fmul,reg)
#define stack_mul_tr_pop(reg)   gen_stack_tr(fmulp,reg)
#define stack_mul_mt(mem)       gen_stack_mt(fmul,mem)

#define stack_mov_tm_pop(mem)   gen_stack_tm(fstpl,mem)

#define stack_zero_push()       gen_stack_t(fldz)

#endif /* x87double */

#ifdef SSE

/* Peculiarities of SSE: Alignment is good, but not mandatory. It is possible to
 * load/store from misaligned adresses using movups at a cost of some cycles. Loading
 * using mul/add must always be aligned. Alignment is 16 bytes.
 * No muladd.
 */



#define gen_vec_mr(op,mem,reg) \
        __asm__ __volatile__ (#op " %0, " #reg \
                              :  /* nothing */ \
                              : "m" (((mem)[0])), "m" (((mem)[1])), "m" (((mem)[2])), "m" (((mem)[3])))


#define gen_vec_rm(op,reg,mem) \
        __asm__ __volatile__ (#op " " #reg ", %0" \
                              : "=m" (((mem)[0])), "=m" (((mem)[1])), "=m" (((mem)[2])), "=m" (((mem)[3])) \
                              :  /* nothing */ )




#define VECLEN 4

#define reg0 %%xmm0
#define reg1 %%xmm1
#define reg2 %%xmm2
#define reg3 %%xmm3
#define reg4 %%xmm4
#define reg5 %%xmm5
#define reg6 %%xmm6
#define reg7 %%xmm7

#define vec_mov_mr(mem,reg)     gen_vec_mr(movups,mem,reg)
#define vec_mov_rm(reg,mem)     gen_vec_rm(movups,reg,mem)
#define vec_mov_mr_a(mem,reg)   gen_vec_mr(movaps,mem,reg)
#define vec_mov_rm_a(reg,mem)   gen_vec_rm(movaps,reg,mem)
#define vec_mov_rr(reg1,reg2)   gen_vec_rr(movaps,reg1,reg2)

#define vec_add_mr_a(mem,reg)   gen_vec_mr(addps,mem,reg)
#define vec_mul_mr_a(mem,reg)   gen_vec_mr(mulps,mem,reg)

#define vec_add_rr(mem,reg)     gen_vec_rr(addps,mem,reg)
#define vec_mul_rr(mem,reg)     gen_vec_rr(mulps,mem,reg)

#define vec_mov_mr_1(mem,reg)   gen_vec_mr(movss,mem,reg)
#define vec_mov_rm_1(reg,mem)   gen_vec_rm(movss,reg,mem)
#define vec_mov_rr_1(reg1,reg2) gen_vec_rr(movss,reg1,reg2)

#define vec_add_mr_1(mem,reg)   gen_vec_mr(addss,mem,reg)
#define vec_add_rr_1(reg1,reg2) gen_vec_rr(addss,reg1,reg2)

#define vec_mul_mr_1(mem,reg)   gen_vec_mr(mulss,mem,reg)
#define vec_mul_rr_1(reg1,reg2) gen_vec_rr(mulss,reg1,reg2)

#define vec_unpack_low(reg1,reg2)  gen_vec_rr(unpcklps,reg1,reg2)
#define vec_unpack_high(reg1,reg2) gen_vec_rr(unpckhps,reg1,reg2)
#define vec_shuffle(mode,reg1,reg2) vec_shuffle_wrap(mode,reg1,reg2)
#define vec_shuffle_wrap(mode,reg1,reg2) \
        __asm__ __volatile__ ("shufps " #mode ", " #reg1 ", " #reg2 \
			      : /* nothing */\
			      : /* nothing */)

/* Hack! */
/* To use this instruction be sure that register 7 is not in use!!! */
/* It must be possible to reduce this sequence to only four instructions.
 * please tell me how! */
#define vec_sum(reg) vec_sum_wrap(reg)
#define vec_sum_wrap(reg) \
        __asm__ __volatile__ ("movhlps " #reg ", %%xmm7\n"\
    			      "addps " #reg ", %%xmm7\n"\
    			      "movaps %%xmm7, " #reg "\n"\
                              "shufps $1, " #reg ", %%xmm7\n"\
    			      "addss %%xmm7, " #reg "\n"\
			      : /* nothing */\
			      : /* nothing */)


#define vec_splat(mem,reg)      vec_splat_wrap(mem,reg)
#define vec_splat_wrap(mem,reg) \
        __asm__ __volatile__ ("movss %0, " #reg "\n"\
			      "unpcklps " #reg ", " #reg "\n"\
			      "movlhps " #reg ", " #reg "\n"\
			      : /* nothing */ \
                              : "m" ((mem)[0]))


/* This instruction sequence appears courtesy of Camm Maguire. */
#define vec_sum_full(reg0,reg1,reg2,reg3,regout,empty0,empty1) vec_sum_full_wrap(reg0,reg1,reg2,reg3,regout,empty0,empty1)
#define vec_sum_full_wrap(reg0,reg1,reg2,reg3,regout,empty0,empty1) \
      __asm__ __volatile__ ("movaps " #reg0 "," #empty0 "\n"\
			    "unpcklps " #reg1 "," #reg0 "\n"\
			    "movaps " #reg2 "," #empty1 "\n"\
			    "unpckhps " #reg1 "," #empty0 "\n"\
			    "unpcklps " #reg3 "," #reg2 "\n"\
			    "addps  " #empty0 "," #reg0 "\n"\
			    "unpckhps " #reg3 "," #empty1 "\n"\
			    "movaps " #reg0 "," #regout "\n"\
			    "addps  " #empty1 "," #reg2 "\n"\
			    "shufps $0x44," #reg2 "," #reg0 "\n"\
			    "shufps $0xee," #reg2 "," #regout "\n"\
			    "addps  " #reg0 "," #regout "\n"\
			    : /* nothing */  \
			    : /* nothing */)			



typedef float vector[VECLEN];

#endif





#ifdef SSE2

/* Peculiarities of SSE: Alignment is good, but not mandatory. It is possible to
 * load/store from misaligned adresses using movups at a cost of some cycles. Loading
 * using mul/add must always be aligned. Alignment is 16 bytes.
 * No muladd.
 */



#define gen_vec_mr(op,mem,reg) \
        __asm__ __volatile__ (#op " %0, " #reg \
                              :  /* nothing */ \
                              : "m" (((mem)[0])), "m" (((mem)[1])))


#define gen_vec_rm(op,reg,mem) \
        __asm__ __volatile__ (#op " " #reg ", %0" \
                              : "=m" (((mem)[0])), "=m" (((mem)[1])) \
                              :  /* nothing */ )




#define VECLEN 2

#define reg0 %%xmm0
#define reg1 %%xmm1
#define reg2 %%xmm2
#define reg3 %%xmm3
#define reg4 %%xmm4
#define reg5 %%xmm5
#define reg6 %%xmm6
#define reg7 %%xmm7


#define vec_mov_mr(mem,reg)     gen_vec_mr(movupd,mem,reg)
#define vec_mov_rm(reg,mem)     gen_vec_rm(movupd,reg,mem)
#define vec_mov_mr_a(mem,reg)   gen_vec_mr(movapd,mem,reg)
#define vec_mov_rm_a(reg,mem)   gen_vec_rm(movapd,reg,mem)
#define vec_mov_rr(reg1,reg2)   gen_vec_rr(movapd,reg1,reg2)

#define vec_add_mr_a(mem,reg)   gen_vec_mr(addpd,mem,reg)
#define vec_mul_mr_a(mem,reg)   gen_vec_mr(mulpd,mem,reg)

#define vec_add_rr(mem,reg)     gen_vec_rr(addpd,mem,reg)
#define vec_mul_rr(mem,reg)     gen_vec_rr(mulpd,mem,reg)

#define vec_mov_mr_1(mem,reg)   gen_vec_mr(movsd,mem,reg)
#define vec_mov_rm_1(reg,mem)   gen_vec_rm(movsd,reg,mem)
#define vec_mov_rr_1(reg1,reg2) gen_vec_rr(movsd,reg1,reg2)

#define vec_add_mr_1(mem,reg)   gen_vec_mr(addsd,mem,reg)
#define vec_add_rr_1(reg1,reg2) gen_vec_rr(addsd,reg1,reg2)

#define vec_mul_mr_1(mem,reg)   gen_vec_mr(mulsd,mem,reg)
#define vec_mul_rr_1(reg1,reg2) gen_vec_rr(mulsd,reg1,reg2)

#define vec_splat(mem,reg)      vec_splat_wrap(mem,reg)
#define vec_splat_wrap(mem,reg) \
        __asm__ __volatile__ ("movsd %0, " #reg "\n"\
			      "movlhpd " #reg ", " #reg \
			      : /* nothing */ \
                              : "m" ((mem)[0]))

/* Hack! */
/* To use this instruction be sure that register 7 is not in use!!! */
#define vec_sum(reg) vec_sum_wrap(reg)
#define vec_sum_wrap(reg) \
        __asm__ __volatile__ ("movhlps " #reg ", %%xmm7\n"\
    			      "addpd %%xmm7, " #reg "\n"\
			      : /* nothing */\
			      : /* nothing */)

#define vec_sum_full(reg1,reg2,empty1) vec_sum_full_wrap(reg1,reg2,empty1)
#define vec_sum_full_wrap(reg1,reg2,empty1) \
        __asm__ __volatile__ ("movhlps " #reg2 ", " #empty1 "\n"\
                              "movlhps " #reg2 ", " #empty1 "\n"\
    			      "addpd " #empty1 ", " #reg1 "\n"\
			      : /* nothing */\
			      : /* nothing */)


typedef double vector[VECLEN];

#endif








#ifdef THREEDNOW

/* Peculiarities of 3DNOW. Alignment is not an issue,
 * all alignments are legal, however alignment gives a speed increase.
 * The vec_acc instruction can be used to sum to registers at once more efficiently
 * than a series of vec_sum and vec_store_one
 * No muladd.
 */


#define gen_vec_mr(op,mem,reg) \
        __asm__ __volatile__ (#op " %0, " #reg \
                              :  /* nothing */ \
                              : "m" (((mem)[0])), "m" (((mem)[1])))

#define gen_vec_rm(op,reg,mem) \
        __asm__ __volatile__ (#op " " #reg ", %0" \
                              : "=m" (((mem)[0])), "=m" (((mem)[1])) \
			      :  /* nothing */ )




#define VECLEN 2

#define reg0 %%mm0
#define reg1 %%mm1
#define reg2 %%mm2
#define reg3 %%mm3
#define reg4 %%mm4
#define reg5 %%mm5
#define reg6 %%mm6
#define reg7 %%mm7

#define vec_add_mr(mem,reg)     gen_vec_mr(pfadd,mem,reg)
#define vec_mul_mr(mem,reg)     gen_vec_mr(pfmul,mem,reg)
#define vec_mov_mr(mem,reg)     gen_vec_mr(movq,mem,reg)
#define vec_mov_rm(reg,mem)     gen_vec_rm(movq,reg,mem)
#define vec_add_rr(reg1,reg2)   gen_vec_rr(pfadd,reg1,reg2)
#define vec_mul_rr(reg1,reg2)   gen_vec_rr(pfmul,reg1,reg2)
#define vec_acc_rr(reg1,reg2)   gen_vec_rr(pfacc,reg1,reg2)
#define vec_mov_rr(reg1,reg2)   gen_vec_rr(movq,reg1,reg2)

#define vec_sum(reg)            gen_vec_rr(pfacc,reg,reg)
#define vec_sum_full(reg1,reg2)  gen_vec_rr(pfacc,reg1,reg2)

#define vec_mov_mr_1(mem,reg)   gen_vec_mr(movd,mem,reg)
#define vec_mov_rm_1(reg,mem)   gen_vec_rm(movd,reg,mem)
#define vec_mov_rr_1(reg1,reg2) gen_vec_rr(movd,reg1,reg2)

#define vec_add_rr_1(reg1,reg2) gen_vec_rr(pfadd,reg1,reg2)
#define vec_mul_rr_1(reg1,reg2) gen_vec_rr(pfmul,reg1,reg2)


#define vec_splat(mem,reg)      vec_splat_wrap(mem,reg)
#define vec_splat_wrap(mem,reg) \
        __asm__ __volatile__ ("movd %0, " #reg "\n"\
			      "punpckldq " #reg ", " #reg \
			      : /* nothing */ \
                              : "m" ((mem)[0]))


#define vec_load_apart(mem1,mem2,reg) vec_load_apart_wrap(mem1,mem2,reg)
#define vec_load_apart_wrap(mem1,mem2,reg) \
        __asm__ __volatile__ ("movd %0, " #reg "\n"\
			      "punpckldq %1, " #reg \
			      : /* nothing */ \
                              : "m" ((mem1)[0]), "m" (((mem2)[0])))


#define vec_zero(reg)           gen_vec_rr(pxor,reg,reg)

#define vec_enter()             __asm__ __volatile__ ("femms")
#define vec_exit()              __asm__ __volatile__ ("femms")

#define align()                 __asm__ __volatile__ (".align 16")


typedef float vector[VECLEN];

#endif





#ifdef ALTIVEC

#define VECLEN 4

#define reg0 %%vr0
#define reg1 %%vr1
#define reg2 %%vr2
#define reg3 %%vr3
#define reg4 %%vr4
#define reg5 %%vr5
#define reg6 %%vr6
#define reg7 %%vr7
#define reg8 %%vr8
#define reg9 %%vr9
#define reg10 %%vr10
#define reg11 %%vr11
#define reg12 %%vr12
#define reg13 %%vr13
#define reg14 %%vr14
#define reg15 %%vr15
#define reg16 %%vr16
#define reg17 %%vr17
#define reg18 %%vr18
#define reg19 %%vr19
#define reg20 %%vr20
#define reg21 %%vr21
#define reg22 %%vr22
#define reg23 %%vr23
#define reg24 %%vr24
#define reg25 %%vr25
#define reg26 %%vr26
#define reg27 %%vr27
#define reg28 %%vr28
#define reg29 %%vr29
#define reg30 %%vr30
#define reg31 %%vr31

#define gen_vec_mr(op,mem,reg) \
        __asm__ __volatile__ (#op " %0, " #reg \
                              :  /* nothing */ \
                              : "m" (((mem)[0])), "m" (((mem)[1])), "m" (((mem)[2])), "m" (((mem)[3])))


#define gen_vec_rm(op,reg,mem) \
        __asm__ __volatile__ (#op " " #reg ", %0" \
                              : "=m" (((mem)[0])), "=m" (((mem)[1])), "=m" (((mem)[2])), "=m" (((mem)[3])) \
                              :  /* nothing */ )


#define gen_alti3(op,reg1,reg2,regout) \
        __asm__ __volatile__ (#op " " #reg1 ", " #reg2 ", " #regout \
                              :  /* nothing */ \
                              : /* nothing */)

#define gen_alti_muladd(op,reg1,reg2,regout) \
        __asm__ __volatile__ (#op " " #reg1 ", " #reg2 ", " #regout ", " #regout \
                              :  /* nothing */ \
                              : /* nothing */)



#define vec_mov_mr_a(mem,reg) gen_vec_mr(lvx,mem,reg)
#define vec_mov_rm_a(reg,mem) gen_vec_rm(svx,reg,mem)
#define vec_muladd(reg1,reg2,regout) gen_alti3(vmaddfp,reg1,reg2,regout)

#define vec_zero(reg) gen_alti3(vxor,reg,reg,reg)


typedef float vector[VECLEN];

#endif


#ifdef ALTIVEC_C

/* These macros have been written by, or greatly inspired by,
 *  Nicholas A. Coult . Thanks.
 */

/* assumes that last four registers are not in use! */
#define transpose(x0,x1,x2,x3) \
reg28 = vec_mergeh(x0,x2); \
reg29 = vec_mergeh(x1,x3); \
reg30 = vec_mergel(x0,x2); \
reg31 = vec_mergel(x1,x3); \
x0 = vec_mergeh(reg28,reg29); \
x1 = vec_mergel(reg28,reg29); \
x2 = vec_mergeh(reg30,reg31); \
x3 = vec_mergel(reg30,reg31)

#define vec_mov_rm(v, where) \
low = vec_ld(0, (where)); \
high = vec_ld(16, (where)); \
p_vector = vec_lvsr(0, (int *)(where)); \
mask  = vec_perm((vector unsigned char)(0), (vector unsigned char)(-1), p_vector); \
v = vec_perm(v, v, p_vector); \
low = vec_sel(low,  v, mask); \
high = vec_sel(v, high, mask); \
vec_st(low,  0, (where)); \
vec_st(high, 16, (where))

#define vec_mov_mr_a(mem,reg)  reg = vec_ld(0, mem)

#define vec_mov_mr(u,v) \
p_vector = (vector unsigned char)vec_lvsl(0, (int*)(v)); \
low = (vector unsigned char)vec_ld(0, (v)); \
high = (vector unsigned char)vec_ld(16, (v)); \
u=(vector float)vec_perm(low, high, p_vector)

#define vec_muladd(reg1,reg2,regout) regout = vec_madd(reg1,reg2,regout)
#define vec_add_rr(reg1,reg2)        reg2 = vec_add(reg1,reg2)

#define vec_zero(reg)                reg = vec_xor(reg,reg)

#define vec_sum_full(reg0,reg1,reg2,reg3,regout,empty0,empty1) \
transpose(reg0, reg1,reg2,reg3,regout,empty0,empty1); \
empty0 = vec_add(reg0,reg1); \
empty1 = vec_add(reg2,reg3); \
regout = vec_add(empty0,empty1)


#endif /* ALTIVEC_C */








