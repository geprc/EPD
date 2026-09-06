"""网页资源与 ESP32 固件之间的轻量契约测试。"""

import re
import unittest
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlparse


PROJECT_ROOT = Path(__file__).resolve().parents[1]
WEB_ROOT = PROJECT_ROOT / "data" / "www"
HTML_PATH = WEB_ROOT / "index.html"
JS_PATH = WEB_ROOT / "js" / "draw.js"
FIRMWARE_PATH = PROJECT_ROOT / "src" / "main.cpp"
MAX_WEB_BYTES = 256 * 1024


class PageParser(HTMLParser):
    """收集入口页面中的资源地址和元素 ID。"""

    def __init__(self):
        super().__init__()
        self.ids = []
        self.assets = []

    def handle_starttag(self, tag, attrs):
        attributes = dict(attrs)
        if "id" in attributes:
            self.ids.append(attributes["id"])
        if tag == "link" and attributes.get("rel") == "stylesheet":
            self.assets.append(attributes.get("href", ""))
        elif tag == "script" and attributes.get("src"):
            self.assets.append(attributes["src"])
        elif tag == "img" and attributes.get("src"):
            self.assets.append(attributes["src"])


def read_text(path):
    return path.read_text(encoding="utf-8")


def javascript_constant(source, name):
    match = re.search(rf"const\s+{re.escape(name)}\s*=\s*(\d+)\s*;", source)
    if not match:
        raise AssertionError(f"JavaScript 中未找到常量 {name}")
    return int(match.group(1))


def firmware_constant(source, name):
    match = re.search(
        rf"constexpr\s+uint\d+_t\s+{re.escape(name)}\s*=\s*(\d+)\s*;",
        source,
    )
    if not match:
        raise AssertionError(f"固件中未找到常量 {name}")
    return int(match.group(1))


class WebContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = read_text(HTML_PATH)
        cls.javascript = read_text(JS_PATH)
        cls.firmware = read_text(FIRMWARE_PATH)
        cls.page = PageParser()
        cls.page.feed(cls.html)

    def test_html_has_unique_ids(self):
        self.assertEqual(len(self.page.ids), len(set(self.page.ids)))

    def test_javascript_elements_exist_in_html(self):
        queried_ids = re.findall(
            r"querySelector\(\s*['\"]#([A-Za-z][\w-]*)['\"]\s*\)",
            self.javascript,
        )
        self.assertTrue(queried_ids, "没有检测到 JavaScript DOM 查询")
        self.assertEqual(set(), set(queried_ids) - set(self.page.ids))

    def test_required_editor_controls_exist(self):
        required = {
            "canvas", "pen", "eraser", "brush-size", "text-input",
            "add-text", "image-input", "invert", "undo", "clear", "send",
        }
        self.assertEqual(set(), required - set(self.page.ids))

    def test_bitmap_module_loads_before_editor(self):
        scripts = re.findall(r"<script\s+src=['\"]([^'\"]+)", self.html)
        self.assertIn("js/bitmap.js", scripts)
        self.assertIn("js/draw.js", scripts)
        self.assertLess(scripts.index("js/bitmap.js"), scripts.index("js/draw.js"))

    def test_assets_are_local_and_exist(self):
        self.assertTrue(self.page.assets, "入口页面没有引用任何资源")
        for asset in self.page.assets:
            with self.subTest(asset=asset):
                parsed = urlparse(asset)
                self.assertFalse(parsed.scheme or parsed.netloc, "不应依赖远程资源")
                self.assertFalse(asset.startswith("/"), "资源应使用相对路径")
                self.assertTrue((WEB_ROOT / parsed.path).is_file())

    def test_display_dimensions_match_driver(self):
        self.assertEqual(200, javascript_constant(self.javascript, "WIDTH"))
        self.assertEqual(200, javascript_constant(self.javascript, "HEIGHT"))
        self.assertIn("GxEPD2_154::WIDTH", self.firmware)
        self.assertIn("GxEPD2_154::HEIGHT", self.firmware)

    def test_websocket_port_matches_firmware(self):
        self.assertEqual(
            firmware_constant(self.firmware, "kWebSocketPort"),
            javascript_constant(self.javascript, "WEBSOCKET_PORT"),
        )

    def test_bitmap_payload_is_5000_bytes(self):
        width = javascript_constant(self.javascript, "WIDTH")
        height = javascript_constant(self.javascript, "HEIGHT")
        self.assertEqual(5000, width * height // 8)
        self.assertIn("length != config::kBitmapSize", self.firmware)

    def test_local_preview_avoids_websocket(self):
        self.assertIn("location.protocol === 'file:'", self.javascript)

    def test_web_content_fits_budget(self):
        total = sum(path.stat().st_size for path in WEB_ROOT.rglob("*") if path.is_file())
        self.assertLessEqual(total, MAX_WEB_BYTES)

    def test_legacy_experiments_are_removed(self):
        self.assertFalse((PROJECT_ROOT / "src" / "others.x").exists())
        self.assertFalse((WEB_ROOT / "new_index.html").exists())


if __name__ == "__main__":
    unittest.main()
