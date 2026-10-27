/*
 * fileutils.h - path and directory helpers used by the editor and the file
 * dialog.
 */
#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<std::string, bool>> listDirectory(const std::string &dir);
std::string joinPath(const std::string &a, const std::string &b);
std::string dirName(const std::string &path);
std::string fileName(const std::string &path);
std::string parentDirectory(const std::string &dir);
std::string pathWithoutExtension(const std::string &path);

#endif // FILEUTILS_H
