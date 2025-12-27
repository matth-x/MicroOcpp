// // matth-x/MicroOcpp
// // Copyright Matthias Akstaller 2019 - 2024
// // MIT License

#include <MicroOcpp/Model/California/CaliforniaUtils.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

#include <cstring>

#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA

namespace MicroOcpp {
namespace v16 {
namespace CaliforniaUtils 
{
bool validateDefaultPrice(const char* defaultPrice, void *userPtr)
{
    if (defaultPrice == nullptr)
        return false;
    
    (void)userPtr;
    
    size_t capacity = 512;
    auto doc = initJsonDoc("v16.California.DefaultPriceValidator", capacity);
    DeserializationError err = DeserializationError::NoMemory;
    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc = initJsonDoc("v16.California.DefaultPriceValidator", capacity);
        err = deserializeJson(doc, defaultPrice, strlen(defaultPrice));
        capacity *= 2;
    }
    if (err != DeserializationError::Ok) {
        MO_DBG_DEBUG("DefaultPrice JSON deserialization failed: %s", err.c_str());
        return false;
    }

    bool priceTextFound = false;
    bool priceTextOfflineFound = false;
    bool chargingPriceFound = false;
    auto defaultPriceObject = doc.as<JsonObject>();
    for (JsonPair kv : defaultPriceObject) {
        const char* key = kv.key().c_str();
        if (strcmp(key, "priceText") == 0 && !priceTextFound) {
            if (!kv.value().is<const char *>()) {
                MO_DBG_DEBUG("DefaultPrice.priceText must be a string");
                return false;
            }
            priceTextFound = true;
        } else if (strcmp(key, "priceTextOffline") == 0 && !priceTextOfflineFound) {
            if (!kv.value().is<const char *>()) {
                MO_DBG_DEBUG("DefaultPrice.priceTextOffline must be a string");
                return false;
            }
            priceTextOfflineFound = true;
        } else if (strcmp(key, "chargingPrice") == 0 && !chargingPriceFound) {
            if (!kv.value().is<JsonObject>()) {
                MO_DBG_DEBUG("DefaultPrice.chargingPrice must be an object");
                return false;
            }
            auto chargingPriceObject = kv.value().as<JsonObject>();
            for (JsonPair priceKv : chargingPriceObject) {
                const char* priceKey = priceKv.key().c_str();
                if (strcmp(priceKey, "kWhPrice") == 0 || strcmp(priceKey, "hourPrice") == 0 || strcmp(priceKey, "flatFee") == 0) {
                    if (!priceKv.value().is<double>()) {
                        MO_DBG_DEBUG("DefaultPrice.chargingPrice.%s must be a number", priceKey);
                        return false;
                    }
                } else {
                    MO_DBG_DEBUG("Unknown key in DefaultPrice.chargingPrice: %s", priceKey);
                    return false;
                }
            }
            chargingPriceFound = true;
        } else {
            if (priceTextFound)
                MO_DBG_DEBUG("DefaultPrice.priceText repeated");
            else if (priceTextOfflineFound)
                MO_DBG_DEBUG("DefaultPrice.priceTextOffline repeated");
            else if (chargingPriceFound)
                MO_DBG_DEBUG("DefaultPrice.chargingPrice repeated");
            else
                MO_DBG_DEBUG("Unknown key in DefaultPrice: %s", key);
            return false;
        }
    }

    if (!priceTextFound) {
        MO_DBG_DEBUG("DefaultPrice.priceText is required");
        return false;
    }

    if ((chargingPriceFound && !priceTextOfflineFound)) {
        MO_DBG_DEBUG("DefaultPrice.chargingPrice requires DefaultPrice.priceTextOffline to be set");
        return false;
    } 

