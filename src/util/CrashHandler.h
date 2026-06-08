#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <QString>

// Lightweight, dependency-free crash handler. On a fatal fault it writes a
// stack trace to stderr and to a log file so field crashes are diagnosable
// without a debugger. Install() once, as early as possible in main().
namespace CrashHandler
{
	// logDir: directory the crash log is written to (created if needed).
	// Returns the full path the crash log would be written to.
	QString Install(const QString &logDir);

	// Path of the crash log for the current process (valid after Install()).
	QString LogPath();
}

#endif // CRASHHANDLER_H
