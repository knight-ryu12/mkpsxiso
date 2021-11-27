#include "platform.h"
#include "cd.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
//#include <io.h>
#endif

#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
static std::wstring UTF8ToUTF16(std::string_view str)
{
	std::wstring result;
	const int count = MultiByteToWideChar(CP_UTF8, 0, str.data(), str.length(), nullptr, 0);
	if (count != 0)
	{
		result.resize(count);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), str.length(), result.data(), count);
	}
	return result;
}

static std::string UTF16ToUTF8(std::wstring_view str)
{
	std::string result;
	int count = WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), nullptr, 0, nullptr, nullptr);
	if (count != 0)
	{
		result.resize(count);
		WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), result.data(), count, nullptr, nullptr);
	}
	return result;
}

static FILETIME TimetToFileTime(time_t t)
{
	FILETIME ft;
    LARGE_INTEGER ll;
	ll.QuadPart = t * 10000000ll + 116444736000000000ll;
    ft.dwLowDateTime = ll.LowPart;
    ft.dwHighDateTime = ll.HighPart;
	return ft;
}

time_t timegm(struct tm* tm)
{
	return _mkgmtime(tm);
}
#endif

FILE* OpenFile(const std::filesystem::path& path, const char* mode)
{
#if defined(_WIN32)
	FILE* file = nullptr;
	_wfopen_s(&file, path.c_str(), UTF8ToUTF16(mode).c_str());
	return file;
#else
	return ::fopen(path.c_str(), mode);
#endif
}

std::optional<struct stat64> Stat(const std::filesystem::path& path)
{
	struct stat64 fileAttrib;
#if defined(_WIN32)
	if (_wstat64(path.c_str(), &fileAttrib) != 0)
#else
    if (stat64(path.c_str(), &fileAttrib) != 0)
#endif
	{
		return std::nullopt;
	}

	return fileAttrib;
}

int64_t GetSize(const std::filesystem::path& path)
{
	auto fileAttrib = Stat(path);
	return fileAttrib.has_value() ? fileAttrib->st_size : -1;
}


void UpdateTimestamps(const std::filesystem::path& path, const cd::ISO_DATESTAMP& entryDate)
{
	tm timeBuf {};
	timeBuf.tm_year = entryDate.year;
	timeBuf.tm_mon = entryDate.month - 1;
	timeBuf.tm_mday = entryDate.day;
	timeBuf.tm_hour = entryDate.hour;
	timeBuf.tm_min = entryDate.minute - (15 * entryDate.GMToffs);
	timeBuf.tm_sec = entryDate.second;
	const time_t time = timegm(&timeBuf);

// utime can't update timestamps of directories on Windows, so a platform-specific approach is needed
#if defined(_WIN32)
	HANDLE file = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (file != INVALID_HANDLE_VALUE)
	{
		const FILETIME ft = TimetToFileTime(time);
		SetFileTime(file, &ft, nullptr, &ft);

		CloseHandle(file);
	}
#else
	struct timespec times[2];
	times[0].tv_nsec = UTIME_OMIT;

	times[1].tv_sec = time;
	times[1].tv_nsec = 0;
	utimensat(AT_FDCWD, path.c_str(), times, 0);
#endif
}

extern int Main(int argc, char* argv[]);

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
{
	std::vector<std::string> u8Arguments;
	u8Arguments.reserve(argc);
	for (int i = 0; i < argc; ++i)
	{
		u8Arguments.emplace_back(UTF16ToUTF8(argv[i]));
	}

	std::vector<const char*> u8argv;
	u8Arguments.reserve(argc + 1);
	for (std::string& str : u8Arguments)
	{
		u8argv.emplace_back(str.c_str());
	}
	u8argv.emplace_back(nullptr);

	return Main(argc, u8argv.data());
}
#else
int main(int argc, char* argv[])
{
	return Main(argc, argv);
}
#endif