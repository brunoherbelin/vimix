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

#ifndef EXPORTER_H
#define EXPORTER_H

#include <string>
#include <list>
#include <atomic>
#include <future>

#include "Playlist.h"

/**
 * @brief Copies a playlist's files to a destination folder in a background thread.
 *
 * Follows the same start / stop / finished / success / progress pattern as
 * the Transcoder class so it can be driven from the UI in exactly the same way.
 */
class Exporter
{
public:
    /**
     * @brief Construct an Exporter for the given playlist and destination.
     * @param playlist      Playlist whose referenced files are to be exported.
     * @param destination   Absolute path of the destination folder.
     * @param copy_media    If true, also copy media files; otherwise only session files.
     * @param compress      If true (and VIMIX_USE_MINIZ is defined), pack all copied
     *                      files into @p archive_name .zip and remove the flat copies.
     * @param archive_name  Stem of the ZIP file (e.g. "myplaylist" → "myplaylist.zip").
     *                      Falls back to the destination folder name when empty.
     */
    Exporter(const Playlist &playlist, const std::string &destination, bool copy_media,
             bool compress = false, const std::string &archive_name = "");

    /**
     * @brief Destroy the Exporter.
     *
     * If a copy is still in progress, it is cancelled and the destructor
     * blocks until the background thread exits.
     */
    ~Exporter();

    /**
     * @brief Start copying files in a background thread.
     * @return true on success, false if already started or destination invalid.
     */
    bool start();

    /**
     * @brief Signal the background thread to stop at the next file boundary.
     *
     * Does not block; call finished() to know when the thread has exited.
     */
    void stop();

    /**
     * @brief Check whether the copy has completed (success or cancelled).
     * @return true once the background thread has exited.
     */
    bool finished() const;

    /**
     * @brief Check whether all files were copied without cancellation.
     * @return true only when finished() and not cancelled.
     */
    bool success() const;

    /**
     * @brief Copy progress in [0, 1].
     * @return Fraction of files already processed (0 before start, 1 when done).
     */
    float progress() const;

    /**
     * @brief Error description, if any.
     * @return Human-readable message; empty string if no error occurred.
     */
    const std::string &error() const { return error_message_; }

    /**
     * @brief Total number of files to copy.
     */
    int count() const { return total_; }

#ifdef VIMIX_USE_MINIZ
    /**
     * @brief True while the ZIP archive is being built (after all copies finish).
     *
     * When compress is active, progress() reports 0–0.5 during the copy phase
     * and 0.5–1.0 during the compression phase.  Inspect this flag to show a
     * "compressing…" overlay on the progress bar.
     */
    bool compressing() const { return compressing_.load(); }
#endif

private:
    std::list<std::string> files_;
    std::string            destination_;
    std::string            error_message_;

    int                   total_;
    std::atomic<int>      done_;
    std::atomic<bool>     cancel_;
    std::atomic<bool>     finished_;
    std::atomic<bool>     success_;

    std::future<void>     task_;

#ifdef VIMIX_USE_MINIZ
    bool              compress_;
    std::string       archive_name_;
    std::atomic<bool> compressing_;
    std::atomic<int>  compress_done_;
#endif
};

#endif // EXPORTER_H
