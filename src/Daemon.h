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

#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <string>

/// Daemonizes a process.
class Daemon
{
	public:
		virtual ~Daemon();

		/**
		  * Turns the current process into a daemon process.
		  * Returns false if exit is required.
		  */
		static bool daemonize(const std::string &directoryName,
			bool closeAllFiles = true);

		/// Closes all file descriptors.
		static void closeAllFiles(void);

	protected:
		Daemon();

	private:
		// Daemon objects cannot be copied
		Daemon(const Daemon &other);
		Daemon &operator=(const Daemon &other);

};

#endif // _DAEMON_H_
