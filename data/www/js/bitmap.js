(function (root, factory) {
    'use strict';
    const api = factory();
    if (typeof module === 'object' && module.exports) module.exports = api;
    root.EpdBitmap = api;
}(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    function validate(data, width, height) {
        if (!data || data.length !== width * height * 4) {
            throw new RangeError('RGBA data length does not match dimensions');
        }
    }

    function luminance(data, channel) {
        return (data[channel] * 299
            + data[channel + 1] * 587
            + data[channel + 2] * 114) / 1000;
    }

    function packRgba(data, width, height, threshold = 128) {
        validate(data, width, height);
        const bitmap = new Uint8Array(Math.ceil(width * height / 8));
        for (let pixel = 0; pixel < width * height; pixel += 1) {
            if (luminance(data, pixel * 4) < threshold) {
                bitmap[pixel >> 3] |= 0x80 >> (pixel & 7);
            }
        }
        return bitmap;
    }

    function makeMonochrome(data, width, height, threshold = 128) {
        validate(data, width, height);
        for (let channel = 0; channel < data.length; channel += 4) {
            const value = luminance(data, channel) < threshold ? 0 : 255;
            data[channel] = value;
            data[channel + 1] = value;
            data[channel + 2] = value;
            data[channel + 3] = 255;
        }
        return data;
    }

    function invertRgba(data, width, height) {
        validate(data, width, height);
        for (let channel = 0; channel < data.length; channel += 4) {
            data[channel] = 255 - data[channel];
            data[channel + 1] = 255 - data[channel + 1];
            data[channel + 2] = 255 - data[channel + 2];
        }
        return data;
    }

    function createHistory(limit) {
        if (!Number.isInteger(limit) || limit < 1) {
            throw new RangeError('History limit must be a positive integer');
        }
        const entries = [];
        return {
            push(value) {
                entries.push(value);
                if (entries.length > limit) entries.shift();
            },
            pop() { return entries.pop(); },
            get length() { return entries.length; },
        };
    }

    return { packRgba, makeMonochrome, invertRgba, createHistory };
}));
