//===-- POSIXThread.cpp -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

// C Includes
#include <errno.h>

// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/Watchpoint.h"
#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/State.h"
#include "lldb/Host/Host.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/ThreadSpec.h"
#include "POSIXStopInfo.h"
#include "POSIXThread.h"
#include "ProcessPOSIX.h"
#include "ProcessPOSIXLog.h"
#include "ProcessMonitor.h"
#include "RegisterContextPOSIXProcessMonitor_mips64.h"
#include "RegisterContextPOSIXProcessMonitor_x86.h"
#include "RegisterContextLinux_i386.h"
#include "RegisterContextLinux_x86_64.h"
#include "RegisterContextFreeBSD_i386.h"
#include "RegisterContextFreeBSD_mips64.h"
#include "RegisterContextFreeBSD_x86_64.h"

#include "UnwindLLDB.h"

using namespace lldb;
using namespace lldb_private;


POSIXThread::POSIXThread(Process &process, lldb::tid_t tid)
    : Thread(process, tid),
      m_frame_ap (),
      m_breakpoint (),
      m_thread_name_valid (false),
      m_thread_name (),
      m_posix_thread(NULL)
{
    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log && log->GetMask().Test(POSIX_LOG_VERBOSE))
        log->Printf ("POSIXThread::%s (tid = %" PRIi64 ")", __FUNCTION__, tid);

    // Set the current watchpoints for this thread.
    Target &target = GetProcess()->GetTarget();
    const WatchpointList &wp_list = target.GetWatchpointList();
    size_t wp_size = wp_list.GetSize();

    for (uint32_t wp_idx = 0; wp_idx < wp_size; wp_idx++)
    {
        lldb::WatchpointSP wp = wp_list.GetByIndex(wp_idx);
        if (wp.get() && wp->IsEnabled())
        {
            // This watchpoint as been enabled; obviously this "new" thread
            // has been created since that watchpoint was enabled.  Since
            // the POSIXBreakpointProtocol has yet to be initialized, its
            // m_watchpoints_initialized member will be FALSE.  Attempting to
            // read the debug status register to determine if a watchpoint
            // has been hit would result in the zeroing of that register.
            // Since the active debug registers would have been cloned when
            // this thread was created, simply force the m_watchpoints_initized
            // member to TRUE and avoid resetting dr6 and dr7.
            GetPOSIXBreakpointProtocol()->ForceWatchpointsInitialized();
        }
    }
}

POSIXThread::~POSIXThread()
{
    DestroyThread();
}

ProcessMonitor &
POSIXThread::GetMonitor()
{
    ProcessSP base = GetProcess();
    ProcessPOSIX &process = static_cast<ProcessPOSIX&>(*base);
    return process.GetMonitor();
}

// Overridden by FreeBSDThread; this is used only on Linux.
void
POSIXThread::RefreshStateAfterStop()
{
    // Invalidate all registers in our register context. We don't set "force" to
    // true because the stop reply packet might have had some register values
    // that were expedited and these will already be copied into the register
    // context by the time this function gets called. The KDPRegisterContext
    // class has been made smart enough to detect when it needs to invalidate
    // which registers are valid by putting hooks in the register read and 
    // register supply functions where they check the process stop ID and do
    // the right thing.
    //if (StateIsStoppedState(GetState())
    {
        const bool force = false;
        GetRegisterContext()->InvalidateIfNeeded (force);
    }
    // FIXME: This should probably happen somewhere else.
    SetResumeState(eStateRunning);
    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log)
        log->Printf ("POSIXThread::%s (tid = %" PRIi64 ") setting thread resume state to running", __FUNCTION__, GetID());
}

const char *
POSIXThread::GetInfo()
{
    return NULL;
}

void
POSIXThread::SetName (const char *name)
{
    m_thread_name_valid = (name && name[0]);
    if (m_thread_name_valid)
        m_thread_name.assign (name);
    else
        m_thread_name.clear();
}

const char *
POSIXThread::GetName ()
{
    if (!m_thread_name_valid)
    {
        SetName(Host::GetThreadName(GetProcess()->GetID(), GetID()).c_str());
        m_thread_name_valid = true;
    }

    if (m_thread_name.empty())
        return NULL;
    return m_thread_name.c_str();
}

