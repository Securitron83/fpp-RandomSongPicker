/*
 * fpp-RandomSongPicker
 *
 * Commands:
 *
 * "Insert Random Item with History"
 *   Picks a random song from a source playlist, skipping recently played songs.
 *   Writes only the mainPlaylist section of the output playlist — Lead In/Out
 *   sections are preserved if the file already exists.
 *   When Start Playback is true, calls Player::InsertPlaylistAsNext with the source
 *   playlist at the exact absolute index of the picked item (leadIn count + main index).
 *   This mirrors FPP's own InsertRandomItemFromPlaylist but with a known position
 *   instead of the -2 random sentinel, so history is preserved. The command completes,
 *   FPP plays that one item as the next entry, then the calling playlist resumes —
 *   Lead Out still runs. FPP sees the item as a sequence entry so overlay /
 *   scrolling-text "now playing" shows the correct song title.
 *   History resets automatically once every song has been played.
 *
 *   Source Playlist  — playlist to pick from
 *   Output Playlist  — playlist to write (default: RandomPick)
 *   History Size     — songs to exclude before repeating (default: 10)
 *   Start Playback   — queue the output playlist as next item (default: true)
 *   Replace Previous — overwrite mainPlaylist vs. append after previous item (default: true)
 *
 * "Clear Playlist Main"
 *   Clears the mainPlaylist section of a playlist, leaving Lead In/Out intact.
 *
 * History is stored per source playlist at:
 *   /home/fpp/media/logs/song_picker_<source>_history.txt
 * Delete that file to reset the play history.
 */

// jsoncpp must come before FPP headers
#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

// FPP plugin API
#include <Plugin.h>
#include <Player.h>
#include <commands/Commands.h>
#include <log.h>

// Standard library
#include <algorithm>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const std::string PLAYLISTS_DIR = "/home/fpp/media/playlists/";
static const std::string HISTORY_DIR   = "/home/fpp/media/logs/";

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
class RandomSongPickerPlugin;

class InsertRandomWithHistoryCommand : public Command {
public:
    explicit InsertRandomWithHistoryCommand(RandomSongPickerPlugin* plugin);
    std::unique_ptr<Result> run(const std::vector<std::string>& a) override;
private:
    RandomSongPickerPlugin* m_plugin;
};

class ClearPlaylistMainCommand : public Command {
public:
    explicit ClearPlaylistMainCommand(RandomSongPickerPlugin* plugin);
    std::unique_ptr<Result> run(const std::vector<std::string>& a) override;
private:
    RandomSongPickerPlugin* m_plugin;
};

// ---------------------------------------------------------------------------
// Plugin
// ---------------------------------------------------------------------------
class RandomSongPickerPlugin : public FPPPlugin {
public:
    RandomSongPickerPlugin() : FPPPlugin("RandomSongPicker") {
        LogInfo(VB_PLUGIN, "RandomSongPicker: initialising\n");
        registerCommands();
    }

    ~RandomSongPickerPlugin() {
        unregisterCommands();
        LogInfo(VB_PLUGIN, "RandomSongPicker: shutdown\n");
    }

