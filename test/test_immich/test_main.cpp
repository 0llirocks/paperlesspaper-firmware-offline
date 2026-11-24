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
   TEST_ASSERT_EQUAL_STRING("{\"albumIds\":[\"0b1c2d3e-4f50-4617-8899-aabbccddeeff\"],\"type\":\"IMAGE\",\"size\":2,\"withExif\":false,\"withPeople\":false}",
                            Immich::randomSearchBody(A, 2).c_str());
   TEST_ASSERT_EQUAL_STRING("{\"albumIds\":[\"0b1c2d3e-4f50-4617-8899-aabbccddeeff\"],\"type\":\"IMAGE\"}", Immich::statisticsBody(A).c_str());
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
   const std::string json = std::string("[{\"id\":\"") + A + "\",\"type\":\"IMAGE\",\"owner\":{\"name\":\"x\"},\"exifInfo\":null}," +
                            "{\"id\":\"" + B + "\",\"type\":\"IMAGE\"},{\"id\":\"not-an-id\"}]";
   std::vector<std::string> ids;
   TEST_ASSERT_TRUE(Immich::parseAssetIds(json.c_str(), json.size(), ids));
   TEST_ASSERT_EQUAL(2, ids.size());
   TEST_ASSERT_EQUAL_STRING(A, ids[0].c_str());

   TEST_ASSERT_TRUE(Immich::parseAssetIds("[]", 2, ids));
   TEST_ASSERT_EQUAL(0, ids.size());
   TEST_ASSERT_FALSE(Immich::parseAssetIds("{\"message\":\"x\"}", 15, ids));
   TEST_ASSERT_FALSE(Immich::parseAssetIds("[{", 2, ids));
}

static void test_parse_statistics() {
   uint32_t total = 99;
   TEST_ASSERT_TRUE(Immich::parseStatisticsTotal("{\"total\":1234}", 14, total));
   TEST_ASSERT_EQUAL_UINT32(1234, total);
   TEST_ASSERT_FALSE(Immich::parseStatisticsTotal("{\"count\":1}", 11, total));
   TEST_ASSERT_FALSE(Immich::parseStatisticsTotal("{\"total\":-1}", 12, total));
}

static void test_choose_asset() {
   TEST_ASSERT_EQUAL_STRING(B, Immich::chooseAsset({A, B}, A).c_str());
   TEST_ASSERT_EQUAL_STRING(A, Immich::chooseAsset({A, B}, "").c_str());
   TEST_ASSERT_EQUAL_STRING(A, Immich::chooseAsset({A}, A).c_str()); // the only picture of the album
   TEST_ASSERT_EQUAL_STRING("", Immich::chooseAsset({}, A).c_str());
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
   RUN_TEST(test_parse_statistics);
   RUN_TEST(test_choose_asset);
   return UNITY_END();
}
