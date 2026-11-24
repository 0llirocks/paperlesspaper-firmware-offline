// Unit tests of src/immich.cpp on the PC: pio test -e native
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "immich.h"

static const char *A = "0b1c2d3e-4f50-4617-8899-aabbccddeeff";
static const char *B = "11111111-2222-4333-8444-555555555555";

void setUp() {}
void tearDown() {}

static void test_uuid() {
   TEST_ASSERT_TRUE(Immich::isUuid(A));
   TEST_ASSERT_FALSE(Immich::isUuid("0b1c2d3e-4f50-4617-8899-aabbccddeef"));   // too short
   TEST_ASSERT_FALSE(Immich::isUuid("0b1c2d3e+4f50-4617-8899-aabbccddeeff"));  // separator
   TEST_ASSERT_FALSE(Immich::isUuid("0b1c2d3e-4f50-4617-8899-aabbccddeefg"));  // not hex
}

static void test_album_list() {
   std::vector<std::string> ids;
   TEST_ASSERT_TRUE(Immich::parseAlbumList("", ids));
   TEST_ASSERT_EQUAL(0, ids.size());

   const std::string text = std::string(" ") + A + ",\n" + "https://photos.example/albums/" + B + "/photos/x ; " + A;
   TEST_ASSERT_TRUE(Immich::parseAlbumList(text, ids));
   TEST_ASSERT_EQUAL(2, ids.size()); // the duplicate is removed
   TEST_ASSERT_EQUAL_STRING(A, ids[0].c_str());
   TEST_ASSERT_EQUAL_STRING(B, ids[1].c_str());
   TEST_ASSERT_EQUAL_STRING((std::string(A) + "," + B).c_str(), Immich::joinAlbumList(ids).c_str());

   TEST_ASSERT_TRUE(Immich::parseAlbumList("0B1C2D3E-4F50-4617-8899-AABBCCDDEEFF", ids));
   TEST_ASSERT_EQUAL_STRING(A, ids[0].c_str()); // lower case

   TEST_ASSERT_FALSE(Immich::parseAlbumList(std::string(A) + ",kein-album", ids));
   TEST_ASSERT_EQUAL(0, ids.size());
}

static void test_album_list_limit() {
   std::string text;
   char id[40];
   for (int i = 0; i < 11; i++) {
      snprintf(id, sizeof(id), "00000000-0000-4000-8000-%012d", i);
      if (i == 10) {
         std::vector<std::string> ten;
         TEST_ASSERT_TRUE(Immich::parseAlbumList(text, ten));
         TEST_ASSERT_EQUAL(10, ten.size());
         TEST_ASSERT_EQUAL(Immich::MAX_ALBUM_LIST_LEN, Immich::joinAlbumList(ten).size());
      }
      text += id;
      text += ',';
   }
   std::vector<std::string> ids;
   TEST_ASSERT_FALSE(Immich::parseAlbumList(text, ids)); // 11 albums
}

static void test_base_url() {
   TEST_ASSERT_EQUAL_STRING("https://photos.example", Immich::baseUrl("https://photos.example/").c_str());
   TEST_ASSERT_EQUAL_STRING("https://photos.example", Immich::baseUrl("https://photos.example/api/").c_str());
   TEST_ASSERT_EQUAL_STRING("http://nas:2283", Immich::baseUrl("http://nas:2283").c_str());
}

static void test_bodies() {
   TEST_ASSERT_EQUAL_STRING("{\"filter\":{\"albumIds\":{\"any\":[\"0b1c2d3e-4f50-4617-8899-aabbccddeeff\",\"11111111-2222-4333-8444-555555555555\"]},"
                            "\"type\":{\"eq\":\"IMAGE\"}},\"size\":10,\"withExif\":false,\"withPeople\":false}",
                            Immich::randomSearchBody({A, B}, 10).c_str());
   TEST_ASSERT_EQUAL_STRING("{\"albumIds\":[\"0b1c2d3e-4f50-4617-8899-aabbccddeeff\"],\"type\":\"IMAGE\",\"size\":2,\"withExif\":false,\"withPeople\":false}",
                            Immich::legacyRandomSearchBody(A, 2).c_str());
   TEST_ASSERT_EQUAL_STRING("{\"albumIds\":[\"0b1c2d3e-4f50-4617-8899-aabbccddeeff\"],\"type\":\"IMAGE\"}", Immich::legacyStatisticsBody(A).c_str());
   TEST_ASSERT_EQUAL_STRING("/api/assets/0b1c2d3e-4f50-4617-8899-aabbccddeeff/thumbnail?size=preview", Immich::previewPath(A).c_str());
}

static void test_pick_weighted() {
   const std::vector<uint32_t> counts = {3, 0, 2};
   TEST_ASSERT_EQUAL(0, Immich::pickWeighted(counts, 0));
   TEST_ASSERT_EQUAL(0, Immich::pickWeighted(counts, 2));
   TEST_ASSERT_EQUAL(2, Immich::pickWeighted(counts, 3)); // the empty album is never picked
   TEST_ASSERT_EQUAL(2, Immich::pickWeighted(counts, 4));
   TEST_ASSERT_EQUAL(0, Immich::pickWeighted(counts, 5)); // wraps around
   TEST_ASSERT_EQUAL(-1, Immich::pickWeighted({0, 0}, 7));
   TEST_ASSERT_EQUAL(-1, Immich::pickWeighted({}, 0));
}

