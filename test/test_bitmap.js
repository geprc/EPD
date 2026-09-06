'use strict';

const api = typeof module === 'object' && module.exports
    ? require('../data/www/js/bitmap.js')
    : globalThis.EpdBitmap;
let passed = 0;

function equal(actual, expected, message) {
    const actualText = JSON.stringify(Array.from(actual));
    const expectedText = JSON.stringify(expected);
    if (actualText !== expectedText) {
        throw new Error(`${message}: expected ${expectedText}, got ${actualText}`);
    }
    passed += 1;
}

function throws(callback, message) {
    try {
        callback();
    } catch (error) {
        passed += 1;
        return;
    }
    throw new Error(`${message}: expected an exception`);
}

function rgba(values) {
    return new Uint8ClampedArray(values.flatMap(value => [value, value, value, 255]));
}

equal(api.packRgba(rgba([0, 255, 0, 255, 0, 255, 0, 255]), 8, 1), [0xaa], 'MSB-first packing');
equal(api.packRgba(rgba([0, 255, 0]), 3, 1), [0xa0], 'partial byte padding');
equal(api.packRgba(rgba([127, 128]), 2, 1), [0x80], 'threshold boundary');

const monochrome = rgba([20, 240]);
api.makeMonochrome(monochrome, 2, 1);
equal(monochrome, [0, 0, 0, 255, 255, 255, 255, 255], 'monochrome conversion');

const inverted = new Uint8ClampedArray([10, 20, 30, 99]);
api.invertRgba(inverted, 1, 1);
equal(inverted, [245, 235, 225, 99], 'RGBA inversion preserves alpha');
throws(() => api.packRgba(new Uint8ClampedArray(3), 1, 1), 'invalid RGBA length');

const history = api.createHistory(2);
history.push('first');
history.push('second');
history.push('third');
equal([history.length, history.pop(), history.pop()], [2, 'third', 'second'], 'history limit and LIFO order');
throws(() => api.createHistory(0), 'invalid history limit');

const result = `bitmap.js: ${passed} tests passed`;
if (typeof document === 'object') {
    document.body.dataset.status = 'passed';
    document.body.textContent = result;
} else {
    console.log(result);
}
