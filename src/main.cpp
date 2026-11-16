//
//  main.cpp
//  Kufar Telegram Notifier
//
//  Created by Macintosh on 02.06.2022.
//

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <unordered_set>

#include "json.hpp"
#include "kufar.hpp"
#include "telegram.hpp"
#include "networking.hpp"
#include "helperfunctions.hpp"

using namespace std;
using namespace Kufar;
using namespace Telegram;
using nlohmann::json;

const string CACHE_FILE_NAME = "cached-data.json";
const string CONFIGURATION_FILE_NAME = "kufar-configuration.json";

struct ConfigurationFile {
    string path;
    json contents;
};

struct CacheFile {
    string path;
    json contents;
};

struct Files {
    ConfigurationFile configuration;
    CacheFile cache;
};

struct ProgramConfiguration {
    vector<KufarConfiguration> kufarConfiguration;
    TelegramConfiguration telegramConfiguration;
    Files files;
    
    int queryDelaySeconds = 5;
    int loopDelaySeconds = 30;
};

void loadJSONConfigurationData(const json &data, ProgramConfiguration &programConfiguration) {
    {
        json telegramData = data.at("telegram");
        auto &telegram = programConfiguration.telegramConfiguration;
        const char *token = getenv("TELEGRAM_BOT_TOKEN");
        telegram.botToken = token ? string(token) : telegramData.value("bot-token", string());
        const auto colon = telegram.botToken.find(':');
        if (colon == string::npos || colon == 0 || colon + 1 == telegram.botToken.size() ||
            telegram.botToken.substr(0, colon).find_first_not_of("0123456789") != string::npos ||
            telegram.botToken.substr(colon + 1).find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != string::npos) {
            throw runtime_error("Invalid Telegram bot token; set TELEGRAM_BOT_TOKEN or telegram.bot-token");
        }
        const auto &chat = telegramData.at("chat-id");
        if (!chat.is_number_integer() || chat.get<long double>() < -4503599627370495.0L ||
            chat.get<long double>() > 4503599627370495.0L || chat.get<int64_t>() == 0) {
            throw runtime_error("telegram.chat-id must be a nonzero signed integer (groups have negative IDs)");
        }
        telegram.chatID = chat.get<int64_t>();
    }
    {
        json queriesData = data.at("queries");
        
        if (!queriesData.is_array() || queriesData.empty()) {
            throw runtime_error("queries must be a nonempty array");
        }
        for (const json &query : queriesData) {
            KufarConfiguration kufarConfiguration;
            
            kufarConfiguration.onlyTitleSearch = getOptionalValue<bool>(query, "only-title-search");
            kufarConfiguration.tag = getOptionalValue<string>(query, "tag");
            if (query.contains("price")) {
                json queryPriceData = query.at("price");
                kufarConfiguration.priceRange.priceMin = getOptionalValue<int>(queryPriceData, "min");
                kufarConfiguration.priceRange.priceMax = getOptionalValue<int>(queryPriceData, "max");
            }
            
            kufarConfiguration.language = getOptionalValue<string>(query, "language");
            kufarConfiguration.limit = getOptionalValue<int>(query, "limit");
            kufarConfiguration.currency = getOptionalValue<string>(query, "currency");
            kufarConfiguration.condition = getOptionalValue<ItemCondition>(query, "condition");
            kufarConfiguration.sellerType = getOptionalValue<SellerType>(query, "seller-type");
            kufarConfiguration.kufarDeliveryRequired = getOptionalValue<bool>(query, "kufar-delivery-required");
            kufarConfiguration.kufarPaymentRequired = getOptionalValue<bool>(query, "kufar-payment-required");
            kufarConfiguration.kufarHalvaRequired = getOptionalValue<bool>(query, "kufar-halva-required");
            kufarConfiguration.onlyWithPhotos = getOptionalValue<bool>(query, "only-with-photos");
            kufarConfiguration.onlyWithVideos = getOptionalValue<bool>(query, "only-with-videos");
            kufarConfiguration.onlyWithExchangeAvailable = getOptionalValue<bool>(query, "only-with-exchange-available");
            kufarConfiguration.sortType = getOptionalValue<SortType>(query, "sort-type");
            kufarConfiguration.category = getOptionalValue<Category>(query, "category");
            kufarConfiguration.subCategory = getOptionalValue<int>(query, "sub-category");
            kufarConfiguration.region = getOptionalValue<Region>(query, "region");
            kufarConfiguration.areas = getOptionalValue<vector<int>>(query, "areas");
            const auto &q = kufarConfiguration;
            if ((q.limit && (*q.limit < 1 || *q.limit > 100)) ||
                (q.category && int(*q.category) <= 0) ||
                (q.subCategory && *q.subCategory <= 0) ||
                (q.priceRange.priceMin && *q.priceRange.priceMin < 0) ||
                (q.priceRange.priceMax && *q.priceRange.priceMax < 0) ||
                (q.priceRange.priceMin && q.priceRange.priceMax && *q.priceRange.priceMin > *q.priceRange.priceMax) ||
                (q.condition && (int(*q.condition) < 1 || int(*q.condition) > 2)) ||
                (q.sellerType && (int(*q.sellerType) < 0 || int(*q.sellerType) > 1)) ||
                (q.sortType && (int(*q.sortType) < 1 || int(*q.sortType) > 2)) ||
                (q.region && (int(*q.region) < 1 || int(*q.region) > 7))) {
                throw runtime_error("Invalid query limit, price range or enum value");
            }
            if (q.areas) {
                for (const auto &area : query.at("areas")) {
                    if (!area.is_number_integer() || area.get<long double>() <= 0 ||
                        area.get<long double>() > numeric_limits<int>::max()) {
                        throw runtime_error("areas must contain positive integer IDs");
                    }
                }
            }
            programConfiguration.kufarConfiguration.push_back(kufarConfiguration);
        }
    }
    {
        if (data.contains("delays")) {
            json delaysData = data.at("delays");
            programConfiguration.queryDelaySeconds = getOptionalValue<int>(delaysData, "query").value_or(5);
            programConfiguration.loopDelaySeconds = getOptionalValue<int>(delaysData, "loop").value_or(30);
        }
    }
    if (programConfiguration.queryDelaySeconds < 1 || programConfiguration.queryDelaySeconds > 86400 ||
        programConfiguration.loopDelaySeconds < 1 || programConfiguration.loopDelaySeconds > 86400) {
        throw runtime_error("delays must be between 1 and 86400 seconds");
    }
}

