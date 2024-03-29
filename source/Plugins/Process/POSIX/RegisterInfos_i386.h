//===-- RegisterInfos_i386.h -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
#include "llvm/Support/Compiler.h"

#ifdef DECLARE_REGISTER_INFOS_I386_STRUCT

// Computes the offset of the given GPR in the user data area.
#define GPR_OFFSET(regname) \
    (LLVM_EXTENSION offsetof(GPR, regname))

// Computes the offset of the given FPR in the extended data area.
#define FPR_OFFSET(regname)  \
    (LLVM_EXTENSION offsetof(FPR, xstate) + \
     LLVM_EXTENSION offsetof(FXSAVE, regname))

// Computes the offset of the YMM register assembled from register halves.
#define YMM_OFFSET(regname) \
    (LLVM_EXTENSION offsetof(YMM, regname))

// Number of bytes needed to represent a FPR.
#if !defined(FPR_SIZE)
#define FPR_SIZE(reg) sizeof(((FXSAVE*)NULL)->reg)
#endif

// Number of bytes needed to represent the i'th FP register.
#define FP_SIZE sizeof(((MMSReg*)NULL)->bytes)

// Number of bytes needed to represent an XMM register.
#define XMM_SIZE sizeof(XMMReg)

// Number of bytes needed to represent a YMM register.
#define YMM_SIZE sizeof(YMMReg)

