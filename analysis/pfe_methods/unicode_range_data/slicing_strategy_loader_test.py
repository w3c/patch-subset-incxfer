"""Unit tests for the slicing_strategy_loader module."""

import unittest

from analysis.pfe_methods.unicode_range_data import slicing_strategy_loader


class SlicingStrategyLoaderTest(unittest.TestCase):

  def test_load_slicing_strategy_caches(self):
    strategy_1 = slicing_strategy_loader.load_slicing_strategy("non_cjk_slices")
    strategy_2 = slicing_strategy_loader.load_slicing_strategy("non_cjk_slices")
    strategy_3 = slicing_strategy_loader.load_slicing_strategy(
        "japanese_slices")
    self.assertIs(strategy_1, strategy_2)
    self.assertNotEqual(strategy_1, strategy_3)

  def test_load_slicing_strategy(self):
    # A slicing strategy should be a list of sets of integers
    strategy = slicing_strategy_loader.load_slicing_strategy("non_cjk_slices")
    self.assertEqual(type(strategy), list)
    self.assertGreater(len(strategy), 20)

    self.assertEqual(type(strategy[0]), set)
    for code_point in strategy[0]:
      self.assertEqual(type(code_point), int)

  def test_get_available_strategies(self):
    strategies = slicing_strategy_loader.get_available_strategies()
    # Should at least have 1 non cjk, 3 chinese, 1 jp, 1 kr
    self.assertGreaterEqual(len(strategies), 6)

    # spot check a couple
    self.assertIn("non_cjk_slices", strategies)
    self.assertIn("japanese_slices", strategies)

  def test_all_available_strategies_can_load(self):
    for strategy in slicing_strategy_loader.get_available_strategies():
      self.assertTrue(slicing_strategy_loader.load_slicing_strategy(strategy))

  def test_codepoints_in_font(self):
    with open("patch_subset/testdata/Roboto-Regular.abcd.ttf",
              "rb") as font_file:
      font_bytes = font_file.read()

    codepoints = slicing_strategy_loader.codepoints_in_font(font_bytes)
    self.assertEqual(codepoints, {0x61, 0x62, 0x63, 0x64})

  def test_slicing_strategy_for_font_non_cjk(self):
    with open("patch_subset/testdata/Roboto-Regular.ttf", "rb") as font_file:
      font_bytes = font_file.read()
    self.assertEqual(
        "non_cjk_slices",
        slicing_strategy_loader.slicing_strategy_for_font(font_bytes))

  def test_slicing_strategy_for_font_cjk(self):
    with open("patch_subset/testdata/NotoSansJP-Regular.otf",
              "rb") as font_file:
      font_bytes = font_file.read()
    self.assertEqual(
        "japanese_slices",
        slicing_strategy_loader.slicing_strategy_for_font(font_bytes))


if __name__ == '__main__':
  unittest.main()
