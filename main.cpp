#include <gst/gst.h>
#include <gst/video/video.h>
#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <iomanip>

class MyVideoPlayer {
private:
    std::string m_streamUrl;
    GstElement* m_pipeline = nullptr;
    GMainLoop* m_mainloop = nullptr;
    bool m_quit = false;
    std::thread m_inputThread;

public:
    MyVideoPlayer(const std::string& url) : m_streamUrl(url) {
        std::cout << "MyVideoPlayer initializing for: " << url << std::endl;
    }

    ~MyVideoPlayer() {
        cleanup();
    }

    bool setupPlayer() {
        gst_init(nullptr, nullptr);

        // Creating the main playbin
        m_pipeline = gst_element_factory_make("playbin", "my-player");
        if (!m_pipeline) {
            std::cerr << "FATAL: Could not create playbin!" << std::endl;
            return false;
        }

        
        g_object_set(m_pipeline, "uri", m_streamUrl.c_str(), nullptr); // uri help to play HLS/DASH content 

    
        g_object_set(m_pipeline, 
                    "latency", 500,                  
                    "buffer-size", 4 * 1024 * 1024,   // 4MB buffer for handling HLS/DASH 
                    "ring-buffer-max-size", 0,        
                    nullptr);

         
		// later once if we have license we can include DRM decryptor here
        // GstElement* drm_decrypt = create_drm_bin();
        // g_object_set(m_pipeline, "video-sink", drm_decrypt, nullptr);

        // Bus monitoring
        GstBus* bus = gst_element_get_bus(m_pipeline);
        gst_bus_add_watch(bus, busMessageHandler, this);
        gst_object_unref(bus);

        m_mainloop = g_main_loop_new(nullptr, FALSE);
        std::cout << "Player setup complete. Ready to play!" << std::endl;
        return true;
    }

    void startPlayback() {
        if (!m_pipeline) {
            std::cerr << "Pipeline not initialized!" << std::endl;
            return;
        }

        std::cout << " Starting playback..." << std::endl;
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

        // Show stream info after discovery (async)
        std::thread infoThread(&MyVideoPlayer::showStreamDetails, this);
        infoThread.detach();

        // Interactive controls thread
        m_inputThread = std::thread(&MyVideoPlayer::handleUserInput, this);

        g_main_loop_run(m_mainloop);

        // Cleanup thread gracefully
        if (m_inputThread.joinable()) {
            m_quit = true;
            m_inputThread.join();
        }
    }

private:
    static gboolean busMessageHandler(GstBus* bus, GstMessage* msg, gpointer user_data) {
        auto* self = static_cast<MyVideoPlayer*>(user_data);
        return self->handleBusMessage(bus, msg);
    }

    gboolean handleBusMessage(GstBus*, GstMessage* msg) {
        GstMessageType type = GST_MESSAGE_TYPE(msg);

        switch (type) {
            case GST_MESSAGE_ERROR: {
                GError* err = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_error(msg, &err, &debug);

                std::cerr << "PLAYER ERROR: " 
                          << (err ? err->message : "Unknown error")
                          << "Debug: " << (debug ? debug : "No debug info")
                          << std::endl;

                g_clear_error(&err);
                g_free(debug);
                g_main_loop_quit(m_mainloop);
                break;
            }

            case GST_MESSAGE_EOS:
                std::cout << "End of Stream reached" << std::endl;
                g_main_loop_quit(m_mainloop);
                break;

            case GST_MESSAGE_STATE_CHANGED:
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(m_pipeline)) {
                    GstState old, current, pending;
                    gst_message_parse_state_changed(msg, &old, &current, &pending);
                    std::cout << "State: " << gst_element_state_get_name(old)
                              << "State: " << gst_element_state_get_name(current) << std::endl;
                }
                break;

            default:
                break;
        }
        return TRUE;
    }

    void handleUserInput() {
        std::cout << "Controls:";
        std::cout << "  'a0', 'a1'... = Audio track";
        std::cout << "  's0', 's1'... = Subtitle track";
        std::cout << "  'q'           = Quit";

        std::string cmd;
        while (!m_quit && std::getline(std::cin, cmd)) {
            if (cmd == "q") {
                std::cout << "Shutting down..." << std::endl;
                g_main_loop_quit(m_mainloop);
                break;
            }

            if (cmd.size() > 1 && (cmd[0] == 'a' || cmd[0] == 's')) {
                try {
                    int trackId = std::stoi(cmd.substr(1));
                    if (cmd[0] == 'a') {
                        switchAudio(trackId);
                    } else {
                        switchSubtitle(trackId);
                    }
                } catch (...) {
                    std::cout << "Invalid track number" << std::endl;
                }
            }
        }
    }

    void showStreamDetails() {
        // Give playbin time to discover streams
        std::this_thread::sleep_for(std::chrono::seconds(3));

        if (!m_pipeline) return;

        gint audioTracks = 0, subtitleTracks = 0, videoTracks = 0;
        g_object_get(m_pipeline,
                    "n-audio", &audioTracks,
                    "n-text", &subtitleTracks,
                    "n-video", &videoTracks,
                    nullptr);

        std::cout << "Stream Discovery:";
        std::cout << "Video tracks: " << videoTracks << std::endl;
        std::cout << "Audio tracks: " << audioTracks << std::endl;
        std::cout << "Subtitle tracks: " << subtitleTracks << std::endl;

        listTracks("Audio", audioTracks, "get-audio-tags");
        listTracks("Subtitle", subtitleTracks, "get-text-tags");
    }

    void listTracks(const std::string& type, gint count, const char* signalName) {
        for (gint i = 0; i < count; ++i) {
            GstTagList* tags = nullptr;
            g_signal_emit_by_name(m_pipeline, signalName, i, &tags);

            if (tags) {
                gchar* lang = nullptr;
                if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &lang)) {
                    std::cout << "  " << type << "[" << i << "] " << lang << std::endl;
                    g_free(lang);
                }
                gst_tag_list_unref(tags);
            }
        }
    }

    void switchAudio(int trackId) {
        g_object_set(m_pipeline, "current-audio", trackId, nullptr);
        std::cout << "Switched to audio track #" << trackId << std::endl;
    }

    void switchSubtitle(int trackId) {
        g_object_set(m_pipeline, "current-text", trackId, nullptr);
        std::cout << "Switched to subtitle track #" << trackId << std::endl;
    }

    void cleanup() {
        if (m_pipeline) {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
        }
        if (m_mainloop) {
            if (g_main_loop_is_running(m_mainloop)) {
                g_main_loop_quit(m_mainloop);
            }
            g_main_loop_unref(m_mainloop);
            m_mainloop = nullptr;
        }
        std::cout << "Cleanup complete" << std::endl;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <stream_url>";
        std::cerr << "Example: " << argv[0] << " https://test-streams.mux.dev/x36xhzz/x36xhzz.m3u8";
        return 1;
    }

    MyVideoPlayer player(argv[1]);

    if (!player.setupPlayer()) {
        return 2;
    }

    player.startPlayback();
    return 0;
}