static void test_parse_assets() {
   // shortened answer of /api/search/random: an array of assets
   const std::string json = std::string("[{\"id\":\"") + A + "\",\"type\":\"IMAGE\",\"width\":4032,\"height\":3024,\"owner\":{\"name\":\"x\"},\"exifInfo\":null}," +
                            "{\"id\":\"" + B + "\",\"type\":\"IMAGE\",\"width\":null,\"height\":null},{\"id\":\"not-an-id\"}]";
   std::vector<Immich::Asset> assets;
   TEST_ASSERT_TRUE(Immich::parseAssets(json.c_str(), json.size(), assets));
   TEST_ASSERT_EQUAL(2, assets.size());
   TEST_ASSERT_EQUAL_STRING(A, assets[0].id.c_str());
   TEST_ASSERT_EQUAL_UINT32(4032, assets[0].width);
   TEST_ASSERT_EQUAL_UINT32(3024, assets[0].height);
   TEST_ASSERT_EQUAL_UINT32(0, assets[1].width); // not known yet

   TEST_ASSERT_TRUE(Immich::parseAssets("[]", 2, assets));
   TEST_ASSERT_EQUAL(0, assets.size());
   TEST_ASSERT_FALSE(Immich::parseAssets("{\"message\":\"x\"}", 15, assets));
   TEST_ASSERT_FALSE(Immich::parseAssets("[{", 2, assets));
}

static Immich::Asset asset(const char *id, uint32_t width, uint32_t height) {
   Immich::Asset a;
   a.id = id;
   a.width = width;
   a.height = height;
   return a;
}

static void test_suits_display() {
   // 7.3": 800 x 480 (landscape)
   TEST_ASSERT_TRUE(Immich::suitsDisplay(asset(A, 4032, 3024), 800, 480));  // 4:3
   TEST_ASSERT_TRUE(Immich::suitsDisplay(asset(A, 1920, 1080), 800, 480));  // 16:9
   TEST_ASSERT_FALSE(Immich::suitsDisplay(asset(A, 3024, 4032), 800, 480)); // portrait
   TEST_ASSERT_FALSE(Immich::suitsDisplay(asset(A, 3000, 3000), 800, 480)); // square
   TEST_ASSERT_FALSE(Immich::suitsDisplay(asset(A, 6000, 1500), 800, 480)); // panorama
   TEST_ASSERT_FALSE(Immich::suitsDisplay(asset(A, 0, 0), 800, 480));       // unknown
   // 13.3": 1200 x 1600 (portrait)
   TEST_ASSERT_TRUE(Immich::suitsDisplay(asset(A, 3024, 4032), 1200, 1600));
   TEST_ASSERT_TRUE(Immich::suitsDisplay(asset(A, 1080, 1920), 1200, 1600)); // phone, 9:16
   TEST_ASSERT_FALSE(Immich::suitsDisplay(asset(A, 4032, 3024), 1200, 1600));
}

static void test_parse_statistics() {
   uint32_t total = 99;
   TEST_ASSERT_TRUE(Immich::parseStatisticsTotal("{\"total\":1234}", 14, total));
   TEST_ASSERT_EQUAL_UINT32(1234, total);
   TEST_ASSERT_FALSE(Immich::parseStatisticsTotal("{\"count\":1}", 11, total));
   TEST_ASSERT_FALSE(Immich::parseStatisticsTotal("{\"total\":-1}", 12, total));
}

static void test_choose_asset() {
   static const char *C = "22222222-3333-4444-8555-666666666666";
   const Immich::Asset portrait = asset(A, 3024, 4032), landscape = asset(B, 4032, 3024), square = asset(C, 3000, 3000);

   // the first suitable picture, not the first one
   TEST_ASSERT_EQUAL(1, Immich::chooseAsset({portrait, landscape}, "", 800, 480));
   TEST_ASSERT_EQUAL(0, Immich::chooseAsset({portrait, landscape}, "", 1200, 1600));
   // not the picture shown last time, even if it suits ...
   TEST_ASSERT_EQUAL(2, Immich::chooseAsset({portrait, landscape, asset(C, 1920, 1080)}, B, 800, 480));
   // ... then rather the closest of the others: a square comes closer to 5:3 than a portrait
   TEST_ASSERT_EQUAL(2, Immich::chooseAsset({portrait, landscape, square}, B, 800, 480));
   // nothing suits: the closest one
   TEST_ASSERT_EQUAL(1, Immich::chooseAsset({portrait, square}, "", 800, 480));
   // unknown sizes: the first one that is not the last picture
   TEST_ASSERT_EQUAL(1, Immich::chooseAsset({asset(A, 0, 0), asset(B, 0, 0)}, A, 800, 480));
   // the only picture of the albums
   TEST_ASSERT_EQUAL(0, Immich::chooseAsset({landscape}, B, 800, 480));
   TEST_ASSERT_EQUAL(-1, Immich::chooseAsset({}, A, 800, 480));
}

int main(int, char **) {
   UNITY_BEGIN();
   RUN_TEST(test_uuid);
   RUN_TEST(test_album_list);
   RUN_TEST(test_album_list_limit);
   RUN_TEST(test_base_url);
   RUN_TEST(test_bodies);
   RUN_TEST(test_pick_weighted);
   RUN_TEST(test_parse_assets);
   RUN_TEST(test_suits_display);
   RUN_TEST(test_parse_statistics);
   RUN_TEST(test_choose_asset);
   return UNITY_END();
}
