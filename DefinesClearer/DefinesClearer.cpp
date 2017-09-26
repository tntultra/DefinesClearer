// DefinesClearer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <map>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <regex>

using std::vector;
using std::set;
using std::string;
using namespace boost::filesystem;

string ProjectName;

vector<path> get_all_files(path p) {
	vector<path> v;
	for (auto&& x : directory_iterator(p)) {
		auto path = x.path();
		if (!is_directory(path) && path.extension() == ".cpp")
			v.push_back(path);
	}
	return v;
}

void get_vpw_defines(path p) {
	ifstream vpw_s{ p };
	string entry, name, value;
	size_t pos;
	while (std::getline(vpw_s, entry)) {
		if ((pos = entry.find("<Set")) != string::npos) {
			auto namePos = entry.find("Name=\"", pos);
			auto cb = entry.find("\"", namePos + 6);
			auto len = cb - namePos - 6;
			name = entry.substr(namePos + 6, len);

			auto valuePos = entry.find("Value=", cb);
			char bracket = entry[valuePos + 6];
			cb = entry.find(bracket, valuePos + 7);
			len = cb - valuePos - 7;
			value = entry.substr(valuePos + 7, len);

			if (bracket == '\"') {
				size_t prev = 0;
				string finalValue;
				while ((pos = value.find("%(", prev)) != string::npos) {
					finalValue += value.substr(prev, pos - prev);
					auto cb = value.find(")", pos);
					auto len = cb - pos - 2;
					auto macro = value.substr(pos + 2, len);
					auto it = VPW_DEFINES.find(macro);
					if (it == end(VPW_DEFINES)) {
						//get path variable
						macro = get_env_var(macro);
					}
					else {//alrdy exists
						macro = it->second;
					}
					finalValue += macro;
					prev = cb + 1;
				}
				finalValue += value.substr(prev);

				VPW_DEFINES[name] = finalValue;
			}
			else {//multiple defines

			}
		}
	}
}

set<string> get_defines_from_project_file(path p)
{
	set<string> allDefines;
	ifstream ifs{ p };
	string entry, previous;
	while (std::getline(ifs, entry)) {
		size_t namePos;
		if (entry.find("Defines=") != string::npos) {
			size_t currentDefinePos = 0;
			while ((currentDefinePos = entry.find("\"/D", currentDefinePos + 3)) != string::npos) {
				allDefines.insert(entry.substr(currentDefinePos + 3, entry.find('\"', currentDefinePos + 3)- currentDefinePos - 3));
			}
			return allDefines;
		}
		else if ((namePos = entry.find("Name=\"")) != string::npos) {
			if (previous.find("<Config") != string::npos && !ProjectName.length()) {
				ProjectName = entry.substr(namePos + 6, entry.find('\"', namePos + 7) - namePos - 6);
				std::cout << ProjectName;
			}
		}
		previous = entry;
	}
	return allDefines;
}

class ConjunctionMacroExpression { // a && b ... && c
	std::vector<std::pair<string, bool> > MacroNames;
public:
	explicit ConjunctionMacroExpression(string&& s) {
		size_t pos = 0, open = 0, close = 0;
		while (pos = s.find("defined", close) != string::npos) {
			open = s.find('(', pos);
			close = s.find(')', open);
			auto macroName = s.substr(open + 1, close - open - 1);
			bool defined = (s[pos - 1] != '!');
			MacroNames.push_back(std::make_pair(macroName, defined));
		}
	}

	bool eval(const set<string>& allDefs) const {
		for (auto&& p : MacroNames) {
			if ((p.second && allDefs.find(p.first) == end(allDefs)) ||
				(!p.second && allDefs.find(p.first) != end(allDefs))) {
				return false;
			}
		}
		return true;
	}
};

bool simple_eval(const string& s, size_t open, size_t close, const set<string>& allDefines) {
	bool defined = (s[open - 1] != '!');
	auto ob = s.find('(', open);
	auto cb = s.find(')', ob);
	auto macroName = s.substr(ob + 1, cb - ob - 1);
	return (defined && allDefines.find(macroName) != end(allDefines)) || (!defined && allDefines.find(macroName) == end(allDefines));
}

bool full_eval_simple_line(const string& entry, bool removeRow, const set<string>& allDefines) {
	bool conjunction = (entry.find("&&") != string::npos);
	if ((conjunction && !removeRow) || (!conjunction && removeRow)) {
		//statement eval not determined yet
		size_t pos, cb = 0;
		while ((pos = entry.find("defined", cb)) != string::npos) {
			cb = entry.find(')', pos + 7);
			auto eval = simple_eval(entry, pos + 6, cb, allDefines);
			if (!conjunction && eval) {
				return false;
			}
			else if (conjunction && !eval) {
				return true;
			}
		}
		//if (pos == string::npos) {
			if (conjunction) {
				return false;
			}
			else {
				return true;
			}
		//}
	}
}


