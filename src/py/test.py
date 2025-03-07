import unittest
from carp import carp_json_option_validate, carp_table_add_option, carp_table

class TestCarpTableAddOption(unittest.TestCase):
    def test_add_option(self):
        carp_table_add_option("foo", {"requiresArguments": "true", "callback": "none"})
        carp_table_add_option("bar", {"requiresArguments": "false", "callback": "none"})
        self.assertEqual(
            carp_table,
            [{"name": "foo", "requiresArguments": "true", "callback": "none"},
             {"name": "bar", "requiresArguments": "false", "callback": "none"}])

    def test_naming_collision(self):
        with self.assertRaises(SystemExit):
            carp_table_add_option("foo", {"requiresArguments": "true", "callback": "none"})

class TestCarpJsonOptionValidate(unittest.TestCase):
    def test_missing_option_name(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({})
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"requiresArguments": False, "callback": "none"})

    def test_wrong_option_type(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": True})
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"long": 100})

    def test_unrecognized_option_field(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "f", "requiresArguments": False, "callback": "none", "foo": 100})

    def test_missing_required_field(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "f", "requiresArguments": True})
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "f", "callback": "none"})

    def test_wrong_option_spec_type(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "f", "requiresArguments": True, "callback": True})
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "f", "requiresArguments": "asdf", "callback": "none"})

    def test_options_clean(self):
        self.assertEqual(
            carp_json_option_validate({"short": "f", "requiresArguments": False, "callback": "none"}),
            [{"short": "f", "requiresArguments": "false", "callback": "none"}])

        self.assertEqual(
            carp_json_option_validate({"long": "foo", "requiresArguments": False, "callback": "none"}),
            [{"long": "foo", "requiresArguments": "false", "callback": "none"}])

        self.assertEqual(
            carp_json_option_validate({"short": "f", "long": "foo", "requiresArguments": True, "callback": "none"}),
            [{"long": "foo", "requiresArguments": "true", "callback": "none"},
             {"short": "f", "requiresArguments": "true", "callback": "none"}])

    def test_empty_option(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": ""})
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"long": ""})

    def test_short_option_multiple_characters(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "ff"})

    def test_same_short_and_long(self):
        with self.assertRaises(SystemExit):
            carp_json_option_validate({"short": "f", "long": "f"})


if __name__ == '__main__':
    unittest.main()