lldb::RegisterContextSP
POSIXThread::GetRegisterContext()
{
    if (!m_reg_context_sp)
    {
        m_posix_thread = NULL;

        RegisterInfoInterface *reg_interface = NULL;
        const ArchSpec &target_arch = GetProcess()->GetTarget().GetArchitecture();

        switch (target_arch.GetTriple().getOS())
        {
            case llvm::Triple::FreeBSD:
                switch (target_arch.GetCore())
                {
                    case ArchSpec::eCore_mips64:
                        reg_interface = new RegisterContextFreeBSD_mips64(target_arch);
                        break;
                    case ArchSpec::eCore_x86_32_i386:
                    case ArchSpec::eCore_x86_32_i486:
                    case ArchSpec::eCore_x86_32_i486sx:
                        reg_interface = new RegisterContextFreeBSD_i386(target_arch);
                        break;
                    case ArchSpec::eCore_x86_64_x86_64:
                        reg_interface = new RegisterContextFreeBSD_x86_64(target_arch);
                        break;
                    default:
                        break;
                }
                break;

            case llvm::Triple::Linux:
                switch (target_arch.GetCore())
                {
                    case ArchSpec::eCore_x86_32_i386:
                    case ArchSpec::eCore_x86_32_i486:
                    case ArchSpec::eCore_x86_32_i486sx:
                    case ArchSpec::eCore_x86_64_x86_64:
                        if (Host::GetArchitecture().GetAddressByteSize() == 4)
                        {
                            // 32-bit hosts run with a RegisterContextLinux_i386 context.
                            reg_interface = static_cast<RegisterInfoInterface*>(new RegisterContextLinux_i386(target_arch));
                        }
                        else
                        {
                            assert((Host::GetArchitecture().GetAddressByteSize() == 8) && "Register setting path assumes this is a 64-bit host");
                            // X86_64 hosts know how to work with 64-bit and 32-bit EXEs using the x86_64 register context.
                            reg_interface = static_cast<RegisterInfoInterface*>(new RegisterContextLinux_x86_64(target_arch));
                        }
                        break;
                    default:
                        break;
                }

            default:
                break;
        }

        assert(reg_interface && "OS or CPU not supported!");

        switch (target_arch.GetCore())
        {
            case ArchSpec::eCore_mips64:
                {
                    RegisterContextPOSIXProcessMonitor_mips64 *reg_ctx = new RegisterContextPOSIXProcessMonitor_mips64(*this, 0, reg_interface);
                    m_posix_thread = reg_ctx;
                    m_reg_context_sp.reset(reg_ctx);
                    break;
                }
            case ArchSpec::eCore_x86_32_i386:
            case ArchSpec::eCore_x86_32_i486:
            case ArchSpec::eCore_x86_32_i486sx:
            case ArchSpec::eCore_x86_64_x86_64:
                {
                    RegisterContextPOSIXProcessMonitor_x86_64 *reg_ctx = new RegisterContextPOSIXProcessMonitor_x86_64(*this, 0, reg_interface);
                    m_posix_thread = reg_ctx;
                    m_reg_context_sp.reset(reg_ctx);
                    break;
                }
            default:
                break;
        }
    }
    return m_reg_context_sp;
}

lldb::RegisterContextSP
POSIXThread::CreateRegisterContextForFrame(lldb_private::StackFrame *frame)
{
    lldb::RegisterContextSP reg_ctx_sp;
    uint32_t concrete_frame_idx = 0;

    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log && log->GetMask().Test(POSIX_LOG_VERBOSE))
        log->Printf ("POSIXThread::%s ()", __FUNCTION__);

    if (frame)
        concrete_frame_idx = frame->GetConcreteFrameIndex();

    if (concrete_frame_idx == 0)
        reg_ctx_sp = GetRegisterContext();
    else
    {
        assert(GetUnwinder());
        reg_ctx_sp = GetUnwinder()->CreateRegisterContextForFrame(frame);
    }

    return reg_ctx_sp;
}

