/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
 *  Copyright 2009-2012 Fabrice Colin
 * 
 *  This code is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <sys/types.h>
#include <unistd.h>
#include <string>

/// Process handling.
class Process
{
	public:
		Process(const std::string &commandLine, bool pipeInput);
		virtual ~Process();

		/// Launches a child process to run the given command.
		pid_t launch(const std::string &input);

		/// Checks whether the process is running.
		bool isRunning(void);

		/// Wait for the process to exit.
		bool wait(int &exitStatus,
			bool noHang = false);

		/// Reads from the process' output.
		std::string readFromOutput(bool dontBlock = false);

		/// Convenience method to run a command and get its output.
		static std::string runCommand(const std::string &commandLine,
			const std::string &inputString,
			bool readOutput, int &exitStatus);

	protected:
		std::string m_commandLine;
		bool m_pipeInput;
		pid_t m_pid;
		std::string m_temporaryFile;
		int m_fds[2];

		void closeFds(void);

	private:
		// Process objects cannot be copied
		Process(const Process &other);
		Process &operator=(const Process &other);

};

#endif // _PROCESS_H_
