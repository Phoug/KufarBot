#include <curl/curl.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include "networking.hpp"

namespace Networking {
    namespace {
        struct CurlRuntime {
            CurlRuntime() {
                if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
                    throw std::runtime_error("Cannot initialize libcurl");
                }
            }
            ~CurlRuntime() { curl_global_cleanup(); }
        };

        auto makeHandle() {
            static CurlRuntime runtime;
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
            if (!curl) { throw std::runtime_error("Cannot create libcurl handle"); }
            return curl;
        }

        size_t writeFunction(char *ptr, size_t size, size_t count, void *userdata) noexcept {
            auto &data = *static_cast<std::string *>(userdata);
            constexpr size_t maxResponse = 4 * 1024 * 1024;
            if (size != 0 && count > (maxResponse - data.size()) / size) { return 0; }
            try { data.append(ptr, size * count); }
            catch (...) { return 0; }
            return size * count;
        }
    }

    std::string urlEncode(const std::string &text) {
        if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("URL parameter too large");
        }
        auto curl = makeHandle();
        std::unique_ptr<char, decltype(&curl_free)> encoded(
            curl_easy_escape(curl.get(), text.data(), static_cast<int>(text.size())), curl_free);
        if (!encoded) { throw std::runtime_error("Cannot encode URL parameter"); }
        return encoded.get();
    }

    std::string getJSONFromURL(const std::string &url, const std::optional<std::string> &postBody) {
        auto curl = makeHandle();
        auto *handle = curl.get();
        std::string response;
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
#ifdef _WIN32
        curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeFunction);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
        if (postBody) {
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postBody->c_str());
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(postBody->size()));
        }
        const auto result = curl_easy_perform(handle);
        if (result != CURLE_OK) {
            // URLs contain the bot token; never include them or response bodies in errors.
            throw std::runtime_error(std::string("Network error: ") + curl_easy_strerror(result));
        }
        long status = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
        if (status < 200 || status >= 300) {
            throw HTTPError(status, response);
        }
        return response;
    }
}
