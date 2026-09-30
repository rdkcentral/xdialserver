import pathlib
import unittest


class RegistrationDispatchGuardrail(unittest.TestCase):
    def test_registry_mutation_runs_on_main_context(self):
        source = (pathlib.Path(__file__).parents[1] / "server/gdialservice.cpp").read_text()
        callback = source.index("server_register_application_on_main")
        unregister = source.index("gdial_rest_server_unregister_all_apps", callback)
        attach = source.index("g_source_attach(source, server_main_context)", unregister)
        wait = source.index("dispatch.condition.wait", attach)
        self.assertLess(callback, unregister)
        self.assertLess(attach, wait)
        self.assertIn("g_main_context_is_owner(server_main_context)", source)


if __name__ == "__main__":
    unittest.main()
