import pathlib
import unittest


class UriValidationGuardrail(unittest.TestCase):
    def test_empty_path_components_are_rejected_before_copy(self):
        source = (pathlib.Path(__file__).parents[1] / "server/gdial-rest.c").read_text()
        loop = source.index("for (i = 0; elements[i] != NULL; i++)", source.rindex("g_strsplit(&path[1]"))
        copy = source.index("g_strlcpy(base", loop)
        rejection = source.index("if (elements[i][0] == '\\0')", loop)
        self.assertLess(rejection, copy)
        self.assertIn("while (elements[j] != NULL && i < element_num", source)


if __name__ == "__main__":
    unittest.main()
