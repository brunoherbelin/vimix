#include <fstream>
#include <iostream>
#include <sstream>

#include <stb_image.h>
#include <glad/glad.h>

#ifdef WIN32
#include <include/dirent.h>
#define PATH_SEP '\\'
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#elif defined(LINUX) or defined(APPLE)
#include <sys/types.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#include "ImGuiToolkit.h"
#include "FileDialog.h"

static std::string s_fs_root(1u, PATH_SEP);
static std::string currentFileDialog;

inline bool replaceString(::std::string& str, const ::std::string& oldStr, const ::std::string& newStr)
{
	bool found = false;
	size_t pos = 0;
	while ((pos = str.find(oldStr, pos)) != ::std::string::npos)
	{
		found = true;
		str.replace(pos, oldStr.length(), newStr);
		pos += newStr.length();
	}
	return found;
}

inline std::vector<std::string> splitStringToVector(const ::std::string& text, char delimiter, bool pushEmpty)
{
	std::vector<std::string> arr;
	if (text.size() > 0)
	{
		std::string::size_type start = 0;
		std::string::size_type end = text.find(delimiter, start);
		while (end != std::string::npos)
		{
			std::string token = text.substr(start, end - start);
			if (token.size() > 0 || (token.size() == 0 && pushEmpty))
				arr.push_back(token);
			start = end + 1;
			end = text.find(delimiter, start);
		}
		arr.push_back(text.substr(start));
	}
	return arr;
}

inline std::vector<std::string> GetDrivesList()
{
	std::vector<std::string> res;

#ifdef WIN32
	DWORD mydrives = 2048;
	char lpBuffer[2048];

	DWORD countChars = GetLogicalDriveStrings(mydrives, lpBuffer);

	if (countChars > 0)
	{
		std::string var = std::string(lpBuffer, countChars);
		replaceString(var, "\\", "");
		res = splitStringToVector(var, '\0', false);
	}
#endif

	return res;
}

inline bool IsDirectoryExist(const std::string& name)
{
	bool bExists = false;

	if (name.size() > 0)
	{
		DIR *pDir = 0;
		pDir = opendir(name.c_str());
		if (pDir != NULL)
		{
			bExists = true;
			(void)closedir(pDir);
		}
	}

	return bExists;    // this is not a directory!
}

inline bool CreateDirectoryIfNotExist(const std::string& name)
{
	bool res = false;

	if (name.size() > 0)
	{
		if (!IsDirectoryExist(name))
		{
			res = true;

#ifdef WIN32
			CreateDirectory(name.c_str(), NULL);
#elif defined(LINUX) or defined(APPLE)
			char buffer[PATH_MAX] = {};
			snprintf(buffer, PATH_MAX, "mkdir -p %s", name.c_str());
			const int dir_err = std::system(buffer);
			if (dir_err == -1)
			{
				std::cout << "Error creating directory " << name << std::endl;
				res = false;
			}
#endif
		}
	}

	return res;
}

struct PathStruct
{
	std::string path;
	std::string name;
	std::string ext;

	bool isOk;

	PathStruct()
	{
		isOk = false;
	}
};

inline PathStruct ParsePathFileName(const std::string& vPathFileName)
{
	PathStruct res;

	if (vPathFileName.size() > 0)
	{
		std::string pfn = vPathFileName;
		std::string separator(1u, PATH_SEP);
		replaceString(pfn, "\\", separator);
		replaceString(pfn, "/", separator);

		size_t lastSlash = pfn.find_last_of(separator);
		if (lastSlash != std::string::npos)
		{
			res.name = pfn.substr(lastSlash + 1);
			res.path = pfn.substr(0, lastSlash);
			res.isOk = true;
		}

		size_t lastPoint = pfn.find_last_of('.');
		if (lastPoint != std::string::npos)
		{
			if (!res.isOk)
			{
				res.name = pfn;
				res.isOk = true;
			}
			res.ext = pfn.substr(lastPoint + 1);
			replaceString(res.name, "." + res.ext, "");
		}
	}

	return res;
}

