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
// Pictures per random search. Immich cannot search by orientation, so the frame picks one that
// suits its display from these (about 1 KB of JSON each).
constexpr int RANDOM_COUNT = 10;
constexpr size_t MAX_JSON_SIZE = 24576; // largest answer that is read

bool isUuid(const std::string &text);

// Album IDs from a list separated by commas, semicolons, spaces or line breaks. An entry can also
// be the link of an album in the Immich web app ("https://photos.example/albums/<id>").
// The IDs are lower-cased and duplicates removed. false (and `ids` empty): an entry is not an album,
// or there are more than MAX_ALBUMS. An empty list is valid.
bool parseAlbumList(const std::string &text, std::vector<std::string> &ids);
std::string joinAlbumList(const std::vector<std::string> &ids); // "id1,id2"

// "https://photos.example/api/" -> "https://photos.example"
std::string baseUrl(const std::string &url);

// Body of POST /api/search/random in the search shape of Immich 3.2 and later: random pictures
// of all albums together (albumIds "any"), so every picture is equally likely.
std::string randomSearchBody(const std::vector<std::string> &albumIds, int count);

// Older Immich: several albumIds would only match pictures that are in all of these albums, so
// the frame counts the pictures of every album (statistics), draws one album by that number
// (pickWeighted) and searches in that album alone.
std::string legacyRandomSearchBody(const std::string &albumId, int count);
std::string legacyStatisticsBody(const std::string &albumId);

// Path of the preview picture (JPEG unless the server is set to WebP).
std::string previewPath(const std::string &assetId);

// The album that holds picture number `pick` (0 .. sum of counts - 1) when all pictures are put in a
// row: so every picture is shown equally often, not every album. -1 if all albums are empty.
int pickWeighted(const std::vector<uint32_t> &counts, uint64_t pick);
uint64_t sum(const std::vector<uint32_t> &counts);

struct Asset {
   std::string id;
   uint32_t width = 0;  // as shown (Immich applies the EXIF orientation and edits); 0: unknown
   uint32_t height = 0;
};

// Assets in an answer of /api/search/random (a JSON array of assets), in their (random) order.
bool parseAssets(const char *json, size_t length, std::vector<Asset> &assets);
// "total" of an answer of /api/search/statistics.
bool parseStatisticsTotal(const char *json, size_t length, uint32_t &total);

// Whether a picture of this size has the orientation of the display (both landscape or both
// portrait), so that it fills the display apart from small stripes.
bool suitsDisplay(const Asset &asset, int displayWidth, int displayHeight);

// The picture to show, in this order:
//   1. the first one that suits the display and is not `last` (the picture shown last time),
//   2. otherwise the one whose aspect ratio comes closest to the display, not `last`,
//   3. `last` if it is the only one.
// Returns the index in `assets`, -1 if the list is empty.
int chooseAsset(const std::vector<Asset> &assets, const std::string &last, int displayWidth, int displayHeight);

} // namespace Immich
