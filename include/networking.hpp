//
//  networking.hpp
//  Kufar Telegram Notifier
//
//  Created by Macintosh on 04.06.2022.
//

#ifndef networking_hpp
#define networking_hpp

#include <string>
#include <optional>
#include <stdexcept>

namespace Networking {
    struct HTTPError : std::runtime_error {
        long status;
        std::string body;
        HTTPError(long code, const std::string &response)
            : std::runtime_error("HTTP error: " + std::to_string(code)), status(code), body(response) {}
    };
    std::string urlEncode(const std::string &);
    std::string getJSONFromURL(const std::string &, const std::optional<std::string> &postBody = std::nullopt);
};

#endif /* networking_hpp */