inline void AppendToBuffer(char* vBuffer, size_t vBufferLen, const std::string &vStr)
{
    std::string st = vStr;
    size_t len = vBufferLen - 1u;
    size_t slen = strlen(vBuffer);

    if (st != "" && st != "\n")
    {
        replaceString(st, "\n", "");
        replaceString(st, "\r", "");
    }
    vBuffer[slen] = '\0';
    std::string str = std::string(vBuffer);
    if (str.size() > 0) str += "\n";
    str += vStr;
    if (len > str.size()) len = str.size();
#ifdef MSVC
    strncpy_s(vBuffer, vBufferLen, str.c_str(), len);
#else
    strncpy(vBuffer, str.c_str(), len);
#endif
    vBuffer[len] = '\0';
}

inline void ResetBuffer(char* vBuffer)
{
	vBuffer[0] = '\0';
}

char FileDialog::FileNameBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
char FileDialog::DirectoryNameBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
char FileDialog::SearchBuffer[MAX_FILE_DIALOG_NAME_BUFFER] = "";
int FileDialog::FilterIndex = 0;

FileDialog::FileDialog()
{
	m_AnyWindowsHovered = false;
	IsOk = false;
	m_ShowDialog = false;
	m_ShowDrives = false;
	m_CreateDirectoryMode = false;
	dlg_optionsPane = 0;
	dlg_optionsPaneWidth = 250;
	dlg_filters = "";
}

FileDialog::~FileDialog()
{

}

/* Alphabetical sorting */
/*#ifdef WIN32
static int alphaSort(const void *a, const void *b)
{
	return strcoll(((dirent*)a)->d_name, ((dirent*)b)->d_name);
}
#elif defined(LINUX) or defined(APPLE)*/
static int alphaSort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}
//#endif

static bool stringComparator(const FileInfoStruct& a, const FileInfoStruct& b)
{
	bool res;
	if (a.type != b.type) res = (a.type < b.type);
	else res = (a.fileName < b.fileName);
	return res;
}

void FileDialog::ScanDir(const std::string& vPath)
{
    struct dirent **files               = NULL;
	std::string		path				= vPath;

#if defined(LINUX) or defined(APPLE)
    if (path.size()>0)
    {
        if (path[0] != PATH_SEP)
        {
            //path = PATH_SEP + path;
        }
    }
#endif

    if (0u == m_CurrentPath_Decomposition.size())
    {
        SetCurrentDir(path);
    }

    if (0u != m_CurrentPath_Decomposition.size())
    {
#ifdef WIN32
		if (path == s_fs_root)
		{
			path += PATH_SEP;
		}
#endif
        int n = scandir(path.c_str(), &files, NULL, alphaSort);
        if (n > 0)
        {
			m_FileList.clear();

            for (int i = 0; i < n; ++i)
            {
                struct dirent *ent = files[i];

                FileInfoStruct infos;

                infos.fileName = ent->d_name;
                if (("." != infos.fileName)/* && (".." != infos.fileName)*/)
                {
                    switch (ent->d_type)
                    {
                        case DT_REG: infos.type = 'f'; break;
                        case DT_DIR: infos.type = 'd'; break;
                        case DT_LNK: infos.type = 'l'; break;
                    }

                    if (infos.type == 'f')
                    {
                        size_t lpt = infos.fileName.find_last_of('.');
                        if (lpt != std::string::npos)
                        {
                            infos.ext = infos.fileName.substr(lpt);
                        }
                    }

                	bool is_hidden = false;
                    #ifdef OSWIN
                    std::string dir = path + std::string(ent->d_name);
                    // IF system file skip it...
                    if (FILE_ATTRIBUTE_SYSTEM & GetFileAttributesA(dir.c_str()))
                        continue;
                    if (FILE_ATTRIBUTE_HIDDEN & GetFileAttributesA(dir.c_str()))
                        is_hidden = true;
                    #else
                    if(infos.fileName[0] == '.')
                        is_hidden = true;
                    #endif // OSWIN

					if (!is_hidden)
                    	m_FileList.push_back(infos);
                }
            }

            for (int i = 0; i < n; ++i)
            {
                free(files[i]);
            }
            free(files);
        }

		std::sort(m_FileList.begin(), m_FileList.end(), stringComparator);
    }
}

