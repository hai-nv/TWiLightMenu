/*
	inifile.cpp
	Copyright (C) 2007 Acekard, www.acekard.com
	Copyright (C) 2007-2009 somebody
	Copyright (C) 2009 yellow wood goblin

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/inifile.h"
#include "common/stringtool.h"

#include <cstdio>
#include <cstdlib>

bool gbar2Fix = false;

// No longer used by LoadIniFile (it reads in chunks now); kept for reference.
__attribute__((unused)) static bool freadLine(FILE *f, std::string &str)
{
	str.clear();
__read:
	char p = 0;

	size_t readed = fread(&p, 1, 1, f);
	if (0 == readed) {
		str = "";
		return false;
	}
	if ('\n' == p || '\r' == p) {
		str = "";
		return true;
	}

	while (p != '\n' && p != '\r' && readed) {
		str += p;
		readed = fread(&p, 1, 1, f);
	}

	if (str.empty() || "" == str) {
		goto __read;
	}

	return true;
}

static void trimString(std::string &str)
{
	size_t first = str.find_first_not_of(" \t"), last;
	if (first == str.npos) {
		str.clear();
	} else {
		last = str.find_last_not_of(" \t");
		if ((last + 1) < str.length())
			str.erase(last + 1);
		if (first > 0)
			str.erase(0, first);
	}
}

CIniFile::CIniFile()
{
	m_bLastResult = false;
	m_bModified = false;
	m_bReadOnly = false;
}

CIniFile::CIniFile(const std::string &filename)
{
	m_sFileName = filename;
	m_bLastResult = false;
	m_bModified = false;
	m_bReadOnly = false;
	LoadIniFile(m_sFileName);
}

CIniFile::~CIniFile()
{
	if (m_FileContainer.size() > 0) {
		m_FileContainer.clear();
	}
}

void CIniFile::SetString(const std::string &Section, const std::string &Item, const std::string &Value)
{
	if (m_bReadOnly)
		return;
	if (GetFileString(Section, Item) != Value) {
		SetFileString(Section, Item, Value);
		m_bModified = true;
	}
}

void CIniFile::SetInt(const std::string &Section, const std::string &Item, int Value)
{
	if (m_bReadOnly)
		return;
	std::string strtemp = formatString("%d", Value);

	if (GetFileString(Section, Item) != strtemp) {
		SetFileString(Section, Item, strtemp);
		m_bModified = true;
	}
}

std::string CIniFile::GetString(const std::string &Section, const std::string &Item)
{
	return GetFileString(Section, Item);
}

std::string CIniFile::GetString(const std::string &Section, const std::string &Item, const std::string &DefaultValue)
{
	std::string temp = GetString(Section, Item);
	if (!m_bLastResult) {
		SetString(Section, Item, DefaultValue);
		temp = DefaultValue;
	}
	return temp;
}

void CIniFile::GetStringVector(const std::string &Section, const std::string &Item, std::vector<std::string> &strings, char delimiter)
{
	std::string strValue = GetFileString(Section, Item);
	strings.clear();
	size_t pos;
	while ((pos = strValue.find(delimiter), strValue.npos != pos)) {
		const std::string string = strValue.substr(0, pos);
		if (string.length()) {
			strings.push_back(string);
		}
		strValue = strValue.substr(pos + 1, strValue.npos);
	}
	if (strValue.length()) {
		strings.push_back(strValue);
	}
}

void CIniFile::SetStringVector(const std::string &Section, const std::string &Item, std::vector<std::string> &strings, char delimiter)
{
	std::string strValue;
	for (size_t ii = 0; ii < strings.size(); ++ii) {
		if (ii)
			strValue += delimiter;
		strValue += strings[ii];
	}
	SetString(Section, Item, strValue);
}

int CIniFile::GetInt(const std::string &Section, const std::string &Item)
{
	std::string value = GetFileString(Section, Item);
	if (value.size() > 2 && '0' == value[0] && ('x' == value[1] || 'X' == value[1]))
		return strtol(value.c_str(), NULL, 16);
	else
		return strtol(value.c_str(), NULL, 10);
}

int CIniFile::GetInt(const std::string &Section, const std::string &Item, int DefaultValue)
{
	int temp;
	temp = GetInt(Section, Item);
	if (!m_bLastResult) {
		SetInt(Section, Item, DefaultValue);
		temp = DefaultValue;
	}
	return temp;
}

bool CIniFile::LoadIniFile(const std::string &FileName)
{
	//dbg_printf("load %s\n",FileName.c_str());
	if (FileName != "")
		m_sFileName = FileName;

	FILE *f = fopen(FileName.c_str(), "rb");

	if (NULL == f)
		return false;

	//check for utf8 bom.
	char bom[3];
	if (fread(bom, 3, 1, f) == 1 && bom[0] == 0xef && bom[1] == 0xbb && bom[2] == 0xbf)
		;
	else
		fseek(f, 0, SEEK_SET);

	std::string strline;
	m_FileContainer.clear();
	m_Cache.clear();

	// Same result as reading line by line: lines are split at \n or \r,
	// trimmed, and empty / comment lines are skipped. Read in chunks, not byte by byte.
	char buf[512];
	size_t got;
	while ((got = fread(buf, 1, sizeof(buf), f)) > 0) {
		size_t start = 0;
		for (size_t k = 0; k < got; k++) {
			if (buf[k] == '\n' || buf[k] == '\r') {
				strline.append(buf + start, k - start);
				start = k + 1;
				trimString(strline);
				if (!strline.empty() && ';' != strline[0] && '/' != strline[0] && '!' != strline[0])
					m_FileContainer.push_back(strline);
				strline.clear();
			}
		}
		strline.append(buf + start, got - start);
	}
	trimString(strline);
	if (!strline.empty() && ';' != strline[0] && '/' != strline[0] && '!' != strline[0])
		m_FileContainer.push_back(strline);

	fclose(f);

	m_bLastResult = false;
	m_bModified = false;

	return true;
}

bool CIniFile::SaveIniFileModified(const std::string &FileName)
{
	if (m_bModified == true) {
		return SaveIniFile(FileName);
	}

	return true;
}

bool CIniFile::SaveIniFile(const std::string &FileName)
{
	if (FileName != "")
		m_sFileName = FileName;

	FILE *f = fopen(m_sFileName.c_str(), "wb");
	if (NULL == f) {
		return false;
	}

	for (size_t ii = 0; ii < m_FileContainer.size(); ii++) {
		std::string &strline = m_FileContainer[ii];
		size_t notSpace = strline.find_first_not_of(' ');
		strline = strline.substr(notSpace);
		if (strline.find('[') == 0 && ii > 0) {
			if (!m_FileContainer[ii - 1].empty() && m_FileContainer[ii - 1] != "")
				fwrite((gbar2Fix ? "\n" : "\r\n"), 1, 2-gbar2Fix, f);
		}
		if (!strline.empty() && strline != "") {
			fwrite(strline.c_str(), 1, strline.length(), f);
			fwrite((gbar2Fix ? "\n" : "\r\n"), 1, 2-gbar2Fix, f);
		}
	}

	fclose(f);

	m_bModified = false;

	return true;
}

std::string CIniFile::GetFileString(const std::string &Section, const std::string &Item)
{
	size_t ii = 0;
	const size_t iFileLines = m_FileContainer.size();

	if (m_bReadOnly) {
		SectionCache::iterator it = m_Cache.find(Section);
		if ((it != m_Cache.end()))
			ii = it->second;
	}

	m_bLastResult = false;

	// Lines are read by reference: copying every line made each lookup very slow.
	while (ii < iFileLines) {
		const std::string &strline = m_FileContainer[ii++];

		size_t rBracketPos = 0;
		if ('[' == strline[0])
			rBracketPos = strline.find(']');
		if (rBracketPos > 0 && rBracketPos != std::string::npos) {
			if (m_bReadOnly)
				m_Cache.insert(std::make_pair(strline.substr(1, rBracketPos - 1), ii - 1));
			if (strline.compare(1, rBracketPos - 1, Section) == 0) {
				while (ii < iFileLines) {
					const std::string &itemLine = m_FileContainer[ii++];
					size_t equalsignPos = itemLine.find('=');
					if (equalsignPos != itemLine.npos) {
						size_t last = equalsignPos ? itemLine.find_last_not_of(" \t", equalsignPos - 1) : itemLine.npos;
						bool same;
						if (last == itemLine.npos)
							same = Item.empty();
						else
							same = (last + 1 == Item.size()) && (itemLine.compare(0, last + 1, Item) == 0);

						if (same) {
							size_t first = itemLine.find_first_not_of(" \t", equalsignPos + 1);
							m_bLastResult = true;
							if (first == itemLine.npos)
								return std::string();
							return itemLine.substr(first);
						}
					} else if ('[' == itemLine[0]) {
						break;
					}
				}
				break;
			}
		}
	}
	return std::string();
}

void CIniFile::SetFileString(const std::string &Section, const std::string &Item, const std::string &Value)
{
	if (m_bReadOnly)
		return;

	size_t ii = 0;
	size_t iFileLines = m_FileContainer.size();

	while (ii < iFileLines) {
		const std::string &strline = m_FileContainer[ii++];

		size_t rBracketPos = 0;
		if ('[' == strline[0])
			rBracketPos = strline.find(']');
		if (rBracketPos > 0 && rBracketPos != std::string::npos) {
			if (strline.compare(1, rBracketPos - 1, Section) == 0) {
				while (ii < iFileLines) {
					const std::string &itemLine = m_FileContainer[ii++];
					size_t equalsignPos = itemLine.find('=');
					if (equalsignPos != itemLine.npos) {
						size_t last = equalsignPos ? itemLine.find_last_not_of(" \t", equalsignPos - 1) : itemLine.npos;
						bool same;
						if (last == itemLine.npos)
							same = Item.empty();
						else
							same = (last + 1 == Item.size()) && (itemLine.compare(0, last + 1, Item) == 0);

						if (same) {
							ReplaceLine(ii - 1, Item + (gbar2Fix ? "=" : " = ") + Value);
							return;
						}
					} else if ('[' == itemLine[0]) {
						InsertLine(ii - 1, Item + (gbar2Fix ? "=" : " = ") + Value);
						return;
					}
				}
				InsertLine(ii, Item + (gbar2Fix ? "=" : " = ") + Value);
				return;
			}
		}
	}

	InsertLine(ii, "[" + Section + "]");
	InsertLine(ii + 1, Item + (gbar2Fix ? "=" : " = ") + Value);
	return;
}

bool CIniFile::InsertLine(size_t line, const std::string &str)
{
	m_FileContainer.insert(m_FileContainer.begin() + line, str);
	return true;
}

bool CIniFile::ReplaceLine(size_t line, const std::string &str)
{
	m_FileContainer[line] = str;
	return true;
}
