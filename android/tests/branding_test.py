import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'integration/chromium/tools/grit/grit'))
from phishark_brand import brand_message

class BrandTest(unittest.TestCase):
    def test_product(self):
        self.assertEqual(brand_message('Welcome to Chrome', 'Welcome to Cromite', 'IDS_FRE_WELCOME'), 'Welcome to PhiShark Browser')
    def test_links(self):
        self.assertEqual(brand_message('', 'Cromite https://cromite.org/chrome chrome://settings', 'IDS_HELP'), 'PhiShark Browser https://cromite.org/chrome chrome://settings')
    def test_legal(self):
        self.assertEqual(brand_message('Copyright Chromium', 'Copyright Cromite', 'IDS_COPYRIGHT'), 'Copyright Chromium')
    def test_other_product(self):
        self.assertEqual(brand_message('', 'Cromite Web Store', 'IDS_WEB_STORE'), 'Cromite Web Store')
    def test_non_message(self):
        self.assertEqual(brand_message('', 'Cromite', ''), 'Cromite')

if __name__ == '__main__': unittest.main()