void FileDialog::SetCurrentDir(const std::string& vPath)
{
	std::string path = vPath;
#ifdef WIN32
	if (s_fs_root == path)
		path += PATH_SEP;
#endif
    DIR  *dir = opendir(path.c_str());
    char  real_path[PATH_MAX];

    if (NULL == dir)
    {
		path = ".";
        dir = opendir(path.c_str());
    }

    if (NULL != dir)
    {
#ifdef WIN32
		size_t numchar = GetFullPathName(path.c_str(), PATH_MAX-1, real_path, 0);
#elif defined(LINUX) or defined(APPLE)
		char *numchar = realpath(path.c_str(), real_path);
#endif
		if (numchar != 0)
        {
			m_CurrentPath = real_path;
			if (m_CurrentPath[m_CurrentPath.size()-1] == PATH_SEP)
			{
				m_CurrentPath = m_CurrentPath.substr(0, m_CurrentPath.size() - 1);
			}
			m_CurrentPath_Decomposition = splitStringToVector(m_CurrentPath, PATH_SEP, false);
#if defined(LINUX) or defined(APPLE)
			m_CurrentPath_Decomposition.insert(m_CurrentPath_Decomposition.begin(), std::string(1u, PATH_SEP));
#endif
			if (m_CurrentPath_Decomposition.size()>0)
            {
#ifdef WIN32
                s_fs_root = m_CurrentPath_Decomposition[0];
#endif
            }
		}

        closedir(dir);
    }
}

bool FileDialog::CreateDir(const std::string& vPath)
{
	bool res = false;

	if (vPath.size())
	{
		std::string path = m_CurrentPath + PATH_SEP + vPath;

		res = CreateDirectoryIfNotExist(path);
	}

	return res;
}

void FileDialog::ComposeNewPath(std::vector<std::string>::iterator vIter)
{
    m_CurrentPath = "";

    while (true)
    {
		if (!m_CurrentPath.empty())
		{
#ifdef WIN32
			m_CurrentPath = *vIter + PATH_SEP + m_CurrentPath;
#elif defined(LINUX) or defined(APPLE)
			if (*vIter == s_fs_root)
			{
				m_CurrentPath = *vIter + m_CurrentPath;
			}
			else
			{
				m_CurrentPath = *vIter + PATH_SEP + m_CurrentPath;
			}
#endif
		}
		else
		{
			m_CurrentPath = *vIter;
		}

        if (vIter == m_CurrentPath_Decomposition.begin())
        {
#if defined(LINUX) or defined(APPLE)
            if (m_CurrentPath[0] != PATH_SEP)
                m_CurrentPath = PATH_SEP + m_CurrentPath;
#endif
            break;
        }

        --vIter;
    }
}

void FileDialog::GetDrives()
{
	auto res = GetDrivesList();
	if (res.size() > 0)
	{
		m_CurrentPath = "";
		m_CurrentPath_Decomposition.clear();
		m_FileList.clear();
		for (auto it = res.begin(); it != res.end(); ++it)
		{
			FileInfoStruct infos;
			infos.fileName = *it;
			infos.type = 'd';

			if (infos.fileName.size() > 0)
			{
				m_FileList.push_back(infos);
			}
		}
		m_ShowDrives = true;
	}
}

void FileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
	const std::string& vPath, const std::string& vDefaultFileName,
	std::function<void(std::string, bool*)> vOptionsPane, size_t vOptionsPaneWidth, const std::string& vUserString)
{
	if (m_ShowDialog) // si deja ouvert on ne fou pas la merde en voulant en ecrire une autre
		return;

	dlg_key = vKey;
	dlg_name = std::string(vName);
	dlg_filters = vFilters;
	dlg_path = vPath;
	dlg_defaultFileName = vDefaultFileName;
	dlg_optionsPane = vOptionsPane;
	dlg_userString = vUserString;
	dlg_optionsPaneWidth = vOptionsPaneWidth;

	dlg_defaultExt = "";

	m_ShowDialog = true;
}

void FileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
	const std::string& vFilePathName,
	std::function<void(std::string, bool*)> vOptionsPane, size_t vOptionsPaneWidth, const std::string& vUserString)
{
	if (m_ShowDialog) // si deja ouvert on ne fou pas la merde en voulant en ecrire une autre
		return;

	dlg_key = vKey;
	dlg_name = std::string(vName);
	dlg_filters = vFilters;

	auto ps = ParsePathFileName(vFilePathName);
	if (ps.isOk)
	{
		dlg_path = ps.path;
		dlg_defaultFileName = vFilePathName;
		dlg_defaultExt = "." + ps.ext;
	}
	else
	{
		dlg_path = ".";
		dlg_defaultFileName = "";
		dlg_defaultExt = "";
	}

	dlg_optionsPane = vOptionsPane;
	dlg_userString = vUserString;
	dlg_optionsPaneWidth = vOptionsPaneWidth;

	m_ShowDialog = true;
}

void FileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
	const std::string& vFilePathName, const std::string& vUserString)
{
	if (m_ShowDialog) // si deja ouvert on ne fou pas la merde en voulant en ecrire une autre
		return;

	dlg_key = vKey;
	dlg_name = std::string(vName);
	dlg_filters = vFilters;

	auto ps = ParsePathFileName(vFilePathName);
	if (ps.isOk)
	{
		dlg_path = ps.path;
		dlg_defaultFileName = vFilePathName;
		dlg_defaultExt = "." + ps.ext;
	}
	else
	{
		dlg_path = ".";
		dlg_defaultFileName = "";
		dlg_defaultExt = "";
	}

	dlg_optionsPane = 0;
	dlg_userString = vUserString;
	dlg_optionsPaneWidth = 0;

	m_ShowDialog = true;
}

void FileDialog::OpenDialog(const std::string& vKey, const char* vName, const char* vFilters,
	const std::string& vPath, const std::string& vDefaultFileName, const std::string& vUserString)
{
	if (m_ShowDialog) // si deja ouvert on ne fou pas la merde en voulant en ecrire une autre
		return;

	dlg_key = vKey;
	dlg_name = std::string(vName);
	dlg_filters = vFilters;
	dlg_path = vPath;
	dlg_defaultFileName = vDefaultFileName;
	dlg_optionsPane = 0;
	dlg_userString = vUserString;
	dlg_optionsPaneWidth = 0;

	dlg_defaultExt = "";

	m_ShowDialog = true;
}

void FileDialog::CloseDialog(const std::string& vKey)
{
	if (dlg_key == vKey)
	{
		dlg_key.clear();
		m_ShowDialog = false;
	}	
}

void FileDialog::SetPath(const std::string& vPath)
{
	m_ShowDrives = false;
	m_CurrentPath = vPath;
	m_FileList.clear();
	m_CurrentPath_Decomposition.clear();
	ScanDir(m_CurrentPath);
}