void convert_and_save_file(path f, const set<string>& allDefines) {
	ifstream ifs{ f };
	auto filename = f.filename();
	auto parentPath = f.parent_path();
	auto outputFolder = parentPath.append(ProjectName);
	if (!exists(outputFolder)) {
		create_directory(outputFolder);
	}
	auto outputFileName = outputFolder.string() + "\\" + filename.string();
	ofstream ofs{ outputFileName };
	string entry;
	string macro;
	bool removeRow = false;
	size_t macroDepth = 0;
	size_t depthToRemove = 0;
	while (std::getline(ifs, entry)) {
		size_t pos, nameLen;
		//eval
		if ((pos = entry.find("#ifdef")) != string::npos) {
			
				++macroDepth;
				if (macroDepth > 1 && removeRow) {
					continue;
				}
				pos += 7;
				for (nameLen = 0; pos + nameLen < entry.length() && !isspace(entry[pos+nameLen]); ++nameLen);
				auto macroName = entry.substr(pos, nameLen);
				if (macroName == "MAKE_RFR" && filename.string() == "hypertcf.cpp") {
					int a = removeRow + 10;
					++a;
					if (a == 4)
						--a;
				}
				if (allDefines.find(macroName) == end(allDefines)) {
					macro = entry;
					removeRow = true;
					depthToRemove = macroDepth;
				}
			}
		else if ((pos = entry.find("#ifndef")) != string::npos) {
				++macroDepth;
				if (macroDepth > 1 && removeRow) {
					continue;
				}
				pos += 8;
				for (nameLen = 0; pos + nameLen < entry.length() && !isspace(entry[pos + nameLen]); ++nameLen);
				auto macroName = entry.substr(pos, nameLen);
				if (allDefines.find(macroName) != end(allDefines)) {
					macro = entry;
					removeRow = true;
					depthToRemove = macroDepth;
				}
			}
		else if (entry.find("#if") != string::npos || entry.find("#elif") != string::npos) {
				++macroDepth;
				if (macroDepth > 1 && removeRow) {
					continue;
				}
				//skip conditional defines
				if (entry.find(">") != string::npos || entry.find("<") != string::npos) {
					continue;
				}
				size_t pos = 0, open = 0, close = 0;
				//open = entry.find("(d");
				close = entry.find("))", open);
				//process only "simple" evaluations
				if (close == string::npos) {
					//assuming only one logical sign in whole entry
					removeRow = full_eval_simple_line(entry, removeRow, allDefines);
					if (removeRow) {
						macro = entry;
						depthToRemove = macroDepth;
					}
				}
			}
		else if (entry.find("#else") != string::npos) {
			removeRow = !removeRow;
		}
		else if (entry.find("#endif") != string::npos) {
			if (macro == "#if defined(MAKE_SAMLIC) && defined(MAKE_FEP)") {//"conditional breakpoint" haha
				int a = removeRow + 10;
				++a;
				if (a == 4)
					--a;
			}
			if (depthToRemove == macroDepth) {
				removeRow = false;
				depthToRemove = 0;
			}
			--macroDepth;
			if (macroDepth == 0) {
				removeRow = false;
			}
		}
		else if (entry.find ("//") == string::npos && //on commented line
			entry.find("@note") == string::npos && // on noted brief line
			entry.find("#error") == string::npos && // on error line
			(pos = entry.find("defined")) != string::npos) {
			if (entry[pos - 1] != '!' && !isspace(entry[pos - 1])) {
				//part of word/variable name
				continue;
			}
			boost::trim_left(entry);
			if (entry[0] == '*') {
				//part of brief
				continue;
			}
			//defines break from previous line
			removeRow = full_eval_simple_line(entry, removeRow, allDefines);
			if (removeRow) {
				macro = entry;
				depthToRemove = macroDepth;
			}
		}
		else {
			if (entry == "			if (!IsRFRAllow())") {//another "conditional breakpoint"
				int a = removeRow+10;
				++a;
				if (a == 4)
					--a;
			}
			if (!removeRow) {
				ofs << entry << std::endl;
			}
		}
	}
}

int main(int argc, char* argv[])
{
	if (argc > 1) {
		path VPJFile(argv[1]);
		path FolderToClear (argv[2]);
		if (exists(VPJFile) && is_regular_file(VPJFile) && exists(FolderToClear) && is_directory(FolderToClear)) {
			auto allDefines = get_defines_from_project_file(VPJFile);
			auto allFiles = get_all_files(FolderToClear);
			for (auto&& f : allFiles) {
				convert_and_save_file(f, allDefines);
			}
		}
	}
  return 0;
}
