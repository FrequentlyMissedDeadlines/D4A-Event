/**
 * @file BotServerWeb.cpp
 * @brief Implementation of BotServerWeb — thin HTTP transport for the bot protocol.
 */

#include "BotCommunication/BotServerWeb.h"
#include "RollingLogger.h"
#include "FlashStringHelper.h"
#include <Arduino.h>        // FPSTR()
#include <esp_camera.h>
#include <img_converters.h> // frame2jpg()

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BotServerWeb::BotServerWeb(AmakerBotService &bot, uint16_t port)
    : bot_(bot), port_(port)
{
}

// ---------------------------------------------------------------------------
// hexDecode
// ---------------------------------------------------------------------------

bool BotServerWeb::hexDecode(const char *hex, size_t hex_len, std::string &out)
{
    if (!hex || hex_len == 0 || hex_len % 2 != 0)
        return false;

    out.clear();
    out.reserve(hex_len / 2);

    for (size_t i = 0; i < hex_len; i += 2)
    {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0)
            return false;

        out += static_cast<char>((hi << 4) | lo);
    }
    return true;
}

// ---------------------------------------------------------------------------
// hexEncode
// ---------------------------------------------------------------------------

std::string BotServerWeb::hexEncode(const uint8_t *data, size_t len)
{
    static constexpr char hex_chars[] = "0123456789abcdef";

    std::string out;
    out.reserve(len * 2);

    for (size_t i = 0; i < len; ++i)
    {
        out += hex_chars[data[i] >> 4];
        out += hex_chars[data[i] & 0x0F];
    }
    return out;
}
// ---------------------------------------------------------------------------

void BotServerWeb::register_get_botserver()
{
    // Capture `this` — BotServerWeb is long-lived (typically static in main.cpp)
    server_->on(BotServerWebConsts::path_botserver, HTTP_GET,
        [this](AsyncWebServerRequest *request)
        {
            // ---- Validate `cmd` parameter ----
            if (!request->hasParam(FPSTR(BotServerWebConsts::param_cmd)))
            {
                ++dropped_count_;
                request->send(400, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_missing_cmd));
                return;
            }

            const String &hex_str = request->getParam(
                FPSTR(BotServerWebConsts::param_cmd))->value();

            // ---- Hex-decode the binary frame ----
            std::string frame;
            if (!hexDecode(hex_str.c_str(), hex_str.length(), frame))
            {
                ++dropped_count_;
                request->send(400, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_invalid_hex));
                return;
            }

            ++rx_count_;

            // ---- Dispatch — lock-free, synchronous ----
            const std::string response = bot_.dispatch(
                reinterpret_cast<const uint8_t *>(frame.data()), frame.size());

            // ---- Reply ----
            if (response.empty())
            {
                // Handler processed the frame but has nothing to say
                request->send(204);
                return;
            }

            ++tx_count_;
            // AsyncWebServer requires the response buffer to stay valid until
            // the send completes. Using send_P / a heap copy via String is the
            // standard pattern for dynamic binary content in ESPAsyncWebServer.
            // We copy into an Arduino String (which owns its buffer) and send it.
            // For the small frames used by the bot protocol this is fine.
            uint8_t *buf = reinterpret_cast<uint8_t *>(malloc(response.size()));
            if (!buf)
            {
                request->send(500, FPSTR(BotServerWebConsts::mime_text), "OOM");
                return;
            }
            memcpy(buf, response.data(), response.size());
            AsyncWebServerResponse *resp = request->beginResponse(
                200, FPSTR(BotServerWebConsts::mime_octet),
                buf, response.size());
            resp->addHeader("Cache-Control", "no-store");
            request->send(resp);
            free(buf);
        });
}

// ---------------------------------------------------------------------------
// start
// ---------------------------------------------------------------------------

bool BotServerWeb::start()
{
    server_ = new AsyncWebServer(port_);
    if (!server_)
    {
        if (logger_)
            logger_->error(FPSTR(BotServerWebConsts::msg_no_alloc));
        return false;
    }

    // Mount LittleFS — partition label must match platformio.ini board_build.partitions
    if (!LittleFS.begin(false, "/littlefs", 10,
                        reinterpret_cast<const char*>(FPSTR(BotServerWebConsts::littlefs_partition))))
    {
        if (logger_)
            logger_->error(FPSTR(BotServerWebConsts::msg_fs_failed));
        // Non-fatal: /botserver still works without the filesystem
    }
    else
    {
        if (logger_)
            logger_->info(FPSTR(BotServerWebConsts::msg_fs_ok));
    }

    register_get_botserver();
    server_->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)   
    {
        request->redirect(FPSTR(BotServerWebConsts::default_file));
    });

    server_->onNotFound([this](AsyncWebServerRequest *request)
    {
        ++dropped_count_;
        request->send(404, FPSTR(BotServerWebConsts::mime_html), "<html><body><h1>404 Not Found</h1> Try <a href=\"/\">/</a></body></html>");
    });
    // Static files served last so that /botserver takes precedence.
    // 1-day cache (86400 s) — use Ctrl+Shift+R in the browser to bypass during development.
    server_->serveStatic(reinterpret_cast<const char*>(FPSTR(BotServerWebConsts::static_url_root)),
                         LittleFS,
                         reinterpret_cast<const char*>(FPSTR(BotServerWebConsts::static_fs_root)))
           .setDefaultFile(reinterpret_cast<const char*>(FPSTR(BotServerWebConsts::default_file)))
           .setCacheControl(reinterpret_cast<const char*>(FPSTR(BotServerWebConsts::cache_control)));

    server_->begin();

    if (logger_)
        logger_->info(fpstr_to_string(FPSTR(BotServerWebConsts::msg_start_ok))
                      + std::to_string(port_));
    return true;
}

