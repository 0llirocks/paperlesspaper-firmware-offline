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

std::string randomSearchBody(const std::vector<std::string> &albumIds, int count) {
   std::string ids;
   for (size_t i = 0; i < albumIds.size(); i++) {
      if (i > 0)
         ids += ',';
      ids += "\"" + albumIds[i] + "\"";
   }
   // The flat fields of older versions (albumIds, type) must not be mixed with "filter".
   return "{\"filter\":{\"albumIds\":{\"any\":[" + ids + "]},\"type\":{\"eq\":\"IMAGE\"}},\"size\":" + std::to_string(count) +
          ",\"withExif\":false,\"withPeople\":false}";
}

std::string legacyRandomSearchBody(const std::string &albumId, int count) {
   return "{\"albumIds\":[\"" + albumId + "\"],\"type\":\"IMAGE\",\"size\":" + std::to_string(count) +
          ",\"withExif\":false,\"withPeople\":false}";
}

std::string legacyStatisticsBody(const std::string &albumId) {
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

bool parseAssets(const char *json, size_t length, std::vector<Asset> &assets) {
   assets.clear();
   JsonDocument filter; // an asset carries much more (owner, dates, paths, ...): only this is kept
   filter[0]["id"] = true;
   filter[0]["width"] = true;
   filter[0]["height"] = true;
   JsonDocument doc;
   if (deserializeJson(doc, json, length, DeserializationOption::Filter(filter)))
      return false;
   if (!doc.is<JsonArray>())
      return false;
   for (JsonVariant item : doc.as<JsonArray>()) {
      const char *id = item["id"];
      if (id == nullptr || !isUuid(id))
         continue;
      Asset asset;
      asset.id = id;
      // null while Immich has not read the metadata yet
      if (item["width"].is<uint32_t>() && item["height"].is<uint32_t>()) {
         asset.width = item["width"].as<uint32_t>();
         asset.height = item["height"].as<uint32_t>();
      }
      assets.push_back(asset);
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

// How far the aspect ratio of a picture is from the display: 1 = the same, 1.25 = a 4:3 photo on
// the 5:3 display. A picture of unknown size counts as far away.
static double aspectDistance(const Asset &asset, int displayWidth, int displayHeight) {
   if (asset.width == 0 || asset.height == 0 || displayWidth <= 0 || displayHeight <= 0)
      return 1e9;
   const double picture = (double)asset.width / asset.height;
   const double display = (double)displayWidth / displayHeight;
   return (picture > display) ? picture / display : display / picture;
}

// A 4:3 or 16:9 photo suits the 5:3 display (narrow stripes), a square or panorama one does not.
// Above 1 this also means: the same orientation as the display.
static constexpr double MAX_SUITABLE_DISTANCE = 1.5;

bool suitsDisplay(const Asset &asset, int displayWidth, int displayHeight) {
   return aspectDistance(asset, displayWidth, displayHeight) <= MAX_SUITABLE_DISTANCE;
}

int chooseAsset(const std::vector<Asset> &assets, const std::string &last, int displayWidth, int displayHeight) {
   int closest = -1;
   double closestDistance = 0;
   for (size_t i = 0; i < assets.size(); i++) {
      if (assets[i].id == last)
         continue;
      const double distance = aspectDistance(assets[i], displayWidth, displayHeight);
      if (distance <= MAX_SUITABLE_DISTANCE)
         return (int)i; // the search order is random: the first suitable one
      if (closest < 0 || distance < closestDistance) {
         closest = (int)i;
         closestDistance = distance;
      }
   }
   if (closest >= 0)
      return closest;
   return assets.empty() ? -1 : 0; // only the picture shown last time
}

} // namespace Immich
