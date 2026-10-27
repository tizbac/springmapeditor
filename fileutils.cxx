/*
 * fileutils.cxx - path and directory helpers.
 */
#include "fileutils.h"

#include <algorithm>
#include <cstring>

#include <dirent.h>

std::vector<std::pair<std::string, bool>> listDirectory(const std::string &dir)
{
	std::vector<std::pair<std::string, bool>> entries;
	DIR *d = opendir(dir.c_str());
	if (!d)
		return entries;

	static const char *imageExtensions[] = {".png", ".jpg",  ".jpeg", ".bmp", ".gif",
	                                        ".tga", ".webp", ".sd7",  ".sdz"};

	struct dirent *ent;
	while ((ent = readdir(d)) != nullptr)
	{
		std::string name = ent->d_name;
		if (name == "." || name == "..")
			continue;

		bool isDir = (ent->d_type == DT_DIR);
		bool isMap = name.size() > 4 && name.compare(name.size() - 4, 4, ".smf") == 0;
		bool isImage = false;
		for (const char *ext : imageExtensions)
		{
			size_t len = strlen(ext);
			if (name.size() >= len && name.compare(name.size() - len, len, ext) == 0)
			{
				isImage = true;
				break;
			}
		}

		if (isDir || isMap || isImage)
			entries.push_back(std::make_pair(name, isDir));
	}
	closedir(d);

	std::sort(entries.begin(), entries.end(),
	          [](const std::pair<std::string, bool> &a, const std::pair<std::string, bool> &b) {
		          if (a.second != b.second)
			          return a.second > b.second;
		          return a.first < b.first;
	          });
	return entries;
}

std::string joinPath(const std::string &a, const std::string &b)
{
	if (a.empty())
		return b;
	if (a[a.size() - 1] == '/')
		return a + b;
	return a + "/" + b;
}

std::string dirName(const std::string &path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return ".";
	if (slash == 0)
		return "/";
	return path.substr(0, slash);
}

std::string fileName(const std::string &path)
{
	size_t slash = path.find_last_of('/');
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string parentDirectory(const std::string &dir)
{
	if (dir.empty() || dir == "." || dir == "/")
		return std::string();
	std::string parent = dirName(dir);
	if (parent == dir)
		return std::string();
	return parent;
}

std::string pathWithoutExtension(const std::string &path)
{
	size_t dot = path.find_last_of('.');
	size_t slash = path.find_last_of('/');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return path;
	return path.substr(0, dot);
}