    std::string pickWithHistory(const std::string& sourceName,
                                const std::string& outputName,
                                int historySize,
                                bool startPlayback,
                                bool replacePrevious) {
        std::lock_guard<std::mutex> lk(m_mutex);

        // 1. Read source playlist
        Json::Value items;
        int leadInCount = 0;
        std::string err = readPlaylist(sourceName, items, &leadInCount);
        if (!err.empty()) return err;
        if (items.empty())
            return "Source playlist '" + sourceName + "' has no items in mainPlaylist";

        // 2. Load history
        std::vector<std::string> history = readHistory(sourceName);

        // 3. Build candidate list — items not in recent history
        std::vector<int> candidates;
        for (int i = 0; i < (int)items.size(); ++i) {
            std::string id = getIdentifier(items[i]);
            if (!id.empty() && std::find(history.begin(), history.end(), id) == history.end())
                candidates.push_back(i);
        }

        // If all songs are in history, reset and pick from full list
        if (candidates.empty()) {
            LogInfo(VB_GENERAL, "RandomSongPicker: all songs in history for '%s' — resetting\n",
                    sourceName.c_str());
            history.clear();
            for (int i = 0; i < (int)items.size(); ++i)
                candidates.push_back(i);
        }

        // 4. Random pick
        std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
        int picked = candidates[dist(m_rng)];
        const Json::Value& item = items[picked];
        std::string pickedId = getIdentifier(item);

        LogInfo(VB_GENERAL, "RandomSongPicker: picked '%s' from '%s'\n",
                pickedId.c_str(), sourceName.c_str());

        // 5. Update history (FIFO)
        history.push_back(pickedId);
        if ((int)history.size() > historySize)
            history.erase(history.begin(), history.begin() + ((int)history.size() - historySize));
        writeHistory(sourceName, history);

        // 6. Write output playlist
        err = writeOutputPlaylist(outputName, item, replacePrevious);
        if (!err.empty()) return err;

        // 7. Optionally queue the picked item as the next entry — insert the source playlist
        //    at the exact absolute index of the picked item (leadIn items + mainPlaylist index).
        //    This mirrors FPP's built-in InsertRandomItemFromPlaylist but with a known position
        //    instead of the -2 "pick random" sentinel, so history tracking is preserved.
        //    The command finishes cleanly; FPP's SwitchToInsertedPlaylist fires on the next
        //    tick, plays that one item from the source playlist, then the calling playlist
        //    resumes — Lead Out still runs.
        if (startPlayback) {
            int absolutePos = leadInCount + picked;
            Player::INSTANCE.InsertPlaylistAsNext(sourceName, absolutePos, absolutePos);
            LogInfo(VB_GENERAL, "RandomSongPicker: queued '%s' (pos %d) from '%s' as next item\n",
                    pickedId.c_str(), absolutePos, sourceName.c_str());
        }

        return "Picked: " + pickedId;
    }

    std::string clearPlaylistMain(const std::string& name) {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::string path = PLAYLISTS_DIR + name + ".json";

        std::ifstream fin(path);
        if (!fin.is_open()) return "Cannot open playlist: " + path;

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        if (!Json::parseFromStream(builder, fin, &root, &errs))
            return "Failed to parse '" + path + "': " + errs;
        fin.close();

        root["mainPlaylist"]                   = Json::Value(Json::arrayValue);
        root["playlistInfo"]["total_duration"] = 0.0;
        root["playlistInfo"]["total_items"]    = 0;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "  ";

        std::ofstream fout(path, std::ios::trunc);
        if (!fout.is_open()) return "Cannot write playlist: " + path;
        fout << Json::writeString(writer, root);

        LogInfo(VB_GENERAL, "RandomSongPicker: cleared mainPlaylist in '%s'\n", name.c_str());
        return "Cleared: " + name;
    }

private:
    std::mutex               m_mutex;
    std::mt19937             m_rng{std::random_device{}()};
    std::vector<std::string> m_registeredCommands;

    static std::string getIdentifier(const Json::Value& item) {
        if (item.isMember("sequenceName") && !item["sequenceName"].asString().empty())
            return item["sequenceName"].asString();
        if (item.isMember("mediaName") && !item["mediaName"].asString().empty())
            return item["mediaName"].asString();
        return {};
    }

    std::string readPlaylist(const std::string& name, Json::Value& items, int* leadInCount = nullptr) {
        std::string path = PLAYLISTS_DIR + name + ".json";
        std::ifstream f(path);
        if (!f.is_open()) return "Cannot open source playlist: " + path;
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        if (!Json::parseFromStream(builder, f, &root, &errs))
            return "Failed to parse '" + path + "': " + errs;
        if (!root.isMember("mainPlaylist") || !root["mainPlaylist"].isArray())
            return "'" + path + "' has no mainPlaylist array";
        items = root["mainPlaylist"];
        if (leadInCount)
            *leadInCount = (root.isMember("leadIn") && root["leadIn"].isArray())
                           ? (int)root["leadIn"].size() : 0;
        return {};
    }

