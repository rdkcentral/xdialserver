import pathlib
import unittest


class DialDataPathGuardrail(unittest.TestCase):
    def test_registry_rejects_unsafe_application_names(self):
        source = (pathlib.Path(__file__).parents[1] / "server/plat/gdial_app_registry.c").read_text()
        self.assertIn("static gboolean gdial_app_name_is_safe", source)
        self.assertIn("g_return_val_if_fail(gdial_app_name_is_safe(app_name), NULL);", source)

    def test_file_sinks_require_safe_application_names(self):
        source = (pathlib.Path(__file__).parents[1] / "server/gdial-app.c").read_text()
        self.assertIn("g_ascii_isalnum(*cursor)", source)
        self.assertEqual(source.count("g_return_val_if_fail(gdial_app_name_is_safe(app_name), FALSE);"), 3)
        for value in ('*cursor == \'/\'', '*cursor == \'\\\\\''):
            self.assertNotIn(value, source[source.index("static gboolean gdial_app_name_is_safe"):source.index("static guint gdial_app_signals")])


if __name__ == "__main__":
    unittest.main()
