import pathlib
import unittest


class SystemActionGuardrail(unittest.TestCase):
    def test_power_actions_fail_closed(self):
        source = (pathlib.Path(__file__).parents[1] / "server/plat/gdial.cpp").read_text()
        self.assertIn("if (!system_key || system_key[0] == '\\0' || supplied == parsed_query.end())", source)
        self.assertEqual(source.count("if (!is_authorized_system_action(parsed_query))"), 2)
        self.assertNotIn("user provided '%s'", source)
        self.assertIn("difference |= static_cast<unsigned char>", source)


if __name__ == "__main__":
    unittest.main()
