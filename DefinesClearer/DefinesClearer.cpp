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
				pos += 6;
				for (nameLen = 0; pos + nameLen < entry.length() && !isspace(entry[pos+nameLen]); ++nameLen);
				auto macroName = entry.substr(pos, nameLen);
				if (allDefines.find(macroName) == end(allDefines)) {
					removeRow = true;
					depthToRemove = macroDepth;
				}
			}
		else if ((pos = entry.find("#ifndef")) != string::npos) {
				++macroDepth;
				if (macroDepth > 1 && removeRow) {
					continue;
				}
				pos += 7;
				for (nameLen = 0; pos + nameLen < entry.length() && !isspace(entry[pos + nameLen]); ++nameLen);
				auto macroName = entry.substr(pos, nameLen);
				if (allDefines.find(macroName) != end(allDefines)) {
					removeRow = true;
					depthToRemove = macroDepth;
				}
			}
		else if (entry.find("#if ") != string::npos || entry.find("#elif ") != string::npos) {
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
					bool conjunction = (entry.find("&&") != string::npos);
					bool or = (entry.find("||") != string::npos);
					if (conjunction && or ) {
						continue;
					}
					size_t cb = 0;
					while ((pos = entry.find("defined", cb)) != string::npos) {
						cb = entry.find(')', pos+7);
						auto eval = simple_eval(entry, pos+6, cb, allDefines);
						if (!conjunction && eval) {
							removeRow = false;
							break;
						}
						else if (conjunction && !eval) {
							removeRow = true;
							break;
						}
					}
					if (conjunction) {
						removeRow = false;
					}
					else {
						removeRow = true;
					}
				}

				/*
				removeRow = true;
				//evaluate macro expression
				boost::algorithm::erase_all(entry, " ");
				size_t pos = 0, open = 0, close = 0;
				while (open = entry.find("(d", open) != string::npos && removeRow) {
					close = entry.find("))", open);
					while (pos = entry.find("defined", close) != string::npos && pos < open) {
						if (simple_eval(entry, pos, close, allDefines)) {
							removeRow = false;
							continue;
						}
					}
					if (removeRow) {
						ConjunctionMacroExpression ce{ entry.substr(open + 1, close - open - 1) };
						if (ce.eval(allDefines)) {
							removeRow = false;
						}
					}
				}
				//eval after last compound statement
				bool conjunction = false;
				while ((pos = entry.find("defined", pos+2)) != string::npos && removeRow) {
					auto cb = entry.find(')', pos);
					if (cb != entry.length() - 1 && entry[cb + 1] == '&') {
						conjunction = true;
					}
					auto eval = simple_eval(entry, pos, cb, allDefines);
					if (!conjunction && eval) {
						removeRow = false;
						break;
					}
					else if (conjunction && !eval) {
						break;
					}
					else if (conjunction && eval && cb == entry.length()-1) {
						removeRow = false;
					}
				}
				if (entry.find(">") != string::npos || entry.find("<") != string::npos) {
					removeRow = false;//for now
				}
				if (removeRow) {
					depthToRemove = macroDepth;
				}*/
			}
		else if (entry.find("#else") != string::npos) {
			removeRow = !removeRow;
		}
		else if (entry.find("#endif") != string::npos) {
			if (depthToRemove == macroDepth) {
				removeRow = false;
				depthToRemove = 0;
			}
			--macroDepth;
			if (macroDepth == 0) {
				removeRow = false;
			}
		}
		else if (entry.find("defined") != string::npos) {
			//defines breaked from previous line
		}
		else {
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