bool FileDialog::Render(const std::string& vKey, ImVec2 geometry)
{
	std::string name = dlg_name + "##" + dlg_key;

	if (m_ShowDialog) {
        ImGui::OpenPopup(name.c_str());
		// m_ShowDialog = false;
	}

	if (dlg_key == vKey)
	{
		bool res = false;

		if (m_Name != name)
		{
			m_FileList.clear();
			m_CurrentPath_Decomposition.clear();
		}

		IsOk = false;

        if (ImGui::BeginPopupModal(name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{

			m_Name = name;

			m_AnyWindowsHovered |= ImGui::IsWindowHovered();

			if (dlg_path.size() == 0) dlg_path = ".";

			if (m_FileList.size() == 0 && !m_ShowDrives)
			{
				replaceString(dlg_defaultFileName, dlg_path, ""); // local path

				if (dlg_defaultFileName.size() > 0)
				{
					ResetBuffer(FileNameBuffer);
					AppendToBuffer(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, dlg_defaultFileName);
					m_SelectedFileName = dlg_defaultFileName;

					if (dlg_defaultExt.size() > 0)
					{
						m_SelectedExt = dlg_defaultExt;

						FileDialog::FilterIndex = 0;
						size_t size = 0;
						const char* p = dlg_filters;       // FIXME-OPT: Avoid computing this, or at least only when combo is open
						while (*p)
						{
							size += strlen(p) + 1;
							p += size;
						}
						int idx = 0;
						auto arr = splitStringToVector(std::string(dlg_filters, size), '\0', false);
						for (auto it = arr.begin(); it != arr.end(); ++it)
						{
							if (m_SelectedExt == *it)
							{
								FileDialog::FilterIndex = idx;
								break;
							}
							idx++;
						}
					}
				}

				ScanDir(dlg_path);
			}

            if (ImGuiToolkit::ButtonIcon(3, 8))
			// if (ImGui::Button("+")) // new Folder
			{
				if (!m_CreateDirectoryMode)
				{
					m_CreateDirectoryMode = true;
					ResetBuffer(DirectoryNameBuffer);
				}
			}

			if (m_CreateDirectoryMode)
			{
				ImGui::SameLine();

				ImGui::PushItemWidth(100.0f);
				ImGui::InputText("##DirectoryFileName", DirectoryNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER);
				ImGui::PopItemWidth();

				ImGui::SameLine();

				if (ImGui::Button("OK"))
				{
					std::string newDir = std::string(DirectoryNameBuffer);
					if (CreateDir(newDir))
					{
						SetPath(m_CurrentPath + PATH_SEP + newDir);
					}

					m_CreateDirectoryMode = false;
				}

				ImGui::SameLine();

				if (ImGui::Button("Cancel"))
				{
					m_CreateDirectoryMode = false;
				}
			}

			ImGui::SameLine();

			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

			ImGui::SameLine();

			// if (ImGui::Button("R")) // HOME
			if (ImGuiToolkit::ButtonIcon(2,10))
			{
				SetPath("."); // Go Home
			}

#ifdef WIN32
			bool drivesClick = false;

			ImGui::SameLine();

			if (ImGui::Button("Drives"))
			{
				drivesClick = true;
			}
#endif

			ImGui::SameLine();

			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

			// show current path
			bool pathClick = false;
			if (m_CurrentPath_Decomposition.size() > 0)
			{
				ImGui::SameLine();
				for (std::vector<std::string>::iterator itPathDecomp = m_CurrentPath_Decomposition.begin();
					itPathDecomp != m_CurrentPath_Decomposition.end(); ++itPathDecomp)
				{
					if (itPathDecomp != m_CurrentPath_Decomposition.begin())
						ImGui::SameLine();
					if (ImGui::Button((*itPathDecomp).c_str()))
					{
						ComposeNewPath(itPathDecomp);
						pathClick = true;
						break;
					}
				}
			}


			// search field
			ImGuiToolkit::Icon(14, 12);
			ImGui::SameLine();
			if (ImGui::InputText("##ImGuiFileDialogSearchFiled", SearchBuffer, MAX_FILE_DIALOG_NAME_BUFFER))
			{
				searchTag = SearchBuffer;
			}
			// reset search
			ImGui::SameLine();
			if (ImGuiToolkit::ButtonIcon(13, 2))
			{
				ResetBuffer(SearchBuffer);
				searchTag.clear();
			}


            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
			ImVec2 dsize = geometry - ImVec2((float)dlg_optionsPaneWidth, 180.0f);
			ImGui::BeginChild("##FileDialog_FileList", dsize);

			for (std::vector<FileInfoStruct>::iterator it = m_FileList.begin(); it != m_FileList.end(); ++it)
			{
				const FileInfoStruct& infos = *it;

				bool show = true;

				std::string str;
				if (infos.type == 'd') str = "[Dir]  " + infos.fileName;
				if (infos.type == 'l') str = "[Link] " + infos.fileName;
				if (infos.type == 'f') str = "[File] " + infos.fileName;
				if (infos.type == 'f' && m_SelectedExt.size() > 0 && (infos.ext != m_SelectedExt && m_SelectedExt != ".*"))
				{
					show = false;
				}
				if (searchTag.size() > 0 && infos.fileName.find(searchTag) == std::string::npos)
				{
					show = false;
				}
				if (show == true)
				{
				    ImVec4 c;
				    bool showColor = false;
					if (infos.type == 'd') { ImGuiToolkit::Icon(5, 8); showColor=true; c = ImVec4(1.0, 1.0, 0.0, 1.0); };
					if (infos.type == 'l') { ImGuiToolkit::Icon(18, 13); showColor=true; c = ImVec4(1.0, 1.0, 1.0, 0.6); };
					if (infos.type == 'f') { 
						showColor = GetFilterColor(infos.ext, &c); 
						if (showColor) ImGuiToolkit::Icon(14, 7); else ImGuiToolkit::Icon(8, 8); 
					}

				    if (showColor) 
				        ImGui::PushStyleColor(ImGuiCol_Text, c);

					ImGui::SameLine();

					if (ImGui::Selectable(infos.fileName.c_str(), (infos.fileName == m_SelectedFileName)))
					{
						if (infos.type == 'd')
						{
							if ((*it).fileName == "..")
							{
								if (m_CurrentPath_Decomposition.size() > 1)
								{
									ComposeNewPath(m_CurrentPath_Decomposition.end() - 2);
									pathClick = true;
								}
							}
							else
							{
								std::string newPath;

								if (m_ShowDrives)
								{
									newPath = infos.fileName + PATH_SEP;
								}
								else
								{
#ifdef LINUX
									if (s_fs_root == m_CurrentPath)
									{
										newPath = m_CurrentPath + infos.fileName;
									}
									else
									{
#endif
										newPath = m_CurrentPath + PATH_SEP + infos.fileName;
#ifdef LINUX
									}
#endif
								}

								if (IsDirectoryExist(newPath))
								{
									if (m_ShowDrives)
									{
										m_CurrentPath = infos.fileName;
										s_fs_root = m_CurrentPath;
									}
									else
									{
										m_CurrentPath = newPath;
									}
									pathClick = true;
								}
							}
						}
						else
						{
							m_SelectedFileName = infos.fileName;
							ResetBuffer(FileNameBuffer);
							AppendToBuffer(FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER, m_SelectedFileName);
						}
						if (showColor)
                        	ImGui::PopStyleColor();
						break;
					}

                    if (showColor)
                        ImGui::PopStyleColor();
                }
			}

			// changement de repertoire
			if (pathClick == true)
			{
				SetPath(m_CurrentPath);
			}

#ifdef WIN32
			if (drivesClick == true)
			{
				GetDrives();
			}
#endif

			ImGui::EndChild();
			ImGui::PopFont();

			bool _CanWeContinue = true;

			if (dlg_optionsPane)
			{
				ImGui::SameLine();

				dsize.x = (float)dlg_optionsPaneWidth;

				ImGui::BeginChild("##FileTypes", dsize);

				dlg_optionsPane(m_SelectedExt, &_CanWeContinue);

				ImGui::EndChild();
			}

			ImGui::Text("File Name : ");

			ImGui::SameLine();

			float width = ImGui::GetContentRegionAvail().x;
			if (dlg_filters) width -= 120.0f;
			ImGui::PushItemWidth(width);
			ImGui::InputText("##FileName", FileNameBuffer, MAX_FILE_DIALOG_NAME_BUFFER);
			ImGui::PopItemWidth();

			if (dlg_filters)
			{
				ImGui::SameLine();

				ImGui::PushItemWidth(100.0f);
				bool comboClick = ImGui::Combo("##Filters", &FilterIndex, dlg_filters) || m_SelectedExt.size() == 0;
				ImGui::PopItemWidth();
				if (comboClick == true)
				{
					int itemIdx = 0;
					const char* p = dlg_filters;
					while (*p)
					{
						if (FilterIndex == itemIdx)
						{
							m_SelectedExt = std::string(p);
							break;
						}
						p += strlen(p) + 1;
						itemIdx++;
					}
				}
			}

			//width = ImGui::GetContentRegionAvail().x;
			float button_width = 200.f;
			if (ImGui::Button(" Cancel ", ImVec2(button_width, 0)))
			{
				IsOk = false;
				res = true;
			}

			if (_CanWeContinue)
			{
				ImGui::SameLine();
				float w = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;
				ImGui::Dummy(ImVec2(w - button_width, 0)); ImGui::SameLine(); // right align
				if (ImGui::Button(" Ok ", ImVec2(button_width, 0)))
				{
					if ('\0' != FileNameBuffer[0])
					{
						IsOk = true;
						res = true;
					}
				}
        		ImGui::SetItemDefaultFocus();

			}

			if(res) {
        		ImGui::CloseCurrentPopup();
				m_ShowDialog = false;
			}
		
			ImGui::EndPopup();

		}

		// ImGui::End();

		if (res == true)
		{
			m_FileList.clear();
		}

		return res;
	}

	return false;
}

std::string FileDialog::GetFilepathName()
{
    std::string  result = m_CurrentPath;

    if (s_fs_root != result)
    {
        result += PATH_SEP;
    }

    result += GetCurrentFileName();

    return result;
}

std::string FileDialog::GetCurrentPath()
{
    return m_CurrentPath;
}

std::string FileDialog::GetCurrentFileName()
{
    std::string result = FileNameBuffer;

	// size_t lastPoint = result.find_last_of('.');
	// if (lastPoint != std::string::npos)
	// {
	// 	result = result.substr(0, lastPoint);
	// }

	// result += m_SelectedExt;

    return result;
}

std::string FileDialog::GetCurrentFilter()
{
    return m_SelectedExt;
}

std::string FileDialog::GetUserString()
{
	return dlg_userString;
}

void FileDialog::SetFilterColor(const std::string &vFilter, ImVec4 vColor)
{
    m_FilterColor[vFilter] = vColor;
}

bool FileDialog::GetFilterColor(const std::string &vFilter, ImVec4 *vColor)
{
    if (vColor)
    {
        if (m_FilterColor.find(vFilter) != m_FilterColor.end()) // found
        {
            *vColor = m_FilterColor[vFilter];
            return true;
        }
    }
    return false;;
}

void FileDialog::ClearFilterColor()
{
    m_FilterColor.clear();
}



// template info panel
inline void InfosPane(std::string vFilter, bool *vCantContinue)
// if vCantContinue is false, the user cant validate the dialog
{
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "Infos Pane");

    ImGui::Text("Selected Filter : %s", vFilter.c_str());

    // File Manager information pannel
    bool canValidateDialog = false;
    ImGui::Checkbox("if not checked you cant validate the dialog", &canValidateDialog);

    if (vCantContinue)
        *vCantContinue = canValidateDialog;
}

inline void TextInfosPane(const std::string &vFilter, bool *vCantContinue) // if vCantContinue is false, the user cant validate the dialog
{
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "Text");

    static std::string filepathcurrent;
    static std::string text;

    // if different file selected
    if ( filepathcurrent.compare(FileDialog::Instance()->GetFilepathName()) != 0)
    {
        filepathcurrent = FileDialog::Instance()->GetFilepathName();

        // fill text with file content
        std::ifstream textfilestream(filepathcurrent, std::ios::in);
        if(textfilestream.is_open()){
            std::stringstream sstr;
            sstr << textfilestream.rdbuf();
            text = sstr.str();
            textfilestream.close();
        }
        else
        {
            // empty text
            text = "";
        }

    }

    // show text
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 340);
    ImGui::Text("%s", text.c_str());

    // release Ok button if text is not empty
    if (vCantContinue)
        *vCantContinue = text.size() > 0;
}

