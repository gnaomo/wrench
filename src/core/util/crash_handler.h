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

#ifndef CORE_CRASH_HANDLER_H
#define CORE_CRASH_HANDLER_H

// Installs signal handlers (SIGSEGV/SIGABRT/SIGFPE/SIGILL) that print:
//  - the current wrench ERROR_CONTEXT stack (e.g. which asset was being
//    unpacked/packed), so crashes originating from *inside* the standard
//    library (e.g. a libstdc++ hardening/_GLIBCXX_ASSERTIONS bounds-check
//    failure) still tell you what wrench was doing, not just where inside
//    stl_vector.h it happened.
//  - a symbolised backtrace, where the platform supports it (Linux/macOS
//    via backtrace()/backtrace_symbols(); a no-op stub on other platforms).
// This is installed automatically for every executable that links against
// core via a static initialiser, so there's nothing to call manually.
void install_crash_handler();

#endif
