/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 *  Copyright 2008 Global Sign In
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>

#include "Daemon.h"

using std::string;

Daemon::Daemon()
{
}

Daemon::~Daemon()
{
}

bool Daemon::daemonize(const string &directoryName, bool closeAllFiles)
{
	struct sigaction newAction;
	pid_t pid = fork();

	if (pid != 0)
	{
		// Parent exits
		return false;
	}

	// Child becomes process group leader
	setsid();

	// Fork again
	pid = fork();
	if (pid != 0)
	{
		// First child exits
		return false;
	}

	// Prevent terminal hangup
	sigemptyset(&newAction.sa_mask);
	newAction.sa_flags = 0;
	newAction.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &newAction, NULL);

	if (directoryName.empty() == false)
	{
		// Change directory
		chdir(directoryName.c_str());
	}

	// Reset the file mode creation mask
	umask(0);

	// Close all file descriptors ?
	if (closeAllFiles == true)
	{
		Daemon::closeAllFiles();
	}

	return true;
}

void Daemon::closeAllFiles(void)
{
	struct rlimit fdLimit;

	if (getrlimit(RLIMIT_NOFILE, &fdLimit) == 0)
	{
		for (int fdNum = 0; fdNum < (int)fdLimit.rlim_max; ++fdNum)
		{
			close(fdNum);
		}
	}
}