lldb::addr_t
POSIXThread::GetThreadPointer ()
{
    ProcessMonitor &monitor = GetMonitor();
    addr_t addr;
    if (monitor.ReadThreadPointer (GetID(), addr))
        return addr;
    else
        return LLDB_INVALID_ADDRESS;
}

bool
POSIXThread::CalculateStopInfo()
{
    SetStopInfo (m_stop_info_sp);
    return true;
}

Unwind *
POSIXThread::GetUnwinder()
{
    if (m_unwinder_ap.get() == NULL)
        m_unwinder_ap.reset(new UnwindLLDB(*this));

    return m_unwinder_ap.get();
}

// Overridden by FreeBSDThread; this is used only on Linux.
void
POSIXThread::WillResume(lldb::StateType resume_state)
{
    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log)
        log->Printf ("POSIXThread::%s (tid = %" PRIi64 ") setting thread resume state to %s", __FUNCTION__, GetID(), StateAsCString(resume_state));
    // TODO: the line below shouldn't really be done, but
    // the POSIXThread might rely on this so I will leave this in for now
    SetResumeState(resume_state);
}

void
POSIXThread::DidStop()
{
    // Don't set the thread state to stopped unless we really stopped.
}

bool
POSIXThread::Resume()
{
    lldb::StateType resume_state = GetResumeState();
    ProcessMonitor &monitor = GetMonitor();
    bool status;

    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log)
        log->Printf ("POSIXThread::%s (), resume_state = %s", __FUNCTION__,
                         StateAsCString(resume_state));

    switch (resume_state)
    {
    default:
        assert(false && "Unexpected state for resume!");
        status = false;
        break;

    case lldb::eStateRunning:
        SetState(resume_state);
        status = monitor.Resume(GetID(), GetResumeSignal());
        break;

    case lldb::eStateStepping:
        SetState(resume_state);
        status = monitor.SingleStep(GetID(), GetResumeSignal());
        break;
    case lldb::eStateStopped:
    case lldb::eStateSuspended:
        status = true;
        break;
    }

    return status;
}

void
POSIXThread::Notify(const ProcessMessage &message)
{
    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log)
        log->Printf ("POSIXThread::%s () message kind = '%s' for tid %" PRIu64,
                     __FUNCTION__, message.PrintKind(), GetID());

    switch (message.GetKind())
    {
    default:
        assert(false && "Unexpected message kind!");
        break;

    case ProcessMessage::eExitMessage:
        // Nothing to be done.
        break;

    case ProcessMessage::eLimboMessage:
        LimboNotify(message);
        break;

    case ProcessMessage::eSignalMessage:
        SignalNotify(message);
        break;

    case ProcessMessage::eSignalDeliveredMessage:
        SignalDeliveredNotify(message);
        break;

    case ProcessMessage::eTraceMessage:
        TraceNotify(message);
        break;

    case ProcessMessage::eBreakpointMessage:
        BreakNotify(message);
        break;

    case ProcessMessage::eWatchpointMessage:
        WatchNotify(message);
        break;

    case ProcessMessage::eCrashMessage:
        CrashNotify(message);
        break;

    case ProcessMessage::eNewThreadMessage:
        ThreadNotify(message);
        break;

    case ProcessMessage::eExecMessage:
        ExecNotify(message);
        break;
    }
}

bool
POSIXThread::EnableHardwareWatchpoint(Watchpoint *wp)
{
    bool wp_set = false;
    if (wp)
    {
        addr_t wp_addr = wp->GetLoadAddress();
        size_t wp_size = wp->GetByteSize();
        bool wp_read = wp->WatchpointRead();
        bool wp_write = wp->WatchpointWrite();
        uint32_t wp_hw_index = wp->GetHardwareIndex();
        POSIXBreakpointProtocol* reg_ctx = GetPOSIXBreakpointProtocol();
        if (reg_ctx)
            wp_set = reg_ctx->SetHardwareWatchpointWithIndex(wp_addr, wp_size,
                                                             wp_read, wp_write,
                                                             wp_hw_index);
    }
    return wp_set;
}

