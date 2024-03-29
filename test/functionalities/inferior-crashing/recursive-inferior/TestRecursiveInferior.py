"""Test that lldb functions correctly after the inferior has crashed while in a recursive routine."""

import os, time
import unittest2
import lldb, lldbutil
from lldbtest import *

class CrashingRecursiveInferiorTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_recursive_inferior_crashing_dsym(self):
        """Test that lldb reliably catches the inferior crashing (command)."""
        self.buildDsym()
        self.recursive_inferior_crashing()

    def test_recursive_inferior_crashing_dwarf(self):
        """Test that lldb reliably catches the inferior crashing (command)."""
        self.buildDwarf()
        self.recursive_inferior_crashing()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_recursive_inferior_crashing_registers_dsym(self):
        """Test that lldb reliably reads registers from the inferior after crashing (command)."""
        self.buildDsym()
        self.recursive_inferior_crashing_registers()

    def test_recursive_inferior_crashing_register_dwarf(self):
        """Test that lldb reliably reads registers from the inferior after crashing (command)."""
        self.buildDwarf()
        self.recursive_inferior_crashing_registers()

    @python_api_test
    def test_recursive_inferior_crashing_python(self):
        """Test that lldb reliably catches the inferior crashing (Python API)."""
        self.buildDefault()
        self.recursive_inferior_crashing_python()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_recursive_inferior_crashing_expr_dsym(self):
        """Test that the lldb expression interpreter can read from the inferior after crashing (command)."""
        self.buildDsym()
        self.recursive_inferior_crashing_expr()

    def test_recursive_inferior_crashing_expr_dwarf(self):
        """Test that the lldb expression interpreter can read from the inferior after crashing (command)."""
        self.buildDwarf()
        self.recursive_inferior_crashing_expr()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_recursive_inferior_crashing_step_dsym(self):
        """Test that lldb functions correctly after stepping through a crash."""
        self.buildDsym()
        self.recursive_inferior_crashing_step()

    def test_recursive_inferior_crashing_step_dwarf(self):
        """Test that stepping after a crash behaves correctly."""
        self.buildDwarf()
        self.recursive_inferior_crashing_step()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_recursive_inferior_crashing_step_after_break_dsym(self):
        """Test that stepping after a crash behaves correctly."""
        self.buildDsym()
        self.recursive_inferior_crashing_step_after_break()

    @skipIfFreeBSD # llvm.org/pr16684
    def test_recursive_inferior_crashing_step_after_break_dwarf(self):
        """Test that lldb functions correctly after stepping through a crash."""
        self.buildDwarf()
        self.recursive_inferior_crashing_step_after_break()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_recursive_inferior_crashing_expr_step_and_expr_dsym(self):
        """Test that lldb expressions work before and after stepping after a crash."""
        self.buildDsym()
        self.recursive_inferior_crashing_expr_step_expr()

    @expectedFailureFreeBSD('llvm.org/pr15989') # Couldn't allocate space for the stack frame
    @expectedFailureLinux # llvm.org/pr15989 - Couldn't allocate space for the stack frame
    def test_recursive_inferior_crashing_expr_step_and_expr_dwarf(self):
        """Test that lldb expressions work before and after stepping after a crash."""
        self.buildDwarf()
        self.recursive_inferior_crashing_expr_step_expr()

    def set_breakpoint(self, line):
        lldbutil.run_break_set_by_file_and_line (self, "main.c", line, num_expected_locations=1, loc_exact=True)

    def check_stop_reason(self):
        if sys.platform.startswith("darwin"):
            stop_reason = 'stop reason = EXC_BAD_ACCESS'
        else:
            stop_reason = 'stop reason = invalid address'

        # The stop reason of the thread should be a bad access exception.
        self.expect("thread list", STOPPED_DUE_TO_EXC_BAD_ACCESS,
            substrs = ['stopped',
                       stop_reason])

        return stop_reason

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line number of the crash.
        self.line = line_number('main.c', '// Crash here.')

    def recursive_inferior_crashing(self):
        """Inferior crashes upon launching; lldb should catch the event and stop."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        self.runCmd("run", RUN_SUCCEEDED)
        stop_reason = self.check_stop_reason()

        # And it should report a backtrace that includes main and the crash site.
        self.expect("thread backtrace all",
            substrs = [stop_reason, 'main', 'argc', 'argv', 'recursive_function'])

        # And it should report the correct line number.
        self.expect("thread backtrace all",
            substrs = [stop_reason,
                       'main.c:%d' % self.line])

    def recursive_inferior_crashing_python(self):
        """Inferior crashes upon launching; lldb should catch the event and stop."""
        exe = os.path.join(os.getcwd(), "a.out")

        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target, VALID_TARGET)

        # Now launch the process, and do not stop at entry point.
        # Both argv and envp are null.
        process = target.LaunchSimple (None, None, self.get_process_working_directory())

        if process.GetState() != lldb.eStateStopped:
            self.fail("Process should be in the 'stopped' state, "
                      "instead the actual state is: '%s'" %
                      lldbutil.state_type_to_str(process.GetState()))

        thread = lldbutil.get_stopped_thread(process, lldb.eStopReasonException)
        if not thread:
            self.fail("Fail to stop the thread upon bad access exception")

        if self.TraceOn():
            lldbutil.print_stacktrace(thread)

    def recursive_inferior_crashing_registers(self):
        """Test that lldb can read registers after crashing."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        self.runCmd("run", RUN_SUCCEEDED)
        self.check_stop_reason()

        # lldb should be able to read from registers from the inferior after crashing.
        self.expect("register read eax",
            substrs = ['eax = 0x'])

    def recursive_inferior_crashing_expr(self):
        """Test that the lldb expression interpreter can read symbols after crashing."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        self.runCmd("run", RUN_SUCCEEDED)
        self.check_stop_reason()

        # The lldb expression interpreter should be able to read from addresses of the inferior after a crash.
        self.expect("p i",
            startstr = '(int) $0 =')

    def recursive_inferior_crashing_step(self):
        """Test that lldb functions correctly after stepping through a crash."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        self.set_breakpoint(self.line)
        self.runCmd("run", RUN_SUCCEEDED)

        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['main.c:%d' % self.line,
                       'stop reason = breakpoint'])

        self.runCmd("next")
        stop_reason = self.check_stop_reason()

        # The lldb expression interpreter should be able to read from addresses of the inferior after a crash.
        self.expect("p i",
            substrs = ['(int) $0 ='])

        # lldb should be able to read from registers from the inferior after crashing.
        self.expect("register read eax",
            substrs = ['eax = 0x'])

        # And it should report the correct line number.
        self.expect("thread backtrace all",
            substrs = [stop_reason,
                       'main.c:%d' % self.line])

    def recursive_inferior_crashing_step_after_break(self):
        """Test that lldb behaves correctly when stepping after a crash."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        self.runCmd("run", RUN_SUCCEEDED)
        self.check_stop_reason()

        expected_state = 'exited' # Provide the exit code.
        if sys.platform.startswith("darwin"):
            expected_state = 'stopped' # TODO: Determine why 'next' and 'continue' have no effect after a crash.

        self.expect("next",
            substrs = ['Process', expected_state])

        if not(sys.platform.startswith("darwin")): # if stopped, we will have a process around
            self.expect("thread list", error=True,substrs = ['Process must be launched'])

    def recursive_inferior_crashing_expr_step_expr(self):
        """Test that lldb expressions work before and after stepping after a crash."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        self.runCmd("run", RUN_SUCCEEDED)
        self.check_stop_reason()

        # The lldb expression interpreter should be able to read from addresses of the inferior after a crash.
        self.expect("p null",
            startstr = '(char *) $0 = 0x0')

        self.runCmd("next")

        # The lldb expression interpreter should be able to read from addresses of the inferior after a step.
        self.expect("p null",
            startstr = '(char *) $1 = 0x0')

        self.check_stop_reason()

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