inline void ImageInfosPane(const std::string &vFilter, bool *vCantContinue) // if vCantContinue is false, the user cant validate the dialog
{
    // opengl texture
    static GLuint tex = 0;
    static std::string filepathcurrent;
    static std::string message = "Please select an image (" + vFilter + ").";
    static unsigned char* img = NULL;
    static ImVec2 image_size(330, 330);

    // generate texture (once) & clear
    if (tex == 0) {
        glGenTextures(1, &tex);
        glBindTexture( GL_TEXTURE_2D, tex);
        unsigned char clearColor[4] = {0};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
    }

    // if different file selected
    if ( filepathcurrent.compare(FileDialog::Instance()->GetFilepathName()) != 0)
    {
        // remember path
        filepathcurrent = FileDialog::Instance()->GetFilepathName();

        // prepare texture
        glBindTexture( GL_TEXTURE_2D, tex);

        // load image
        int w, h, n;
        img = stbi_load(filepathcurrent.c_str(), &w, &h, &n, 4);
        if (img != NULL) {

            // apply img to texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

            // adjust image display aspect ratio
            image_size.y =  image_size.x * static_cast<float>(h) / static_cast<float>(w);

            // free loaded image
            stbi_image_free(img);

            message = FileDialog::Instance()->GetCurrentFileName() + "(" + std::to_string(w) + "x" + std::to_string(h) + ")";
        }
        else {
            // clear image
            unsigned char clearColor[4] = {0};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);

            message = "Please select an image (" + vFilter + ").";
        }

    }

    // draw text and image
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "%s", message.c_str());
    ImGui::Image((void*)(intptr_t)tex, image_size);

    // release Ok button if image is not null
    if (vCantContinue)
        *vCantContinue = img!=NULL;
}

