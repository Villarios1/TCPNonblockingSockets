#pragma once
#include <filesystem>

namespace PNet
{
	struct FileInfo
	{
		std::filesystem::path filePath; //local
		std::filesystem::path fileName;
		uint64_t fileSize;
		uint32_t chunksCount;
	};
}