void printJSONConfigurationData(const ProgramConfiguration &programConfiguration) {
    cout <<
    "- Telegram:\n"
        "\t- Токен: " << "[hidden]" << "\n"
        "\t- ID Чата: " << programConfiguration.telegramConfiguration.chatID << "\n\n"
    "- Запросы:\n";
    
    for (const auto &query : programConfiguration.kufarConfiguration) {
        cout <<
        "\t- Название: " << query.tag << "\n"
        "\t- Поиск только по заголовку: " << query.onlyTitleSearch << "\n"
        "\t- Цена:\n"
            "\t\t- Минимальная: " << query.priceRange.priceMin << " BYN\n"
            "\t\t- Максимальная: " << query.priceRange.priceMax << " BYN\n"
        "\t- Язык: " << query.language << "\n"
        "\t- Макс. кол-во объявлений за один запрос: " << query.limit << "\n"
        "\t- Валюта: " << query.currency << "\n"
        "\t- Состояние: " << (query.condition.has_value() ?
             EnumString::itemCondition(query.condition.value()) : PROPERTY_UNDEFINED) << "\n"
        "\t- Продавец: " << (query.sellerType.has_value() ?
             EnumString::sellerType(query.sellerType.value()) : PROPERTY_UNDEFINED) << "\n"
        "\t- Только с Kufar Доставкой: " << query.kufarDeliveryRequired << "\n"
        "\t- Только с Kufar Оплатой: " << query.kufarPaymentRequired << "\n"
        "\t- Только с Kufar Рассрочкой (Халва): " << query.kufarHalvaRequired << "\n"
        "\t- Только с фото: " << query.onlyWithPhotos << "\n"
        "\t- Только с видео: " << query.onlyWithVideos << "\n"
        "\t- Только с возможностью обмена: " << query.onlyWithExchangeAvailable << "\n"
        "\t- Тип сортировки: " << (query.sortType.has_value() ? EnumString::sortType(query.sortType.value()) : PROPERTY_UNDEFINED) << "\n"
        "\t- Категория: " << (query.category.has_value() ?
                              EnumString::category(query.category.value()) : PROPERTY_UNDEFINED) << "\n"
        "\t- Подкатегория: " << (query.subCategory.has_value() ? EnumString::subCategory(query.subCategory.value()) : PROPERTY_UNDEFINED) << "\n"
        "\t- Город: " << (query.region.has_value() ? EnumString::region(query.region.value()) : PROPERTY_UNDEFINED)<< "\n"
        "\t- Район: ";
        
        if (query.areas.has_value()) {
            unsigned int currentIndex = 0;
            const uint64_t &vectorSize = query.areas.value().size();
            
            for (const auto &area : query.areas.value()){
                cout << EnumString::area(area);
                
                if (currentIndex++ < vectorSize - 1) {
                    cout << ", ";
                }
            }
        } else {
            cout << PROPERTY_UNDEFINED;
        }
        
        cout << "\n\n";
    }
    
    cout <<
    "- Задержки:\n"
        "\t- Перед новым запросом: " << programConfiguration.queryDelaySeconds << "с. \n"
        "\t- После прохода всего списка запросов: " << programConfiguration.loopDelaySeconds << "c." << endl;
}

json getJSONDataFromPath(const string &JSONFilePath) {
    cout << "[Загрузка файла]: " << '"' << JSONFilePath << '"' << endl;

    if (!fileExists(JSONFilePath)){
        cout << "[ОШИБКА]: Файл не существует по данному пути или к нему нет доступа." << endl;
        exit(1);
    }
    
    if (getFileSize(JSONFilePath) > 64000000) {
        cout << "[ОШИБКА]: Размер файла превышает 64МБ." << endl;
        exit(1);
    }
        
    try {
       json textFromFile = parseJSON(getTextFromFile(JSONFilePath));
       return textFromFile;
    } catch (const exception &) {
       cout << "[ОШИБКА]: Невозможно получить данные из файла " << '"' << JSONFilePath << '"' << endl;
       cerr << "Invalid JSON (details hidden to protect credentials)." << endl;
       exit(1);
    }

}

