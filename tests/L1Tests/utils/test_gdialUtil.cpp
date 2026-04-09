/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file test_gdialUtil.cpp
 * @brief Unit tests for gdial-util.c (server/gdial-util.h)
 *
 * Functions under test:
 *   gdial_util_is_ascii_printable
 *   gdial_util_str_str_hashtable_to_string
 *   gdial_util_str_str_hashtable_from_string
 *   gdial_util_str_str_hashtable_to_xml_string
 *   gdial_util_str_str_hashtable_dup
 *   gdial_util_str_str_hashtable_equal
 *   gdial_util_str_str_hashtable_merge
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include <glib.h>
#include "gdial-util.h"
}

/* ================================================================== */
/* Fixture: owns two GHashTable* and destroys them in TearDown         */
/* ================================================================== */
class GDialUtilHashTableTest : public ::testing::Test {
protected:
    GHashTable *ht1 = nullptr;
    GHashTable *ht2 = nullptr;

    void SetUp() override {
        ht1 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        ht2 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }

    void TearDown() override {
        if (ht1) { g_hash_table_destroy(ht1); ht1 = nullptr; }
        if (ht2) { g_hash_table_destroy(ht2); ht2 = nullptr; }
    }
};

/* ================================================================== */
/* gdial_util_is_ascii_printable                                       */
/* ================================================================== */

TEST(GDialUtilTest, IsPrintable_AllPrintableChars) {
    const gchar *s = "Hello World!";
    EXPECT_TRUE(gdial_util_is_ascii_printable(s, strlen(s)));
}

TEST(GDialUtilTest, IsPrintable_WithWhitespace) {
    /* Tabs and newlines count as g_ascii_isspace → should pass */
    const gchar *s = "foo\tbar\n";
    EXPECT_TRUE(gdial_util_is_ascii_printable(s, strlen(s)));
}

TEST(GDialUtilTest, IsPrintable_NonPrintableControlChar) {
    const gchar data[] = { '\x01', '\x02', '\0' };
    EXPECT_FALSE(gdial_util_is_ascii_printable(data, 2));
}

TEST(GDialUtilTest, IsPrintable_NullDataReturnsFalse) {
    EXPECT_FALSE(gdial_util_is_ascii_printable(nullptr, 4));
}

TEST(GDialUtilTest, IsPrintable_ZeroLengthReturnsFalse) {
    EXPECT_FALSE(gdial_util_is_ascii_printable("abc", 0));
}

/* ================================================================== */
/* gdial_util_str_str_hashtable_to_string                              */
/* ================================================================== */

TEST_F(GDialUtilHashTableTest, ToStr_ContainsKeyAndValue) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v"));
    gsize len = 0;
    gchar *s = gdial_util_str_str_hashtable_to_string(ht1, "=", FALSE, &len);
    ASSERT_NE(s, nullptr);
    EXPECT_NE(g_strstr_len(s, -1, "k"), nullptr);
    EXPECT_NE(g_strstr_len(s, -1, "v"), nullptr);
    EXPECT_GT(len, (gsize)0);
    g_free(s);
}

TEST_F(GDialUtilHashTableTest, ToStr_NullDelimiterDefaultsToSpace) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v"));
    gsize len = 0;
    gchar *s = gdial_util_str_str_hashtable_to_string(ht1, nullptr, FALSE, &len);
    ASSERT_NE(s, nullptr);
    EXPECT_NE(g_strstr_len(s, -1, " "), nullptr);
    g_free(s);
}

TEST_F(GDialUtilHashTableTest, ToStr_WithNewline) {
    g_hash_table_insert(ht1, g_strdup("key"), g_strdup("val"));
    gsize len = 0;
    gchar *s = gdial_util_str_str_hashtable_to_string(ht1, "=", TRUE, &len);
    ASSERT_NE(s, nullptr);
    EXPECT_NE(g_strstr_len(s, -1, "\r\n"), nullptr);
    g_free(s);
}

TEST_F(GDialUtilHashTableTest, ToStr_NullTableReturnsNull) {
    gsize len = 0;
    EXPECT_EQ(gdial_util_str_str_hashtable_to_string(nullptr, "=", FALSE, &len), nullptr);
}

TEST_F(GDialUtilHashTableTest, ToStr_NullLengthReturnsNull) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v"));
    EXPECT_EQ(gdial_util_str_str_hashtable_to_string(ht1, "=", FALSE, nullptr), nullptr);
}

/* ================================================================== */
/* gdial_util_str_str_hashtable_from_string                            */
/* ================================================================== */

TEST_F(GDialUtilHashTableTest, FromStr_ParsesSinglePair) {
    /* Format expected by the parser: "key val\r\n" */
    const gchar *str = "mykey myval\r\n";
    EXPECT_TRUE(gdial_util_str_str_hashtable_from_string(str, strlen(str), ht1));
    EXPECT_STREQ((gchar *)g_hash_table_lookup(ht1, "mykey"), "myval");
}

TEST_F(GDialUtilHashTableTest, FromStr_NullStringReturnsFalse) {
    EXPECT_FALSE(gdial_util_str_str_hashtable_from_string(nullptr, 4, ht1));
}

TEST_F(GDialUtilHashTableTest, FromStr_NullTableReturnsFalse) {
    EXPECT_FALSE(gdial_util_str_str_hashtable_from_string("k v\r\n", 6, nullptr));
}

