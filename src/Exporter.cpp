/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2025 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include "Exporter.h"

#include <set>
#include <filesystem>
#include <tinyxml2.h>

#include "Log.h"
#include "defines.h"
#include "Toolkit/SystemToolkit.h"

using namespace tinyxml2;

// Forward declaration (patchSessionNode and patchSessionFile call each other for SessionSource)
static void patchSessionNode(XMLElement *sessionNode,
                              const std::string &dest,
                              const std::set<std::string> &files_set,
                              const std::set<std::string> &dirs_set);

// Rewrite every file reference inside a session XML node tree.
// Only references whose original absolute path is present in files_set
// (or whose sequence folder is present in dirs_set) are updated.
static void patchSessionNode(XMLElement *sessionNode,
                              const std::string &dest,
                              const std::set<std::string> &files_set,
                              const std::set<std::string> &dirs_set)
{
    for (XMLElement *sourceNode = sessionNode->FirstChildElement("Source");
         sourceNode != nullptr;
         sourceNode = sourceNode->NextSiblingElement("Source"))
    {
        const char *pType = sourceNode->Attribute("type");
        if (!pType) continue;
        const std::string type(pType);

        if (type == "MediaSource") {
            // <uri relative="...">absolute_path</uri>
            XMLElement *uriNode = sourceNode->FirstChildElement("uri");
            if (uriNode) {
                const char *text = uriNode->GetText();
                if (text && files_set.count(std::string(text))) {
                    const std::string fname = SystemToolkit::filename(text);
                    const std::string newpath = dest + PATH_SEP + fname;
                    uriNode->SetText(newpath.c_str());
                    uriNode->SetAttribute("relative", fname.c_str());
                }
            }
        }
        else if (type == "SessionSource") {
            // <path relative="...">absolute_path</path>
            XMLElement *pathNode = sourceNode->FirstChildElement("path");
            if (pathNode) {
                const char *text = pathNode->GetText();
                if (text && files_set.count(std::string(text))) {
                    const std::string fname = SystemToolkit::filename(text);
                    const std::string newpath = dest + PATH_SEP + fname;
                    pathNode->SetText(newpath.c_str());
                    pathNode->SetAttribute("relative", fname.c_str());
                }
            }
        }
        else if (type == "GroupSource") {
            // Inline session: recurse into its <Session> node
            XMLElement *innerSession = sourceNode->FirstChildElement("Session");
            if (innerSession)
                patchSessionNode(innerSession, dest, files_set, dirs_set);
        }
        else if (type == "MultiFileSource") {
            // <Sequence min=... max=... codec=... relative="...">printf_pattern</Sequence>
            // The individual frame files are identified via their parent directory (dirs_set).
            XMLElement *seqNode = sourceNode->FirstChildElement("Sequence");
            if (seqNode) {
                const char *text = seqNode->GetText();
                if (text) {
                    const std::string location(text);
                    const std::string seq_folder = SystemToolkit::path_filename(location);
                    if (dirs_set.count(seq_folder)) {
                        const std::string fname = SystemToolkit::filename(location);
                        const std::string newpath = dest + PATH_SEP + fname;
                        seqNode->SetText(newpath.c_str());
                        seqNode->SetAttribute("relative", fname.c_str());
                    }
                }
            }
        }
    }
}

// Load a copied .mix file, patch all embedded file references, and save it back.
static void patchSessionFile(const std::string &dst_path,
                              const std::string &dest,
                              const std::set<std::string> &files_set,
                              const std::set<std::string> &dirs_set)
{
    XMLDocument xmlDoc;
    if (xmlDoc.LoadFile(dst_path.c_str()) != XML_SUCCESS) return;

    XMLElement *sessionNode = xmlDoc.FirstChildElement("Session");
    if (!sessionNode) return;

    patchSessionNode(sessionNode, dest, files_set, dirs_set);

    xmlDoc.SaveFile(dst_path.c_str());
}


Exporter::Exporter(const std::list<std::string> &files, const std::string &destination)
    : files_(files)
    , destination_(destination)
    , total_((int)files.size())
    , done_(0)
    , cancel_(false)
    , finished_(false)
    , success_(false)
{
}

Exporter::~Exporter()
{
    // Signal background thread and wait for it to finish
    cancel_ = true;
    if (task_.valid())
        task_.wait();
}

bool Exporter::start()
{
    if (task_.valid()) {
        error_message_ = "Exporter already started.";
        return false;
    }

    if (!SystemToolkit::file_exists(destination_) &&
        !SystemToolkit::create_directory(destination_)) {
        error_message_ = "Cannot create destination folder: " + destination_;
        Log::Warning("Exporter: %s", error_message_.c_str());
        return false;
    }

    Log::Info("Exporter: Copying %d file(s) to '%s'.", total_, destination_.c_str());

    // Build lookup structures once so the background thread can patch session files.
    // files_set  : absolute source paths being copied (for MediaSource and SessionSource)
    // dirs_set   : parent directories of copied files (for MultiFileSource sequences)
    std::set<std::string> files_set(files_.begin(), files_.end());
    std::set<std::string> dirs_set;
    for (const auto &f : files_)
        dirs_set.insert(SystemToolkit::path_filename(f));

    task_ = std::async(std::launch::async, [this, files_set, dirs_set]() {
        for (const auto &src : files_) {
            if (cancel_) break;
            const std::string dst = destination_ + PATH_SEP + SystemToolkit::filename(src);
            try {
                std::filesystem::copy_file(src, dst,
                    std::filesystem::copy_options::overwrite_existing);
                // After copying a session file, rewrite its internal paths so they
                // point to files in the export folder instead of their original locations.
                if (SystemToolkit::has_extension(src, VIMIX_FILE_EXT))
                    patchSessionFile(dst, destination_, files_set, dirs_set);
            }
            catch (const std::filesystem::filesystem_error &e) {
                Log::Warning("Exporter: Could not copy '%s': %s", src.c_str(), e.what());
            }
            ++done_;
        }
        success_ = !cancel_ && (done_ == total_);
        finished_ = true;
    });

    return true;
}

void Exporter::stop()
{
    cancel_ = true;
}

bool Exporter::finished() const
{
    return finished_.load();
}

bool Exporter::success() const
{
    return success_.load();
}

float Exporter::progress() const
{
    if (total_ <= 0) return 0.f;
    return (float)done_.load() / (float)total_;
}
