#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <filesystem.h>
#include <stdlib.h>
#include <Core/Exceptions/FileException.h>
#include <Common/Util/StringUtil.h>
#include <Infrastructure/Logger.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif

class FileUtil
{
public:
	static fs::path ToPath(const std::string& pathStr)
	{
		return fs::path(StringUtil::ToWide(pathStr));
	}

	static bool ReadFile(const fs::path& filePath, std::vector<unsigned char>& data)
	{
		if (!fs::exists(filePath))
		{
			return false;
		}

		std::ifstream file(filePath, std::ios::in | std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return false;
		}

		auto size = fs::file_size(filePath);
		data.resize((size_t)size);

		file.seekg(0, std::ios::beg);
		file.read((char*)&data[0], size);
		file.close();

		return true;
	}

	static void RenameFile(const fs::path& source, const fs::path& destination)
	{
		std::error_code ec;
		fs::remove(destination, ec);
		if (ec)
		{
			LOG_ERROR_F("Failed to remove {}. Error: {}", destination, ec.message());
			throw FILE_EXCEPTION_F("Failed to remove {}. Error: {}", destination, ec.message());
		}

		fs::rename(source, destination, ec);
		if (ec)
		{
			LOG_ERROR_F("Failed to rename {} to {}. Error: {}", source, destination, ec.message());
			throw FILE_EXCEPTION_F("Failed to rename {} to {}. Error: {}", source, destination, ec.message());
		}
	}

	static void SafeWriteToFile(const fs::path& filePath, const std::vector<unsigned char>& data)
	{
		const fs::path tmpFilePath = ToPath(filePath.u8string() + ".tmp");
		std::ofstream file(tmpFilePath, std::ios::out | std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			LOG_ERROR_F("Failed to open {}", tmpFilePath);
			throw FILE_EXCEPTION_F("Failed to open {}", tmpFilePath);
		}

		file.write((const char*)&data[0], data.size());
		file.close();

		RenameFile(tmpFilePath, filePath);
	}

	static void WriteTextToFile(const fs::path& filePath, const std::string& text)
	{
		std::ofstream file(filePath, std::ios::out | std::ios::trunc);
		if (!file.is_open())
		{
			LOG_ERROR_F("Failed to open {}", filePath);
			throw FILE_EXCEPTION_F("Failed to open {}", filePath);
		}

		file.write(text.c_str(), text.size());
		file.close();
	}

	static bool RemoveFile(const fs::path& filePath) noexcept
	{
		std::error_code ec;
		const uintmax_t removed = fs::remove_all(filePath, ec);

		LOG_INFO_F("Removed {} with status {}", filePath, ec.message());

		return removed > 0 && ec.value() == 0;
	}

	static void CopyDirectory(const fs::path& sourceDir, const fs::path& destDir)
	{
		std::error_code ec;
		fs::create_directories(destDir, ec);
		if (ec)
		{
			throw std::system_error(ec);
		}

		if (!fs::exists(sourceDir, ec) || !fs::exists(destDir, ec))
		{
			throw std::system_error(ec);
		}

		fs::copy(sourceDir, destDir, fs::copy_options::recursive, ec);
		if (ec)
		{
			throw std::system_error(ec);
		}
	}

	static bool CreateDirectories(const fs::path& directory) noexcept
	{
		std::error_code ec;
		const bool created = fs::create_directories(directory, ec);
		if (ec)
		{
			LOG_ERROR_F("Error while creating {}. Error: ", directory, ec.message());
		}

		return created;
	}

	static bool Exists(const fs::path& path) noexcept
	{
		std::error_code ec;
		const bool exists = fs::exists(path, ec);
		if (ec)
		{
			LOG_ERROR_F("Error while checking if {} exists. Error: ", path, ec.message());
		}

		return exists;
	}

	static fs::path GetHomeDirectory()
	{
		#ifdef _WIN32
		wchar_t homeDriveBuf[MAX_PATH_LEN];
		size_t homeDriveLen = MAX_PATH_LEN;
		_wgetenv_s(&homeDriveLen, homeDriveBuf, sizeof(homeDriveBuf) - 1, L"HOMEDRIVE");
		std::wstring homeDrive(homeDriveBuf, (std::max)(1ULL, homeDriveLen) - 1);

		wchar_t homePathBuf[MAX_PATH_LEN];
		size_t homePathLen = MAX_PATH_LEN;
		_wgetenv_s(&homePathLen, homePathBuf, sizeof(homePathBuf) - 1, L"HOMEPATH");
		std::wstring homePath(homePathBuf, (std::max)(1ULL, homePathLen) - 1);

		return fs::path(homeDrive) / homePath;
		#else
		char* pHomePath = getenv("HOME");
		if (pHomePath != nullptr)
		{
			return fs::path(pHomePath);
		}
		else
		{
			return fs::path("~");
		}
		#endif
	}

	static bool TruncateFile(const fs::path& filePath, const uint64_t size)
	{
#if defined(WIN32)
		HANDLE hFile = CreateFile(StringUtil::ToWide(filePath.u8string()).c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		LARGE_INTEGER li;
		li.QuadPart = size;
		const bool success = SetFilePointerEx(hFile, li, NULL, FILE_BEGIN) && SetEndOfFile(hFile);

		CloseHandle(hFile);

		return success;
#else
		return truncate(filePath.c_str(), size) == 0;
#endif
	}

	static std::vector<std::string> GetSubDirectories(const fs::path& filePath, const bool includeHidden)
	{
		std::vector<std::string> listOfFiles;
		try {
			// Check if given path exists and points to a directory
			if (fs::exists(filePath) && fs::is_directory(filePath))
			{
				// Create a Recursive Directory Iterator object and points to the starting of directory
				fs::recursive_directory_iterator iter(filePath);

				// Create a Recursive Directory Iterator object pointing to end.
				fs::recursive_directory_iterator end;

				// Iterate till end
				while (iter != end)
				{
					// Check if current entry is a directory and if exists in skip list
					if (fs::is_directory(iter->path()))
					{
						// Skip the iteration of current directory pointed by iterator
						// c++17 fstem API to skip current directory iteration
						iter.disable_recursion_pending();
						std::string filename = StringUtil::ToUTF8(iter->path().filename().wstring());
						if (includeHidden || !StringUtil::StartsWith(filename, "."))
						{
							listOfFiles.push_back(filename);
						}
					}

					std::error_code ec;
					// Increment the iterator to point to next entry in recursive iteration
					iter.increment(ec);
					if (ec)
					{
						LOG_ERROR_F("Error While Accessing: ({}) - {}", iter->path().string(), ec.message());
						break;
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			LOG_ERROR_F("Exception thrown: {}", e);
		}

		return listOfFiles;
	}

	static size_t GetFileSize(const fs::path& file)
	{
		std::error_code error;
		size_t fileSize = fs::file_size(file, error);
		if (error.value() != 0)
		{
			return 0;
		}

		return fileSize;
	}

private:
	static const size_t MAX_PATH_LEN = 260;
};