    std::string historyPath(const std::string& sourceName) {
        return HISTORY_DIR + "song_picker_" + sourceName + "_history.txt";
    }

    std::vector<std::string> readHistory(const std::string& sourceName) {
        std::vector<std::string> hist;
        std::ifstream f(historyPath(sourceName));
        if (!f.is_open()) return hist;
        std::string line;
        while (std::getline(f, line))
            if (!line.empty()) hist.push_back(line);
        return hist;
    }

    void writeHistory(const std::string& sourceName, const std::vector<std::string>& history) {
        std::ofstream f(historyPath(sourceName), std::ios::trunc);
        if (!f.is_open()) {
            LogWarn(VB_GENERAL, "RandomSongPicker: cannot write history to %s\n",
                    historyPath(sourceName).c_str());
            return;
        }
        for (const auto& e : history) f << e << "\n";
    }

    std::string writeOutputPlaylist(const std::string& name, const Json::Value& item, bool replacePrevious) {
        std::string path = PLAYLISTS_DIR + name + ".json";
        double itemDuration = item.isMember("duration") ? item["duration"].asDouble() : 0.0;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "  ";

        // Append mode: read existing playlist and add item to the end
        if (!replacePrevious) {
            std::ifstream fin(path);
            if (fin.is_open()) {
                Json::Value existing;
                Json::CharReaderBuilder builder;
                std::string errs;
                if (Json::parseFromStream(builder, fin, &existing, &errs) &&
                    existing.isMember("mainPlaylist") && existing["mainPlaylist"].isArray()) {
                    existing["mainPlaylist"].append(item);
                    double prev = existing["playlistInfo"].isMember("total_duration")
                                      ? existing["playlistInfo"]["total_duration"].asDouble() : 0.0;
                    existing["playlistInfo"]["total_duration"] = prev + itemDuration;
                    existing["playlistInfo"]["total_items"]    = (int)existing["mainPlaylist"].size();
                    std::ofstream fout(path, std::ios::trunc);
                    if (!fout.is_open()) return "Cannot write output playlist: " + path;
                    fout << Json::writeString(writer, existing);
                    return {};
                }
            }
            // Fall through: no valid existing playlist — create fresh
        }

        // Replace mode: replace mainPlaylist only, preserve leadIn/leadOut
        Json::Value leadIn  = Json::Value(Json::arrayValue);
        Json::Value leadOut = Json::Value(Json::arrayValue);
        {
            std::ifstream fin(path);
            if (fin.is_open()) {
                Json::Value existing;
                Json::CharReaderBuilder builder;
                std::string errs;
                if (Json::parseFromStream(builder, fin, &existing, &errs)) {
                    if (existing.isMember("leadIn")  && existing["leadIn"].isArray())
                        leadIn  = existing["leadIn"];
                    if (existing.isMember("leadOut") && existing["leadOut"].isArray())
                        leadOut = existing["leadOut"];
                }
            }
        }

        Json::Value root;
        root["name"]      = name;
        root["version"]   = 3;
        root["repeat"]    = 0;
        root["loopCount"] = 0;
        root["empty"]     = false;
        root["desc"]      = "Generated by RandomSongPicker";
        root["random"]    = 0;
        root["leadIn"]    = leadIn;
        root["leadOut"]   = leadOut;
        root["mainPlaylist"].append(item);
        root["playlistInfo"]["total_duration"] = itemDuration;
        root["playlistInfo"]["total_items"]    = 1;

        std::ofstream f(path, std::ios::trunc);
        if (!f.is_open()) return "Cannot write output playlist: " + path;
        f << Json::writeString(writer, root);
        return {};
    }

    void registerCommands() {
        auto* cmd1 = new InsertRandomWithHistoryCommand(this);
        CommandManager::INSTANCE.addCommand(cmd1);
        auto* cmd2 = new ClearPlaylistMainCommand(this);
        CommandManager::INSTANCE.addCommand(cmd2);
        m_registeredCommands = {cmd1->name, cmd2->name};
        LogInfo(VB_PLUGIN, "RandomSongPicker: registered %zu commands\n",
                m_registeredCommands.size());
    }

