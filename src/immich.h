#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

// Immich (https://immich.app) as picture source (README, "Immich").
//
// Only the parts without network and hardware are here, so that they can be tested on a PC
// (test/test_immich, `pio test -e native`). The requests themselves are made in main.cpp.
namespace Immich {

constexpr size_t MAX_ALBUMS = 10;
constexpr size_t ID_LEN = 36;                                                    // "8-4-4-4-12" UUID
constexpr size_t MAX_ALBUM_LIST_LEN = MAX_ALBUMS * ID_LEN + (MAX_ALBUMS - 1);   // 369: IDs separated by ','
constexpr int RANDOM_COUNT = 2;                                                  // pictures per random search
constexpr size_t MAX_JSON_SIZE = 16384;                                          // largest answer that is read

bool isUuid(const std::string &text);

// Album IDs from a list separated by commas, semicolons, spaces or line breaks. An entry can also
// be the link of an album in the Immich web app ("https://photos.example/albums/<id>").
// The IDs are lower-cased and duplicates removed. false (and `ids` empty): an entry is not an album,
// or there are more than MAX_ALBUMS. An empty list is valid.
bool parseAlbumList(const std::string &text, std::vector<std::string> &ids);
std::string joinAlbumList(const std::vector<std::string> &ids); // "id1,id2"

// "https://photos.example/api/" -> "https://photos.example"
std::string baseUrl(const std::string &url);

// Bodies of POST /api/search/random and POST /api/search/statistics for one album.
// (Several albumIds in one request would only match pictures that are in all of these albums.)
std::string randomSearchBody(const std::string &albumId, int count);
std::string statisticsBody(const std::string &albumId);

// Path of the preview picture (JPEG unless the server is set to WebP).
std::string previewPath(const std::string &assetId);

// The album that holds picture number `pick` (0 .. sum of counts - 1) when all pictures are put in a
// row: so every picture is shown equally often, not every album. -1 if all albums are empty.
int pickWeighted(const std::vector<uint32_t> &counts, uint64_t pick);
uint64_t sum(const std::vector<uint32_t> &counts);

// IDs of the assets in an answer of /api/search/random (a JSON array of assets), in their order.
bool parseAssetIds(const char *json, size_t length, std::vector<std::string> &ids);
// "total" of an answer of /api/search/statistics.
bool parseStatisticsTotal(const char *json, size_t length, uint32_t &total);

// The first ID that is not `last` (the picture shown last time). `last` if it is the only one,
// "" if the list is empty.
std::string chooseAsset(const std::vector<std::string> &ids, const std::string &last);

} // namespace Immich
