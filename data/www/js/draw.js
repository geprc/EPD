(() => {
    'use strict';

    // 必须与固件中的 GxEPD2_154 分辨率和 WebSocket 端口保持一致。
    const DISPLAY_WIDTH = 200;
    const DISPLAY_HEIGHT = 200;
    const WEBSOCKET_PORT = 81;
    const RECONNECT_DELAY_MS = 1500;

    const canvas = document.querySelector('#canvas');
    const sendButton = document.querySelector('#send');
    const clearButton = document.querySelector('#clear');
    const status = document.querySelector('#status');
    const context = canvas.getContext('2d', { willReadFrequently: true });

    let socket = null;
    let reconnectTimer = null;
    let isDrawing = false;

    function setStatus(message, state = '') {
        status.textContent = message;
        status.className = `status ${state}`.trim();
    }

    function clearCanvas() {
        context.fillStyle = '#fff';
        context.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    function canvasPoint(event) {
        // Canvas 内部分辨率固定为 200 × 200，CSS 可以任意缩放显示尺寸。
        // 指针坐标需要从页面坐标系映射回屏幕像素坐标系。
        const bounds = canvas.getBoundingClientRect();
        return {
            x: (event.clientX - bounds.left) * DISPLAY_WIDTH / bounds.width,
            y: (event.clientY - bounds.top) * DISPLAY_HEIGHT / bounds.height,
        };
    }

    function startDrawing(event) {
        event.preventDefault();
        const point = canvasPoint(event);
        isDrawing = true;
        canvas.setPointerCapture(event.pointerId);
        context.beginPath();
        context.moveTo(point.x, point.y);
    }

    function continueDrawing(event) {
        if (!isDrawing) return;
        event.preventDefault();
        const point = canvasPoint(event);
        context.lineTo(point.x, point.y);
        context.stroke();
    }

    function stopDrawing(event) {
        if (!isDrawing) return;
        event.preventDefault();
        isDrawing = false;
        context.closePath();
    }

    function packBitmap() {
        const pixels = context.getImageData(
            0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT
        ).data;
        const bitmap = new Uint8Array(DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);

        // 固件和 Adafruit GFX 都使用行优先、MSB-first 的 1-bit 位图格式。
        // 浏览器画布允许抗锯齿，因此先按亮度阈值将像素二值化。
        for (let pixelIndex = 0; pixelIndex < DISPLAY_WIDTH * DISPLAY_HEIGHT;
            pixelIndex += 1) {
            const channelIndex = pixelIndex * 4;
            const luminance = (
                pixels[channelIndex] * 299
                + pixels[channelIndex + 1] * 587
                + pixels[channelIndex + 2] * 114
            ) / 1000;
            if (luminance < 128) {
                bitmap[pixelIndex >> 3] |= 0x80 >> (pixelIndex & 7);
            }
        }
        return bitmap;
    }

    function sendBitmap() {
        if (!socket || socket.readyState !== WebSocket.OPEN) {
            setStatus('设备尚未连接，请稍后重试。', 'error');
            return;
        }
        socket.send(packBitmap());
        setStatus('图像已发送，墨水屏刷新需要几秒。', 'connected');
    }

    function connectWebSocket() {
        // 使用 hostname 而不是 host，避免页面 URL 显式包含 :80 时形成 ip:80:81。
        const protocol = location.protocol === 'https:' ? 'wss' : 'ws';
        socket = new WebSocket(
            `${protocol}://${location.hostname}:${WEBSOCKET_PORT}`
        );
        socket.binaryType = 'arraybuffer';

        socket.addEventListener('open', () => {
            sendButton.disabled = false;
            setStatus('设备已连接，可以开始绘制。', 'connected');
        });
        socket.addEventListener('close', () => {
            sendButton.disabled = true;
            setStatus('连接已断开，正在重连…', 'error');
            clearTimeout(reconnectTimer);
            // 设备重启或 Wi-Fi 短暂中断后自动恢复，无需刷新页面。
            reconnectTimer = setTimeout(connectWebSocket, RECONNECT_DELAY_MS);
        });
        socket.addEventListener('error', () => {
            setStatus('WebSocket 连接失败。', 'error');
        });
    }

    canvas.width = DISPLAY_WIDTH;
    canvas.height = DISPLAY_HEIGHT;
    context.strokeStyle = '#000';
    context.lineWidth = 2;
    context.lineCap = 'round';
    context.lineJoin = 'round';
    clearCanvas();

    canvas.addEventListener('pointerdown', startDrawing);
    canvas.addEventListener('pointermove', continueDrawing);
    canvas.addEventListener('pointerup', stopDrawing);
    canvas.addEventListener('pointercancel', stopDrawing);
    sendButton.addEventListener('click', sendBitmap);
    clearButton.addEventListener('click', () => {
        clearCanvas();
        setStatus('画布已清空。');
    });

    sendButton.disabled = true;
    connectWebSocket();
})();
