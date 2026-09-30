import pathlib
import unittest


class CacheSecurityGuardrail(unittest.TestCase):
    def test_cache_operations_are_locked_and_reads_are_copied(self):
        root = pathlib.Path(__file__).parents[1]
        helper = (root / "server/plat/gdialobjCacheHelper.hpp").read_text()
        caller = (root / "server/plat/gdialappcache.cpp").read_text()
        self.assertGreaterEqual(helper.count("std::lock_guard<std::mutex> lock(objectsMutex);"), 5)
        self.assertIn("bool findObject(const std::string& appName, AppInfo& entry) const", helper)
        self.assertIn("entry = *it->second;", helper)
        self.assertNotIn("AppInfo* appEntry = ObjectCache->findObject", caller)


if __name__ == "__main__":
    unittest.main()
