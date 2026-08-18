/*
	wrench - A set of modding tools for the Ratchet & Clank PS2 games.
	Copyright (C) 2019-2026 chaoticgd

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <core/util/crash_handler.h>

#include <core/util/error_util.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>

#if defined(__linux__) || defined(__APPLE__)
	#define WRENCH_HAVE_BACKTRACE 1
	#include <execinfo.h>
	#include <cxxabi.h>
#endif

// NOTE: Calling fprintf/malloc/backtrace_symbols() from a signal handler
// isn't strictly async-signal-safe, but in practice this is a best-effort
// diagnostic aid for a crashed development tool (not something running in
// a hardened production service), and every mainstream libc has handled
// this pattern fine in testing. If it ever itself crashes while handling a
// crash, the default handler still runs afterwards.
static void print_context_and_backtrace(int sig)
{
	fflush(stdout);
	fflush(stderr);
	
	const char* signame = "unknown signal";
	switch (sig) {
		case SIGSEGV: signame = "SIGSEGV (segmentation fault)"; break;
		case SIGABRT: signame = "SIGABRT (abort -- e.g. a failed libstdc++/_GLIBCXX_ASSERTIONS bounds check, a failed verify_fatal(), or an uncaught C++ exception)"; break;
		case SIGFPE:  signame = "SIGFPE (floating point exception, e.g. integer divide by zero)"; break;
		case SIGILL:  signame = "SIGILL (illegal instruction)"; break;
	}
	
	fprintf(stderr, "\n\033[31m*** wrench crashed: %s ***\033[0m\n", signame);
	fprintf(stderr, "Context:%s\n",
		(UTIL_ERROR_CONTEXT_STRING && UTIL_ERROR_CONTEXT_STRING[0]) ?
			UTIL_ERROR_CONTEXT_STRING : " (no ERROR_CONTEXT was set, so this crash happened outside of any tracked asset operation)");
	
#ifdef WRENCH_HAVE_BACKTRACE
	fprintf(stderr, "Backtrace:\n");
	void* frames[64];
	int frame_count = backtrace(frames, 64);
	char** symbols = backtrace_symbols(frames, frame_count);
	if (symbols) {
		for (int i = 0; i < frame_count; i++) {
			// backtrace_symbols() lines typically look like:
			//   ./wrenchbuild(_ZN...+0x123) [0xaddress]
			// Try to pull out the mangled C++ symbol name and demangle it
			// so the backtrace is actually readable.
			char* line = symbols[i];
			char* name_begin = strchr(line, '(');
			char* name_end = name_begin ? strchr(name_begin, '+') : nullptr;
			bool printed = false;
			if (name_begin && name_end && name_end > name_begin + 1) {
				std::string mangled(name_begin + 1, name_end - (name_begin + 1));
				int status = -1;
				char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
				if (status == 0 && demangled) {
					size_t prefix_len = (size_t) (name_begin - line) + 1; // includes the '('
					fprintf(stderr, "  #%-2d %.*s%s%s\n", i, (int) prefix_len, line, demangled, name_end);
					free(demangled);
					printed = true;
				}
			}
			if (!printed) {
				fprintf(stderr, "  #%-2d %s\n", i, line);
			}
		}
		free(symbols);
	} else {
		fprintf(stderr, "  (backtrace_symbols() failed, likely out of memory)\n");
	}
	fprintf(stderr, "(Tip: run through gdb/lldb with debug symbols for file:line info, e.g. `gdb --args ./wrenchbuild ...` then `run` then `bt`.)\n");
#else
	fprintf(stderr, "Backtrace: not available on this platform.\n");
#endif
	
	fflush(stderr);
}

static void crash_signal_handler(int sig)
{
	print_context_and_backtrace(sig);
	
	// Restore the default handler and re-raise so the OS still does its
	// normal thing afterwards (core dump, correct process exit status,
	// a debugger catching it if one is attached, etc).
	signal(sig, SIG_DFL);
	raise(sig);
}

static int install_crash_handler_now()
{
	signal(SIGSEGV, crash_signal_handler);
	signal(SIGABRT, crash_signal_handler);
	signal(SIGFPE, crash_signal_handler);
	signal(SIGILL, crash_signal_handler);
	return 0;
}

void install_crash_handler()
{
	install_crash_handler_now();
}

// Installed automatically for every executable that links against core, so
// no individual main() needs to remember to call install_crash_handler().
static int _crash_handler_auto_install = install_crash_handler_now();
