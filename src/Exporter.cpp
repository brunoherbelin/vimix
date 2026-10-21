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

#ifdef VIMIX_USE_MINIZ
#include <miniz.h>
#endif

#include "Log.h"
#include "defines.h"
#include "Playlist.h"
#include "Toolkit/BaseToolkit.h"
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

        if (type == "SplitMediaSource") {
            // <SplitFiles><file relative="...">absolute_path</file>...</SplitFiles>
            XMLElement *listNode = sourceNode->FirstChildElement("SplitFiles");
            for (XMLElement *fileNode = listNode ? listNode->FirstChildElement("file") : nullptr;
                 fileNode != nullptr; fileNode = fileNode->NextSiblingElement("file")) {
                const char *text = fileNode->GetText();
                if (text && files_set.count(std::string(text))) {
                    const std::string fname = SystemToolkit::filename(text);
                    const std::string newpath = dest + PATH_SEP + fname;
                    fileNode->SetText(newpath.c_str());
                    fileNode->SetAttribute("relative", fname.c_str());
                }
            }
        }
        else if (type == "MediaSource" || type == "AudioSource") {
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
        else if (type == "CloneSource" || type == "ShaderSource") {
            // <Filter filename="absolute_path" ...> — external .glsl shader file
            XMLElement *filterNode = sourceNode->FirstChildElement("Filter");
            if (filterNode) {
                const char *filename = nullptr;
                if (filterNode->QueryStringAttribute("filename", &filename) == XML_SUCCESS
                    && filename != nullptr && filename[0] != '\0'
                    && files_set.count(std::string(filename))) {
                    const std::string fname = SystemToolkit::filename(filename);
                    filterNode->SetAttribute("filename", (dest + PATH_SEP + fname).c_str());
                }
            }
        }
    }
}

// Load a copied .mix file, patch all embedded file references, and save it back.
// If discard_versions is true, the entire <Snapshots> section is removed.
static void patchSessionFile(const std::string &dst_path,
                              const std::string &dest,
                              const std::set<std::string> &files_set,
                              const std::set<std::string> &dirs_set,
                              bool discard_versions)
{
    XMLDocument xmlDoc;
    if (xmlDoc.LoadFile(dst_path.c_str()) != XML_SUCCESS) return;

    XMLElement *sessionNode = xmlDoc.FirstChildElement("Session");
    if (!sessionNode) return;

    patchSessionNode(sessionNode, dest, files_set, dirs_set);

    if (discard_versions) {
        XMLElement *snapshots = xmlDoc.FirstChildElement("Snapshots");
        if (snapshots)
            xmlDoc.DeleteChild(snapshots);
    }

    xmlDoc.SaveFile(dst_path.c_str());
}

// Rewrite every <path> of a playlist file to point to the same filename in dest.
static void patchPlaylistFile(const std::string &playlist_path, const std::string &dest)
{
    XMLDocument xmlDoc;
    if (xmlDoc.LoadFile(playlist_path.c_str()) != XML_SUCCESS) return;

    XMLElement *pRoot = xmlDoc.FirstChildElement("vimixplaylist");
    if (!pRoot) return;

    for (XMLElement *pathNode = pRoot->FirstChildElement("path");
         pathNode != nullptr; pathNode = pathNode->NextSiblingElement("path")) {
        const char *text = pathNode->GetText();
        if (text) {
            const std::string fname = SystemToolkit::filename(text);
            pathNode->SetText((dest + PATH_SEP + fname).c_str());
            pathNode->SetAttribute("relative", fname.c_str());
        }
    }

    xmlDoc.SaveFile(playlist_path.c_str());
}

Exporter::Exporter(const Playlist &playlist, const std::string &destination, bool copy_media,
                   bool discard_versions, bool compress, const std::string &archive_name,
                   bool write_playlist)
    : destination_(destination)
    , archive_name_(archive_name)
    , source_playlist_(playlist.filename())
    , discard_versions_(discard_versions)
    , write_playlist_(write_playlist)
    , done_(0)
    , cancel_(false)
    , finished_(false)
    , success_(false)
#ifdef VIMIX_USE_MINIZ
    , compress_(compress)
    , compressing_(false)
    , compress_done_(0)
#endif
{
#ifndef VIMIX_USE_MINIZ
    (void)compress;
#endif
    const std::list<std::string> all_files = playlist.paths();
    for (const auto &f : all_files) {
        if (copy_media || SystemToolkit::has_extension(f, VIMIX_FILE_EXT))
            files_.push_back(f);
    }
    total_ = (int)files_.size();

    // top-level sessions of the playlist, in order, to list in the exported playlist file
    for (size_t i = 0; i < playlist.size(); ++i) {
        if (SystemToolkit::has_extension(playlist.at(i), VIMIX_FILE_EXT))
            sessions_.push_back(playlist.at(i));
    }
}

