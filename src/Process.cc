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

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <fstream>

#include "config.h"
#include "Daemon.h"
#include "Process.h"

using std::clog;
using std::endl;
using std::string;
using std::ofstream;

Process::Process(const string &commandLine, bool pipeInput) :
	m_commandLine(commandLine),
	m_pipeInput(pipeInput),
	m_pid(0)
{
	m_fds[0] = m_fds[1] = -1;
}

Process::~Process()
{
	if (m_pipeInput == true)
	{
		closeFds();
	}
	if (m_temporaryFile.empty() == false)
	{
		unlink(m_temporaryFile.c_str());
	}
}

void Process::closeFds(void)
{
	close(m_fds[0]);
	close(m_fds[1]);
	m_fds[0] = m_fds[1] = -1;
}

pid_t Process::launch(const string &input)
{
	string commandLine(m_commandLine);
	char outTemplate[19] = "/tmp/processXXXXXX";
	int outFd = -1;

#ifdef HAVE_SOCKETPAIR
	if ((m_pipeInput == true) &&
		(socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, m_fds) != 0))
	{
		return -1;
	}
#else
	clog << "Cannot launch '" << commandLine << "'" << endl;
	return -1;
#endif

	// Any input ?
	if (input.empty() == false)
	{
		outFd = mkstemp(outTemplate);
		if (outFd == -1)
		{
			return false;
		}

		if (write(outFd, (const void*)input.c_str(), input.length()) < (ssize_t)input.length())
		{
			close(outFd);

			return false;
		}
		lseek(outFd, 0, SEEK_SET);
	}

	// Fork and execute the command
#ifdef HAVE_FORK
	m_pid = fork();
#endif
	if (m_pid == 0)
	{
		// Child process
		if (outFd > -1)
		{
			// Connect stdin
			dup2(outFd, 0);
		}

		if (m_pipeInput == true)
		{
			// Connect stdout, stderr and stdlog
			dup2(m_fds[1], 1);
			dup2(m_fds[1], 2);
			dup2(m_fds[1], 3);
		}
		else
		{
			// Close every file descriptor
			Daemon::closeAllFiles();
		}

		execl("/bin/sh", "/bin/sh", "-c", commandLine.c_str(), (void*)NULL);

		exit(EXIT_FAILURE);
	}

	// Parent process
	if (input.empty() == false)
	{
		close(outFd);
		m_temporaryFile = outTemplate;
	}

	if (m_pid == -1)
	{
		// The fork failed
		if (m_pipeInput == true)
		{
			closeFds();
		}

		return -1;
	}

	return m_pid;
}

bool Process::isRunning(void)
{
	if (kill(m_pid, 0) == 0)
	{
		return true;
	}

	return false;
}

bool Process::wait(int &exitStatus,
	bool noHang)
{
	int childStatus = 0;
	int options = 0;
	bool exited = false;

	if (noHang == true)
	{
		options = WNOHANG;
	}

	// Wait for this PID
	pid_t childPid = waitpid(m_pid, &childStatus, options);
	while (childPid > 0)
	{
		if (WIFEXITED(childStatus))
		{
			clog << "Child process " << m_pid
				<< " exited with return code "
				<< WEXITSTATUS(childStatus) << endl;
			exitStatus = WEXITSTATUS(childStatus);
			exited = true;
		}
		else if (WIFSIGNALED(childStatus))
		{
			clog << "Child process " << m_pid
				<< " was killed by signal "
				<< WTERMSIG(childStatus) << endl;
		}
		else if (WIFSTOPPED(childStatus))
		{
			clog << "Child process " << m_pid
				<< " was stopped by signal "
				<< WSTOPSIG(childStatus) << endl;
		}

		childPid = waitpid(m_pid, &childStatus, options);
	}

	return exited;
}

string Process::readFromOutput(bool dontBlock)
{
	string fileBuffer;
	ssize_t bytesRead = 0, totalSize = 0;

	if ((m_pipeInput == false) ||
		(m_fds[0] == -1))
	{
		return "";
	}

	if (dontBlock == true)
	{
		// Make reads non-blocking
		int flags = fcntl(m_fds[0], F_GETFL, 0);
		fcntl(m_fds[0], F_SETFL, flags|O_NONBLOCK);
	}

	do
	{
		char readBuffer[4096];

		bytesRead = read(m_fds[0], readBuffer, 4096);
		if (bytesRead > 0)
		{
			totalSize += bytesRead;
			fileBuffer.append(readBuffer, bytesRead);
			if (bytesRead < 4096)
			{
				// Next read will block
				break;
			}
		}
		else if (bytesRead == -1)
		{
			// An error occured
			if (errno != EINTR)
			{
				break;
			}

			// Try again
			bytesRead = 1;
		}
	} while (bytesRead > 0);

	return fileBuffer;
}

string Process::runCommand(const string &commandLine,
	const string &inputString, bool readOutput,
	int &exitStatus)
{
	string output;

	try
	{
		// Nice the new process
		Process process(commandLine, true);

		pid_t commandPid = process.launch(inputString);
		if (commandPid > 0)
		{
			clog << "Running command " << commandLine << " as "
				<< " under PID " << commandPid << endl;

			// Wait for its exit
			process.wait(exitStatus);

			if (readOutput == true)
			{
				output = process.readFromOutput(true);
			}
		}
		else
		{
			clog << "Couldn't run command " << commandLine << endl;
		}
	}
	catch (...)
	{
		clog << "Failed to run command " << commandLine << endl; 
		output.clear();
	}

	return output;
}