// Note that the size and offset will be updated by platform-specific classes.
#define DEFINE_GPR(reg, alt, kind1, kind2, kind3, kind4)            \
    { #reg, alt, sizeof(GPR::reg), GPR_OFFSET(reg), eEncodingUint,  \
      eFormatHex, { kind1, kind2, kind3, kind4, gpr_##reg##_i386 }, NULL, NULL }

#define DEFINE_FPR(name, reg, kind1, kind2, kind3, kind4)           \
    { #name, NULL, FPR_SIZE(reg), FPR_OFFSET(reg), eEncodingUint,   \
      eFormatHex, { kind1, kind2, kind3, kind4, fpu_##name##_i386 }, NULL, NULL }

// RegisterKind: GCC, DWARF, Generic, GDB, LLDB

#define DEFINE_FP_ST(reg, i)                                       \
    { #reg#i, NULL, FP_SIZE, LLVM_EXTENSION FPR_OFFSET(stmm[i]),    \
      eEncodingVector, eFormatVectorOfUInt8,                       \
      { gcc_st##i##_i386, dwarf_st##i##_i386, LLDB_INVALID_REGNUM, gdb_st##i##_i386, fpu_st##i##_i386 }, \
      NULL, NULL }

#define DEFINE_FP_MM(reg, i)                                                \
    { #reg#i, NULL, sizeof(uint64_t), LLVM_EXTENSION FPR_OFFSET(stmm[i]),   \
      eEncodingUint, eFormatHex,                                            \
      { gcc_mm##i##_i386, dwarf_mm##i##_i386, LLDB_INVALID_REGNUM, gdb_mm##i##_i386, fpu_mm##i##_i386 }, \
      NULL, NULL }

#define DEFINE_XMM(reg, i)                                         \
    { #reg#i, NULL, XMM_SIZE, LLVM_EXTENSION FPR_OFFSET(reg[i]),   \
      eEncodingVector, eFormatVectorOfUInt8,                       \
      { gcc_##reg##i##_i386, dwarf_##reg##i##_i386, LLDB_INVALID_REGNUM, gdb_##reg##i##_i386, fpu_##reg##i##_i386}, \
      NULL, NULL }

// I believe the YMM registers use dwarf_xmm_%_i386 register numbers and then differentiate based on register size.
#define DEFINE_YMM(reg, i)                                         \
    { #reg#i, NULL, YMM_SIZE, LLVM_EXTENSION YMM_OFFSET(reg[i]),   \
      eEncodingVector, eFormatVectorOfUInt8,                       \
      { LLDB_INVALID_REGNUM, dwarf_xmm##i##_i386, LLDB_INVALID_REGNUM, gdb_##reg##i##h_i386, fpu_##reg##i##_i386 }, \
      NULL, NULL }

#define DEFINE_DR(reg, i)                                               \
    { #reg#i, NULL, DR_SIZE, DR_OFFSET(i), eEncodingUint, eFormatHex,   \
      { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM,  \
        LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM }, NULL, NULL }

#define DEFINE_GPR_PSEUDO_16(reg16, reg32)                  \
    { #reg16, NULL, 2, GPR_OFFSET(reg32), eEncodingUint,    \
      eFormatHex, { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gpr_##reg16##_i386 }, RegisterContextPOSIX_x86::g_contained_##reg32, RegisterContextPOSIX_x86::g_invalidate_##reg32 }
#define DEFINE_GPR_PSEUDO_8H(reg8, reg32)                   \
    { #reg8, NULL, 1, GPR_OFFSET(reg32)+1, eEncodingUint,   \
      eFormatHex, { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gpr_##reg8##_i386 }, RegisterContextPOSIX_x86::g_contained_##reg32, RegisterContextPOSIX_x86::g_invalidate_##reg32 }
#define DEFINE_GPR_PSEUDO_8L(reg8, reg32)                   \
    { #reg8, NULL, 1, GPR_OFFSET(reg32), eEncodingUint,     \
      eFormatHex, { LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gpr_##reg8##_i386 }, RegisterContextPOSIX_x86::g_contained_##reg32, RegisterContextPOSIX_x86::g_invalidate_##reg32 }
 
static RegisterInfo
g_register_infos_i386[] =
{
    // General purpose registers.
    DEFINE_GPR(eax,    NULL,    gcc_eax_i386,        dwarf_eax_i386,      LLDB_INVALID_REGNUM,       gdb_eax_i386),
    DEFINE_GPR(ebx,    NULL,    gcc_ebx_i386,        dwarf_ebx_i386,      LLDB_INVALID_REGNUM,       gdb_ebx_i386),
    DEFINE_GPR(ecx,    NULL,    gcc_ecx_i386,        dwarf_ecx_i386,      LLDB_INVALID_REGNUM,       gdb_ecx_i386),
    DEFINE_GPR(edx,    NULL,    gcc_edx_i386,        dwarf_edx_i386,      LLDB_INVALID_REGNUM,       gdb_edx_i386),
    DEFINE_GPR(edi,    NULL,    gcc_edi_i386,        dwarf_edi_i386,      LLDB_INVALID_REGNUM,       gdb_edi_i386),
    DEFINE_GPR(esi,    NULL,    gcc_esi_i386,        dwarf_esi_i386,      LLDB_INVALID_REGNUM,       gdb_esi_i386),
    DEFINE_GPR(ebp,    "fp",    gcc_ebp_i386,        dwarf_ebp_i386,      LLDB_REGNUM_GENERIC_FP,    gdb_ebp_i386),
    DEFINE_GPR(esp,    "sp",    gcc_esp_i386,        dwarf_esp_i386,      LLDB_REGNUM_GENERIC_SP,    gdb_esp_i386),
    DEFINE_GPR(eip,    "pc",    gcc_eip_i386,        dwarf_eip_i386,      LLDB_REGNUM_GENERIC_PC,    gdb_eip_i386),
    DEFINE_GPR(eflags, "flags", gcc_eflags_i386,     dwarf_eflags_i386,   LLDB_REGNUM_GENERIC_FLAGS, gdb_eflags_i386),
    DEFINE_GPR(cs,     NULL,    LLDB_INVALID_REGNUM, dwarf_cs_i386,       LLDB_INVALID_REGNUM,       gdb_cs_i386),
    DEFINE_GPR(fs,     NULL,    LLDB_INVALID_REGNUM, dwarf_fs_i386,       LLDB_INVALID_REGNUM,       gdb_fs_i386),
    DEFINE_GPR(gs,     NULL,    LLDB_INVALID_REGNUM, dwarf_gs_i386,       LLDB_INVALID_REGNUM,       gdb_gs_i386),
    DEFINE_GPR(ss,     NULL,    LLDB_INVALID_REGNUM, dwarf_ss_i386,       LLDB_INVALID_REGNUM,       gdb_ss_i386),
    DEFINE_GPR(ds,     NULL,    LLDB_INVALID_REGNUM, dwarf_ds_i386,       LLDB_INVALID_REGNUM,       gdb_ds_i386),
    DEFINE_GPR(es,     NULL,    LLDB_INVALID_REGNUM, dwarf_es_i386,       LLDB_INVALID_REGNUM,       gdb_es_i386),

    DEFINE_GPR_PSEUDO_16(ax,  eax),
    DEFINE_GPR_PSEUDO_16(bx,  ebx),
    DEFINE_GPR_PSEUDO_16(cx,  ecx),
    DEFINE_GPR_PSEUDO_16(dx,  edx),
    DEFINE_GPR_PSEUDO_16(di,  edi),
    DEFINE_GPR_PSEUDO_16(si,  esi),
    DEFINE_GPR_PSEUDO_16(bp,  ebp),
    DEFINE_GPR_PSEUDO_16(sp,  esp),
    DEFINE_GPR_PSEUDO_8H(ah,  eax),
    DEFINE_GPR_PSEUDO_8H(bh,  ebx),
    DEFINE_GPR_PSEUDO_8H(ch,  ecx),
    DEFINE_GPR_PSEUDO_8H(dh,  edx),
    DEFINE_GPR_PSEUDO_8L(al,  eax),
    DEFINE_GPR_PSEUDO_8L(bl,  ebx),
    DEFINE_GPR_PSEUDO_8L(cl,  ecx),
    DEFINE_GPR_PSEUDO_8L(dl,  edx),

    // i387 Floating point registers.
    DEFINE_FPR(fctrl,     fctrl,          LLDB_INVALID_REGNUM, dwarf_fctrl_i386,    LLDB_INVALID_REGNUM, gdb_fctrl_i386),
    DEFINE_FPR(fstat,     fstat,          LLDB_INVALID_REGNUM, dwarf_fstat_i386,    LLDB_INVALID_REGNUM, gdb_fstat_i386),
    DEFINE_FPR(ftag,      ftag,           LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_ftag_i386),
    DEFINE_FPR(fop,       fop,            LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fop_i386),
    DEFINE_FPR(fiseg,     ptr.i386.fiseg, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fiseg_i386),
    DEFINE_FPR(fioff,     ptr.i386.fioff, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fioff_i386),
    DEFINE_FPR(foseg,     ptr.i386.foseg, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_foseg_i386),
    DEFINE_FPR(fooff,     ptr.i386.fooff, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, gdb_fooff_i386),
    DEFINE_FPR(mxcsr,     mxcsr,          LLDB_INVALID_REGNUM, dwarf_mxcsr_i386,    LLDB_INVALID_REGNUM, gdb_mxcsr_i386),
    DEFINE_FPR(mxcsrmask, mxcsrmask,      LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),

    // FP registers.
    DEFINE_FP_ST(st, 0),
    DEFINE_FP_ST(st, 1),
    DEFINE_FP_ST(st, 2),
    DEFINE_FP_ST(st, 3),
    DEFINE_FP_ST(st, 4),
    DEFINE_FP_ST(st, 5),
    DEFINE_FP_ST(st, 6),
    DEFINE_FP_ST(st, 7),
    DEFINE_FP_MM(mm, 0),
    DEFINE_FP_MM(mm, 1),
    DEFINE_FP_MM(mm, 2),
    DEFINE_FP_MM(mm, 3),
    DEFINE_FP_MM(mm, 4),
    DEFINE_FP_MM(mm, 5),
    DEFINE_FP_MM(mm, 6),
    DEFINE_FP_MM(mm, 7),

    // XMM registers
    DEFINE_XMM(xmm, 0),
    DEFINE_XMM(xmm, 1),
    DEFINE_XMM(xmm, 2),
    DEFINE_XMM(xmm, 3),
    DEFINE_XMM(xmm, 4),
    DEFINE_XMM(xmm, 5),
    DEFINE_XMM(xmm, 6),
    DEFINE_XMM(xmm, 7),

    // Copy of YMM registers assembled from xmm and ymmh
    DEFINE_YMM(ymm, 0),
    DEFINE_YMM(ymm, 1),
    DEFINE_YMM(ymm, 2),
    DEFINE_YMM(ymm, 3),
    DEFINE_YMM(ymm, 4),
    DEFINE_YMM(ymm, 5),
    DEFINE_YMM(ymm, 6),
    DEFINE_YMM(ymm, 7),

    // Debug registers for lldb internal use
    DEFINE_DR(dr, 0),
    DEFINE_DR(dr, 1),
    DEFINE_DR(dr, 2),
    DEFINE_DR(dr, 3),
    DEFINE_DR(dr, 4),
    DEFINE_DR(dr, 5),
    DEFINE_DR(dr, 6),
    DEFINE_DR(dr, 7)
};
static_assert((sizeof(g_register_infos_i386) / sizeof(g_register_infos_i386[0])) == k_num_registers_i386,
    "g_register_infos_x86_64 has wrong number of register infos");

#undef GPR_OFFSET
#undef FPR_OFFSET
#undef YMM_OFFSET
#undef FPR_SIZE
#undef FP_SIZE
#undef XMM_SIZE
#undef YMM_SIZE
#undef DEFINE_GPR
#undef DEFINE_FPR
#undef DEFINE_FP
#undef DEFINE_XMM
#undef DEFINE_YMM
#undef DEFINE_DR
#undef DEFINE_GPR_PSEUDO_16
#undef DEFINE_GPR_PSEUDO_8H
#undef DEFINE_GPR_PSEUDO_8L

#endif // DECLARE_REGISTER_INFOS_I386_STRUCT