void FileDialog::RenderCurrent()
{
    ImVec2 geometry(1200.0, 640.0);

    if ( !currentFileDialog.empty() && FileDialog::Instance()->Render(currentFileDialog.c_str(), geometry)) {
        if (FileDialog::Instance()->IsOk == true) {
            std::string filePathNameText = FileDialog::Instance()->GetFilepathName();
            // done
            currentFileDialog = "";
        }
        FileDialog::Instance()->CloseDialog(currentFileDialog.c_str());
    }
}


void FileDialog::SetCurrentOpenText()
{
    currentFileDialog = "ChooseFileText";

    FileDialog::Instance()->ClearFilterColor();
    FileDialog::Instance()->SetFilterColor(".cpp", ImVec4(1,1,0,0.5));
    FileDialog::Instance()->SetFilterColor(".h", ImVec4(0,1,0,0.5));
    FileDialog::Instance()->SetFilterColor(".hpp", ImVec4(0,0,1,0.5));

    FileDialog::Instance()->OpenDialog(currentFileDialog.c_str(), "Open Text File", ".cpp\0.h\0.hpp\0\0", ".", "",
            std::bind(&TextInfosPane, std::placeholders::_1, std::placeholders::_2), 350, "Text info");

}

void FileDialog::SetCurrentOpenImage()
{
    currentFileDialog = "ChooseFileImage";

    FileDialog::Instance()->ClearFilterColor();
    FileDialog::Instance()->SetFilterColor(".png", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".jpg", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".gif", ImVec4(0,1,1,1.0));

    FileDialog::Instance()->OpenDialog(currentFileDialog.c_str(), "Open Image File", ".*\0.png\0.jpg\0.gif\0\0", ".", "",
            std::bind(&ImageInfosPane, std::placeholders::_1, std::placeholders::_2), 350, "Image info");

}

void FileDialog::SetCurrentOpenMedia()
{
    currentFileDialog = "ChooseFileMedia";

    FileDialog::Instance()->ClearFilterColor();
    FileDialog::Instance()->SetFilterColor(".mp4", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".avi", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".mov", ImVec4(0,1,1,1.0));

    FileDialog::Instance()->OpenDialog(currentFileDialog.c_str(), "Open Media File", ".*\0.mp4\0.avi\0.mov\0\0", ".", "", "Media");

}

