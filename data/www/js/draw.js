(() => {
    'use strict';

    // 必须与固件中的 GxEPD2_154 分辨率和 WebSocket 端口保持一致。
    const WIDTH = 200;
    const HEIGHT = 200;
    const WEBSOCKET_PORT = 81;
    const RECONNECT_DELAY_MS = 1500;
    const MAX_UNDO_STEPS = 12;

    const canvas = document.querySelector('#canvas');
    const context = canvas.getContext('2d', { willReadFrequently: true });
    const sendButton = document.querySelector('#send');
    const clearButton = document.querySelector('#clear');
    const undoButton = document.querySelector('#undo');
    const penButton = document.querySelector('#pen');
    const eraserButton = document.querySelector('#eraser');
    const brushSize = document.querySelector('#brush-size');
    const brushSizeValue = document.querySelector('#brush-size-value');
    const textInput = document.querySelector('#text-input');
    const textSize = document.querySelector('#text-size');
    const addTextButton = document.querySelector('#add-text');
    const imageInput = document.querySelector('#image-input');
    const invertButton = document.querySelector('#invert');
    const status = document.querySelector('#status');
    const connection = document.querySelector('#connection');
    const connectionText = document.querySelector('#connection-text');

    let socket;
    let reconnectTimer;
    let drawing = false;
    let erasing = false;
    const history = EpdBitmap.createHistory(MAX_UNDO_STEPS);

    function setStatus(message, state = '') {
        status.textContent = message;
        status.className = `status ${state}`.trim();
    }

    function setConnection(online, label) {
        connection.classList.toggle('online', online);
        connectionText.textContent = label;
    }

    function clearCanvas() {
        context.fillStyle = '#fff';
        context.fillRect(0, 0, WIDTH, HEIGHT);
    }

    function saveHistory() {
        history.push(context.getImageData(0, 0, WIDTH, HEIGHT));
        undoButton.disabled = false;
    }

    function undo() {
        const previous = history.pop();
        if (previous) context.putImageData(previous, 0, 0);
        undoButton.disabled = history.length === 0;
    }

    function selectTool(useEraser) {
        erasing = useEraser;
        penButton.classList.toggle('active', !erasing);
        eraserButton.classList.toggle('active', erasing);
        penButton.setAttribute('aria-pressed', String(!erasing));
        eraserButton.setAttribute('aria-pressed', String(erasing));
        canvas.style.cursor = erasing ? 'cell' : 'crosshair';
    }

    function addText() {
        const value = textInput.value.trim();
        if (!value) {
            textInput.focus();
            return;
        }
        saveHistory();
        context.save();
        context.fillStyle = '#000';
        context.textAlign = 'center';
        context.textBaseline = 'middle';
        let size = Number(textSize.value);
        context.font = `700 ${size}px system-ui, sans-serif`;
        // 较长文字自动缩小，避免超出屏幕边缘。
        while (size > 10 && context.measureText(value).width > WIDTH - 16) {
            size -= 1;
            context.font = `700 ${size}px system-ui, sans-serif`;
        }
        context.fillText(value, WIDTH / 2, HEIGHT / 2, WIDTH - 16);
        context.restore();
        textInput.value = '';
        setStatus('文字已添加到画布中央。');
    }

    function makeMonochrome() {
        const image = context.getImageData(0, 0, WIDTH, HEIGHT);
        EpdBitmap.makeMonochrome(image.data, WIDTH, HEIGHT);
        context.putImageData(image, 0, 0);
    }

    function importImage(file) {
        if (!file || !file.type.startsWith('image/')) return;
        const image = new Image();
        const url = URL.createObjectURL(file);
        image.addEventListener('load', () => {
            saveHistory();
            clearCanvas();
            // 保持原图比例并完整放入方形屏幕，空白区域自动留白。
            const scale = Math.min(WIDTH / image.width, HEIGHT / image.height);
            const width = image.width * scale;
            const height = image.height * scale;
            context.drawImage(image, (WIDTH - width) / 2, (HEIGHT - height) / 2,
                width, height);
            makeMonochrome();
            URL.revokeObjectURL(url);
            imageInput.value = '';
            setStatus('图片已转换为 1-bit 黑白效果。');
        });
        image.addEventListener('error', () => {
            URL.revokeObjectURL(url);
            imageInput.value = '';
            setStatus('无法读取这张图片。', 'error');
        });
        image.src = url;
    }

    function invertCanvas() {
        saveHistory();
        const image = context.getImageData(0, 0, WIDTH, HEIGHT);
        EpdBitmap.invertRgba(image.data, WIDTH, HEIGHT);
        context.putImageData(image, 0, 0);
        setStatus('画布颜色已反相。');
    }

    function canvasPoint(event) {
        // 将页面上的缩放坐标映射回屏幕固定的 200 × 200 像素坐标。
        const bounds = canvas.getBoundingClientRect();
        return {
            x: (event.clientX - bounds.left) * WIDTH / bounds.width,
            y: (event.clientY - bounds.top) * HEIGHT / bounds.height,
        };
    }

    function startDrawing(event) {
        if (event.button !== 0 && event.pointerType === 'mouse') return;
        event.preventDefault();
        saveHistory();
        drawing = true;
        canvas.setPointerCapture(event.pointerId);
        const point = canvasPoint(event);
        context.strokeStyle = erasing ? '#fff' : '#000';
        context.lineWidth = Number(brushSize.value);
        context.beginPath();
        context.moveTo(point.x, point.y);
        // 先画一个点，确保轻触画布也会留下笔迹。
        context.lineTo(point.x + 0.01, point.y + 0.01);
        context.stroke();
    }

    function continueDrawing(event) {
        if (!drawing) return;
        event.preventDefault();
        const point = canvasPoint(event);
        context.lineTo(point.x, point.y);
        context.stroke();
    }

    function stopDrawing(event) {
        if (!drawing) return;
        event.preventDefault();
        drawing = false;
        context.closePath();
        if (canvas.hasPointerCapture(event.pointerId)) {
            canvas.releasePointerCapture(event.pointerId);
        }
    }

    function packBitmap() {
        const pixels = context.getImageData(0, 0, WIDTH, HEIGHT).data;
        // 转为 Adafruit GFX 使用的行优先、MSB-first 单色位图。
        return EpdBitmap.packRgba(pixels, WIDTH, HEIGHT);
    }

    function sendBitmap() {
        if (!socket || socket.readyState !== WebSocket.OPEN) {
            setStatus('设备尚未连接，请稍后重试。', 'error');
            return;
        }
        socket.send(packBitmap());
        setStatus('已发送，墨水屏刷新需要几秒。', 'connected');
    }

    function connectWebSocket() {
        clearTimeout(reconnectTimer);
        const protocol = location.protocol === 'https:' ? 'wss' : 'ws';
        socket = new WebSocket(`${protocol}://${location.hostname}:${WEBSOCKET_PORT}`);
        socket.binaryType = 'arraybuffer';
        socket.addEventListener('open', () => {
            sendButton.disabled = false;
            setConnection(true, '已连接');
            setStatus('设备已连接', 'connected');
        });
        socket.addEventListener('close', () => {
            sendButton.disabled = true;
            setConnection(false, '离线');
            setStatus('连接已断开，正在重连…', 'error');
            reconnectTimer = setTimeout(connectWebSocket, RECONNECT_DELAY_MS);
        });
        socket.addEventListener('error', () => setStatus('连接失败，正在重试…', 'error'));
    }

    canvas.width = WIDTH;
    canvas.height = HEIGHT;
    context.lineCap = 'round';
    context.lineJoin = 'round';
    clearCanvas();

    canvas.addEventListener('pointerdown', startDrawing);
    canvas.addEventListener('pointermove', continueDrawing);
    canvas.addEventListener('pointerup', stopDrawing);
    canvas.addEventListener('pointercancel', stopDrawing);
    sendButton.addEventListener('click', sendBitmap);
    undoButton.addEventListener('click', undo);
    penButton.addEventListener('click', () => selectTool(false));
    eraserButton.addEventListener('click', () => selectTool(true));
    brushSize.addEventListener('input', () => {
        brushSizeValue.value = `${brushSize.value} px`;
    });
    addTextButton.addEventListener('click', addText);
    textInput.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') addText();
    });
    imageInput.addEventListener('change', () => importImage(imageInput.files[0]));
    invertButton.addEventListener('click', invertCanvas);
    clearButton.addEventListener('click', () => {
        saveHistory();
        clearCanvas();
        setStatus('画布已清空。');
    });

    document.addEventListener('keydown', (event) => {
        if (event.target.matches('input, select')) return;
        if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'z') {
            event.preventDefault();
            undo();
        } else if (event.key.toLowerCase() === 'p') {
            selectTool(false);
        } else if (event.key.toLowerCase() === 'e') {
            selectTool(true);
        }
    });

    if (location.protocol === 'file:') {
        setConnection(false, '预览');
        setStatus('本地预览模式；发送功能需从设备页面使用。');
    } else {
        connectWebSocket();
    }
})();