const string prefixConfigurationFile = "--config=";
const string prefixCacheFile = "--cache=";

/**
  Загрузка файлов:
  kufar-configuration.json,
  cached-data.json
 */

Files getFiles(const int &argsCount, char **args) {
    Files files;
    
    for (int i = 1; i < argsCount; i++){
        string currentArgument = args[i];
        
        if(stringHasPrefix(currentArgument, prefixConfigurationFile)) {
            currentArgument.erase(0, prefixConfigurationFile.length());
            files.configuration.path = currentArgument;
        } else if (stringHasPrefix(currentArgument, prefixCacheFile)) {
            currentArgument.erase(0, prefixCacheFile.length());
            files.cache.path = currentArgument;
        } else {
            throw runtime_error("Unknown argument; use --config=PATH and --cache=PATH");
        }
        
    }
    
    if (files.configuration.path.empty()) {
        files.configuration.path = (filesystem::current_path() / CONFIGURATION_FILE_NAME).string();
    }
    if (files.cache.path.empty()) {
        files.cache.path = (filesystem::current_path() / CACHE_FILE_NAME).string();
    }
    
    if (filesystem::weakly_canonical(files.configuration.path) == filesystem::weakly_canonical(files.cache.path) ||
        (filesystem::exists(files.configuration.path) && filesystem::exists(files.cache.path) &&
         filesystem::equivalent(files.configuration.path, files.cache.path))) {
        throw runtime_error("Configuration and cache must be different files");
    }
    files.configuration.contents = getJSONDataFromPath(files.configuration.path);
    files.cache.contents = filesystem::exists(files.cache.path) ? getJSONDataFromPath(files.cache.path) : json::array();
    if (!files.cache.contents.is_array()) { throw runtime_error("Cache must be a JSON array"); }
    for (const auto &id : files.cache.contents) {
        if (!id.is_number_integer() || id.get<long double>() <= 0 ||
            (id.is_number_unsigned() && id.get<uint64_t>() > uint64_t(numeric_limits<int64_t>::max()))) {
            throw runtime_error("Cache must contain positive 64-bit integer IDs");
        }
    }
    
    return files;
}

int main(int argc, char **argv) try {
    ProgramConfiguration programConfiguration;
    unordered_set<int64_t> viewedAds;
    
    programConfiguration.files = getFiles(argc, argv);
    loadJSONConfigurationData(programConfiguration.files.configuration.contents, programConfiguration);
    printJSONConfigurationData(programConfiguration);

    viewedAds = programConfiguration.files.cache.contents.get<unordered_set<int64_t>>();
    // Verify persistence before sending anything, including recovery from an unfinished write.
    saveFile(programConfiguration.files.cache.path, json(viewedAds).dump());

    while (true) {
        for (auto requestConfiguration : programConfiguration.kufarConfiguration) {
            
            try {
                for (const auto &advert : getAds(requestConfiguration)) {
                    if (viewedAds.count(advert.id) == 0) {
                        cout << "[New]: ID " << advert.id << endl;

                        try {
                            sendAdvert(programConfiguration.telegramConfiguration, advert);
                        } catch (const exception &exc) {
                            cerr << "[ERROR (sendAdvert)]: " << exc.what() << endl;
                            break; // Retry on the next poll; do not flood Telegram after a rate limit.
                        }
                        viewedAds.insert(advert.id);
                        try {
                            saveFile(programConfiguration.files.cache.path, json(viewedAds).dump());
                        } catch (...) {
                            cerr << "Cannot persist cache; stopping to avoid repeated notifications." << endl;
                            return 1;
                        }
                        
                    }
                    this_thread::sleep_for(chrono::seconds(1));
                }
            } catch (const nlohmann::json::exception &) {
                cerr << "[ERROR (getAds)]: Invalid Kufar JSON response." << endl;
            } catch (const exception &exc) {
                cerr << "[ERROR (getAds)]: " << exc.what() << endl;
            }
            DEBUG_MSG("[DEBUG]: " << "(QueryDelay) Sleeping for: " << programConfiguration.queryDelaySeconds << "s.");
            this_thread::sleep_for(chrono::seconds(programConfiguration.queryDelaySeconds));

        }
        DEBUG_MSG("[DEBUG]: " << "(LoopDelay) Sleeping for: " << programConfiguration.loopDelaySeconds << "s.");
        this_thread::sleep_for(chrono::seconds(programConfiguration.loopDelaySeconds));
    }
    return 0;
}
 catch (const nlohmann::json::exception &) {
    // JSON conversion errors may include secret values from the configuration.
    cerr << "Startup failed: check JSON types, Telegram credentials, query ranges, delays and file permissions." << endl;
    return 1;
} catch (const std::exception &error) {
    cerr << "Startup failed: " << error.what() << endl;
    return 1;
}