bool
POSIXThread::DisableHardwareWatchpoint(Watchpoint *wp)
{
    bool result = false;
    if (wp)
    {
        lldb::RegisterContextSP reg_ctx_sp = GetRegisterContext();
        if (reg_ctx_sp.get())
            result = reg_ctx_sp->ClearHardwareWatchpoint(wp->GetHardwareIndex());
    }
    return result;
}

uint32_t
POSIXThread::NumSupportedHardwareWatchpoints()
{
    lldb::RegisterContextSP reg_ctx_sp = GetRegisterContext();
    if (reg_ctx_sp.get())
        return reg_ctx_sp->NumSupportedHardwareWatchpoints();
    return 0;
}

uint32_t
POSIXThread::FindVacantWatchpointIndex()
{
    uint32_t hw_index = LLDB_INVALID_INDEX32;
    uint32_t num_hw_wps = NumSupportedHardwareWatchpoints();
    uint32_t wp_idx;
    POSIXBreakpointProtocol* reg_ctx = GetPOSIXBreakpointProtocol();
    if (reg_ctx)
    {
        for (wp_idx = 0; wp_idx < num_hw_wps; wp_idx++)
        {
            if (reg_ctx->IsWatchpointVacant(wp_idx))
            {
                hw_index = wp_idx;
                break;
            }
        }
    }
    return hw_index;
}

void
POSIXThread::BreakNotify(const ProcessMessage &message)
{
    bool status;
    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));

    assert(GetRegisterContext());
    status = GetPOSIXBreakpointProtocol()->UpdateAfterBreakpoint();
    assert(status && "Breakpoint update failed!");

    // With our register state restored, resolve the breakpoint object
    // corresponding to our current PC.
    assert(GetRegisterContext());
    lldb::addr_t pc = GetRegisterContext()->GetPC();
    if (log)
        log->Printf ("POSIXThread::%s () PC=0x%8.8" PRIx64, __FUNCTION__, pc);
    lldb::BreakpointSiteSP bp_site(GetProcess()->GetBreakpointSiteList().FindByAddress(pc));

    // If the breakpoint is for this thread, then we'll report the hit, but if it is for another thread,
    // we create a stop reason with should_stop=false.  If there is no breakpoint location, then report
    // an invalid stop reason. We don't need to worry about stepping over the breakpoint here, that will
    // be taken care of when the thread resumes and notices that there's a breakpoint under the pc.
    if (bp_site)
    {
        lldb::break_id_t bp_id = bp_site->GetID();
        if (bp_site->ValidForThisThread(this))
            SetStopInfo (StopInfo::CreateStopReasonWithBreakpointSiteID(*this, bp_id));
        else
        {
            const bool should_stop = false;
            SetStopInfo (StopInfo::CreateStopReasonWithBreakpointSiteID(*this, bp_id, should_stop));
        }
    }
    else
        SetStopInfo(StopInfoSP());
}

void
POSIXThread::WatchNotify(const ProcessMessage &message)
{
    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));

    lldb::addr_t halt_addr = message.GetHWAddress();
    if (log)
        log->Printf ("POSIXThread::%s () Hardware Watchpoint Address = 0x%8.8"
                     PRIx64, __FUNCTION__, halt_addr);

    POSIXBreakpointProtocol* reg_ctx = GetPOSIXBreakpointProtocol();
    if (reg_ctx)
    {
        uint32_t num_hw_wps = reg_ctx->NumSupportedHardwareWatchpoints();
        uint32_t wp_idx;
        for (wp_idx = 0; wp_idx < num_hw_wps; wp_idx++)
        {
            if (reg_ctx->IsWatchpointHit(wp_idx))
            {
                // Clear the watchpoint hit here
                reg_ctx->ClearWatchpointHits();
                break;
            }
        }

        if (wp_idx == num_hw_wps)
            return;

        Target &target = GetProcess()->GetTarget();
        lldb::addr_t wp_monitor_addr = reg_ctx->GetWatchpointAddress(wp_idx);
        const WatchpointList &wp_list = target.GetWatchpointList();
        lldb::WatchpointSP wp_sp = wp_list.FindByAddress(wp_monitor_addr);

        assert(wp_sp.get() && "No watchpoint found");
        SetStopInfo (StopInfo::CreateStopReasonWithWatchpointID(*this,
                                                                wp_sp->GetID()));
    }
}

