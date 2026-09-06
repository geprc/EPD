# 测试

`test_web_contract.py` 使用 Python 标准库检查网页资源和固件之间的协议约定，
不需要连接 ESP32：

```bash
python3 -m unittest discover -s test -p "test_*.py" -v
```

测试范围：

- HTML 引用的本地 CSS、JavaScript 和图片均存在
- 页面不依赖 CDN，能够离线运行
- JavaScript 使用的屏幕尺寸和 WebSocket 端口与固件一致
- JavaScript 查询的 DOM 元素均存在且 ID 不重复
- WebSocket 传输的 1-bit 位图大小固定为 5,000 bytes
- 网页资源总量保持在适合 SPIFFS 的范围内

像素算法测试直接调用网页实际使用的 `bitmap.js`，覆盖位图位序、阈值边界、
不足一字节的补零、图片二值化、反相和撤销历史上限：

```bash
node test/test_bitmap.js
```

没有安装 Node.js 时，也可以用浏览器直接打开 `test/test_bitmap.html`；页面显示
`bitmap.js: 8 tests passed` 即为通过。

完整构建验证：

```bash
pio run
pio run -t buildfs
```

屏幕刷新、残影和 RST/BUSY 引脚行为依赖实际硬件，需要在设备上验证。
