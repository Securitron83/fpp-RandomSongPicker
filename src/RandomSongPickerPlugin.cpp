/*
 * fpp-RandomSongPicker — "Insert Random Item with History"
 *
 * Like FPP's built-in random playlist picker, but tracks recently played
 * songs so nothing repeats until the full source list has been exhausted.
 *
 * Command: "Insert Random Item with History"
 *   Source Playlist  — playlist to pick from (dropdown via api/playlists/playable)
 *   Output Playlist  — single-song playlist to write (dropdown, default: RandomPick)
 *   History Size     — songs to exclude before repeating (default: 10)
 *   Start Playback   — start the output playlist immediately (default: true)
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
#include <commands/Commands.h>
#include <log.h>

// HTTP (for starting playback via local FPP API)
#include <curl/curl.h>

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
// HTTP helper — POST JSON body, discard response
// ---------------------------------------------------------------------------
static size_t discardCallback(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}

static bool httpPost(const std::string& url, const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,      1L);
    curl_easy_setopt(curl, CURLOPT_POST,          1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCallback);
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK;
}

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
                                bool startPlayback) {
        std::lock_guard<std::mutex> lk(m_mutex);

        // 1. Read source playlist
        Json::Value items;
        std::string err = readPlaylist(sourceName, items);
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
        err = writeOutputPlaylist(outputName, item);
        if (!err.empty()) return err;

        // 7. Optionally start playback
        if (startPlayback) {
            std::string body = "{\"playlist\":\"" + outputName + "\",\"loop\":0}";
            if (!httpPost("http://127.0.0.1/api/v1/playlist/play", body)) {
                LogWarn(VB_GENERAL, "RandomSongPicker: failed to start '%s'\n", outputName.c_str());
                return "Picked '" + pickedId + "' but failed to start playback";
            }
            LogInfo(VB_GENERAL, "RandomSongPicker: started '%s'\n", outputName.c_str());
        }

        return "Picked: " + pickedId;
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

    std::string readPlaylist(const std::string& name, Json::Value& items) {
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

    std::string writeOutputPlaylist(const std::string& name, const Json::Value& item) {
        std::string path = PLAYLISTS_DIR + name + ".json";
        double duration = item.isMember("duration") ? item["duration"].asDouble() : 0.0;

        Json::Value root;
        root["name"]      = name;
        root["version"]   = 3;
        root["repeat"]    = 0;
        root["loopCount"] = 0;
        root["empty"]     = false;
        root["desc"]      = "Generated by RandomSongPicker";
        root["random"]    = 0;
        root["leadIn"]    = Json::Value(Json::arrayValue);
        root["leadOut"]   = Json::Value(Json::arrayValue);
        root["mainPlaylist"].append(item);
        root["playlistInfo"]["total_duration"] = duration;
        root["playlistInfo"]["total_items"]    = 1;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "  ";

        std::ofstream f(path, std::ios::trunc);
        if (!f.is_open()) return "Cannot write output playlist: " + path;
        f << Json::writeString(writer, root);
        return {};
    }

    void registerCommands() {
        auto* cmd = new InsertRandomWithHistoryCommand(this);
        CommandManager::INSTANCE.addCommand(cmd);
        m_registeredCommands = {cmd->name};
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
              "Writes the pick to an output playlist and optionally starts it. "
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
    args.push_back(CommandArg("Start Playback", "bool", "Start the output playlist immediately", true)
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

    std::string result = m_plugin->pickWithHistory(source, output, historySize, startPlayback);
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