// ---------------------------------------------------------------------------
// stop
// ---------------------------------------------------------------------------

void BotServerWeb::stop()
{
    if (!server_)
        return;

    server_->end();
    delete server_;
    server_ = nullptr;

    LittleFS.end();

    if (logger_)
        logger_->info(FPSTR(BotServerWebConsts::msg_stop));
}

// ---------------------------------------------------------------------------
// registerCameraRoutes
// ---------------------------------------------------------------------------

void BotServerWeb::registerCameraRoutes(QueueHandle_t cam_queue)
{
    cam_queue_ = cam_queue;

    // ---- GET /cam/snapshot ----
    server_->on(BotServerWebConsts::path_snapshot, HTTP_GET,
        [this](AsyncWebServerRequest *request)
        {
            if (!cam_queue_)
            {
                request->send(503, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_cam_not_init));
                return;
            }
            if (streaming_active_)
            {
                request->send(503, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_cam_busy));
                return;
            }

            // Flush stale frames, keep only the latest
            camera_fb_t *fb = nullptr;
            camera_fb_t *latest = nullptr;
            while (xQueueReceive(cam_queue_, &fb, 0) == pdTRUE)
            {
                if (latest) esp_camera_fb_return(latest);
                latest = fb;
            }
            if (!latest)
                xQueueReceive(cam_queue_, &latest,
                              pdMS_TO_TICKS(BotServerWebConsts::cam_queue_timeout_ms));

            if (!latest)
            {
                request->send(503, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_cam_capture));
                return;
            }

            // Convert RGB565 → JPEG if needed
            uint8_t *jpg_buf = nullptr;
            size_t   jpg_len = 0;
            bool     owns_buf = false;

            const bool is_jpeg = latest->len >= 2 &&
                                 latest->buf[0] == 0xFF &&
                                 latest->buf[1] == 0xD8;
            if (is_jpeg)
            {
                jpg_buf  = latest->buf;
                jpg_len  = latest->len;
                owns_buf = false;
            }
            else
            {
                owns_buf = frame2jpg(latest, BotServerWebConsts::cam_jpeg_quality,
                                     &jpg_buf, &jpg_len);
            }

            esp_camera_fb_return(latest);

            if (!jpg_buf || jpg_len == 0)
            {
                if (owns_buf && jpg_buf) free(jpg_buf);
                request->send(503, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_cam_capture));
                return;
            }

            AsyncWebServerResponse *resp =
                request->beginResponse(200,
                                       FPSTR(BotServerWebConsts::mime_jpeg),
                                       jpg_buf, jpg_len);
            resp->addHeader("Content-Disposition",
                            FPSTR(BotServerWebConsts::snapshot_filename));
            resp->addHeader("Cache-Control", "no-store");
            request->send(resp);

            if (owns_buf) free(jpg_buf);
        });

    // ---- GET /cam/stream ----
    server_->on(BotServerWebConsts::path_stream, HTTP_GET,
        [this](AsyncWebServerRequest *request)
        {
            if (!cam_queue_)
            {
                request->send(503, FPSTR(BotServerWebConsts::mime_text),
                              FPSTR(BotServerWebConsts::err_cam_not_init));
                return;
            }

            streaming_active_ = true;

            // Per-frame state kept alive across chunked callback invocations.
            // shared_ptr ensures cleanup on connection close.
            struct StreamState
            {
                camera_fb_t *fb_held    = nullptr;  ///< held for JPEG zero-copy path
                uint8_t     *jpg_buf    = nullptr;  ///< data ptr (owned only in RGB565→JPEG path)
                size_t       jpg_len    = 0;
                size_t       offset     = 0;
                char         header[80] = {};
                size_t       header_len = 0;

                ~StreamState()
                {
                    if (fb_held) { esp_camera_fb_return(fb_held); fb_held = nullptr; }
                    else if (jpg_buf) { free(jpg_buf); jpg_buf = nullptr; }
                }
            };

            auto state = std::make_shared<StreamState>();

            AsyncWebServerResponse *response = request->beginChunkedResponse(
                FPSTR(BotServerWebConsts::mime_multipart),
                [this, state](uint8_t *buffer, size_t maxLen, size_t /*index*/) -> size_t
                {
                    if (!streaming_active_)
                        return 0; // end stream

                    // Acquire a fresh frame if we don't have one in progress
                    if (!state->jpg_buf)
                    {
                        camera_fb_t *fb = nullptr;
                        camera_fb_t *latest = nullptr;

                        // Drain stale frames — keep only the most recent
                        while (xQueueReceive(cam_queue_, &fb, 0) == pdTRUE)
                        {
                            if (latest) esp_camera_fb_return(latest);
                            latest = fb;
                        }

                        // If queue was empty, block briefly for the next frame.
                        // RESPONSE_TRY_AGAIN alone cannot reliably restart the
                        // chunked callback because AsyncTCP only re-triggers on
                        // TCP ACK — if no data was sent there is no ACK.
                        if (!latest)
                        {
                            xQueueReceive(cam_queue_, &latest,
                                          pdMS_TO_TICKS(BotServerWebConsts::cam_stream_wait_ms));
                        }

                        if (!latest || !latest->buf || latest->len == 0)
                        {
                            if (latest) esp_camera_fb_return(latest);
                            return RESPONSE_TRY_AGAIN;
                        }

                        const bool is_jpeg = latest->len >= 2 &&
                                             latest->buf[0] == 0xFF &&
                                             latest->buf[1] == 0xD8;
                        if (is_jpeg)
                        {
                            // Copy frame so the DMA buffer is returned to the
                            // camera task before the next capture cycle.
                            state->jpg_len = latest->len;
                            state->jpg_buf = static_cast<uint8_t *>(ps_malloc(state->jpg_len));
                            if (!state->jpg_buf)
                                state->jpg_buf = static_cast<uint8_t *>(malloc(state->jpg_len));
                            if (!state->jpg_buf)
                            {
                                esp_camera_fb_return(latest);
                                return RESPONSE_TRY_AGAIN;
                            }
                            memcpy(state->jpg_buf, latest->buf, state->jpg_len);
                            esp_camera_fb_return(latest); // release DMA buffer immediately
                        }
                        else
                        {
                            if (!frame2jpg(latest,
                                           BotServerWebConsts::cam_jpeg_quality,
                                           &state->jpg_buf, &state->jpg_len)
                                || !state->jpg_buf)
                            {
                                if (state->jpg_buf) { free(state->jpg_buf); state->jpg_buf = nullptr; }
                                esp_camera_fb_return(latest);
                                return RESPONSE_TRY_AGAIN;
                            }
                            esp_camera_fb_return(latest);
                        }

                        state->header_len = snprintf(
                            state->header, sizeof(state->header),
                            "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                            static_cast<unsigned>(state->jpg_len));
                        state->offset = 0;
                    }

                    // Send header + JPEG payload in chunks that fit maxLen
                    const size_t total     = state->header_len + state->jpg_len;
                    const size_t remaining = total - state->offset;
                    const size_t to_write  = (remaining < maxLen) ? remaining : maxLen;
                    size_t written = 0;
                    size_t pos     = state->offset;

                    while (written < to_write)
                    {
                        if (pos < state->header_len)
                        {
                            size_t chunk = std::min(to_write - written,
                                                    state->header_len - pos);
                            memcpy(buffer + written, state->header + pos, chunk);
                            written += chunk; pos += chunk;
                        }
                        else
                        {
                            size_t jpg_offset = pos - state->header_len;
                            size_t chunk = std::min(to_write - written,
                                                    state->jpg_len - jpg_offset);
                            memcpy(buffer + written,
                                   state->jpg_buf + jpg_offset, chunk);
                            written += chunk; pos += chunk;
                        }
                    }

                    state->offset += written;

                    if (state->offset >= total)
                    {
                        if (state->fb_held) { esp_camera_fb_return(state->fb_held); state->fb_held = nullptr; }
                        if (state->jpg_buf) { free(state->jpg_buf); }
                        state->jpg_buf    = nullptr;
                        state->jpg_len    = 0;
                        state->header_len = 0;
                        state->offset     = 0;
                    }

                    return written;
                });

            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            response->addHeader("Pragma",  "no-cache");
            response->addHeader("Expires", "0");
            response->addHeader("Access-Control-Allow-Origin", "*");

            request->onDisconnect([this]()
            {
                streaming_active_ = false;
                if (logger_) logger_->info("Camera stream client disconnected");
            });

            request->send(response);
            if (logger_) logger_->info("Camera MJPEG stream started");
        });

    if (logger_)
        logger_->info("Camera routes registered (/cam/snapshot, /cam/stream)");
}
