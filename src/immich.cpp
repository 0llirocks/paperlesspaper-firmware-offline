#include "immich.h"

#include <ArduinoJson.h>
#include <ctype.h>

namespace Immich {

bool isUuid(const std::string &text) {
   if (text.size() != ID_LEN)
      return false;
   for (size_t i = 0; i < text.size(); i++) {
      const char c = text[i];
      if (i == 8 || i == 13 || i == 18 || i == 23) {
         if (c != '-')
            return false;
      }
      else if (!isxdigit((unsigned char)c)) {
         return false;
      }
   }
   return true;
}

// An album link "https://host/albums/<id>[/...]" -> "<id>", anything else unchanged.
static std::string albumIdOf(const std::string &entry) {
   static const char marker[] = "/albums/";
   const size_t at = entry.find(marker);
   if (at == std::string::npos)
      return entry;
   std::string id = entry.substr(at + sizeof(marker) - 1, ID_LEN);
   return id;
}

bool parseAlbumList(const std::string &text, std::vector<std::string> &ids) {
   ids.clear();
   std::string entry;
   for (size_t i = 0; i <= text.size(); i++) {
      const char c = (i < text.size()) ? text[i] : ',';
      if (c == ',' || c == ';' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
         if (!entry.empty()) {
            std::string id = albumIdOf(entry);
            for (char &ch : id)
               ch = (char)tolower((unsigned char)ch);
            if (!isUuid(id)) {
               ids.clear();
               return false;
            }
            bool known = false;
            for (const std::string &k : ids)
               known = known || k == id;
            if (!known) {
               if (ids.size() == MAX_ALBUMS) {
                  ids.clear();
                  return false;
               }
               ids.push_back(id);
            }
            entry.clear();
         }
      }
      else {
         entry += c;
      }
   }
   return true;
}

std::string joinAlbumList(const std::vector<std::string> &ids) {
   std::string out;
   for (size_t i = 0; i < ids.size(); i++) {
      if (i > 0)
         out += ',';
      out += ids[i];
   }
   return out;
}

std::string baseUrl(const std::string &url) {
   std::string base = url;
   while (!base.empty() && base.back() == '/')
      base.pop_back();
   if (base.size() >= 4 && base.compare(base.size() - 4, 4, "/api") == 0)
      base.erase(base.size() - 4);
   while (!base.empty() && base.back() == '/')
      base.pop_back();
   return base;
}

std::string randomSearchBody(const std::string &albumId, int count) {
   return "{\"albumIds\":[\"" + albumId + "\"],\"type\":\"IMAGE\",\"size\":" + std::to_string(count) +
          ",\"withExif\":false,\"withPeople\":false}";
}

std::string statisticsBody(const std::string &albumId) {
   return "{\"albumIds\":[\"" + albumId + "\"],\"type\":\"IMAGE\"}";
}

std::string previewPath(const std::string &assetId) {
   return "/api/assets/" + assetId + "/thumbnail?size=preview";
}

uint64_t sum(const std::vector<uint32_t> &counts) {
   uint64_t total = 0;
   for (uint32_t c : counts)
      total += c;
   return total;
}

int pickWeighted(const std::vector<uint32_t> &counts, uint64_t pick) {
   const uint64_t total = sum(counts);
   if (total == 0)
      return -1;
   pick %= total;
   for (size_t i = 0; i < counts.size(); i++) {
      if (pick < counts[i])
         return (int)i;
      pick -= counts[i];
   }
   return -1; // not reached
}

bool parseAssetIds(const char *json, size_t length, std::vector<std::string> &ids) {
   ids.clear();
   JsonDocument filter;
   filter[0]["id"] = true; // an asset carries much more (owner, dates, ...), only the ID is kept
   JsonDocument doc;
   if (deserializeJson(doc, json, length, DeserializationOption::Filter(filter)))
      return false;
   if (!doc.is<JsonArray>())
      return false;
   for (JsonVariant asset : doc.as<JsonArray>()) {
      const char *id = asset["id"];
      if (id != nullptr && isUuid(id))
         ids.push_back(id);
   }
   return true;
}

bool parseStatisticsTotal(const char *json, size_t length, uint32_t &total) {
   JsonDocument doc;
   if (deserializeJson(doc, json, length))
      return false;
   if (!doc["total"].is<long long>())
      return false;
   const long long t = doc["total"].as<long long>();
   if (t < 0)
      return false;
   total = (t > 0xFFFFFFFFLL) ? 0xFFFFFFFFu : (uint32_t)t;
   return true;
}

std::string chooseAsset(const std::vector<std::string> &ids, const std::string &last) {
   for (const std::string &id : ids) {
      if (id != last)
         return id;
   }
   return ids.empty() ? std::string() : ids.front();
}

} // namespace Immich
