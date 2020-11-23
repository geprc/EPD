//DOM elements
let canvas = document.getElementById('canvas');
let sendBtn = document.getElementById('send');
let clearBtn = document.getElementById('clear');

//canvas
let ctx = canvas.getContext('2d');
let canvasWidth = window.innerWidth * 0.4; //set canvas to 80% of screen
let scaleFactor = canvasWidth / 200; //scale to ePaper display's width
let canvasHeight = canvasWidth; //296 x 128 aspect ratio
canvas.style.width = canvasWidth + 'px';
canvas.style.height = canvasHeight + 'px';
ctx.canvas.width = 200;
ctx.canvas.height = 200;
let scaleFactorX = canvas.offsetWidth / 200;
let scaleFactorY = canvas.offsetHeight / 200;
let lineWidth = 1;
console.log(ctx.canvas.offsetWidth);
console.log(ctx.canvas.offsetHeight);

//websockets
//document.location.host
// let socket = new WebSocket("ws://" + document.location.host + ":81");
// socket.binaryType = 'arraybuffer';
// socket.onopen = () => { console.log('connected!'); }

//events
canvas.ontouchstart = start;
canvas.onmousedown = start;
canvas.ontouchend = stop;
canvas.onmouseup = stop;
canvas.ontouchmove = move;
canvas.onmousemove = move;
sendBtn.onclick = send;
clearBtn.onclick = clearCanvas;

function start(e) {
    e.preventDefault();
    let x, y;
    if (e.type == 'touchstart') {
        touch = e.touches[0];
        x = touch.pageX - canvas.offsetLeft;
        y = touch.pageY - canvas.offsetTop;
    } else {
        x = e.offsetX;
        y = e.offsetY;
    }
    ctx.beginPath();
    ctx.moveTo(x / scaleFactor, y / scaleFactor);
    ctx.lineWidth = lineWidth;
    ctx.isDrawing = true;
}

function stop(e) {
    e.preventDefault();
    ctx.isDrawing = false;
}

function move(e) {
    e.preventDefault();
    let x, y;
    if (e.type == 'touchmove') {
        touch = e.touches[0];
        x = touch.pageX - canvas.offsetLeft;
        y = touch.pageY - canvas.offsetTop;
    } else {
        x = e.offsetX;
        y = e.offsetY;
    }
    if (ctx.isDrawing) {
        ctx.lineTo(x / scaleFactor, y / scaleFactor);
        ctx.stroke();
    }
}

function canvasToByteArray() {
    let byteArray = [];
    let imageData = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
    let byteIndex = 7;
    let byte = 0;

    for (let i = 3; i < imageData.length; i += 4) { //RGBA format, since our image is B&W, we only care about A (4th position)
        if (imageData[i] > 0) { //either black or white, no grayscale
            byte += Math.pow(2, byteIndex); //bit shift on black pixel
        }
        byteIndex--;
        if (i == imageData.length - 4) {
            for (let i = byteIndex; i > -1; i--) { //wrap up loose ends
                byte += Math.pow(2, i);
            }
            byteIndex = -1;
        }
        if (byteIndex < 0) { //once we have a full byte, push to array
            byteArray.push(byte);
            byte = 0;
            byteIndex = 7;
        }
    }
    return byteArray;
}

function send() {
    let byteArray = new Uint8Array(canvasToByteArray());
    socket.send(byteArray.buffer);
}

function clearCanvas() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    send();
}