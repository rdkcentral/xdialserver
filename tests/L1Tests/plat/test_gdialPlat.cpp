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
 * @file test_gdialPlat.cpp
 * @brief Unit tests for GDialAppRegistry (server/plat/gdial_app_registry.c)
 *
 * Functions under test:
 *   gdial_app_registry_new             - object construction
 *   gdial_app_registry_is_allowed_origin - origin allow-list logic
 *   gdial_app_regstry_dispose          - resource cleanup
 */

#include <gtest/gtest.h>

extern "C" {
#include <glib.h>
#include "gdial_app_registry.h"
}

/* ================================================================== */
/* Fixture: auto-disposes the registry created in each test            */
/* ================================================================== */
class GDialAppRegistryTest : public ::testing::Test {
protected:
    GDialAppRegistry *registry = nullptr;

    void TearDown() override {
        if (registry) {
            gdial_app_regstry_dispose(registry);
            registry = nullptr;
        }
    }
};

/* ================================================================== */
/* gdial_app_registry_new                                              */
/* ================================================================== */

TEST_F(GDialAppRegistryTest, New_ValidNameReturnsNonNull) {
    registry = gdial_app_registry_new("Netflix", nullptr, nullptr,
                                      TRUE, FALSE, nullptr);
    ASSERT_NE(registry, nullptr);
}

TEST_F(GDialAppRegistryTest, New_AppNameStoredCorrectly) {
    registry = gdial_app_registry_new("YouTube", nullptr, nullptr,
                                      TRUE, FALSE, nullptr);
    ASSERT_NE(registry, nullptr);
    EXPECT_STREQ(registry->name, "YouTube");
}

TEST_F(GDialAppRegistryTest, New_NullNameReturnsNull) {
    GDialAppRegistry *r = gdial_app_registry_new(nullptr, nullptr, nullptr,
                                                  TRUE, FALSE, nullptr);
    EXPECT_EQ(r, nullptr);
}

TEST_F(GDialAppRegistryTest, New_NonSingletonReturnsNull) {
    /* is_singleton=FALSE is rejected by g_return_val_if_fail */
    GDialAppRegistry *r = gdial_app_registry_new("App", nullptr, nullptr,
                                                  FALSE, FALSE, nullptr);
    EXPECT_EQ(r, nullptr);
}

TEST_F(GDialAppRegistryTest, New_IsSingletonFlagSet) {
    registry = gdial_app_registry_new("App", nullptr, nullptr,
                                      TRUE, FALSE, nullptr);
    ASSERT_NE(registry, nullptr);
    EXPECT_TRUE(registry->is_singleton);
}

TEST_F(GDialAppRegistryTest, New_UseAdditionalDataFlagSet) {
    registry = gdial_app_registry_new("App", nullptr, nullptr,
                                      TRUE, TRUE, nullptr);
    ASSERT_NE(registry, nullptr);
    EXPECT_TRUE(registry->use_additional_data);
}

TEST_F(GDialAppRegistryTest, New_PropertiesCopiedIntoRegistry) {
    GHashTable *props = g_hash_table_new_full(
            g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(props, g_strdup("version"), g_strdup("2.0"));

    registry = gdial_app_registry_new("App", nullptr, props,
                                      TRUE, FALSE, nullptr);
    g_hash_table_destroy(props);

    ASSERT_NE(registry, nullptr);
    ASSERT_NE(registry->properties, nullptr);
    EXPECT_STREQ(
        (gchar *)g_hash_table_lookup(registry->properties, "version"),
        "2.0");
}

TEST_F(GDialAppRegistryTest, New_NullPropertiesLeavesNullPropertyTable) {
    registry = gdial_app_registry_new("App", nullptr, nullptr,
                                      TRUE, FALSE, nullptr);
    ASSERT_NE(registry, nullptr);
    EXPECT_EQ(registry->properties, nullptr);
}

/* ================================================================== */
/* gdial_app_registry_is_allowed_origin                                */
/* ================================================================== */

TEST_F(GDialAppRegistryTest, IsAllowedOrigin_NullRegistryReturnsFalse) {
    EXPECT_FALSE(gdial_app_registry_is_allowed_origin(
                     nullptr, "http://example.com"));
}

TEST_F(GDialAppRegistryTest, IsAllowedOrigin_EmptyAllowListPermitsAll) {
    /* No allowed_origins → all origins are accepted */
    registry = gdial_app_registry_new("App", nullptr, nullptr,
                                      TRUE, FALSE, nullptr);
    ASSERT_NE(registry, nullptr);
    EXPECT_TRUE(gdial_app_registry_is_allowed_origin(
                    registry, "http://any.origin.com"));
}

TEST_F(GDialAppRegistryTest, IsAllowedOrigin_SuffixMatchPermitsOrigin) {
    GList *origins = g_list_append(nullptr, g_strdup("netflix.com"));
    registry = gdial_app_registry_new("Netflix", nullptr, nullptr,
                                      TRUE, FALSE, origins);
    g_list_free_full(origins, g_free);
    ASSERT_NE(registry, nullptr);
    EXPECT_TRUE(gdial_app_registry_is_allowed_origin(
                    registry, "https://www.netflix.com"));
}

TEST_F(GDialAppRegistryTest, IsAllowedOrigin_NonMatchingOriginDenied) {
    GList *origins = g_list_append(nullptr, g_strdup("netflix.com"));
    registry = gdial_app_registry_new("Netflix", nullptr, nullptr,
                                      TRUE, FALSE, origins);
    g_list_free_full(origins, g_free);
    ASSERT_NE(registry, nullptr);
    EXPECT_FALSE(gdial_app_registry_is_allowed_origin(
                     registry, "https://malicious.example.com"));
}

TEST_F(GDialAppRegistryTest, IsAllowedOrigin_NullOriginDenied) {
    GList *origins = g_list_append(nullptr, g_strdup("netflix.com"));
    registry = gdial_app_registry_new("Netflix", nullptr, nullptr,
                                      TRUE, FALSE, origins);
    g_list_free_full(origins, g_free);
    ASSERT_NE(registry, nullptr);
    /* NULL header_origin → GDIAL_STR_ENDS_WITH returns FALSE */
    EXPECT_FALSE(gdial_app_registry_is_allowed_origin(registry, nullptr));
}

/* ================================================================== */
/* gdial_app_regstry_dispose                                           */
/* ================================================================== */

TEST(GDialAppRegistryDisposeTest, Dispose_NullDoesNotCrash) {
    /* g_return_if_fail(app_registry != NULL) must not segfault */
    gdial_app_regstry_dispose(nullptr);
}

TEST(GDialAppRegistryDisposeTest, Dispose_ValidRegistryDoesNotCrash) {
    GDialAppRegistry *r = gdial_app_registry_new("App", nullptr, nullptr,
                                                  TRUE, FALSE, nullptr);
    ASSERT_NE(r, nullptr);
    gdial_app_regstry_dispose(r); /* must not crash or leak under valgrind */
}

TEST(GDialAppRegistryDisposeTest, Dispose_WithPropertiesDoesNotCrash) {
    GHashTable *props = g_hash_table_new_full(
            g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(props, g_strdup("k"), g_strdup("v"));
    GDialAppRegistry *r = gdial_app_registry_new("App", nullptr, props,
                                                  TRUE, FALSE, nullptr);
    g_hash_table_destroy(props);
    ASSERT_NE(r, nullptr);
    gdial_app_regstry_dispose(r);
}