// Name used for the exported playlist and ZIP files: archive_name_ or
// fall back to the last path component of the destination folder.
std::string Exporter::exportName() const
{
    std::string name = archive_name_;
    if (name.empty()) {
        std::string dest_copy = destination_;
        if (!dest_copy.empty() && dest_copy.back() == PATH_SEP)
            dest_copy.pop_back();
        name = SystemToolkit::filename(dest_copy);
    }
    if (name.empty())
        name = "export";
    return name;
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

    if (total_ == 0) {
        error_message_ = "No files to export.";
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

    // error if one of the dirs_set is the same as the destination (can't patch sequence references in that case)
    if (dirs_set.count(destination_) > 0) {
        error_message_ = "Cannot export: destination folder is the same as a the session or a source folder.";
        success_ = false;
        finished_ = true;
        return false;
    }

    // Copies are made directly in the destination folder, unless compressing:
    // then they are staged in a hidden temporary subfolder of the destination
    // (same file system, so no RAM-backed /tmp and an instant move) and only
    // the ZIP file is moved to the destination after compression.
    std::string workdir = destination_;
#ifdef VIMIX_USE_MINIZ
    if (compress_) {
        workdir = destination_ + PATH_SEP + ".vimix_export_" + std::to_string(BaseToolkit::uniqueId());
        std::error_code ec;
        if (!std::filesystem::create_directory(workdir, ec)) {
            error_message_ = "Cannot create temporary folder: " + workdir;
            Log::Warning("Exporter: %s", error_message_.c_str());
            return false;
        }
    }
#endif

    task_ = std::async(std::launch::async, [this, files_set, dirs_set, workdir]() {
        for (const auto &src : files_) {
            if (cancel_) break;
            const std::string dst = workdir + PATH_SEP + SystemToolkit::filename(src);
            try {
                std::filesystem::copy_file(src, dst,
                    std::filesystem::copy_options::overwrite_existing);
                // After copying a session file, rewrite its internal paths so they
                // point to files in the export folder instead of their original locations.
                if (SystemToolkit::has_extension(src, VIMIX_FILE_EXT))
                    patchSessionFile(dst, destination_, files_set, dirs_set, discard_versions_);
            }
            catch (const std::filesystem::filesystem_error &e) {
                Log::Warning("Exporter: Could not copy '%s': %s", src.c_str(), e.what());
            }
            ++done_;
        }
        success_ = !cancel_ && (done_ == total_);

        // names of all files written in workdir (copies + playlist)
        std::list<std::string> exported_files;
        for (const auto &src : files_)
            exported_files.push_back(SystemToolkit::filename(src));

        // Write a playlist file listing the exported sessions (pointing to their copies)
        if (success_ && write_playlist_) {
            Playlist exported;
            for (const auto &src : sessions_) {
                const std::string dst = workdir + PATH_SEP + SystemToolkit::filename(src);
                if (SystemToolkit::file_exists(dst))
                    exported.add(dst);
            }
            std::string playlist_name = exportName() + "." + VIMIX_PLAYLIST_FILE_EXT;
            // never overwrite the original playlist file (export into its own folder)
            if (workdir + PATH_SEP + playlist_name == source_playlist_)
                playlist_name = exportName() + "_export." + VIMIX_PLAYLIST_FILE_EXT;
            exported.saveAs(workdir + PATH_SEP + playlist_name);
            if (exported.filename().empty())
                Log::Warning("Exporter: Could not write playlist '%s'.", playlist_name.c_str());
            else {
                // staged playlist should reference the destination, like the sessions
                if (workdir != destination_)
                    patchPlaylistFile(exported.filename(), destination_);
                exported_files.push_back(playlist_name);
            }
        }

#ifdef VIMIX_USE_MINIZ
        if (success_ && compress_) {
            compressing_ = true;

            const std::string zip_name = exportName() + ".zip";
            const std::string zip_path = workdir + PATH_SEP + zip_name;

            mz_zip_archive zip;
            memset(&zip, 0, sizeof(zip));

            if (mz_zip_writer_init_file(&zip, zip_path.c_str(), 0)) {
                bool zip_ok = true;
                for (const auto &entry_name : exported_files) {
                    if (cancel_) { zip_ok = false; break; }
                    const std::string dst = workdir + PATH_SEP + entry_name;
                    if (!mz_zip_writer_add_file(&zip, entry_name.c_str(), dst.c_str(),
                                                nullptr, 0, MZ_BEST_COMPRESSION))
                        Log::Warning("Exporter: Could not add '%s' to ZIP.", entry_name.c_str());
                    if (compress_done_ < total_)
                        ++compress_done_;
                }
                if (zip_ok) {
                    mz_zip_writer_finalize_archive(&zip);
                    mz_zip_writer_end(&zip);
                    // Move the archive to destination
                    const std::string target = destination_ + PATH_SEP + zip_name;
                    std::error_code ec;
                    std::filesystem::rename(zip_path, target, ec);
                    if (!ec)
                        Log::Info("Exporter: Created ZIP '%s'.", target.c_str());
                    else {
                        error_message_ = "Failed to move ZIP to " + target + ": " + ec.message();
                        Log::Warning("Exporter: %s", error_message_.c_str());
                        success_ = false;
                    }
                } else {
                    // Cancelled mid-zip: partial archive is discarded with workdir
                    mz_zip_writer_end(&zip);
                    success_ = false;
                }
            } else {
                error_message_ = "Failed to create ZIP: " + zip_path;
                Log::Warning("Exporter: %s", error_message_.c_str());
                success_ = false;
            }
            compressing_ = false;
        }
#endif

        // Remove the temporary folder with all staged copies
        if (workdir != destination_) {
            std::error_code ec;
            std::filesystem::remove_all(workdir, ec);
        }

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
#ifdef VIMIX_USE_MINIZ
    if (compress_)
        return (float)(done_.load() + compress_done_.load()) / (float)(2 * total_);
#endif
    return (float)done_.load() / (float)total_;
}