    return true;
}

bool validateTimeOffset(const char* timeOffset, void *userPtr)
{
    if (timeOffset == nullptr)
        return false;

    (void)userPtr;

    if (strcmp(timeOffset, "Z") == 0) {
        return true;
    }

    auto len = strlen(timeOffset);
    if (len < 3 || len > 6) {
        return false;
    }

    auto sign = timeOffset[0];
    if (sign != '+' && sign != '-') {
        return false;
    }

    String timeOffsetStr = String(timeOffset + 1);
    String hourStr, minStr;
    bool hasColon = (timeOffsetStr.find(':') != std::string::npos);
    if (hasColon) {
        // format: +HH:MM
        if (len != 6) {
            return false;
        }
        hourStr = timeOffsetStr.substr(0, 2);
        minStr = timeOffsetStr.substr(3, 5);
    } else {
        // format: +HHMM or +HH
        if (len == 3) {
            // +HH
            hourStr = timeOffsetStr.substr(0, 2);
            minStr = "00";
        } else if (len == 5) {
            // +HHMM
            hourStr = timeOffsetStr.substr(0, 2);
            minStr = timeOffsetStr.substr(2, 4);
        } else {
            return false;
        }
    }

    for (char c : hourStr) if (!isdigit(c)) return false;
    for (char c : minStr) if (!isdigit(c)) return false;

    int hours = std::stoi(hourStr.c_str());
    int minutes = std::stoi(minStr.c_str());

    if (hours < 0 || hours > 14) {
        return false;
    }

    if (minutes < 0 || minutes > 59) {
        return false;
    }

    return true;
}

bool validateNextTimeOffsetTransitionDateTime(const char* nextTimeOffsetTransitionDateTime, void *userPtr)
{
    if (nextTimeOffsetTransitionDateTime == nullptr || userPtr == nullptr)
        return false;

    Context* context = static_cast<Context*>(userPtr);
    Timestamp ts;
    return context->getClock().parseString(nextTimeOffsetTransitionDateTime, ts);
}

bool validateSupportLanguages(const char* supportLanguages, void *userPtr)
{
    if (supportLanguages == nullptr)
        return false;

    String languageTag(supportLanguages);

    if (languageTag.empty()) 
        return false;

    // split by ','
    std::vector<String> languages;
    size_t start = 0, end = 0;
    while ((end = languageTag.find(',', start)) != std::string::npos) {
        languages.push_back(languageTag.substr(start, end - start));
        start = end + 1;
    }
    languages.push_back(languageTag.substr(start));

    if (languages.size() == 0) 
        return false;

    for (const String& lang : languages) {
        // split by '-'
        std::vector<String> subtags;
        size_t subStart = 0, subEnd = 0;
        while ((subEnd = lang.find('-', subStart)) != std::string::npos) {
            subtags.push_back(lang.substr(subStart, subEnd - subStart));
            subStart = subEnd + 1;
        }

        subtags.push_back(lang.substr(subStart));
        if (subtags.size() == 0) 
            return false;

        for (size_t i = 0; i < subtags.size(); i++) {
            const String& subtag = subtags[i];        
            if (i == 0) {
                // main language subtag: 2 or 3 letters
                if (subtag.length() < 2 || subtag.length() > 3) 
                {
                    MO_DBG_DEBUG("SupportLanguages: main language subtag length invalid");
                    return false;
                }

                for (char c : subtag) {
                    if (!std::isalpha(static_cast<unsigned char>(c))) 
                    {
                        MO_DBG_DEBUG("SupportLanguages: non-alpha character in main language subtag");
                        return false;
                    }
                }
            } 
            else if (subtag.length() == 1) {
                // single letter usually an extension indicator
                if (!std::isalnum(static_cast<unsigned char>(subtag[0]))) 
                {
                    MO_DBG_DEBUG("SupportLanguages: invalid single-letter subtag");
                    return false;
                }
            } 
            else if (subtag.length() == 2 && std::isalpha(static_cast<unsigned char>(subtag[0]))) {
                // 2-letter region code
                if (!std::isalpha(static_cast<unsigned char>(subtag[0])) || 
                    !std::isalpha(static_cast<unsigned char>(subtag[1])))
                {
                    MO_DBG_DEBUG("SupportLanguages: invalid 2-letter region code subtag");
                    return false;
                }
            } 
            else if (subtag.length() == 3 && std::isdigit(static_cast<unsigned char>(subtag[0]))) {
                // 3-digit region code
                for (char c : subtag) {
                    if (!std::isdigit(static_cast<unsigned char>(c))) 
                    {
                        MO_DBG_DEBUG("SupportLanguages: invalid 3-digit region code subtag");
                        return false;
                    }
                }
            } 
            else {
                // variant or other subtags: alphanumeric
                for (char c : subtag) {
                    if (!std::isalnum(static_cast<unsigned char>(c))) 
                    {
                        MO_DBG_DEBUG("SupportLanguages: invalid character in subtag");
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

} //namespace CaliforniaUtils
} //namespace v16
} //namespace MicroOcpp

#endif //MO_ENABLE_CALIFORNIA
#endif //MO_ENABLE_V16