void
POSIXThread::TraceNotify(const ProcessMessage &message)
{
    POSIXBreakpointProtocol* reg_ctx = GetPOSIXBreakpointProtocol();
    if (reg_ctx)
    {
        uint32_t num_hw_wps = reg_ctx->NumSupportedHardwareWatchpoints();
        uint32_t wp_idx;
        for (wp_idx = 0; wp_idx < num_hw_wps; wp_idx++)
        {
            if (reg_ctx->IsWatchpointHit(wp_idx))
            {
                WatchNotify(message);
                return;
            }
        }
    }

    SetStopInfo (StopInfo::CreateStopReasonToTrace(*this));
}

void
POSIXThread::LimboNotify(const ProcessMessage &message)
{
    SetStopInfo (lldb::StopInfoSP(new POSIXLimboStopInfo(*this)));
}

void
POSIXThread::SignalNotify(const ProcessMessage &message)
{
    int signo = message.GetSignal();

    SetStopInfo (StopInfo::CreateStopReasonWithSignal(*this, signo));
    SetResumeSignal(signo);
}

void
POSIXThread::SignalDeliveredNotify(const ProcessMessage &message)
{
    int signo = message.GetSignal();

    SetStopInfo (StopInfo::CreateStopReasonWithSignal(*this, signo));
    SetResumeSignal(signo);
}

void
POSIXThread::CrashNotify(const ProcessMessage &message)
{
    // FIXME: Update stop reason as per bugzilla 14598
    int signo = message.GetSignal();

    assert(message.GetKind() == ProcessMessage::eCrashMessage);

    Log *log (ProcessPOSIXLog::GetLogIfAllCategoriesSet (POSIX_LOG_THREAD));
    if (log)
        log->Printf ("POSIXThread::%s () signo = %i, reason = '%s'",
                     __FUNCTION__, signo, message.PrintCrashReason());

    SetStopInfo (lldb::StopInfoSP(new POSIXCrashStopInfo(*this, signo,
                                                         message.GetCrashReason(),
                                                         message.GetFaultAddress())));
    SetResumeSignal(signo);
}

void
POSIXThread::ThreadNotify(const ProcessMessage &message)
{
    SetStopInfo (lldb::StopInfoSP(new POSIXNewThreadStopInfo(*this)));
}

unsigned
POSIXThread::GetRegisterIndexFromOffset(unsigned offset)
{
    unsigned reg = LLDB_INVALID_REGNUM;
    ArchSpec arch = Host::GetArchitecture();

    switch (arch.GetCore())
    {
    default:
        llvm_unreachable("CPU type not supported!");
        break;

    case ArchSpec::eCore_mips64:
    case ArchSpec::eCore_x86_32_i386:
    case ArchSpec::eCore_x86_32_i486:
    case ArchSpec::eCore_x86_32_i486sx:
    case ArchSpec::eCore_x86_64_x86_64:
        {
            POSIXBreakpointProtocol* reg_ctx = GetPOSIXBreakpointProtocol();
            reg = reg_ctx->GetRegisterIndexFromOffset(offset);
        }
        break;
    }
    return reg;
}

void
POSIXThread::ExecNotify(const ProcessMessage &message)
{
    SetStopInfo (StopInfo::CreateStopReasonWithExec(*this));
}

const char *
POSIXThread::GetRegisterName(unsigned reg)
{
    const char * name = nullptr;
    ArchSpec arch = Host::GetArchitecture();

    switch (arch.GetCore())
    {
    default:
        assert(false && "CPU type not supported!");
        break;

    case ArchSpec::eCore_mips64:
    case ArchSpec::eCore_x86_32_i386:
    case ArchSpec::eCore_x86_32_i486:
    case ArchSpec::eCore_x86_32_i486sx:
    case ArchSpec::eCore_x86_64_x86_64:
        name = GetRegisterContext()->GetRegisterName(reg);
        break;
    }
    return name;
}

const char *
POSIXThread::GetRegisterNameFromOffset(unsigned offset)
{
    return GetRegisterName(GetRegisterIndexFromOffset(offset));
}