/* ================================================================== */
/* gdial_util_str_str_hashtable_to_xml_string                          */
/* ================================================================== */

TEST_F(GDialUtilHashTableTest, ToXml_ContainsAttrEqualsValue) {
    g_hash_table_insert(ht1, g_strdup("xmlns"), g_strdup("urn:test"));
    gsize len = 0;
    gchar *s = gdial_util_str_str_hashtable_to_xml_string(ht1, &len);
    ASSERT_NE(s, nullptr);
    EXPECT_NE(g_strstr_len(s, -1, "xmlns"), nullptr);
    EXPECT_NE(g_strstr_len(s, -1, "urn:test"), nullptr);
    g_free(s);
}

TEST_F(GDialUtilHashTableTest, ToXml_NullTableReturnsNull) {
    gsize len = 0;
    EXPECT_EQ(gdial_util_str_str_hashtable_to_xml_string(nullptr, &len), nullptr);
}

/* ================================================================== */
/* gdial_util_str_str_hashtable_dup                                    */
/* ================================================================== */

TEST_F(GDialUtilHashTableTest, Dup_ClonesAllEntries) {
    g_hash_table_insert(ht1, g_strdup("a"), g_strdup("1"));
    g_hash_table_insert(ht1, g_strdup("b"), g_strdup("2"));
    GHashTable *copy = gdial_util_str_str_hashtable_dup(ht1);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(g_hash_table_size(copy), (guint)2);
    EXPECT_STREQ((gchar *)g_hash_table_lookup(copy, "a"), "1");
    EXPECT_STREQ((gchar *)g_hash_table_lookup(copy, "b"), "2");
    g_hash_table_destroy(copy);
}

TEST_F(GDialUtilHashTableTest, Dup_IsDeepCopyNotSamePointer) {
    g_hash_table_insert(ht1, g_strdup("key"), g_strdup("val"));
    GHashTable *copy = gdial_util_str_str_hashtable_dup(ht1);
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy, ht1);
    g_hash_table_destroy(copy);
}

TEST(GDialUtilTest, Dup_NullReturnsNull) {
    EXPECT_EQ(gdial_util_str_str_hashtable_dup(nullptr), nullptr);
}

/* ================================================================== */
/* gdial_util_str_str_hashtable_equal                                  */
/* ================================================================== */

TEST_F(GDialUtilHashTableTest, Equal_EmptyTablesAreEqual) {
    EXPECT_TRUE(gdial_util_str_str_hashtable_equal(ht1, ht2));
}

TEST_F(GDialUtilHashTableTest, Equal_SamePointerIsEqual) {
    g_hash_table_insert(ht1, g_strdup("x"), g_strdup("y"));
    EXPECT_TRUE(gdial_util_str_str_hashtable_equal(ht1, ht1));
}

TEST_F(GDialUtilHashTableTest, Equal_IdenticalContentsAreEqual) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v"));
    g_hash_table_insert(ht2, g_strdup("k"), g_strdup("v"));
    EXPECT_TRUE(gdial_util_str_str_hashtable_equal(ht1, ht2));
}

TEST_F(GDialUtilHashTableTest, Equal_DifferentValuesNotEqual) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v1"));
    g_hash_table_insert(ht2, g_strdup("k"), g_strdup("v2"));
    EXPECT_FALSE(gdial_util_str_str_hashtable_equal(ht1, ht2));
}

TEST_F(GDialUtilHashTableTest, Equal_DifferentSizeNotEqual) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v"));
    /* ht2 is empty */
    EXPECT_FALSE(gdial_util_str_str_hashtable_equal(ht1, ht2));
}

TEST_F(GDialUtilHashTableTest, Equal_NullRightNotEqual) {
    EXPECT_FALSE(gdial_util_str_str_hashtable_equal(ht1, nullptr));
}

TEST_F(GDialUtilHashTableTest, Equal_NullLeftNotEqual) {
    EXPECT_FALSE(gdial_util_str_str_hashtable_equal(nullptr, ht2));
}

/* ================================================================== */
/* gdial_util_str_str_hashtable_merge                                  */
/* ================================================================== */

TEST_F(GDialUtilHashTableTest, Merge_AddsSrcKeysToDst) {
    /* merge() reuses src key/value pointers inside dst via g_hash_table_replace.
     * Use a non-owning src table here to avoid double-free in fixture teardown. */
    GHashTable *src = g_hash_table_new(g_str_hash, g_str_equal);

    g_hash_table_insert(ht1, g_strdup("a"), g_strdup("1"));
    g_hash_table_insert(src, g_strdup("b"), g_strdup("2"));

    GHashTable *result = gdial_util_str_str_hashtable_merge(ht1, src);

    EXPECT_EQ(result, ht1);
    EXPECT_EQ(g_hash_table_size(ht1), (guint)2);
    EXPECT_STREQ((gchar *)g_hash_table_lookup(ht1, "b"), "2");

    g_hash_table_destroy(src);
}

TEST_F(GDialUtilHashTableTest, Merge_NullSrcReturnsDstUnchanged) {
    g_hash_table_insert(ht1, g_strdup("k"), g_strdup("v"));
    GHashTable *result = gdial_util_str_str_hashtable_merge(ht1, nullptr);
    EXPECT_EQ(result, ht1);
    EXPECT_EQ(g_hash_table_size(ht1), (guint)1);
}

