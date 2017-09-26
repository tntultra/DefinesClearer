#pragma once

#include <vector>
#include <string>
#include <set>

#include <boost/filesystem.hpp>

extern std::string ProjectName;

class ConjunctionMacroExpression { // a && b ... && c
	std::vector<std::pair<std::string, bool> > MacroNames;
public:
	explicit ConjunctionMacroExpression(std::string&& s) {
		size_t pos = 0, open = 0, close = 0;
		while (pos = s.find("defined", close) != std::string::npos) {
			open = s.find('(', pos);
			close = s.find(')', open);
			auto macroName = s.substr(open + 1, close - open - 1);
			bool defined = (s[pos - 1] != '!');
			MacroNames.push_back(std::make_pair(macroName, defined));
		}
	}

	bool eval(const std::set<std::string>& allDefs) const {
		for (auto&& p : MacroNames) {
			if ((p.second && allDefs.find(p.first) == end(allDefs)) ||
				(!p.second && allDefs.find(p.first) != end(allDefs))) {
				return false;
			}
		}
		return true;
	}
};

std::vector<boost::filesystem::path> get_all_files(boost::filesystem::path p);
std::set<std::string> get_defines_from_project_file(boost::filesystem::path p);
bool simple_eval(const std::string& s, size_t open, size_t close, const std::set<std::string>& allDefines);
bool full_eval_simple_line(const std::string& entry, bool removeRow, const std::set<std::string>& allDefines);
void convert_and_save_file(boost::filesystem::path f, const std::set<std::string>& allDefines);
void get_vpw_defines(boost::filesystem::path p);