    void unregisterCommands() {
        for (const auto& n : m_registeredCommands)
            CommandManager::INSTANCE.removeCommand(n);
        m_registeredCommands.clear();
    }
};

// ---------------------------------------------------------------------------
// Command constructor — mirrors FPP's built-in playlist command arg style
// ---------------------------------------------------------------------------
InsertRandomWithHistoryCommand::InsertRandomWithHistoryCommand(RandomSongPickerPlugin* plugin)
    : Command("Insert Random Item with History",
              "Pick a random song from a source playlist, skipping recently played songs. "
              "When Start Playback is true, calls Player::InsertPlaylistAsNext (matching "
              "FPP's own built-in insert commands, immediate=false). The command completes, "
              "FPP plays the single-song output playlist as the next item, then the calling "
              "playlist resumes — Lead Out still runs. FPP sees the item as a sequence entry "
              "so overlay / scrolling-text \"now playing\" shows the correct song title. "
              "History resets automatically once every song has been played."),
      m_plugin(plugin) {
    args.push_back(CommandArg("Source Playlist", "string", "Playlist to pick from")
                       .setContentListUrl("api/playlists/playable"));
    args.push_back(CommandArg("Output Playlist", "string", "Single-song playlist to write", true)
                       .setContentListUrl("api/playlists/playable")
                       .setDefaultValue("RandomPick"));
    args.push_back(CommandArg("History Size", "int", "Songs to exclude before repeating", true)
                       .setRange(1, 100)
                       .setDefaultValue("10"));
    args.push_back(CommandArg("Start Playback", "bool", "Queue the picked item as next item in this playlist", true)
                       .setDefaultValue("true"));
    args.push_back(CommandArg("Replace Previous", "bool", "Remove the previous item before inserting new one", true)
                       .setDefaultValue("true"));
}

// ---------------------------------------------------------------------------
// Command run()
// ---------------------------------------------------------------------------
std::unique_ptr<Command::Result>
InsertRandomWithHistoryCommand::run(const std::vector<std::string>& a) {
    if (a.empty() || a[0].empty())
        return std::make_unique<ErrorResult>("Source Playlist is required");

    const std::string& source = a[0];
    std::string output        = (a.size() >= 2 && !a[1].empty()) ? a[1] : "RandomPick";
    int historySize           = 10;
    bool startPlayback        = true;

    if (a.size() >= 3 && !a[2].empty()) {
        try { historySize = std::stoi(a[2]); } catch (...) {}
        if (historySize < 1) historySize = 1;
    }
    if (a.size() >= 4)
        startPlayback = !(a[3] == "false" || a[3] == "0");

    bool replacePrevious = true;
    if (a.size() >= 5)
        replacePrevious = !(a[4] == "false" || a[4] == "0");

    std::string result = m_plugin->pickWithHistory(source, output, historySize, startPlayback, replacePrevious);
    return std::make_unique<Result>(result);
}

// ---------------------------------------------------------------------------
// ClearPlaylistMainCommand
// ---------------------------------------------------------------------------
ClearPlaylistMainCommand::ClearPlaylistMainCommand(RandomSongPickerPlugin* plugin)
    : Command("Clear Playlist Main",
              "Clear the main playlist section of a playlist, leaving Lead In and Lead Out intact."),
      m_plugin(plugin) {
    args.push_back(CommandArg("Playlist", "string", "Playlist to clear")
                       .setContentListUrl("api/playlists/playable"));
}

std::unique_ptr<Command::Result>
ClearPlaylistMainCommand::run(const std::vector<std::string>& a) {
    if (a.empty() || a[0].empty())
        return std::make_unique<ErrorResult>("Playlist is required");
    std::string result = m_plugin->clearPlaylistMain(a[0]);
    return std::make_unique<Result>(result);
}

// ---------------------------------------------------------------------------
// Plugin entry point
// ---------------------------------------------------------------------------
extern "C" {

FPPPlugins::Plugin* createPlugin() {
    return new RandomSongPickerPlugin();
}

} // extern "C"
