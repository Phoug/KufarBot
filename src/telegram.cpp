//
//  telegram.cpp
//  Kufar Telegram Notifier
//
//  Created by Macintosh on 04.06.2022.
//

#include <iostream>
#include "kufar.hpp"
#include "telegram.hpp"
#include "networking.hpp"
#include "json.hpp"
#include "helperfunctions.hpp"
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

namespace Telegram {
    using namespace std;
    using namespace Networking;
    using nlohmann::json;

    const short int MAX_IMAGES_IN_GROUP = 10;
    string makeImageGroupJSON(const vector<string> &images, const string &caption) {
        json j_array = json::array();
        for (size_t i = 0; (i < images.size()) && (i < MAX_IMAGES_IN_GROUP); i++){
            json j_list = json::object({
                    {"type", "photo"},
                    {"media", images[i]}
            });
            
            if (i == 0) {
                j_list.push_back({"caption", caption});
            }
            j_array.push_back(j_list);
        }
        
        return j_array.dump();
    }

    void sendAdvert(const TelegramConfiguration &telegramConfiguration, const Kufar::Ad &ad) {
        const auto *date = localtime(&ad.date);
        if (!date) { throw runtime_error("Invalid advert date"); }
        ostringstream timeStream;
        timeStream << put_time(date, "%Y-%m-%d %H:%M:%S %Z");
        const string formattedTime = timeStream.str();
        ostringstream price;
        price << ad.price / 100 << '.' << setw(2) << setfill('0') << ad.price % 100;
        
        string text = "Ссылка: " + ad.link + "\n";
        
        if (ad.tag.has_value()) {
            text += "#" + ad.tag.value() + "\n";
        }
        
        text += "Название: " + ad.title + "\n"
                "Дата: " + formattedTime + "\n"
                "Цена: " + price.str() + " BYN\n\n"
                "Имя продавца: " + ad.sellerName + "\n"
                "Номер телефона не скрыт: " + (ad.phoneNumberIsVisible ? "Да" : "Нет");
        
        // UTF-8 bytes bound Telegram's character count; keep truncation on a codepoint boundary.
        if (text.size() > 4000) {
            size_t end = 3997;
            while ((static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) { --end; }
            text.resize(end);
            text += "...";
        }
        string url = "https://api.telegram.org/bot" + telegramConfiguration.botToken;
        string body = "chat_id=" + to_string(telegramConfiguration.chatID);
        if (ad.images.empty() || text.size() > 1024) {
            url += "/sendMessage";
            body += "&text=" + urlEncode(text);
        } else if (ad.images.size() == 1) {
            url += "/sendPhoto";
            body += "&photo=" + urlEncode(ad.images.front()) + "&caption=" + urlEncode(text);
        } else {
            url += "/sendMediaGroup";
            body += "&media=" + urlEncode(makeImageGroupJSON(ad.images, text));
        }
        try {
            json response;
            try {
                response = parseJSON(getJSONFromURL(url, body));
            } catch (const HTTPError &error) {
                if (error.status == 400 && url.find("/sendMessage") == string::npos) {
                    // An expired image must not block this advert or the entire query.
                    auto textAdvert = ad;
                    textAdvert.images.clear();
                    sendAdvert(telegramConfiguration, textAdvert);
                    return;
                } else if (error.status == 429) {
                    const auto details = parseJSON(error.body);
                    const auto &delay = details.at("parameters").at("retry_after");
                    if (delay.is_number_integer() && delay.get<long double>() >= 1 && delay.get<long double>() <= 3600) {
                        this_thread::sleep_for(chrono::seconds(delay.get<int>()));
                    }
                    throw runtime_error("Telegram rate limit; notification will be retried");
                } else { throw; }
            }
            if (!response.at("ok").get<bool>()) {
                throw runtime_error("Telegram rejected the notification");
            }
        } catch (const nlohmann::json::exception &) {
            throw runtime_error("Invalid Telegram response");
        }
    }
}
