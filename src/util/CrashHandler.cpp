#include "CrashHandler.h"
#include <QDir>
#include <QCoreApplication>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// The signal/exception handlers run in a constrained context, so they avoid
// Qt and heap allocation and use only low-level, fixed-size buffers that are
// filled in once at Install() time.
static char g_logPath[4096] = {0};

// Detect AddressSanitizer (GCC defines __SANITIZE_ADDRESS__; Clang uses
// __has_feature). Under ASan we must NOT take over the fatal signals, or we'd
// suppress ASan's own reports (the fuzz harnesses rely on them).
#if defined(__SANITIZE_ADDRESS__)
#  define BMS_UNDER_ASAN 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define BMS_UNDER_ASAN 1
#  endif
#endif

namespace CrashHandler { QString LogPath() { return QString::fromLocal8Bit(g_logPath); } }

#if defined(Q_OS_WIN)
// ---------------------------------------------------------------------------
// Windows: SetUnhandledExceptionFilter + dbghelp symbolization.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <dbghelp.h>

static LONG WINAPI WinExceptionFilter(EXCEPTION_POINTERS *info)
{
	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
	SymInitialize(process, NULL, TRUE);

	void *frames[64];
	USHORT n = CaptureStackBackTrace(0, 64, frames, NULL);

	FILE *files[2] = { stderr, g_logPath[0] ? fopen(g_logPath, "w") : NULL };
	const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;

	char symbolBuf[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuf);
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol->MaxNameLen = 255;

	for (int fi = 0; fi < 2; fi++) {
		FILE *f = files[fi];
		if (!f) continue;
		fprintf(f, "\n*** BmsONE crashed (exception 0x%08lx) ***\n", (unsigned long)code);
		for (USHORT i = 0; i < n; i++) {
			DWORD64 addr = (DWORD64)(uintptr_t)frames[i];
			if (SymFromAddr(process, addr, 0, symbol)) {
				fprintf(f, "  #%-2u 0x%016llx %s\n", i, (unsigned long long)addr, symbol->Name);
			} else {
				fprintf(f, "  #%-2u 0x%016llx\n", i, (unsigned long long)addr);
			}
		}
		fflush(f);
		if (f != stderr) fclose(f);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

static void InstallPlatform()
{
#ifndef BMS_UNDER_ASAN
	SetUnhandledExceptionFilter(WinExceptionFilter);
#endif
}

#else
// ---------------------------------------------------------------------------
// POSIX (Linux / macOS): sigaction + backtrace() on an alternate stack so we
// can still report stack-overflow SIGSEGV.
// ---------------------------------------------------------------------------
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

// Async-signal-safe writes only below.
static void SafeWrite(int fd, const char *s) { if (fd >= 0) { ssize_t r = ::write(fd, s, strlen(s)); (void)r; } }
static void SafeWriteInt(int fd, int v)
{
	char buf[16]; int i = sizeof(buf); bool neg = v < 0; unsigned u = neg ? -v : v;
	buf[--i] = '\0';
	do { buf[--i] = char('0' + (u % 10)); u /= 10; } while (u && i > 0);
	if (neg && i > 0) buf[--i] = '-';
	SafeWrite(fd, buf + i);
}

static void PosixHandler(int sig)
{
	void *frames[64];
	int n = backtrace(frames, 64);

	int fds[2] = { STDERR_FILENO, g_logPath[0] ? ::open(g_logPath, O_CREAT | O_WRONLY | O_TRUNC, 0644) : -1 };
	for (int k = 0; k < 2; k++) {
		int fd = fds[k];
		if (fd < 0) continue;
		SafeWrite(fd, "\n*** BmsONE crashed (signal ");
		SafeWriteInt(fd, sig);
		SafeWrite(fd, ") ***\n");
		backtrace_symbols_fd(frames, n, fd);
	}
	if (fds[1] >= 0) ::close(fds[1]);

	// Restore default handler and re-raise so the OS still produces a core/exit.
	signal(sig, SIG_DFL);
	raise(sig);
}

static void InstallPlatform()
{
#ifndef BMS_UNDER_ASAN
	// Alternate signal stack so a stack-overflow SIGSEGV is still catchable.
	// Fixed size: SIGSTKSZ is not a compile-time constant on modern glibc.
	static const size_t kAltStackSize = 64 * 1024;
	static char altStack[kAltStackSize];
	stack_t ss;
	ss.ss_sp = altStack;
	ss.ss_size = sizeof(altStack);
	ss.ss_flags = 0;
	sigaltstack(&ss, nullptr);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = PosixHandler;
	sa.sa_flags = SA_ONSTACK | SA_RESETHAND;
	sigemptyset(&sa.sa_mask);
	for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS }) {
		sigaction(sig, &sa, nullptr);
	}
#endif
}

#endif

namespace CrashHandler
{

QString Install(const QString &logDir)
{
	QDir().mkpath(logDir);
	QString path = QDir(logDir).filePath(
		QString("BmsONE-crash-%1.log").arg(QCoreApplication::applicationPid()));
	QByteArray utf8 = path.toLocal8Bit();
	qstrncpy(g_logPath, utf8.constData(), sizeof(g_logPath));
	InstallPlatform();
	return path;
}

}
