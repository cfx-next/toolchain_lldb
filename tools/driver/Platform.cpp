//===-- Platform.cpp --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// this file is only relevant for Visual C++
#if defined( _MSC_VER )

#include <process.h>
#include <assert.h>

#include "Platform.h"

// the control handler or SIGINT handler
static sighandler_t _ctrlHandler = NULL;

// the default console control handler
BOOL
WINAPI CtrlHandler (DWORD ctrlType)
{
    if ( _ctrlHandler != NULL )
    {
        _ctrlHandler( 0 );
        return TRUE;
    }
    return FALSE;
}

int
ioctl (int d, int request, ...)
{
    switch ( request )
    {
    // request the console windows size
    case ( TIOCGWINSZ ):
        {
            va_list vl;
            va_start(vl,request);
	     // locate the window size structure on stack
	     winsize *ws = va_arg(vl, winsize*);
            // get screen buffer information
            CONSOLE_SCREEN_BUFFER_INFO info;
            if ( GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &info ) == TRUE )
                // fill in the columns
                ws->ws_col = info.dwMaximumWindowSize.X;
            va_end(vl);
            return 0;
        }
        break;
    default:
        assert( !"Not implemented!" );
    }
    return -1;
}

int
kill (pid_t pid, int sig)
{
    // is the app trying to kill itself
    if ( pid == getpid( ) )
        exit( sig );
    //
    assert( !"Not implemented!" );
    return -1;
}

int
tcsetattr (int fd, int optional_actions, const struct termios *termios_p)
{
    assert( !"Not implemented!" );
    return -1;
}

int
tcgetattr (int fildes, struct termios *termios_p)
{
//  assert( !"Not implemented!" );
    // error return value (0=success)
    return -1;
}

sighandler_t
signal (int sig, sighandler_t sigFunc)
{
    switch ( sig )
    {
    case ( SIGINT ):
        {
            _ctrlHandler = sigFunc;
            SetConsoleCtrlHandler( CtrlHandler, TRUE );
        }
        break;
    case ( SIGPIPE  ):
    case ( SIGWINCH ):
    case ( SIGTSTP  ):
    case ( SIGCONT  ):
        // ignore these for now
        break;
    default:
        assert( !"Not implemented!" );
    }
    return 0;
}

#endif
